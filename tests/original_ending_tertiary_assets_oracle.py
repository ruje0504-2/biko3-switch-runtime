"""Actual third-ending CPU assets versus original pose/lookup/clip segments.

All five groups and both action variants use real Japanese XAN/OBJM/FAM,
camera and background resources. Native422015/4230bd,4d2320 placement,
visibility/selection and target-cache segments, actual three-actor4a52bc
and4a65fc, and4df411 math execute unmodified. Resource decoding/allocation
and initial loaded poses are explicit inputs. The entry expression branch,
4dfb96 and4a07a9 execute with actual replacement-availability and explicit
opaque texture handles. Face warm-up/texture decoding and the complete
4d2320 UI/audio/video shell are not native comparisons here. This is not a
rendered or playable ending validation.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import struct

from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_bom_dual_assets_oracle import Native, bindings, NONE, F16, I, Parser, append_native
from original_bom_oracle import Config as BomConfig
from original_ending_camera_oracle import Native as CameraNative, State as CameraState, Vec, values as camera_values
from model_binding import Model, ROOT, decode
from playback_binding import library
from clip_binding import State as ClipState
from bk3_assets import Archive
from original_prop_route_oracle import Native as ScalarNative


class Config(C.Structure):
    _fields_ = [('primary', C.c_char * 32), ('face', C.c_char * 32),
                ('auxiliaries', (C.c_char * 32) * 2), ('visible_nodes', (C.c_char * 32) * 3),
                ('position', C.c_float * 3), ('yaw', C.c_int32), ('camera_yaw', C.c_int32),
                ('expression_a', C.c_int32), ('expression_b', C.c_int32), ('expression_mode', C.c_int32),
                ('actions', C.c_int32 * 80), ('camera_table', ((C.c_uint32 * 4) * 5))]


class Presets(C.Structure):
    _fields_ = [('active', (C.c_float * 3) * 4), ('authored', (C.c_float * 3) * 4)]


class Background(C.Structure):
    _fields_ = [('model', C.POINTER(Model)), ('clips', C.c_void_p), ('pose', C.c_void_p),
                ('materials', C.c_void_p), ('animation', C.c_void_p), ('morph', C.c_void_p),
                ('root', C.c_uint32)]


def bind(lib):
    bindings(lib)
    vp = C.POINTER(C.c_float)
    lib.bk_ending_tertiary_config.argtypes = [C.POINTER(Config), C.c_uint, C.c_uint]
    lib.bk_actor_pose_model.argtypes = [C.c_void_p]
    lib.bk_actor_pose_model.restype = C.POINTER(Model)
    lib.bk_model_x_pose_decode.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(C.POINTER(Model)), C.c_void_p]
    lib.bk_actor_forest_world.argtypes = [C.c_void_p, C.c_uint32]
    lib.bk_actor_forest_world.restype = vp
    lib.bk_actor_forest_restore_global.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p]
    lib.bk_actor_forest_tree.argtypes = [C.c_void_p]
    lib.bk_actor_forest_tree.restype = C.c_void_p
    for name in ['first', 'next', 'parent']:
        fn = getattr(lib, 'bk_frame_tree_' + name)
        fn.argtypes = [C.c_void_p, C.c_uint32]
        fn.restype = C.c_uint32
    lib.bk_ending_tertiary_assets_role.argtypes = [C.c_int]
    lib.bk_ending_tertiary_assets_role.restype = C.c_uint
    lib.bk_actor_pose_request_mode.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_void_p]
    for name, args, result in [
        ('create', [C.c_void_p, C.c_uint, C.c_uint, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.POINTER(CameraState), C.POINTER(Presets), C.c_void_p], C.c_void_p),
        ('create_reloaded', [C.c_void_p, C.c_uint, C.c_uint, C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.POINTER(CameraState), C.POINTER(Presets), C.c_void_p], C.c_void_p),
        ('destroy', [C.c_void_p], None), ('load_background', [C.c_void_p, C.c_void_p, C.c_void_p], C.c_int),
        ('background', [C.c_void_p], C.c_void_p), ('forest', [C.c_void_p], C.c_void_p),
        ('config', [C.c_void_p], C.POINTER(Config)), ('pose', [C.c_void_p, C.c_uint], C.c_void_p),
        ('root', [C.c_void_p, C.c_uint], C.c_uint32), ('registry', [C.c_void_p, C.c_uint], C.c_uint32),
        ('model_name', [C.c_void_p, C.c_uint], C.c_char_p), ('node', [C.c_void_p, C.c_uint], C.c_uint32),
        ('visible_node', [C.c_void_p, C.c_uint], C.c_uint32), ('missing_visible', [C.c_void_p], C.c_uint),
        ('follow', [C.c_void_p], C.c_uint32), ('special', [C.c_void_p], C.c_uint32),
        ('target', [C.c_void_p, C.c_uint], vp), ('bom', [C.c_void_p], C.c_void_p),
        ('materials', [C.c_void_p, C.c_uint], C.c_void_p), ('morph', [C.c_void_p, C.c_uint], C.c_void_p),
        ('mesh', [C.c_void_p, C.c_uint, C.c_uint32], C.c_void_p), ('face', [C.c_void_p], C.c_void_p),
        ('eyes', [C.c_void_p], C.c_void_p),
    ]:
        fn = getattr(lib, 'bk_ending_tertiary_assets_' + name)
        fn.argtypes, fn.restype = args, result
    lib.bk_face_assets_mesh.argtypes = [C.c_void_p, C.c_uint32]
    lib.bk_face_assets_mesh.restype = C.c_void_p
    lib.bk_eye_assets_image.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p]
    lib.bk_eye_assets_image.restype = C.c_void_p
    lib.bk_eye_assets_selected.argtypes = [C.c_void_p]
    lib.bk_eye_assets_selected.restype = C.c_uint32
    for name, args, result in [
        ('create', [C.c_void_p, C.c_char_p, C.c_void_p], C.c_void_p),
        ('destroy', [C.c_void_p], None), ('data', [C.c_void_p], C.POINTER(Background)),
        ('advance', [C.c_void_p, C.c_float, C.c_void_p], C.c_int), ('name', [C.c_void_p], C.c_char_p),
    ]:
        fn = getattr(lib, 'bk_ending_background_assets_' + name)
        fn.argtypes, fn.restype = args, result


class EntryExpression(ScalarNative):
    def run(self, group, alternate):
        self.u.mem_write(0x721b3c, bytes([group]))
        # Opaque texture objects are only copied into the target mesh;
        # unmodified4a07a9 does not dereference their addresses here.
        self.u.mem_write(0x70d370, struct.pack('<6I', 0x11111111,
            0x22222222 if alternate else 0, 0, 0, 0, 0x300c000) + bytes(20))
        self.u.mem_write(0x300c098, struct.pack('<I', 0x11111111))
        self.u.reg_write(UC_X86_REG_EBP, self.stack)
        self.u.reg_write(UC_X86_REG_ESP, self.stack - 0x100)
        self.u.emu_start(0x4d2d27, 0x4d2d84, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == 0x4d2d84
        expression = tuple(struct.unpack('<i', self.u.mem_read(a, 4))[0]
                           for a in [0x721df4, 0x721df0])
        texture = struct.unpack('<I', self.u.mem_read(0x300c098, 4))[0]
        assert texture in [0x11111111, 0x22222222]
        return expression, int(texture == 0x22222222)


class Reference(Native):
    def fragment(self, first, last):
        self.uc.reg_write(UC_X86_REG_EBP, self.stack)
        self.uc.reg_write(UC_X86_REG_ESP, self.stack - 0xd50)
        self.uc.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.uc.emu_start(first, last, count=20000000)
        assert self.uc.reg_read(UC_X86_REG_EIP) == last, (hex(first), hex(self.uc.reg_read(UC_X86_REG_EIP)))

    def root(self, actor):
        return self.frame_bases[actor] + self.roots[actor] * 0x400

    def attach(self, actor, orient=False):
        self.call(0x422015, struct.pack('<2I', self.context, self.root(actor)))
        if orient:
            self.call(0x4230bd, struct.pack('<2I6f', self.root(actor), self.context, 0, 0, 1, 0, 1, 0))

    def retain_background(self, actor):
        # A native background that survives a stage already has its original
        # global link and caches. This is a retained-state fixture, no refresh.
        link = self.alloc(struct.pack('<3I', self.root(actor), 0, 0) + bytes(4))
        self.word(self.context + 0x230, link)
        self.word(self.context + 0x234, link)
        self.word(self.context + 0x238, 1)
        self.word(self.root(actor) + 0x22c, self.context)

    def body(self, group, variant, physical, configs):
        self.uc.mem_write(0x721b3c, bytes([group, 5]))
        self.word(0x721b28, self.clips[0])
        self.word(0x721e04, variant)
        self.attach(0, True)
        if group == 2:
            for actor in [1, 2]:
                self.word(0x721b28 + actor * 4, self.clips[actor])
                self.attach(actor, True)
        self.fragment(0x4d2c8d, 0x4d2d27)  # placement/rotation, before expression service
        if group == 2:
            decoder = Parser(self.exe)
            decoder.decode(configs[0])
            decoder.u.mem_write(decoder.config + 4, b'\\bk3_11.pp\0')
            self.initialize_combined(decoder)
            append_native(decoder, configs[1])
            self.initialize_combined(decoder)
            for actor in [1, 2]:
                self.call(0x4018c8, struct.pack('<2I', self.clips[actor], 1))
            for actor in [1, 2]:
                self.call(0x423a99, struct.pack('<2I', self.root(actor), 1))
        self.call(0x4d4823, struct.pack('<I', variant))
        self.fragment(0x4d3161, 0x4d327c)  # visible lookups and configured primary1
        for role in [3, 4]:
            self.attach(physical[role])
        for role in [3, 4]:
            self.call(0x401d24, struct.pack('<2I', self.clips[physical[role]], 0))
        for role in [3, 4]:
            self.word(0x721b28, self.clips[physical[role]])
            # Third-ending track and primary degrees happen to share the
            # same five values. Execute the original42363b call site.
            self.fragment(0x4d2cde, 0x4d2d27)
        self.word(0x721b28, self.clips[0])
        self.fragment(0x4d32a1, 0x4d3372)  #39 target nodes and group2 special
        self.fragment(0x4d33d4, 0x4d349c)  #cached5/13/0 vectors, before background
        self.fragment(0x4d35c2, 0x4d35ed)  #A_kuch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--groups', default='0,1,2,3,4')
    parser.add_argument('--variants', default='0,1')
    parser.add_argument('--reloads', type=int, default=1)
    parser.add_argument('--effects-steps', type=int, default=0,
                        help='Also compare uninterrupted ordinary/controlled/fixed ANIM/MATA/MORP on real owners.')
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-tertiary-assets-oracle.json')
    args = parser.parse_args()
    groups = [int(g) for g in args.groups.split(',')]
    variants = [int(v) for v in args.variants.split(',')]
    assert groups and len(set(groups)) == len(groups) and all(0 <= g < 5 for g in groups)
    assert variants and len(set(variants)) == len(variants) and all(v in [0, 1] for v in variants)
    assert args.reloads in [0, 1]
    assert args.effects_steps == 0 or args.effects_steps >= 24
    lib, exe = library(), args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e', 'unsupported original EXE'
    bind(lib)
    reference_type = Reference
    if args.effects_steps:
        import ending_mixed_effects_reference as mixed_effects
        mixed_effects.bind(lib)
        class EffectsReference(mixed_effects.MixedEffectsReference, Reference):
            pass
        reference_type = EffectsReference
    assert [lib.bk_ending_tertiary_assets_role(i) for i in range(4)] == [0, 1, 2, 5]
    assert all(lib.bk_ending_tertiary_assets_role(i) == 6 for i in [-1, 4, 5, 0x7fffffff])
    error = C.create_string_buffer(256)
    packs = {name: Archive(args.data / (name + '.pp')) for name in ['bk3_11', 'fambom', 'bk3_04', 'bk3_03']}
    input_hashes = {}
    def read(pack, name):
        entry = next(e for e in packs[pack].entries if e.name.lower() == name.lower())
        raw = packs[pack].read(entry)
        input_hashes[pack + '/' + entry.name] = hashlib.sha256(raw).hexdigest()
        return raw
    bom_raw = [read('fambom', name) for name in ['jouhansin.bom', 'kahansin.bom']]
    vix = {e.name.lower().encode(): read('bk3_11', e.name) for e in packs['bk3_11'].entries if e.name.lower().endswith('.vix')}
    store = lib.bk_resources_create(error)
    assert store
    for name in packs:
        assert lib.bk_resources_mount(store, name.encode(), str(args.data / (name + '.pp')).encode(), error), error.value
    state_hash = hashlib.sha256()
    records, matrices, mapped, worst, rejected = [], 0, 0, 0.0, 0
    camera_native = CameraNative(exe)
    expression_native = EntryExpression(exe)
    entry_expressions = 0

    def compare(actual, expected, label):
        nonlocal worst
        assert len(actual) == len(expected)
        for i, (got, want) in enumerate(zip(actual, expected)):
            if got == want:
                continue
            delta = abs(got - want) / max(1, abs(want))
            worst = max(worst, delta)
            assert math.isfinite(delta) and delta < 3e-5, (label, i, got, want, delta)

    def clip_bytes(pose):
        state = ClipState()
        assert lib.bk_actor_pose_state(pose, C.byref(state))
        return bytes(state)

    try:
        for group in groups:
            for variant in variants:
                for reload in range(args.reloads + 1):
                    owner = background = None
                    models, clips, poses, raws, xans, names, physical = [], [], [], [], [], [], {}
                    try:
                        specs = [(0, 'bk3_11', f'h{group+1:02}_10.xan')]
                        if group == 2:
                            specs += [(1, 'bk3_11', 'h03_30.xan'), (2, 'bk3_11', 'h03_31.xan')]
                        specs += [(3, 'bk3_04', 'cam00_00.xan'), (4, 'bk3_04', 'cam00_03.xan')]
                        background_variant = 1 - variant if reload else variant
                        bg_name = f'm{group+1:02}_{90+background_variant}.xan'
                        specs += [(5, 'bk3_03', bg_name)]
                        # Separate raw loaded actors provide the native VM's
                        # input, never the new constructor's resulting poses.
                        for role, pack, name in specs:
                            xan = read(pack, name)
                            model_name = xan[:256].split(b'\0')[0]
                            raw = read(pack, model_name.decode('ascii'))
                            model = C.POINTER(Model)()
                            if raw.startswith(b'xof 0302txt 0032'):
                                assert lib.bk_model_x_pose_decode(raw, len(raw), C.byref(model), error) == 1, error.value
                            else:
                                ok, model, message = decode(lib, raw)
                                assert ok, message
                            models.append(model)
                            clip = lib.bk_clip_set_decode(xan, len(xan), error)
                            assert clip, error.value
                            clips.append(clip)
                            root = next(i for i, frame in enumerate(model.contents.frames[:model.contents.frame_count]) if frame.parent_index == NONE)
                            pose = lib.bk_actor_pose_create_loaded(model, clip, root, Vec(0, 0, 0), 0, error)
                            assert pose, error.value
                            poses.append(pose)
                            assert lib.bk_actor_pose_root_local(pose, model.contents.frames[root].local, error), error.value
                            physical[role] = len(poses) - 1
                            raws.append(raw)
                            xans.append(xan)
                            names.append(model_name)
                        if reload:
                            background = lib.bk_ending_background_assets_create(store, bg_name.encode(), error)
                            assert background, error.value
                            bdata = lib.bk_ending_background_assets_data(background).contents
                            assert lib.bk_actor_pose_request_mode(bdata.pose, 0, 1, error), error.value
                            assert lib.bk_ending_background_assets_advance(background, .175, error), error.value
                            old_clip = clip_bytes(bdata.pose)
                            # Use genuine live background caches as retained
                            # inputs. Its actor/effect owner is kept separately.
                            ref_poses = list(poses)
                            ref_poses[-1] = bdata.pose
                        else:
                            ref_poses = poses
                        vm = reference_type(exe, vix)
                        vm.exe = exe
                        vm.setup_actors(lib, [m.contents for m in models], ref_poses, names, raws, xans,
                                        linked=False, primary_animations=True)
                        if args.effects_steps:
                            vm.setup_materials(models, raws, physical)
                        if reload:
                            vm.retain_background(physical[5])
                        vm.body(group, variant, physical, bom_raw)
                        config = Config()
                        assert lib.bk_ending_tertiary_config(C.byref(config), group, variant)
                        assert list(config.actions) == list(struct.unpack('<80i', vm.uc.mem_read(0x709db8, 320)))
                        assert bytes(config.camera_table) == bytes(vm.uc.mem_read(0x571148, 80))
                        assert list(config.position) == vm.floats(0x570d9c + group * 120, 3)
                        assert config.yaw == vm.read(0x570cac + group * 40)
                        assert config.camera_yaw == vm.read(0x570c2c + group * 24)
                        assert config.primary == f'h{group+1:02}_10.xan'.encode()
                        assert config.face == f'h{group+1:02}_10.fam'.encode()
                        read('fambom', config.face.decode())
                        camera = CameraState()
                        camera.pose.world[:] = I
                        camera.pose.position[:] = [7, 8, 9]
                        camera.matrix[:] = I
                        camera.focus[:] = [5, -4, 3]
                        camera.yaw, camera.pitch, camera.radius, camera.height, camera.fov = 41, 9, 30, 3, .75
                        initial_camera = CameraState.from_buffer_copy(camera)
                        presets = Presets()
                        C.memset(C.byref(presets), 0xa5, C.sizeof(presets))
                        seed = C.c_uint32(0x47a6 + group * 71 + variant * 31 + reload)
                        clocks = (C.c_uint32 * 4)(100, 1000, 1015, 1031)
                        if reload:
                            owner = lib.bk_ending_tertiary_assets_create_reloaded(store, group, variant, background,
                                clocks, C.byref(seed), C.byref(camera), C.byref(presets), error)
                        else:
                            owner = lib.bk_ending_tertiary_assets_create(store, group, variant, clocks,
                                C.byref(seed), C.byref(camera), C.byref(presets), error)
                        assert owner, (group, variant, reload, error.value)
                        assert bytes(lib.bk_ending_tertiary_assets_config(owner).contents) == bytes(config)
                        eyes = lib.bk_ending_tertiary_assets_eyes(owner)
                        assert eyes
                        initial_expression, eye_slot = expression_native.run(
                            group, bool(lib.bk_eye_assets_image(eyes, 1, None)))
                        assert (config.expression_a, config.expression_b) == initial_expression
                        assert lib.bk_eye_assets_selected(eyes) == eye_slot, (group, 'entry eye slot')
                        state_hash.update(struct.pack('<3i', *initial_expression, eye_slot))
                        entry_expressions += 1
                        forest = lib.bk_ending_tertiary_assets_forest(owner)
                        active_roles = list(physical) if reload else [r for r in physical if r != 5]
                        for role in range(6):
                            present = role in active_roles
                            assert bool(lib.bk_ending_tertiary_assets_pose(owner, role)) == present
                            assert lib.bk_ending_tertiary_assets_registry(owner, role) == (physical[role] if present else NONE)
                        for i in range(39):
                            pointer = vm.read(0x721ef4 + i * 4)
                            expected = (pointer - vm.frame_bases[0]) // 0x400 if pointer else NONE
                            assert lib.bk_ending_tertiary_assets_node(owner, i) == expected
                        for i in range(3):
                            pointer = vm.read(0x709ef8 + i * 4)
                            expected = (pointer - vm.frame_bases[0]) // 0x400 if pointer else NONE
                            assert lib.bk_ending_tertiary_assets_visible_node(owner, i) == expected
                            target = lib.bk_ending_tertiary_assets_target(owner, i)[:3]
                            compare(target, vm.floats(0x70c8d8 + i * 12, 3), (group, 'cached target', i))
                        assert lib.bk_ending_tertiary_assets_follow(owner) == (vm.read(0x719448) - vm.frame_bases[0]) // 0x400
                        if group == 2:
                            assert lib.bk_ending_tertiary_assets_special(owner) == (vm.read(0x719b48) - vm.frame_bases[0]) // 0x400
                        else:
                            assert lib.bk_ending_tertiary_assets_special(owner) == NONE
                        table = vm.floats(0x559a88 + (group * 10 + 5) * 48, 12)
                        for field in range(4):
                            expected = [table[choice * 4 + field] for choice in range(3)]
                            assert list(presets.active[field]) == list(presets.authored[field]) == expected
                        initial_camera.pose.position[:] = [0, 20, 0]
                        initial_camera.yaw, initial_camera.pitch, initial_camera.radius, initial_camera.height = table[:4]
                        initial_camera.fov = 1
                        wanted_camera = camera_native.run(initial_camera, 1, [0, 0], 0, 0,
                            Vec(*vm.floats(0x70c8d8, 3)), Vec(0, 0, 0), Vec(0, 0, 0), 16)
                        compare(camera_values(camera), camera_values(wanted_camera), (group, 'fixed camera'))
                        compare(lib.bk_actor_forest_world(forest, 1)[:16], wanted_camera.pose.world, (group, 'camera anchor'))

                        def compare_poses(label):
                            nonlocal matrices
                            # Tree's camera anchor1 is an explicit portable
                            # object. Compare every actual native actor root's
                            # relative order, including retained background.
                            tree = lib.bk_actor_forest_tree(forest)
                            roles_by_node = {lib.bk_ending_tertiary_assets_root(owner, role): role for role in active_roles}
                            order, node = [], lib.bk_frame_tree_first(tree, 0)
                            while node != NONE:
                                assert lib.bk_frame_tree_parent(tree, node) == 0
                                if node != 1:
                                    assert node in roles_by_node and roles_by_node[node] not in order
                                    order.append(roles_by_node[node])
                                node = lib.bk_frame_tree_next(tree, node)
                            native_by_node = {vm.root(physical[role]): role for role in active_roles}
                            native_order, link = [], vm.read(vm.context + 0x230)
                            while link:
                                node = vm.read(link)
                                assert node in native_by_node and native_by_node[node] not in native_order
                                native_order.append(native_by_node[node])
                                link = vm.read(link + 8)
                            assert order == native_order, (group, variant, reload, label, 'global order', order, native_order)
                            for role in active_roles:
                                actor = physical[role]
                                pose = lib.bk_ending_tertiary_assets_pose(owner, role)
                                count = models[actor].contents.frame_count
                                for frame in range(count):
                                    for getter, offset in [('local', 0x80), ('frame', 0xc0), ('parent_world', 0x100)]:
                                        pointer = getattr(lib, 'bk_actor_pose_' + getter)(pose, frame)
                                        compare(pointer[:16], vm.floats(vm.frame_bases[actor] + frame * 0x400 + offset, 16),
                                                (group, variant, reload, label, role, frame, getter))
                                        matrices += 1
                                        state_hash.update(C.string_at(pointer, 64))
                                    hidden = C.c_uint32()
                                    assert lib.bk_actor_pose_hidden(pose, frame, C.byref(hidden))
                                    assert hidden.value == vm.read(vm.frame_bases[actor] + frame * 0x400 + 0x70)
                                if role != 5 or not reload:
                                    vm.clip = vm.clips[actor]
                                    state = ClipState.from_buffer_copy(clip_bytes(pose))
                                    compare([getattr(state, key) for key, _ in ClipState._fields_], vm.state(), (group, role, 'clip'))
                                else:
                                    assert clip_bytes(pose) == old_clip
                        compare_poses('before outer background')
                        for role in [r for r in physical if r < 3]:
                            model = models[physical[role]].contents
                            face = lib.bk_ending_tertiary_assets_face(owner)
                            morph = lib.bk_ending_tertiary_assets_morph(owner, role)
                            assert lib.bk_ending_tertiary_assets_materials(owner, role)
                            for submesh in range(model.submesh_count):
                                base = lib.bk_morph_group_mesh(morph, submesh)
                                face_mesh = lib.bk_face_assets_mesh(face, submesh) if role == 0 else None
                                assert not (base and face_mesh), ('overlapping effect owner', group, role, submesh)
                                assert lib.bk_ending_tertiary_assets_mesh(owner, role, submesh) == (base or face_mesh)
                        bom_owner = lib.bk_ending_tertiary_assets_bom(owner)
                        assert bool(bom_owner) == (group == 2)
                        if bom_owner:
                            assert lib.bk_bom_dual_assets_count(bom_owner) == 5
                            for i in range(5):
                                kind, mapping, count = C.c_int32(), C.POINTER(C.c_uint32)(), C.c_size_t()
                                assert lib.bk_bom_dual_assets_mapping(bom_owner, i, C.byref(kind), C.byref(mapping), C.byref(count))
                                assert kind.value == vm.read(0x708184 + i * 4)
                                assert count.value == vm.read(0x708164 + i * 4)
                                actual = C.string_at(mapping, count.value * 4) if count.value else b''
                                native = bytes(vm.uc.mem_read(vm.read(0x708124 + i * 4), count.value * 4)) if count.value else b''
                                assert actual == native
                                state_hash.update(actual)
                                mapped += count.value
                        targets_before = [C.string_at(lib.bk_ending_tertiary_assets_target(owner, i), 12) for i in range(3)]
                        if not reload:
                            assert lib.bk_ending_tertiary_assets_load_background(owner, store, error), error.value
                            active_roles.append(5)
                            vm.attach(physical[5], True)
                            vm.call(0x4018c8, struct.pack('<2I', vm.clips[physical[5]], 0))
                        else:
                            assert lib.bk_ending_tertiary_assets_background(owner) == background
                            lib.bk_ending_background_assets_destroy(background)
                            background = None
                        compare_poses('after outer background')
                        kept = lib.bk_ending_tertiary_assets_background(owner)
                        assert lib.bk_ending_background_assets_name(kept) == bg_name.encode()
                        kept_data = lib.bk_ending_background_assets_data(kept).contents
                        assert kept_data.model.contents.frame_count == models[-1].contents.frame_count
                        assert lib.bk_ending_tertiary_assets_load_background(owner, store, error)
                        assert targets_before == [C.string_at(lib.bk_ending_tertiary_assets_target(owner, i), 12) for i in range(3)]
                        state_hash.update(bytes(config))
                        state_hash.update(bytes(camera))
                        state_hash.update(bytes(presets))
                        state_hash.update(struct.pack('<I', seed.value))
                        records.append(dict(group=group, variant=variant, reload=bool(reload), background=bg_name,
                            actors=len(physical), frame_nodes=sum(m.contents.frame_count for m in models),
                            bom_bindings=5 if group == 2 else 0))
                        if args.effects_steps:
                            records[-1]['mixed_effects'] = mixed_effects.verify(
                                lib, vm, owner, models, clips, physical, args.effects_steps,
                                compare_poses, error)
                            print('PASS mixed effects', group, variant, reload,
                                  records[-1]['mixed_effects'], flush=True)
                        print('PASS tertiary assets', group, variant, 'reload' if reload else 'fresh', flush=True)
                    finally:
                        lib.bk_ending_tertiary_assets_destroy(owner)
                        lib.bk_ending_background_assets_destroy(background)
                        for pose in poses:
                            lib.bk_actor_pose_destroy(pose)
                        for clip in clips:
                            lib.bk_clip_set_destroy(clip)
                        for model in models:
                            lib.bk_model_destroy(model)
        config = Config()
        C.memset(C.byref(config), 0x5a, C.sizeof(config))
        before = bytes(config)
        for group, variant in [(5, 0), (NONE, 1), (0, 2), (4, NONE)]:
            assert not lib.bk_ending_tertiary_config(C.byref(config), group, variant)
            assert bytes(config) == before
            rejected += 1
    finally:
        lib.bk_resources_destroy(store)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), records=records,
                  matrices=matrices, mapped_vertices=mapped, atomic_rejections=rejected,
                  max_normalized_error=worst, state_sha256=state_hash.hexdigest(),
                  input_sha256=input_hashes, scope=__doc__, full_loader=False,
                  native_face_comparison=False, gpu_or_device_validation=False)
    result['entry_expression_eye_states'] = entry_expressions
    if args.effects_steps:
        result['mixed_effects_steps_per_actor'] = args.effects_steps
        result['mixed_effects_scope'] = mixed_effects.__doc__
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS tertiary assets TOTAL', len(records), matrices, mapped, worst, state_hash.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
