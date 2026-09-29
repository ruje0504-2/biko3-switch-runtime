"""Retained outer backgrounds during actual4CF318/4D00FA loading.

Run the original loaders with an existing global background, including stale
world/parent caches, nonzero playback and hidden nodes. Resource decoding,
face, device and refcount services use the established asset-oracle boundaries;
native loader calls, attachment, force publication and camera math execute.
Groups0/2/3/4 retain their background. Group1's replacement is covered by the
existing full loader oracles and the two-direction scene reload probe.
"""
import argparse
import ctypes as C
import gc
import hashlib
import json
import math
import struct
from pathlib import Path

from bk3_assets import Archive
from clip_binding import State as ClipState
from model_binding import Model, Material, ROOT
from playback_binding import library
from original_bom_assets_oracle import Config, MaterialNative
from original_ending_normal_assets_oracle import Native as NormalNative
from original_ending_secondary_assets_oracle import Native as SecondaryNative, bindings
from original_ending_camera_assets_oracle import State, Presets, I, values

NONE = 0xffffffff
F16 = C.c_float * 16


class Background(C.Structure):
    _fields_ = [('model', C.POINTER(Model))] + [(name, C.c_void_p) for name in
        ['clips', 'pose', 'materials', 'animation', 'morph']] + [('root', C.c_uint32)]


class AlphaEdit(C.Structure):
    _fields_ = [('frame', C.c_uint32), ('alpha', C.c_float),
                ('rules', C.c_void_p), ('rule_count', C.c_size_t)]


def bind(lib):
    bindings(lib)
    definitions = [
        ('bk_ending_background_assets_create', [C.c_void_p, C.c_char_p, C.c_void_p], C.c_void_p),
        ('bk_ending_background_assets_destroy', [C.c_void_p], None),
        ('bk_ending_background_assets_data', [C.c_void_p], C.POINTER(Background)),
        ('bk_ending_background_assets_advance', [C.c_void_p, C.c_float, C.c_void_p], C.c_int),
        ('bk_actor_pose_request_mode', [C.c_void_p, C.c_uint, C.c_int, C.c_void_p], C.c_int),
        ('bk_actor_forest_visibility', [C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p], C.c_int),
        ('bk_actor_forest_tree', [C.c_void_p], C.c_void_p),
        ('bk_material_pose_alpha', [C.c_void_p, C.POINTER(AlphaEdit), C.c_size_t, C.c_void_p], C.c_int),
        ('bk_material_pose_material', [C.c_void_p, C.c_uint32], C.POINTER(Material)),
    ]
    for kind in ['normal', 'secondary']:
        definitions.extend([
            (f'bk_ending_{kind}_assets_create_reloaded', [C.c_void_p, C.c_uint,
             C.c_uint, C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
             C.POINTER(State), C.POINTER(Presets), C.c_void_p], C.c_void_p),
            (f'bk_ending_{kind}_assets_background', [C.c_void_p], C.c_void_p),
            (f'bk_ending_{kind}_assets_forest', [C.c_void_p], C.c_void_p),
        ])
    for name in ['first', 'next', 'parent']:
        definitions.append((f'bk_frame_tree_{name}', [C.c_void_p, C.c_uint32], C.c_uint32))
    for name, args, result in definitions:
        function = getattr(lib, name)
        function.argtypes, function.restype = args, result


def material_bytes(lib, data):
    return b''.join(bytes(lib.bk_material_pose_material(data.materials, i).contents)
                    for i in range(data.model.contents.material_count))


def capture(lib, pose, model):
    result = []
    for frame in range(model.frame_count):
        hidden = C.c_uint32()
        assert lib.bk_actor_pose_hidden(pose, frame, C.byref(hidden))
        result.append(([getattr(lib, 'bk_actor_pose_' + getter)(pose, frame)[:16]
                        for getter in ['local', 'frame', 'parent_world']], hidden.value))
    return result


