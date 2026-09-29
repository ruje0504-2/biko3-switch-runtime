"""Original4D00FA CPU resource topology and ordered cached-pose initialization.

Resources are decoded at the existing file/model boundary. The original global
attachment, root transforms, visibility, configured clip requests, name lookup,
flow16 camera loading, target capture and fixed camera execute unchanged.
Face warmup/RNG are compared in their independent original controller VM.
UI, media and device services are observing boundaries, not playable-phase2
or rendering acceptance. There are no fabricated auxiliary/BOM actors.
"""
import argparse
from collections import deque
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from bk3_assets import Archive
from clip_binding import State as ClipState
from model_binding import ROOT, Model
from original_ending_secondary_oracle import Config
from original_ending_normal_assets_oracle import Native as NormalNative
from original_ending_normal_assets_oracle import bind as normal_bind
from original_ending_camera_assets_oracle import State, Presets, values, I
from original_ending_face_init import EndingController
from original_face_controller_oracle import State as FaceState
from original_eye_assets_oracle import Native as EyeNative
from playback_binding import library

NONE = 0xffffffff


def bindings(lib):
    normal_bind(lib)
    fp = C.POINTER(C.c_float)
    defs = [
        ('create', [C.c_void_p, C.c_uint, C.c_uint, C.POINTER(C.c_uint32),
                    C.POINTER(C.c_uint32), C.POINTER(State), C.POINTER(Presets),
                    C.c_void_p], C.c_void_p),
        ('destroy', [C.c_void_p], None),
        ('load_background', [C.c_void_p, C.c_void_p, C.c_void_p], C.c_int),
        ('pose', [C.c_void_p, C.c_uint], C.c_void_p),
        ('config', [C.c_void_p], C.POINTER(Config)),
        ('node', [C.c_void_p, C.c_uint], C.c_uint32),
        ('visible_node', [C.c_void_p, C.c_uint], C.c_uint32),
        ('target', [C.c_void_p, C.c_uint], fp),
        ('face_state', [C.c_void_p], C.POINTER(FaceState)),
        ('eyes', [C.c_void_p], C.c_void_p),
    ]
    for name in ['anchor', 'follow', 'oyu', 'missing_visible']:
        defs.append((name, [C.c_void_p], C.c_uint32))
    for name, args, rest in defs:
        fn = getattr(lib, 'bk_ending_secondary_assets_' + name)
        fn.argtypes, fn.restype = args, rest
    lib.bk_eye_assets_selected.argtypes = [C.c_void_p]
    lib.bk_eye_assets_selected.restype = C.c_uint32


