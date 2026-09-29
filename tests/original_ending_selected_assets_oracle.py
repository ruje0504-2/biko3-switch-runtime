"""Actual4D1025/4D1D22 asset assembly and original ordinary model effects.

Independent decoded Japanese actors enter original attachment/orientation,
placement, visibility, configured requests, camera-root rotation,39 lookups,
4026fe(30.f), cached5/13/0 targets and4DF411. ANIM/MATA/MORP sampling executes
unchanged, including subsequent explicit requests/hidden/zero-dt fixtures.
Original4F2DAE face warm-up, FAM eye selection and4A7D10 material operations
are separate real component oracles. File/heap/mesh/texture IO and initial
decoded resources are boundaries. Retained background caches/materials are
explicit input state, not a prior full-game walkthrough. No claim of the
complete4D1025 UI/media shell, phase5/6 controller, renderer or Switch play.
"""
from __future__ import annotations
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
import math
from pathlib import Path
import struct

from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from bk3_assets import Archive
from clip_binding import State as ClipState
from ending_selected_binding import bind, Load, Config, CameraState, Presets, FaceState
from ending_mixed_effects_reference import MixedEffectsReference
from model_binding import ROOT, Model, Material, decode
from original_ending_tertiary_assets_oracle import Reference as Base, NONE, I, Vec
from original_ending_camera_oracle import Native as CameraNative, values as camera_values
from original_ending_face_init import EndingController
from original_eye_assets_oracle import Native as EyeNative
from original_ending_tertiary_material_oracle import Registry
from playback_binding import library


