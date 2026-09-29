"""Real selected actors:495469/4952C8 edits followed by actual plain effects.

Called on the original loader oracle's independently decoded Japanese assets.
Input coordinates/motion, visibility and clip requests are explicit fixtures.
Original motion/sampling instructions are unchanged. Compare caches both
before and after explicit sampling; no world publication is allowed here.
This is not a natural48E75B action sequence or a playable selected scene.
"""
import ctypes as C
import hashlib
import struct

from unicorn.x86_const import UC_X86_REG_FPSW, UC_X86_REG_FPTAG
from clip_binding import State


def verify(lib, vm, owner, clips, steps, check_poses, check_effects, error):
    assert steps >= 24 and steps % 24 == 0
    pose = lib.bk_ending_selected_assets_pose(owner, 0)
    forest = lib.bk_ending_selected_assets_forest(owner)
    root = lib.bk_ending_selected_assets_root(owner, 0)
    active = [i for i in range(128) if lib.bk_clip_definition(clips, i).contents.active]
    assert pose and forest and active
    counters = dict(frames=0, pointer_edits=0, drag_edits=0, requests=0,
                    hidden_frames=0, snapshots=0, rejections=0, preserved_elapsed=0)
    digest = hashlib.sha256()
    modes = {0: 0, 1: 0, 2: 0}
    addresses = {0: 0x402e18, 1: 0x4e18ad, 2: 0x4a9019}
    XY, Motion = C.c_int32 * 2, C.c_float * 2

    def state():
        out = State()
        assert lib.bk_actor_pose_state(pose, C.byref(out))
        return out

    def check(label):
        check_poses(('motion', label))
        check_effects(('motion', label))
        out = state()
        vm.clip = vm.clips[0]
        assert bytes(out) == bytes(State(*vm.state())), (label, 'exact clock')
        digest.update(bytes(out))
        counters['snapshots'] += 1

    for step in range(steps):
        hidden = 7 if step % 12 in [9, 10] else 0
        vm.call(0x423a99, struct.pack('<2I', vm.root(0), hidden))
        assert lib.bk_actor_forest_visibility(forest, root, hidden, error), error.value
        if step % 8 == 0:
            old = state()
            selected = active[(step // 8 + 1) % len(active)]
            if selected == old.slot:
                selected = next((i for i in active if i != old.slot), active[0])
            vm.call(0x4018c8, struct.pack('<2I', vm.clips[0], selected))
            assert lib.bk_actor_pose_request_mode(pose, selected, 1, error), error.value
            counters['requests'] += 1
        before = state()
        descriptor = vm.clips[0] + 0x190 + before.slot * 156
        native_before = bytes(vm.uc.mem_read(descriptor, 156))
        vm.effect_calls.clear()
        vm.uc.reg_write(UC_X86_REG_FPSW, 0)
        vm.uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
        if step % 2 == 0:
            target, menu = XY(720, 440), XY(240, 180)
            cursor = [XY(245, 185), XY(720, 440), XY(1000, 700)][step // 2 % 3]
            if step % 16 == 0:
                target[:] = menu[:]
            inputs = (C.c_uint32 * 11)()
            inputs[9:11] = [value & 0xffffffff for value in cursor]
            vm.uc.mem_write(0x6afd38, bytes(target))
            vm.uc.mem_write(0x7220c8, bytes(menu))
            vm.call(0x495469, struct.pack('<I', vm.clips[0]) + bytes(inputs))
            assert lib.bk_ending_selected_assets_pointer(owner, target, menu, cursor, error), error.value
            counters['pointer_edits'] += 1
        else:
            setting, reverse = (step // 2) % 2, (step // 4) % 2
            motion = Motion((step % 11 - 5) * .75, (step % 7 - 3) * .375)
            if not setting and step % 8 == 1:
                motion[1] = float('nan')
            vm.word(0xb53c38, setting)
            vm.word(0x6ea314, reverse)
            vm.call(0x4952c8, struct.pack('<I', vm.clips[0]) + bytes(motion))
            assert lib.bk_ending_selected_assets_drag(owner, setting, reverse, motion, error), error.value
            counters['drag_edits'] += 1
        assert not vm.effect_calls, ('input unexpectedly sampled', step)
        after = state()
        assert before.elapsed == after.elapsed
        before.source = after.source
        assert bytes(before) == bytes(after), ('input changed unrelated clock fields', step)
        expected = bytearray(native_before)
        expected[0x60:0x64] = vm.uc.mem_read(descriptor + 0x60, 4)
        assert bytes(vm.uc.mem_read(descriptor, 156)) == expected
        counters['preserved_elapsed'] += 1
        check(('after-input', step))

        mode = step % 3
        seconds = C.c_float([0, 1 / 120, 1 / 60, 1 / 30][step % 4]).value
        vm.uc.reg_write(UC_X86_REG_FPSW, 0)
        vm.uc.reg_write(UC_X86_REG_FPTAG, 0xffff)
        vm.call(addresses[mode], struct.pack('<If', vm.clips[0], seconds))
        assert lib.bk_ending_selected_assets_advance_plain(owner, seconds, mode, error), error.value
        if hidden:
            assert not vm.effect_calls, ('hidden primary sampled', step)
        check(('after-sampling', step, mode))
        counters['frames'] += 1
        counters['hidden_frames'] += bool(hidden)
        modes[mode] += 1

    vm.call(0x423a99, struct.pack('<2I', vm.root(0), 0))
    assert lib.bk_actor_forest_visibility(forest, root, 0, error), error.value
    failures = [
        ('drag', owner, 0, 0, None),
        ('drag', owner, 0, 0, Motion(float('nan'), 1)),
        ('drag', owner, 1, 0, Motion(1, float('nan'))),
        ('pointer', owner, None, XY(), XY()),
        ('pointer', owner, XY(), None, XY()),
        ('pointer', owner, XY(), XY(), None),
        ('drag', None, 0, 0, Motion()),
        ('pointer', None, XY(), XY(), XY()),
    ]
    for index, (name, *arguments) in enumerate(failures):
        before = bytes(state())
        assert not getattr(lib, 'bk_ending_selected_assets_' + name)(*arguments, error)
        assert bytes(state()) == before and error.value
        check(('rejected', index))
        counters['rejections'] += 1
    return dict(**counters, by_mode={str(k): v for k, v in modes.items()},
                native_sampling=True, state_sha256=digest.hexdigest(), scope=__doc__)
