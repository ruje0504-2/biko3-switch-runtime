"""Original4026fe ANIM/MATA/MORP on actual secondary-ending resource owners.

Initial placement/publication is imported from the separately verified4D00FA
loader. XAN state, configured requests, animation scheduling, SRT and all
material/vertex sampling execute original instructions. The fixture does not
run47A5D0, face controllers, UI, video, a renderer or a natural playthrough.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path

from bk3_assets import Archive
from clip_binding import State as ClipState
from model_binding import ROOT, Model, Material
from original_ending_secondary_assets_oracle import bindings
from original_ending_camera_assets_oracle import State as Camera, Presets, I
from original_fixed_actor_oracle import Native, Visibility
from original_material_animation_oracle import Native as MaterialNative
from playback_binding import library

EXE_SHA256 = "a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e"


def bind(lib):
    bindings(lib)
    lib.bk_actor_pose_model.argtypes = [C.c_void_p]
    lib.bk_actor_pose_model.restype = C.POINTER(Model)
    lib.bk_actor_pose_visibility.argtypes = [C.c_void_p, C.POINTER(Visibility), C.c_size_t, C.c_void_p]
    lib.bk_actor_pose_request_mode.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_void_p]
    for name, args, result in [
        ('model_name', [C.c_void_p, C.c_uint], C.c_char_p),
        ('advance', [C.c_void_p, C.c_uint, C.c_float, C.c_void_p], C.c_int),
        ('materials', [C.c_void_p, C.c_uint], C.c_void_p),
        ('material_animation', [C.c_void_p, C.c_uint], C.c_void_p),
        ('morph', [C.c_void_p, C.c_uint], C.c_void_p),
    ]:
        fn = getattr(lib, 'bk_ending_secondary_assets_' + name)
        fn.argtypes, fn.restype = args, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--limit', type=int, default=5, choices=range(1, 6))
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == EXE_SHA256, 'unsupported original EXE'
    lib = library()
    bind(lib)
    error = C.create_string_buffer(256)
    store = lib.bk_resources_create(error)
    assert store, error.value
    counts = dict(frames=0, matrices=0, vertices=0, material_fields=0,
                  undefined_material_fields=0, hidden_frames=0, rejections=0)
    worst = 0.0
    records = []

    def equal(got, want, label):
        nonlocal worst
        assert len(got) == len(want), label
        for index, (actual, expected) in enumerate(zip(got, want)):
            if actual == expected:
                continue
            difference = abs(actual - expected) / max(1, abs(expected))
            worst = max(worst, difference)
            assert math.isfinite(difference) and difference < 3e-5, (
                label, index, actual, expected, difference)

    try:
        for pack in ['bk3_09', 'bk3_04', 'bk3_03', 'fambom']:
            assert lib.bk_resources_mount(store, pack.encode(),
                str(args.data / (pack + '.pp')).encode(), error), error.value
        archives = {pack: Archive(args.data / (pack + '.pp'))
                    for pack in ['bk3_09', 'bk3_03']}

        def resource(pack, name):
            archive = archives[pack]
            return archive.read(next(entry for entry in archive.entries
                                     if entry.name.lower() == name.lower()))

        for group in range(args.limit):
            for variant in range(2):
                random = C.c_uint32(0x12345678 + group * 2 + variant)
                clocks = (C.c_uint32 * 4)(100, 101, 102, 103)
                camera, presets = Camera(), Presets()
                camera.pose.world[:] = I
                camera.matrix[:] = I
                owner = lib.bk_ending_secondary_assets_create(
                    store, group, variant, clocks, C.byref(random),
                    C.byref(camera), C.byref(presets), error)
                assert owner, (group, variant, error.value)
                try:
                    assert lib.bk_ending_secondary_assets_load_background(owner, store, error), error.value
                    for actor in [0, 3]:
                        pose = lib.bk_ending_secondary_assets_pose(owner, actor)
                        model_ptr = lib.bk_actor_pose_model(pose)
                        model = model_ptr.contents
                        name = lib.bk_ending_secondary_assets_model_name(owner, actor).decode()
                        pack = 'bk3_09' if actor == 0 else 'bk3_03'
                        raw = resource(pack, name)
                        xan = resource(pack, str(Path(name).with_suffix('.xan')))
                        vm = Native(exe)
                        vm.bind(raw, model)
                        # Same requested slot keeps every authored clock; then
                        # execute the real loader's4018c8 initial request.
                        requested = struct.unpack_from('<i', xan, 512 + 0x148)[0]
                        vm.bind_clip(xan, vm.root, requested, False)
                        vm.call(0x4018c8, struct.pack('<II', vm.clip, 1 if actor == 0 else 0))
                        for frame in range(model.frame_count):
                            for getter, offset in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                                data = C.string_at(getattr(lib, 'bk_actor_pose_' + getter)(pose, frame), 64)
                                vm.uc.mem_write(vm.frames + frame * 0x400 + offset, data)
                            hidden = C.c_uint32()
                            assert lib.bk_actor_pose_hidden(pose, frame, C.byref(hidden))
                            vm.word(vm.frames + frame * 0x400 + 0x70, hidden.value)
                        chunks = {chunk.tag: chunk for chunk in model.chunks[:model.chunk_count]}
                        morph = None
                        material_vm = None
                        try:
                            if b'MORP' in chunks:
                                morph = lib.bk_model_morph_create(model_ptr, error)
                                assert morph, error.value
                                vm.morph(lib, model, morph)
                            if b'MATA' in chunks:
                                chunk = chunks[b'MATA']
                                material_vm = MaterialNative(exe, u=vm.uc)
                                material_vm.load(raw[chunk.offset:chunk.offset + chunk.size], model)
                                vm.word(vm.model + 0x158, material_vm.group)

                            def check(label):
                                context = (group, variant, actor, name, label)
                                state = ClipState()
                                assert lib.bk_actor_pose_state(pose, C.byref(state))
                                equal([getattr(state, key) for key, _ in ClipState._fields_],
                                      vm.state(), (context, 'clock'))
                                for frame in range(model.frame_count):
                                    for getter, offset in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                                        equal(getattr(lib, 'bk_actor_pose_' + getter)(pose, frame)[:16],
                                              vm.floats(vm.frames + frame * 0x400 + offset, 16),
                                              (context, frame, getter))
                                        counts['matrices'] += 1
                                target = lib.bk_ending_secondary_assets_morph(owner, actor)
                                assert bool(target) == bool(morph)
                                if morph:
                                    assert lib.bk_morph_group_time(target) == vm.floats(vm.group_morph + 0x74, 1)[0]
                                    for submesh, ptr in vm.mesh_ids.items():
                                        count, vertices = vm.meshes[ptr]
                                        mesh = lib.bk_morph_group_mesh(target, submesh)
                                        got = C.string_at(lib.bk_morph_mesh_vertices(mesh), count * 60)
                                        want = bytes(vm.uc.mem_read(vertices, count * 60))
                                        if got != want:
                                            for vertex in range(count):
                                                offset = vertex * 60
                                                equal(struct.unpack_from('<9f', got, offset),
                                                      struct.unpack_from('<9f', want, offset),
                                                      (context, submesh, vertex))
                                                assert got[offset + 36:offset + 60] == want[offset + 36:offset + 60]
                                        counts['vertices'] += count
                                if material_vm:
                                    materials = lib.bk_ending_secondary_assets_materials(owner, actor)
                                    animation = lib.bk_ending_secondary_assets_material_animation(owner, actor)
                                    assert lib.bk_material_animation_time(animation) == vm.floats(material_vm.group + 0x74, 1)[0]
                                    for index, ptr in enumerate(material_vm.materials):
                                        material = lib.bk_material_pose_material(materials, index).contents
                                        got = struct.unpack('<17f', C.string_at(C.addressof(material) + Material.diffuse.offset, 68))
                                        want = vm.floats(ptr + 0x70, 17)
                                        for field in range(17):
                                            if material_vm.undefined[ptr] and field in [11, 15]:
                                                assert got[field] == 0
                                                counts['undefined_material_fields'] += 1
                                            else:
                                                equal([got[field]], [want[field]], (context, index, field))
                                                counts['material_fields'] += 1

                            check('loaded')
                            slots = [1, 2, 3, 9, 14, 15, 16] if actor == 0 else [0]
                            for step in range(140):
                                if step % 20 < 2:
                                    slot = slots[(step // 20) % len(slots)]
                                    vm.call(0x4018c8, struct.pack('<II', vm.clip, slot))
                                    assert lib.bk_actor_pose_request_mode(pose, slot, 1, error), error.value
                                hidden = 7 if step % 17 in [4, 5] else 0
                                visibility = Visibility(vm.root, hidden)
                                vm.call(0x423a99, struct.pack('<II', vm.frames + vm.root * 0x400, hidden))
                                assert lib.bk_actor_pose_visibility(pose, C.byref(visibility), 1, error), error.value
                                seconds = C.c_float([0, 1 / 60, .05, .1, .25][step % 5]).value
                                vm.call(0x4026fe, struct.pack('<If', vm.clip, seconds))
                                assert lib.bk_ending_secondary_assets_advance(owner, actor, seconds, error), error.value
                                check(step)
                                counts['frames'] += 1
                                counts['hidden_frames'] += bool(hidden)
                            for seconds in [-1, float('nan'), float('inf')]:
                                # Clear the root guard so each invalid step reaches
                                # validation; all observable data must stay intact.
                                visibility = Visibility(vm.root, 0)
                                assert lib.bk_actor_pose_visibility(pose, C.byref(visibility), 1, error)
                                assert not lib.bk_ending_secondary_assets_advance(owner, actor, seconds, error)
                                check(('rejected', str(seconds)))
                                counts['rejections'] += 1
                            records.append(dict(group=group, variant=variant, actor=actor, name=name,
                                model_sha256=hashlib.sha256(raw).hexdigest(), frames=140,
                                morph_tracks=vm.count if morph else 0, material_animation=material_vm is not None))
                            print('PASS secondary effects', records[-1], flush=True)
                        finally:
                            lib.bk_model_morph_destroy(morph)
                finally:
                    lib.bk_ending_secondary_assets_destroy(owner)
    finally:
        lib.bk_resources_destroy(store)
    result = dict(passed=True, exe_sha256=EXE_SHA256, **counts,
                  max_normalized_error=worst, records=records, scope=__doc__)
    (ROOT / 'local/original-ending-secondary-effects.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS', counts, 'max_normalized_error', worst, flush=True)


if __name__ == '__main__':
    main()