class Native(NormalNative):
    def __init__(self, exe):
        super().__init__(exe, {})
        self.uc.mem_map(0x38000000, 0x800000)
        self.bases, self.clips, self.root_ids = [], [], []
        self.byname = {}
        self.ui_calls = []
        self.eye_calls = []
        self.recent = deque(maxlen=32)
        self.uc.hook_add(UC_HOOK_CODE, self.trace)
        self.uc.hook_add(UC_HOOK_CODE, self.heap, begin=0x52043e, end=0x52043e)
        # UI layout/controller numerics have separate original oracles. Keep
        # the actual loader call ordering and arguments visible here.
        for address in [0x50de40, 0x50e51f, 0x50e633]:
            self.uc.hook_add(UC_HOOK_CODE, self.ui, begin=address, end=address)
        for frame in [self.context, self.rendered]:
            for off in [0x80, 0xc0, 0x100]:
                self.vector(frame + off, I)
        self.uc.mem_write(self.matrix_stack,
                          struct.pack('<5I', self.vt, 1024, self.matrices, 0, 1))
        self.vector(self.matrices, I)
        for address, value in [(0x63f108, self.matrix_stack), (0x63f0fc, 1),
                               (0x63f104, 1), (0x63f100, 0), (0x6455f4, 0),
                               (0x645600, self.context), (0x645604, self.rendered)]:
            self.word(address, value)

    def trace(self, u, address, size, _):
        self.recent.append(address)

    def heap(self, u, address, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        count = self.read(sp + 4)
        assert count < 0x100000
        u.reg_write(UC_X86_REG_EAX, self.alloc(bytes(count)))
        u.reg_write(UC_X86_REG_EIP, self.read(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4)

    def ui(self, u, address, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        self.ui_calls.append(address)
        u.reg_write(UC_X86_REG_EAX, 1)
        u.reg_write(UC_X86_REG_EIP, self.read(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4)

    def io(self, u, address, size, user):
        if address == 0x4a07a9:
            # This call is reached through the unmodified4DFB96 expression
            # helper in4D00FA. Do not mistake an indirect call for absence.
            sp = u.reg_read(UC_X86_REG_ESP)
            self.eye_calls.append(self.read(sp + 8))
        if address == 0x4f2dae:
            sp = u.reg_read(UC_X86_REG_ESP)
            args = [self.read(sp + 4 + i * 4) for i in range(5)]
            self.face_calls.append((self.string(args[3]), self.string(args[4])))
            assert self.string(args[4]).lstrip(b'\\') == self.face_name
            u.reg_write(UC_X86_REG_EAX, 1)
            u.reg_write(UC_X86_REG_EIP, self.read(sp))
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            return
        super().io(u, address, size, user)

    def add_actor(self, model, xan, name, index):
        base = 0x38000000 + index * 0x200000
        clip, model_ptr, links = base + 0x180000, base + 0x186000, base + 0x1a0000
        assert model.frame_count * 0x400 < 0x180000
        assert len(xan) - 512 < 0x6000
        self.uc.mem_write(clip, xan[512:])
        self.word(clip + 0x18c, 0)
        self.word(clip + 0x160, model_ptr)
        roots = [i for i in range(model.frame_count)
                 if model.frames[i].parent_index == NONE]
        assert len(roots) == 1
        root = roots[0]
        self.word(model_ptr + 0x14, base + root * 0x400)
        children = {i: [] for i in range(model.frame_count)}
        for i in range(model.frame_count):
            frame = model.frames[i]
            p = base + i * 0x400
            self.uc.mem_write(p + 8, frame.name + b'\0')
            self.vector(p + 0x80, frame.local)
            self.vector(p + 0xc0, I)
            self.vector(p + 0x100, I)
            if frame.parent_index != NONE:
                children[frame.parent_index].append(i)
                self.word(p + 0x22c, base + frame.parent_index * 0x400)
        for parent, items in children.items():
            p = base + parent * 0x400
            self.word(p + 0x238, len(items))
            self.word(p + 0x230, links + items[0] * 16 if items else 0)
            self.word(p + 0x234, links + items[-1] * 16 if items else 0)
            for j, item in enumerate(items):
                self.word(links + item * 16, base + item * 0x400)
                self.word(links + item * 16 + 8,
                          links + items[j + 1] * 16 if j + 1 < len(items) else 0)
        self.bases.append(base)
        self.clips.append(clip)
        self.root_ids.append(root)
        self.byname[name] = (clip, base + root * 0x400)

    def prepare(self, group, state):
        self.face_name = f'h{group + 1:02}_02.fam'.encode()
        link = 0x373e0000
        self.word(self.context + 0x238, 1)
        self.word(self.context + 0x230, link)
        self.word(self.context + 0x234, link)
        self.word(link, self.rendered)
        self.word(link + 8, 0)
        self.word(self.rendered + 0x22c, self.context)
        for off in [0x80, 0xc0]:
            self.vector(self.rendered + off, state.pose.world)
        self.uc.mem_write(0x721b3c, bytes([group, 1]))
        self.word(0x7219a8, group)
        self.uc.mem_write(0x5767c8, b'\1')
        self.uc.mem_write(0x70ca00, f'\\h{group + 1:02}_02.xan\0'.encode())
        self.vector(0x71af38 + 0x420, state.pose.position)
        self.vector(0x71af38 + 0x4a4, state.matrix)
        self.vector(0x71af38 + 0x5c8, state.focus)
        self.fov = state.fov


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--limit', type=int, default=10)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'local/original-ending-secondary-assets.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    lib = library()
    bindings(lib)
    err = C.create_string_buffer(256)
    store = lib.bk_resources_create(err)
    assert store, err.value
    archives = {}
    for pack in ['bk3_09', 'bk3_04', 'bk3_03', 'fambom']:
        assert lib.bk_resources_mount(store, pack.encode(),
                                      str(args.data / (pack + '.pp')).encode(), err), err.value
        archives[pack] = Archive(args.data / (pack + '.pp'))

    def read(pack, name):
        arc = archives[pack]
        return arc.read(next(entry for entry in arc.entries if entry.name == name))

    matrices = clips = nodes = face_checks = eye_checks = 0
    worst = 0.0
    records = []

    def equal(got, expected, label):
        nonlocal worst
        for index, (g, w) in enumerate(zip(got, expected)):
            if g == w:
                continue
            error = abs(g - w) / max(1, abs(w))
            worst = max(worst, error)
            assert math.isfinite(error) and error < 3e-5, (label, index, g, w, error)

    try:
        for case in range(args.limit):
            group, variant = case // 2 % 5, case % 2
            state = State()
            state.pose.world[:] = state.matrix[:] = I
            state.pose.world[12] = 21
            state.matrix[12] = -17
            state.focus[:] = [7, 8, 9]
            state.fov = .4
            initial = State.from_buffer_copy(state)
            presets = Presets()
            rng = C.c_uint32(123 + case)
            clocks = (C.c_uint32 * 4)(100, 110, 120, 130)
            owner = lib.bk_ending_secondary_assets_create(store, group, variant, clocks,
                         C.byref(rng), C.byref(state), C.byref(presets), err)
            assert owner, (group, variant, err.value)
            try:
                poses = [lib.bk_ending_secondary_assets_pose(owner, i)
                         for i in range(4 if group == 1 else 3)]
                models = [lib.bk_actor_pose_model(pose).contents for pose in poses]
                names = [f'h{group + 1:02}_02.xan', 'cam00_00.xan', 'cam00_03.xan']
                packs = ['bk3_09', 'bk3_04', 'bk3_04']
                if group == 1:
                    names.append('m02_90.xan')
                    packs.append('bk3_03')
                vm = Native(exe)
                for i, (model, pack, name) in enumerate(zip(models, packs, names)):
                    vm.add_actor(model, read(pack, name), name.encode(), i)
                vm.prepare(group, initial)
                try:
                    vm.call(0x4d00fa, b'')
                except Exception:
                    print('native fault', hex(vm.uc.reg_read(UC_X86_REG_EIP)),
                          vm.load_names, vm.face_calls,
                          'recent', [hex(address) for address in vm.recent], flush=True)
                    raise
                assert vm.load_names == [name.encode() for name in names], vm.load_names
                assert len(vm.face_calls) == 1

                def check(label):
                    nonlocal matrices, clips
                    for actor, model in enumerate(models):
                        for frame in range(model.frame_count):
                            for getter, off in [('local', 0x80), ('frame', 0xc0),
                                                ('parent_world', 0x100)]:
                                equal(getattr(lib, 'bk_actor_pose_' + getter)(poses[actor], frame)[:16],
                                      vm.floats(vm.bases[actor] + frame * 0x400 + off, 16),
                                      (case, label, actor, frame, getter))
                                matrices += 1
                        clip = ClipState()
                        assert lib.bk_actor_pose_state(poses[actor], C.byref(clip))
                        equal([getattr(clip, n) for n, _ in ClipState._fields_],
                              vm.clip_state(vm.clips[actor]), (case, label, actor, 'clip'))
                        clips += 1

                check('initial')
                for i in range(39):
                    index = lib.bk_ending_secondary_assets_node(owner, i)
                    assert vm.read(0x721ef4 + i * 4) == (
                        0 if index == NONE else vm.bases[0] + index * 0x400), (case, i)
                    nodes += 1
                for getter, address in [('anchor', 0x719b40), ('follow', 0x719448)]:
                    index = getattr(lib, 'bk_ending_secondary_assets_' + getter)(owner)
                    assert vm.read(address) == vm.bases[0] + index * 0x400
                if group == 2:
                    index = lib.bk_ending_secondary_assets_oyu(owner)
                    assert vm.read(0x719b44) == (0 if index == NONE else vm.bases[0] + index * 0x400)
                missing = 0
                for i in range(3):
                    index = lib.bk_ending_secondary_assets_visible_node(owner, i)
                    assert vm.read(0x709ef8 + i * 4) == (
                        0 if index == NONE else vm.bases[0] + index * 0x400)
                    missing += index == NONE
                    if index != NONE:
                        hidden = C.c_uint32(99)
                        assert lib.bk_actor_pose_hidden(poses[0], index, C.byref(hidden))
                        assert hidden.value == 0
                assert missing == lib.bk_ending_secondary_assets_missing_visible(owner)
                for i in range(3):
                    equal(lib.bk_ending_secondary_assets_target(owner, i)[:3],
                          vm.floats(0x70c8d8 + i * 12, 3), (case, 'target', i))
                equal(values(state), values(vm.camera()), (case, 'camera'))
                assert bytes(presets) == bytes(vm.uc.mem_read(0x71af38 + 0x444, 96))
                config = lib.bk_ending_secondary_assets_config(owner).contents
                assert config.expression_a == vm.read(0x721df4)
                assert config.expression_b == vm.read(0x721df0)
                assert bytes(config.camera_table) == bytes(vm.uc.mem_read(0x709fcc, 80))
                # Real loaded FAM/model target with the original pointer
                # switch, consuming only the actual loader's observed calls.
                # Resource upload remains the existing eye-VM boundary.
                eye_vm = EyeNative(exe)
                eye_vm.decode(read('fambom', bytes(config.face).split(b'\0')[0].decode()))
                model_name = read('bk3_09', names[0])[:256].split(b'\0')[0]
                _, eye_target = eye_vm.seed(models[0], model_name)
                assert vm.eye_calls == [1], vm.eye_calls
                for slot in vm.eye_calls:
                    eye_vm.call(0x4a07a9, eye_vm.texture, slot)
                selected = lib.bk_eye_assets_selected(lib.bk_ending_secondary_assets_eyes(owner))
                if eye_target:
                    assert eye_vm.read(eye_target + 0x98) == eye_vm.read(
                        eye_vm.texture + selected * 4), (case, 'initial eye slot', selected)
                eye_checks += 1
                face = lib.bk_ending_secondary_assets_face_state(owner).contents
                initial_face = FaceState()
                assert lib.bk_face_init(C.byref(initial_face), face.eye_count, face.mouth_count, err)
                face_vm = EndingController(exe)
                face_vm.initialize(initial_face, 123 + case, list(clocks))
                assert bytes(face) == bytes(face_vm.state())
                assert rng.value == struct.unpack('<I', face_vm.u.mem_read(0x58edd8, 4))[0]
                face_checks += 1
                # Outer background attachment is its own mandatory later stage.
                # Preserve the loader's target snapshots across this refresh.
                held_targets = [list(lib.bk_ending_secondary_assets_target(owner, i)[:3]) for i in range(3)]
                assert lib.bk_ending_secondary_assets_load_background(owner, store, err), err.value
                if group != 1:
                    name = f'm{group + 1:02}_{90 + variant}.xan'
                    pose = lib.bk_ending_secondary_assets_pose(owner, 3)
                    model = lib.bk_actor_pose_model(pose).contents
                    vm.add_actor(model, read('bk3_03', name), name.encode(), 3)
                    poses.append(pose)
                    models.append(model)
                    vm.uc.mem_write(0x373d0000, ('\\' + name).encode() + b'\0')
                    vm.call(0x4d460b, struct.pack('<I', 0x373d0000))
                check('background')
                assert held_targets == [list(lib.bk_ending_secondary_assets_target(owner, i)[:3]) for i in range(3)]
                assert lib.bk_ending_secondary_assets_load_background(owner, store, err), err.value
                records.append(dict(group=group, variant=variant,
                                    frames=[m.frame_count for m in models], missing_visible=missing,
                                    fov=state.fov, targets=held_targets))
                print('PASS secondary assets', group, variant, flush=True)
            finally:
                lib.bk_ending_secondary_assets_destroy(owner)
    finally:
        lib.bk_resources_destroy(store)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  entries=len(records), matrices=matrices, clips=clips, nodes=nodes,
                  face_checks=face_checks, eye_checks=eye_checks,
                  max_normalized_error=worst,
                  records=records, scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS secondary assets:', len(records), 'entries;', matrices,
          'matrices;', clips, 'clip states; error', worst, flush=True)


if __name__ == '__main__':
    main()
