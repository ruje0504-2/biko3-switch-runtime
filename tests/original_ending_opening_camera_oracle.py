"""Complete original4e0b7e with retail cam00_03 XAN/model and native
animation, frame/aim and end-source writes. Only input is supplied. Compare
camera, all secondary local/world/parent caches and playback before/after
draw. A stationary target is explicit; no complete ending lifecycle claimed.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
from original_ending_camera_assets_oracle import (
    bindings, Presets, State, Model, Frame, F, U, I, Edit, Visit, ClipState,
    Track, Archive, library, ROOT, values)

Key = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint, C.c_uint,
                  C.POINTER(U), C.c_void_p)


class Input(C.Structure):
    _fields_ = [('context', C.c_void_p), ('key', Key)]


class ClipEdit(C.Structure):
    _fields_ = [('slot', U), ('fields', U), ('chain', C.c_int32),
                ('next', C.c_int32), ('source', F)]


class Native(Track):
    def input(self, u, address, size, user):
        if address != 0x4b76c2:
            return super().input(u, address, size, user)
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, code, mode, extra = struct.unpack('<4I', u.mem_read(sp, 16))
        assert mode == 1 and extra == 0
        assert code == [0, 0x5a, 0x33450, 1][len(self.trace)]
        self.trace.append(code)
        self.before_key.append(self.output_state())
        if len(self.trace) == self.mutate_at:
            slot = struct.unpack('<I', u.mem_read(self.clip + 0x140, 4))[0]
            self.vector(self.clip + 0x1f0 + slot * 0x9c, [self.mutation])
        value = self.keys[len(self.trace) - 1]
        u.reg_write(UC_X86_REG_EAX, value)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)


def adjacent(value, up):
    if value == 0:
        return struct.unpack('<f', struct.pack('<I', 1 if up else 0x80000001))[0]
    bits = struct.unpack('<I', struct.pack('<f', value))[0]
    bits += 1 if (value > 0) == up else -1
    return struct.unpack('<f', struct.pack('<I', bits))[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    lib = library()
    bindings(lib)
    lib.bk_ending_camera_assets_opening.argtypes = [
        C.c_void_p, C.c_void_p, C.POINTER(U), C.POINTER(State), U, F,
        C.POINTER(Input), C.POINTER(C.c_int), C.c_void_p]
    lib.bk_actor_pose_edit_clips.argtypes = [
        C.c_void_p, C.POINTER(ClipEdit), C.c_size_t, C.c_void_p]
    error = C.create_string_buffer(256)
    indices = (U * 2)(1, 2)
    archive = Archive(args.data / 'bk3_04.pp')
    xan = archive.read(next(x for x in archive.entries if x.name == 'cam00_03.xan'))
    store = lib.bk_resources_create(error)
    assert store, error.value
    assert lib.bk_resources_mount(store, b'bk3_04',
                                  str(args.data / 'bk3_04.pp').encode(), error)
    fixture = bytearray(0x5190)
    fixture[:9] = fixture[256:265] = b'target.x\0'
    clips = lib.bk_clip_set_decode(bytes(fixture), len(fixture), error)
    assert clips, error.value
    root = Frame()
    root.id = 1
    root.parent_index = root.mesh_index = 0xffffffff
    root.local[:] = I
    model = Model()
    model.frames = C.pointer(root)
    model.frame_count = 1
    target = lib.bk_actor_pose_create_loaded(
        C.byref(model), clips, 0, (F * 3)(100, 18, 40), 0, error)
    assert target, error.value
    frames = matrices = key_calls = mutations = completions = boundaries = 0
    worst = 0.0

    def equal(got, want, label):
        nonlocal worst
        for i, (a, b) in enumerate(zip(got, want)):
            difference = abs(a - b) / max(1, abs(b))
            worst = max(worst, difference)
            assert math.isfinite(difference) and difference < 3e-6, (
                label, i, a, b, difference)

    def snapshot(pose, source_model):
        return b''.join(C.string_at(getattr(lib, name)(pose, i), 64)
                        for i in range(source_model.frame_count)
                        for name in ('bk_actor_pose_local', 'bk_actor_pose_frame',
                                     'bk_actor_pose_parent_world'))

    def compare(pose, source_model, vm, label):
        nonlocal matrices
        for i in range(source_model.frame_count):
            for name, offset in [('bk_actor_pose_local', 0x80),
                                 ('bk_actor_pose_frame', 0xc0),
                                 ('bk_actor_pose_parent_world', 0x100)]:
                equal(getattr(lib, name)(pose, i)[:16],
                      vm.floats(vm.frames + i * 0x400 + offset, 16),
                      (label, name, i))
                matrices += 1
        state = ClipState()
        assert lib.bk_actor_pose_state(pose, C.byref(state))
        equal([getattr(state, name) for name, _ in ClipState._fields_],
              vm.state(), (label, 'playback'))

    try:
        for profile in range(50):
            group, variant = divmod(profile, 10)
            degrees = 27 + group * 61 + variant * 9
            state = State()
            state.pose.world[:] = state.matrix[:] = I
            state.pose.world[12] = 9
            state.matrix[12] = 21
            state.focus[:] = [7, 8, 9]
            assets = lib.bk_ending_camera_assets_create(
                store, group, variant, degrees, error)
            assert assets, error.value
            forest = None
            try:
                poses = [lib.bk_ending_camera_assets_pose(assets, i) for i in range(2)]
                models = [lib.bk_actor_pose_model(p).contents for p in poses]
                forest = lib.bk_actor_forest_create(
                    (C.c_void_p * 3)(target, *poses), 3, error)
                assert forest, error.value
                target_node = lib.bk_actor_forest_node(forest, 0, 0)
                assert lib.bk_actor_forest_attach(forest, 0, target_node, error)
                assert lib.bk_actor_forest_anchor(forest, 1, state.pose.world, 0, error)
                presets = Presets()
                assert lib.bk_ending_camera_assets_attach(
                    assets, forest, indices, C.byref(state), C.byref(presets), error)
                state.fov = F(.37 + group * .03).value
                vm = Native(exe)
                secondary = models[1]
                vm.bind(C.string_at(secondary.source, secondary.source_size), secondary)
                node = lib.bk_ending_camera_assets_node(assets, 1)
                vm.bind_clip(xan, node)
                vm.connect()
                vm.vector(vm.argument, [0, 1, 0])
                vm.call(0x42363b, struct.pack(
                    '<IIIf', vm.frames + vm.root * 0x400, 1, vm.argument,
                    F(degrees * .01745329238474369).value))
                vm.word(vm.controller + 4, vm.clip)
                vm.word(vm.controller + 12, vm.frames + node * 0x400)
                vm.word(0x719448, vm.head)
                vm.vector(vm.head + 0xf0, [100, 18, 40])
                vm.set_state(state)
                compare(poses[1], secondary, vm, (profile, 'initial'))
                for step in range(120):
                    dt = F([0, .016, .033, .1, .25, 1.25][step % 6]).value
                    boundary = step % 10 < 3
                    hidden = boundary or step % 11 == 3
                    edit = Edit(lib.bk_ending_camera_assets_root(assets, 1), hidden)
                    assert lib.bk_actor_pose_visibility(poses[1], C.byref(edit), 1, error)
                    vm.call(0x423a99, struct.pack('<II', vm.frames + vm.root * 0x400, hidden))
                    active = ClipState()
                    assert lib.bk_actor_pose_state(poses[1], C.byref(active))
                    end = vm.floats(vm.clip + 0x1e8 + active.slot * 0x9c, 1)[0]
                    if boundary:
                        source = F(end - 9).value
                        if step % 10 != 1:
                            source = adjacent(source, step % 10 == 2)
                        change = ClipEdit(active.slot, 4, 0, 0, source)
                        assert lib.bk_actor_pose_edit_clips(poses[1], C.byref(change), 1, error)
                        vm.vector(vm.clip + 0x1f0 + active.slot * 0x9c, [source])
                        boundaries += 1
                    keys = [0x100, 0, 0x10000, 0x80000000]
                    accepted = (step // 3) % 5
                    if not boundary and accepted < 4:
                        keys[accepted] = [1, 0xff, 0x101, 0x80000001][accepted]
                    vm.keys = keys
                    vm.trace, vm.before_key = [], []
                    vm.mutate_at = 2 if not boundary and step % 7 == 3 else 0
                    vm.mutation = F(end - 8.5 if step % 2 else end - 10).value
                    vm.vector(0x733700, [dt])
                    primary = ClipState()
                    assert lib.bk_actor_pose_state(poses[0], C.byref(primary))
                    primary_cache = snapshot(poses[0], models[0])
                    vm.call(0x4e0b7e, struct.pack('<I', vm.controller))
                    wanted_done = vm.uc.reg_read(UC_X86_REG_EAX) & 255
                    trace = []
                    callback_errors = []

                    @Key
                    def key(_context, code, mode, result, _error):
                        try:
                            assert mode == 1 and code == vm.trace[len(trace)]
                            equal(values(state), values(vm.before_key[len(trace)]),
                                  (profile, step, 'before input'))
                            trace.append(code)
                            if len(trace) == vm.mutate_at:
                                change = ClipEdit(active.slot, 4, 0, 0, vm.mutation)
                                assert lib.bk_actor_pose_edit_clips(
                                    poses[1], C.byref(change), 1, error)
                            result[0] = keys[len(trace) - 1]
                            return 1
                        except Exception as exc:
                            callback_errors.append(repr(exc))
                            return 0

                    done = C.c_int(-1)
                    assert lib.bk_ending_camera_assets_opening(
                        assets, forest, indices, C.byref(state), target_node, dt,
                        C.byref(Input(None, key)), C.byref(done), error), (
                            profile, step, error.value, callback_errors)
                    assert not callback_errors
                    assert trace == vm.trace and done.value == wanted_done
                    equal(values(state), values(vm.output_state()), (profile, step, 'camera'))
                    compare(poses[1], secondary, vm, (profile, step, 'before draw'))
                    held = ClipState()
                    assert lib.bk_actor_pose_state(poses[0], C.byref(held))
                    assert bytes(held) == bytes(primary)
                    assert snapshot(poses[0], models[0]) == primary_cache
                    visits, count = C.POINTER(Visit)(), U()
                    only_camera = step % 7 == 3
                    assert lib.bk_actor_forest_draw(
                        forest, 1 if only_camera else 0, C.byref(visits), C.byref(count), error)
                    vm.draw(vm.rendered if only_camera else vm.context)
                    compare(poses[1], secondary, vm, (profile, step, 'after draw'))
                    frames += 1
                    key_calls += len(trace)
                    mutations += 0 < vm.mutate_at <= len(trace)
                    completions += done.value
            finally:
                lib.bk_actor_forest_destroy(forest)
                lib.bk_ending_camera_assets_destroy(assets)
            if profile % 10 == 9:
                print('PASS opening camera group', group, flush=True)
    finally:
        lib.bk_actor_pose_destroy(target)
        lib.bk_clip_set_destroy(clips)
        lib.bk_resources_destroy(store)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  profiles=50, frames=frames, matrices=matrices, key_calls=key_calls,
                  source_mutations=mutations, completions=completions,
                  end_minus_nine_boundary_frames=boundaries,
                  max_relative_error=worst, scope=__doc__)
    (ROOT / 'local/original-ending-opening-camera-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