def restore_native_background(vm, index, saved):
    """Import the existing topology/caches, with no synthetic refresh call."""
    base, root = vm.bases[index], vm.bases[index] + vm.root_ids[index] * 0x400
    for frame, (matrices, hidden) in enumerate(saved):
        for off, matrix in zip([0x80, 0xc0, 0x100], matrices):
            vm.vector(base + frame * 0x400 + off, matrix)
        vm.word(base + frame * 0x400 + 0x70, hidden)
    link = 0x373e0000
    vm.word(vm.context + 0x238, 2)
    vm.word(vm.context + 0x230, link)
    vm.word(vm.context + 0x234, link + 16)
    vm.word(link, vm.rendered)
    vm.word(link + 4, 0)
    vm.word(link + 8, link + 16)
    vm.word(link + 16, root)
    vm.word(link + 20, link)
    vm.word(link + 24, 0)
    vm.word(root + 0x22c, vm.context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--limit', type=int, default=32)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-background-reload.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    lib = library()
    bind(lib)
    err = C.create_string_buffer(256)
    store = lib.bk_resources_create(err)
    assert store, err.value
    arcs = {}
    for pack in ['bk3_08', 'bk3_09', 'bk3_04', 'bk3_03', 'fambom']:
        path = args.data / (pack + '.pp')
        assert lib.bk_resources_mount(store, pack.encode(), str(path).encode(), err), err.value
        arcs[pack] = Archive(path)

    def read(pack, name):
        return arcs[pack].read(next(entry for entry in arcs[pack].entries if entry.name == name))

    files = {entry.name.lower().encode(): arcs['bk3_08'].read(entry)
             for entry in arcs['bk3_08'].entries if entry.name.endswith('.vix')}
    cases = [(kind, group, variant, hidden) for kind in ['normal', 'secondary']
             for group in [0, 2, 3, 4] for variant in range(2) for hidden in [0, 7]]
    matrices = clocks = names_checked = 0
    worst = 0.0
    records = []

    def equal(actual, expected, label):
        nonlocal worst
        assert len(actual) == len(expected), label
        for index, (a, b) in enumerate(zip(actual, expected)):
            if a == b:
                continue
            error = abs(a - b) / max(1, abs(b))
            worst = max(worst, error)
            assert math.isfinite(error) and error < 3e-5, (label, index, a, b, error)

    try:
        for case, (kind, group, variant, hidden) in enumerate(cases[:args.limit]):
            background = old_forest = owner = morph = None
            baseline, clips = [], []
            vm = mat = None
            try:
                name = lib.bk_ending_normal_background(group, variant).decode()
                background = lib.bk_ending_background_assets_create(store, name.encode(), err)
                assert background, err.value
                data = lib.bk_ending_background_assets_data(background).contents
                old_forest = lib.bk_actor_forest_create((C.c_void_p * 1)(data.pose), 1, err)
                assert old_forest, err.value
                root = lib.bk_actor_forest_node(old_forest, 0, data.root)
                assert lib.bk_actor_forest_attach(old_forest, 0, root, err), err.value
                assert lib.bk_actor_pose_request_mode(data.pose, 0, 0, err), err.value
                for seconds in [.017, .031, .2]:
                    assert lib.bk_ending_background_assets_advance(background, seconds, err), err.value
                    assert lib.bk_actor_forest_refresh(old_forest, err), err.value
                # A retained cache can be hidden and differ from its current
                # local transform. Native new-actor attachment force-publishes
                # it; neither a disk reload nor an early fake refresh may win.
                local = F16(*lib.bk_actor_pose_local(data.pose, data.root)[:16])
                local[12] += 3.25 + case
                local[13] -= 2.75
                assert lib.bk_actor_pose_root_local(data.pose, local, err), err.value
                assert lib.bk_actor_forest_visibility(old_forest, root, hidden, err), err.value
                edit = AlphaEdit(data.root, .375, None, 0)
                assert lib.bk_material_pose_alpha(data.materials, C.byref(edit), 1, err), err.value
                before = capture(lib, data.pose, data.model.contents)
                material = material_bytes(lib, data)
                before_clip = ClipState()
                assert lib.bk_actor_pose_state(data.pose, C.byref(before_clip))
                state = State()
                state.pose.world[:] = state.matrix[:] = I
                state.pose.world[12] = 21
                state.matrix[12] = -17
                state.focus[:] = [7, 8, 9]
                state.fov = .4
                initial = State.from_buffer_copy(state)
                presets, rng = Presets(), C.c_uint32(123 + case)
                create = getattr(lib, f'bk_ending_{kind}_assets_create_reloaded')
                owner = create(store, group, variant, background,
                    (C.c_uint32 * 4)(100, 110, 120, 130), C.byref(rng),
                    C.byref(state), C.byref(presets), err)
                assert owner, (case, err.value)
                bg_index = 4 if kind == 'normal' else 3
                poses = [getattr(lib, f'bk_ending_{kind}_assets_pose')(owner, i)
                         for i in range(bg_index + 1)]
                assert poses[-1] == data.pose
                assert getattr(lib, f'bk_ending_{kind}_assets_background')(owner) == background
                models = [lib.bk_actor_pose_model(pose).contents for pose in poses]
                xan_names = ([f'h{group + 1:02}_00.xan', f'h{group + 1:02}_01.xan']
                             if kind == 'normal' else [f'h{group + 1:02}_02.xan'])
                packs = ['bk3_08'] * 2 if kind == 'normal' else ['bk3_09']
                xan_names += ['cam00_00.xan', 'cam00_03.xan', name]
                packs += ['bk3_04', 'bk3_04', 'bk3_03']
                xans = [read(pack, filename) for pack, filename in zip(packs, xan_names)]
                names = [xan[:256].split(b'\0')[0] for xan in xans]
                if kind == 'secondary':
                    vm = SecondaryNative(exe)
                    for i, (model, xan, filename) in enumerate(zip(models, xans, xan_names)):
                        vm.add_actor(model, xan, filename.encode(), i)
                    vm.prepare(group, initial)
                else:
                    for model, xan in zip(models[:2], xans[:2]):
                        clip = lib.bk_clip_set_decode(xan, len(xan), err)
                        assert clip, err.value
                        clips.append(clip)
                        actor_root = next(i for i in range(model.frame_count)
                                          if model.frames[i].parent_index == NONE)
                        pose = lib.bk_actor_pose_create_loaded(C.byref(model), clip,
                            actor_root, (C.c_float * 3)(0, 0, 0), 0, err)
                        assert pose, err.value
                        baseline.append(pose)
                    raw_bom = read('fambom', f'h{group + 1:02}_00.bom')
                    bom = Config()
                    assert lib.bk_bom_decode(raw_bom, len(raw_bom), C.byref(bom), err), err.value
                    vm = NormalNative(exe, files)
                    raw = C.string_at(models[1].source, models[1].source_size)
                    vm.bind(raw, models[1])
                    seed = struct.unpack_from('<i', xans[1], 512 + 0x140)[0]
                    vm.bind_clip(xans[1], vm.root, seed, False)
                    morph = lib.bk_model_morph_create(C.byref(models[1]), err)
                    assert morph, err.value
                    vm.morph(lib, models[1], morph)
                    mat = MaterialNative(exe, u=vm.uc)
                    chunk = next(chunk for chunk in models[1].chunks[:models[1].chunk_count]
                                 if chunk.tag == b'MATA')
                    mat.load(raw[chunk.offset:chunk.offset + chunk.size], models[1])
                    vm.word(vm.model + 0x158, mat.group)
                    vm.prepare(lib, models, baseline, xans, names, bom,
                               group, variant, initial)
                restore_native_background(vm, bg_index, before)
                native_clip = bytes(vm.uc.mem_read(vm.clips[bg_index], 0x160))
                vm.call(0x4cf318 if kind == 'normal' else 0x4d00fa, b'')
                assert vm.load_names == [filename.encode() for filename in xan_names[:-1]], vm.load_names
                names_checked += len(vm.load_names)
                assert bytes(vm.uc.mem_read(vm.clips[bg_index], 0x160)) == native_clip
                after_clip = ClipState()
                assert lib.bk_actor_pose_state(data.pose, C.byref(after_clip))
                assert bytes(before_clip) == bytes(after_clip)
                assert material == material_bytes(lib, data)
                for actor, model in enumerate(models):
                    for frame in range(model.frame_count):
                        for getter, off in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                            equal(getattr(lib, 'bk_actor_pose_' + getter)(poses[actor], frame)[:16],
                                  vm.floats(vm.bases[actor] + frame * 0x400 + off, 16),
                                  (case, kind, actor, frame, getter))
                            matrices += 1
                        visible = C.c_uint32()
                        assert lib.bk_actor_pose_hidden(poses[actor], frame, C.byref(visible))
                        assert visible.value == vm.read(vm.bases[actor] + frame * 0x400 + 0x70)
                    if actor != bg_index:
                        clock = ClipState()
                        assert lib.bk_actor_pose_state(poses[actor], C.byref(clock))
                        equal([getattr(clock, name) for name, _ in ClipState._fields_],
                              vm.clip_state(vm.clips[actor]), (case, kind, actor, 'clip'))
                        clocks += 1
                for target in range(3):
                    equal(getattr(lib, f'bk_ending_{kind}_assets_target')(owner, target)[:3],
                          vm.floats(0x70c8d8 + target * 12, 3), (case, kind, 'target', target))
                equal(values(state), values(vm.camera()), (case, kind, 'camera'))
                assert bytes(presets) == bytes(vm.uc.mem_read(0x71af38 + 0x444, 96))
                records.append(dict(kind=kind, group=group, variant=variant, hidden=hidden,
                                    background=name, frames=sum(model.frame_count for model in models)))
                print('PASS retained background', kind, group, variant, hidden, flush=True)
            finally:
                for pose in baseline:
                    lib.bk_actor_pose_destroy(pose)
                for clip in clips:
                    lib.bk_clip_set_destroy(clip)
                if morph:
                    lib.bk_model_morph_destroy(morph)
                if owner:
                    getattr(lib, f'bk_ending_{kind}_assets_destroy')(owner)
                if old_forest:
                    lib.bk_actor_forest_destroy(old_forest)
                if background:
                    lib.bk_ending_background_assets_destroy(background)
                vm = mat = None
                gc.collect()
    finally:
        lib.bk_resources_destroy(store)
    result = dict(exe_sha256=digest, cases=records, matrices=matrices,
                  clip_states=clocks, observed_load_names=names_checked,
                  max_relative_error=worst,
                  scope='retained-background original loader/cache and camera equivalence; no raster or natural-story claim')
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f'PASS retained background: {len(records)} loads; {matrices} matrices; '
          f'{clocks} clip states; error {worst}')


if __name__ == '__main__':
    main()