class Reference(MixedEffectsReference, Base):
    def fragment(self, first, last):
        if first != 0x4d182f:
            return super().fragment(first, last)
        #These selected models contain up to tens of thousands of ANIM keys.
        #The first native submission constructs its missing-channel caches;
        #the smaller tertiary oracle's20M instruction limit can expire inside
        #that legitimate work. Continue the SAME VM/register state in bounded
        #windows, never skip preprocessing or replace a sampler with C output.
        self.uc.reg_write(UC_X86_REG_EBP, self.stack)
        self.uc.reg_write(UC_X86_REG_ESP, self.stack - 0xd50)
        self.uc.reg_write(UC_X86_REG_FPCW, 0x037f)
        start = first
        for window in range(40):
            self.uc.emu_start(start, last, count=25000000)
            start = self.uc.reg_read(UC_X86_REG_EIP)
            if start == last:
                self.initial_sample_windows = window + 1
                return
            if window in [0, 7, 19]:
                print('Native initial ANIM window', window + 1, 'pc', hex(start), flush=True)
        raise AssertionError(('original initial sampling exceeded1B instructions', hex(start)))

    def body(self, group, variant, selection):
        self.uc.mem_write(0x721b3c, bytes([group, 0x7f]))
        self.word(0x721e04, variant)
        self.word(self.stack + 8, selection)
        self.fragment(0x4d152c, 0x4d1693)
        arguments = bytes(self.uc.mem_read(self.uc.reg_read(UC_X86_REG_ESP), 28))
        self.word(0x721b28, self.clips[0])
        self.attach(0, True)
        self.uc.mem_write(self.stack + 8, arguments)
        self.fragment(0x4d2032, 0x4d2099)
        self.fragment(0x4d169b, 0x4d1820)
        self.missing_visible = self.read(self.stack - 0x774)
        camera_yaw = self.read(0x71b354)
        for actor in [1, 2]:
            self.attach(actor)
        for actor in [1, 2]:
            self.call(0x401d24, struct.pack('<2I', self.clips[actor], 0))
        for actor in [1, 2]:
            self.word(0x721b28, self.clips[actor])
            self.word(self.stack + 0x14, camera_yaw)
            #The same original axis-angle42363B path, with the actual
            #camera yaw (which need not equal the primary's integer yaw).
            self.fragment(0x4d205d, 0x4d2099)
        self.word(0x721b28, self.clips[0])
        self.fragment(0x4d182f, 0x4d194d)  #Includes actual4026FE(30.f).
        self.fragment(0x4d194d, 0x4d1a81)  #Old published targets, not locals.
        self.word(0x719448, 0)
        self.call(0x425904, struct.pack('<3I', self.root(0), 0x575bfc, 0x719448))


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--groups', default='0,1,2,3,4')
    parser.add_argument('--variants', default='0,1')
    parser.add_argument('--selections', default='0,1,2')
    parser.add_argument('--reloads', type=int, choices=[0, 1], default=1)
    parser.add_argument('--steps', type=int, default=24)
    parser.add_argument('--mixed-effects', type=int, default=0,
                        help='Original and portable mixed primary effect steps (0 or a multiple of12 >=24)')
    parser.add_argument('--motion-controls', type=int, default=0,
                        help='Original motion edits and real effect steps (0 or a multiple of24 >=24)')
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-selected-assets.json')
    args = parser.parse_args()
    groups = [int(x) for x in args.groups.split(',')]
    variants = [int(x) for x in args.variants.split(',')]
    selections = [int(x) for x in args.selections.split(',')]
    for values, count in [(groups, 5), (variants, 2), (selections, 3)]:
        assert values and len(set(values)) == len(values) and all(0 <= x < count for x in values)
    assert args.steps >= 0
    assert args.mixed_effects == 0 or (args.mixed_effects >= 24 and args.mixed_effects % 12 == 0)
    assert args.motion_controls == 0 or (args.motion_controls >= 24 and args.motion_controls % 24 == 0)
    exe, lib = args.exe.read_bytes(), library()
    assert hashlib.sha256(exe).hexdigest() == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    bind(lib)
    error = C.create_string_buffer(256)
    packs = {p: Archive(args.data / (p + '.pp')) for p in ['bk3_10', 'bk3_13', 'bk3_03', 'bk3_04', 'fambom']}
    input_hashes = {}

    def read(pack, name):
        entry = next(e for e in packs[pack].entries if e.name.lower() == name.lower())
        raw = packs[pack].read(entry)
        input_hashes[pack + '/' + entry.name] = hashlib.sha256(raw).hexdigest()
        return raw

    store = lib.bk_resources_create(error)
    assert store, error.value
    for pack in packs:
        assert lib.bk_resources_mount(store, pack.encode(), str(args.data / (pack + '.pp')).encode(), error), error.value
    digest, records = hashlib.sha256(), []
    counters = dict(matrices=0, vertices=0, material_fields=0, undefined_w_fields=0,
                    effects=0, requests=0, hidden_updates=0, zero_seconds=0,
                    face_initializations=0, eye_selections=0, material_calls=0,
                    rejections=0, dropped_previous_background_owners=0)
    worst = 0.0
    camera_native, face_native, material_native = CameraNative(exe), EndingController(exe), Registry(exe)

    def equal(got, want, label):
        nonlocal worst
        assert len(got) == len(want), label
        for index, (actual, expected) in enumerate(zip(got, want)):
            if actual == expected:
                continue
            delta = abs(actual - expected) / max(1, abs(expected))
            worst = max(worst, delta)
            assert math.isfinite(delta) and delta < 3e-5, (label, index, actual, expected, delta)

    def clip_bytes(pose):
        state = ClipState()
        assert lib.bk_actor_pose_state(pose, C.byref(state))
        return bytes(state)

    def material_bytes(material):
        return C.string_at(C.addressof(material) + Material.diffuse.offset, 68)

    try:
        for group in groups:
            for variant in variants:
                for selection in selections:
                    modes = ['fresh']
                    if args.reloads:
                        modes.append('retained')
                        if group == 1 and variant == 0:
                            modes.append('replace')
                    for mode in modes:
                        owner = background = None
                        models, clips, poses, raws, xans, names = [], [], [], [], [], []
                        try:
                            config = Config()
                            assert lib.bk_ending_selected_config(C.byref(config), group, variant, selection)
                            selected = selection + 2 if mode == 'retained' else -1
                            replace = group == 1 and variant == 0 and mode != 'retained'
                            background_variant = 1 - variant if mode != 'fresh' else variant
                            retained_name = f'm{group+1:02}_{90+background_variant}.xan'
                            final_background = 'm02_90.xan' if replace else retained_name
                            specs = [(config.pack.decode(), config.primary.decode()),
                                     ('bk3_04', 'cam00_00.xan'), ('bk3_04', 'cam00_03.xan'),
                                     ('bk3_03', final_background)]
                            for pack, name in specs:
                                xan = read(pack, name)
                                model_name = xan[:256].split(b'\0')[0]
                                raw = read(pack, model_name.decode())
                                model = C.POINTER(Model)()
                                if raw.startswith(b'xof 0302txt 0032'):
                                    assert lib.bk_model_x_pose_decode(raw, len(raw), C.byref(model), error) == 1, error.value
                                else:
                                    code, model, message = decode(lib, raw)
                                    assert code == 1, message
                                models.append(model)
                                clip = lib.bk_clip_set_decode(xan, len(xan), error)
                                assert clip, error.value
                                clips.append(clip)
                                m = model.contents
                                root = next(i for i, f in enumerate(m.frames[:m.frame_count]) if f.parent_index == NONE)
                                pose = lib.bk_actor_pose_create_loaded(model, clip, root, Vec(0, 0, 0), 0, error)
                                assert pose, error.value
                                poses.append(pose)
                                assert lib.bk_actor_pose_root_local(pose, m.frames[root].local, error), error.value
                                raws.append(raw); xans.append(xan); names.append(model_name)
                            bg_initial_materials = None
                            ref_poses = list(poses)
                            if mode != 'fresh':
                                background = lib.bk_ending_background_assets_create(store, retained_name.encode(), error)
                                assert background, error.value
                                data = lib.bk_ending_background_assets_data(background).contents
                                assert lib.bk_actor_pose_request_mode(data.pose, 0, 1, error), error.value
                                assert lib.bk_ending_background_assets_advance(background, .175, error), error.value
                                old_background_clip = clip_bytes(data.pose)
                                if not replace:
                                    ref_poses[3] = data.pose
                                    bg_initial_materials = [material_bytes(lib.bk_material_pose_material(data.materials, i).contents)
                                                            for i in range(data.model.contents.material_count)]
                            vm = Reference(exe, {})
                            vm.exe = exe
                            vm.setup_actors(lib, [m.contents for m in models], ref_poses, names, raws, xans,
                                            linked=False, primary_animations=True)
                            vm.setup_materials(models, raws, {0: 0})
                            if mode == 'retained': vm.retain_background(3)
                            vm.body(group, variant, selection)
                            if replace:
                                vm.attach(3, True)
                                vm.call(0x4018c8, struct.pack('<2I', vm.clips[3], 0))
                            active = [0, 1, 2] + ([3] if replace or mode == 'retained' else [])
                            if bg_initial_materials is None:
                                bg_initial_materials = [material_bytes(m) for m in models[3].contents.materials[:models[3].contents.material_count]]
                            camera, presets = CameraState(), Presets()
                            camera.pose.world[:] = I; camera.matrix[:] = I
                            camera.pose.position[:] = [7, 8, 9]; camera.focus[:] = [5, -4, 3]
                            camera.yaw, camera.pitch, camera.radius, camera.height, camera.fov = 41, 9, 30, 3, .75
                            original_camera = CameraState.from_buffer_copy(camera)
                            C.memset(C.byref(presets), 0xa5, C.sizeof(presets))
                            initial_seed = 0x4d1025 + group * 97 + variant * 31 + selection * 7 + len(records)
                            seed, clocks = C.c_uint32(initial_seed), (C.c_uint32 * 4)(100, 1000, 1015, 1031)
                            load = Load(group, variant, selection, selected, b'\\' + config.primary, background)
                            owner = lib.bk_ending_selected_assets_create(store, C.byref(load), clocks,
                                C.byref(seed), C.byref(camera), C.byref(presets), error)
                            assert owner, (group, variant, selection, mode, error.value)
                            assert bytes(lib.bk_ending_selected_assets_config(owner).contents) == bytes(config)
                            assert lib.bk_ending_selected_assets_replaced_background(owner) == replace
                            assert lib.bk_ending_selected_assets_background_first(owner) == (mode == 'retained')
                            forest = lib.bk_ending_selected_assets_forest(owner)
                            assert forest
                            for actor in range(4):
                                assert bool(lib.bk_ending_selected_assets_pose(owner, actor)) == (actor in active)
                            for slot in range(39):
                                address = vm.read(0x721ef4 + slot * 4)
                                expected = (address - vm.frame_bases[0]) // 0x400 if address else NONE
                                assert lib.bk_ending_selected_assets_node(owner, slot) == expected
                            for slot in range(3):
                                address = vm.read(0x709ef8 + slot * 4)
                                expected = (address - vm.frame_bases[0]) // 0x400 if address else NONE
                                assert lib.bk_ending_selected_assets_visible_node(owner, slot) == expected
                                equal(lib.bk_ending_selected_assets_target(owner, slot)[:3], vm.floats(0x70c8d8 + slot * 12, 3), ('target', group, variant, selection, slot))
                            assert lib.bk_ending_selected_assets_missing_visible(owner) == vm.missing_visible
                            for name, address in [('anchor', 0x719b40), ('follow', 0x719448)]:
                                expected = (vm.read(address) - vm.frame_bases[0]) // 0x400
                                assert getattr(lib, 'bk_ending_selected_assets_' + name)(owner) == expected
                            special = vm.read(0x719b48 if variant else 0x719b44) if group == 2 else 0
                            expected = (special - vm.frame_bases[0]) // 0x400 if special else NONE
                            assert lib.bk_ending_selected_assets_special(owner) == expected
                            table = vm.floats(0x559a88 + (group * 10 + config.event) * 48, 12)
                            for field in range(4):
                                assert list(presets.active[field]) == list(presets.authored[field]) == [table[i * 4 + field] for i in range(3)]
                            original_camera.pose.position[:] = [0, 20, 0]
                            original_camera.yaw, original_camera.pitch, original_camera.radius, original_camera.height = table[:4]
                            original_camera.fov = 1
                            wanted_camera = camera_native.run(original_camera, 1, [0, 0], 0, 0,
                                Vec(*vm.floats(0x70c8d8, 3)), Vec(0, 0, 0), Vec(0, 0, 0), 16)
                            wanted_camera.fov = 1
                            equal(camera_values(camera), camera_values(wanted_camera), ('camera', group, variant, selection))
                            equal(lib.bk_actor_forest_world(forest, 1)[:16], wanted_camera.pose.world, ('camera anchor', group))

                            def poses_match(label):
                                tree = lib.bk_actor_forest_tree(forest)
                                by_node = {lib.bk_ending_selected_assets_root(owner, actor): actor for actor in active}
                                order, node = [], lib.bk_frame_tree_first(tree, 0)
                                while node != NONE:
                                    if node != 1:
                                        assert node in by_node and by_node[node] not in order
                                        order.append(by_node[node])
                                    node = lib.bk_frame_tree_next(tree, node)
                                original_order, link = [], vm.read(vm.context + 0x230)
                                native_nodes = {vm.root(actor): actor for actor in active}
                                while link:
                                    original_order.append(native_nodes[vm.read(link)])
                                    link = vm.read(link + 8)
                                assert order == original_order, (label, 'global order', order, original_order)
                                for actor in active:
                                    pose = lib.bk_ending_selected_assets_pose(owner, actor)
                                    for frame in range(models[actor].contents.frame_count):
                                        base = vm.frame_bases[actor] + frame * 0x400
                                        for getter, offset in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                                            pointer = getattr(lib, 'bk_actor_pose_' + getter)(pose, frame)
                                            equal(pointer[:16], vm.floats(base + offset, 16), (label, actor, frame, getter))
                                            digest.update(C.string_at(pointer, 64)); counters['matrices'] += 1
                                        hidden = C.c_uint32()
                                        assert lib.bk_actor_pose_hidden(pose, frame, C.byref(hidden))
                                        assert hidden.value == vm.read(base + 0x70)
                                    state = clip_bytes(pose)
                                    if actor == 3 and mode == 'retained':
                                        assert state == old_background_clip
                                    else:
                                        vm.clip = vm.clips[actor]
                                        equal([getattr(ClipState.from_buffer_copy(state), key) for key, _ in ClipState._fields_], vm.state(), (label, actor, 'clip'))
                                    digest.update(state)

                            #Run the actual alpha helper on independent native
                            #post-MATA values and explicit retained input.
                            material_roles = [3, 0] if mode == 'retained' else [0] + ([3] if replace else [])
                            entries, mapping = [], []
                            for actor in material_roles:
                                for index, material in enumerate(models[actor].contents.materials[:models[actor].contents.material_count]):
                                    value = bytes(vm.uc.mem_read(vm.material_objects[0][index] + 0x70, 68)) if actor == 0 else bg_initial_materials[index]
                                    entries.append((material.name, value)); mapping.append((actor, index))
                            material_native.install(entries)
                            expected_materials = [value for _, value in entries]
                            if group == 0 and variant == 0:
                                for name, alpha in [(b'I_hako', 1.), (b'kurodenwa', 1.), (b'OPkage', -1.)]:
                                    expected_materials = material_native.alpha(name, 1, alpha)
                                    counters['material_calls'] += 1
                            for (actor, index), value in zip(mapping, expected_materials):
                                if actor == 0:
                                    vm.uc.mem_write(vm.material_objects[0][index] + 0x70, value)
                                else:
                                    bg_initial_materials[index] = value

                            def effects_match(label):
                                morph = lib.bk_ending_selected_assets_morph(owner, 0)
                                assert bool(morph) == (0 in vm.aux_effects)
                                if morph:
                                    meshes, native_group = vm.aux_effects[0]
                                    assert lib.bk_morph_group_time(morph) == vm.floats(native_group + 0x74, 1)[0]
                                    for submesh, target in meshes.items():
                                        count, vertices = vm.meshes[target]
                                        mesh = lib.bk_morph_group_mesh(morph, submesh)
                                        assert mesh
                                        got = C.string_at(lib.bk_morph_mesh_vertices(mesh), count * 60)
                                        want = bytes(vm.uc.mem_read(vertices, count * 60))
                                        if got != want:
                                            for vertex in range(count):
                                                offset = vertex * 60
                                                equal(struct.unpack_from('<9f', got, offset), struct.unpack_from('<9f', want, offset), (label, submesh, vertex))
                                                assert got[offset + 36:offset + 60] == want[offset + 36:offset + 60]
                                        digest.update(got); counters['vertices'] += count
                                animation = lib.bk_ending_selected_assets_material_animation(owner, 0)
                                assert bool(animation) == (0 in vm.material_groups)
                                if animation:
                                    assert lib.bk_material_animation_time(animation) == vm.floats(vm.material_groups[0] + 0x74, 1)[0]
                                for actor in [x for x in active if x in [0, 3]]:
                                    materials = lib.bk_ending_selected_assets_materials(owner, actor)
                                    for index in range(models[actor].contents.material_count):
                                        actual = material_bytes(lib.bk_material_pose_material(materials, index).contents)
                                        if actor == 0:
                                            address = vm.material_objects[0][index]
                                            expected = bytes(vm.uc.mem_read(address + 0x70, 68))
                                            undefined = vm.material_undefined[address]
                                        else:
                                            expected, undefined = bg_initial_materials[index], False
                                        for field, (got, want) in enumerate(zip(struct.unpack('<17f', actual), struct.unpack('<17f', expected))):
                                            if undefined and field in [11, 15]:
                                                assert got == 0; counters['undefined_w_fields'] += 1
                                            else:
                                                equal([got], [want], (label, actor, index, field))
                                                counters['material_fields'] += 1
                                        digest.update(actual)
                                counters['effects'] += 1

                            poses_match('loaded'); effects_match('loaded')
                            face = lib.bk_ending_selected_assets_face_state(owner).contents
                            initial_face = FaceState()
                            assert lib.bk_face_init(C.byref(initial_face), face.eye_count, face.mouth_count, error)
                            face_native.initialize(initial_face, initial_seed, list(clocks))
                            assert bytes(face) == bytes(face_native.state())
                            assert seed.value == struct.unpack('<I', face_native.u.mem_read(0x58edd8, 4))[0]
                            counters['face_initializations'] += 1
                            eyes = EyeNative(exe)
                            eyes.decode(read('fambom', config.face.decode()))
                            _, eye_target = eyes.seed(models[0].contents, names[0])
                            eyes.call(0x4a07a9, eyes.texture, 1)
                            slot = lib.bk_eye_assets_selected(lib.bk_ending_selected_assets_eyes(owner))
                            if eye_target:
                                assert eyes.read(eye_target + 0x98) == eyes.read(eyes.texture + slot * 4)
                            counters['eye_selections'] += 1
                            face_owner = lib.bk_ending_selected_assets_face(owner)
                            for submesh in range(models[0].contents.submesh_count):
                                model_mesh = lib.bk_morph_group_mesh(lib.bk_ending_selected_assets_morph(owner, 0), submesh)
                                face_mesh = lib.bk_face_assets_mesh(face_owner, submesh)
                                assert not (model_mesh and face_mesh)
                                assert lib.bk_ending_selected_assets_mesh(owner, 0, submesh) == (model_mesh or face_mesh)
                            held_targets = [C.string_at(lib.bk_ending_selected_assets_target(owner, i), 12) for i in range(3)]
                            if 3 not in active:
                                assert lib.bk_ending_selected_assets_load_background(owner, store, error), error.value
                                vm.attach(3, True)
                                vm.call(0x4018c8, struct.pack('<2I', vm.clips[3], 0))
                                active.append(3)
                            if background:
                                if mode == 'retained':
                                    assert lib.bk_ending_selected_assets_background(owner) == background
                                else:
                                    assert lib.bk_ending_selected_assets_background(owner) != background
                                    assert clip_bytes(lib.bk_ending_background_assets_data(background).contents.pose) == old_background_clip
                                lib.bk_ending_background_assets_destroy(background)
                                background = None
                                counters['dropped_previous_background_owners'] += 1
                            assert lib.bk_ending_background_assets_name(lib.bk_ending_selected_assets_background(owner)) == final_background.encode()
                            assert lib.bk_ending_selected_assets_load_background(owner, store, error), error.value
                            poses_match('background'); effects_match('background')
                            actor_pose = lib.bk_ending_selected_assets_pose(owner, 0)
                            active_clips = [i for i in range(128) if lib.bk_clip_definition(clips[0], i).contents.active]
                            assert active_clips
                            for step in range(args.steps):
                                if step in [4, 18]:
                                    choice = active_clips[(selection + step) % len(active_clips)]
                                    assert lib.bk_actor_pose_request_mode(actor_pose, choice, 1, error), error.value
                                    vm.call(0x4018c8, struct.pack('<2I', vm.clips[0], choice))
                                    counters['requests'] += 1
                                if step in [12, 16]:
                                    hidden = int(step == 12)
                                    assert lib.bk_actor_forest_visibility(forest, lib.bk_ending_selected_assets_root(owner, 0), hidden, error), error.value
                                    vm.call(0x423a99, struct.pack('<2I', vm.root(0), hidden))
                                seconds = [0., 1/120, 1/60, 1/30, .175, 1.25, 0., 0.][step % 8]
                                value = C.c_float(seconds).value
                                assert lib.bk_ending_selected_assets_advance(owner, 0, value, error), error.value
                                vm.call(0x4026fe, struct.pack('<If', vm.clips[0], value))
                                counters['hidden_updates'] += int(12 <= step < 16)
                                counters['zero_seconds'] += int(value == 0)
                                poses_match(('step', step)); effects_match(('step', step))
                            mixed = None
                            if args.mixed_effects:
                                from ending_selected_effects_reference import verify
                                mixed = verify(lib, vm, owner, clips[0], args.mixed_effects,
                                               poses_match, effects_match, error)
                            motion = None
                            if args.motion_controls:
                                from ending_selected_motion_reference import verify as verify_motion
                                motion = verify_motion(lib, vm, owner, clips[0], args.motion_controls,
                                                       poses_match, effects_match, error)
                            assert held_targets == [C.string_at(lib.bk_ending_selected_assets_target(owner, i), 12) for i in range(3)]
                            before = clip_bytes(actor_pose)
                            for value in [-1., float('inf'), float('nan')]:
                                assert not lib.bk_ending_selected_assets_advance(owner, 0, value, error)
                                assert clip_bytes(actor_pose) == before
                                counters['rejections'] += 1
                            digest.update(bytes(config)); digest.update(bytes(camera)); digest.update(bytes(presets))
                            digest.update(bytes(face)); digest.update(struct.pack('<II', seed.value, slot))
                            records.append(dict(group=group, variant=variant, selection=selection, mode=mode,
                                                background=final_background, replaced=replace,
                                                frames=sum(m.contents.frame_count for m in models), steps=args.steps,
                                                native_initial_sample_windows=vm.initial_sample_windows,
                                                **({'mixed_effects': mixed} if mixed is not None else {}),
                                                **({'motion_controls': motion} if motion is not None else {})))
                            print('PASS selected assets', group, variant, selection, mode, flush=True)
                        finally:
                            lib.bk_ending_selected_assets_destroy(owner)
                            lib.bk_ending_background_assets_destroy(background)
                            for pose in poses: lib.bk_actor_pose_destroy(pose)
                            for clip in clips: lib.bk_clip_set_destroy(clip)
                            for model in models: lib.bk_model_destroy(model)
    finally:
        lib.bk_resources_destroy(store)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), records=records,
                  counters=counters, max_normalized_error=worst, state_sha256=digest.hexdigest(),
                  input_sha256=input_hashes, scope=__doc__, full_loader=False,
                  gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS selected assets TOTAL', len(records), counters, 'max', worst, 'sha256', digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
