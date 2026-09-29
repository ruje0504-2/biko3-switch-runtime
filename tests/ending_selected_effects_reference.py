"""Mixed original4026FE/402E18/4E18AD/4A9019 on the selected primary owner.

Called after the original loader comparison with the same independently
decoded native model. All ANIM/MATA/MORP work executes original instructions;
only requests, visibility, clocks and explicit material edits are test inputs.
Neither side publishes worlds here. Background and cached targets must remain
owned and unchanged. This is not a natural selected-stage controller run.
"""
import ctypes as C
import hashlib
import struct

from clip_binding import State
from model_binding import Material
from original_material_animation_oracle import Edit, Values


def verify(lib, vm, owner, clips, steps, check_poses, check_effects, error):
    assert steps >= 24 and steps % 12 == 0
    lib.bk_actor_pose_set_clock.argtypes = [C.c_void_p, C.c_uint,
                                           C.c_float, C.c_float, C.c_void_p]
    lib.bk_actor_pose_set_clock.restype = C.c_int
    lib.bk_material_pose_values.argtypes = [C.c_void_p, C.POINTER(Edit),
                                           C.c_size_t, C.c_void_p]
    lib.bk_material_pose_values.restype = C.c_int
    pose = lib.bk_ending_selected_assets_pose(owner, 0)
    forest = lib.bk_ending_selected_assets_forest(owner)
    root = lib.bk_ending_selected_assets_root(owner, 0)
    materials = lib.bk_ending_selected_assets_materials(owner, 0)
    animation = vm.read(vm.models[0] + 0x148)
    assert pose and forest and animation
    active = [i for i in range(128) if lib.bk_clip_definition(clips, i).contents.active]
    assert active
    counters = dict(frames=0, hidden_frames=0, zero_seconds=0, requests=0,
                    clock_edits=0, material_edits=0, blend_samples=0,
                    held_plain_after_blend=0, rejections=0)
    modes = {-1: 0, 0: 0, 1: 0, 2: 0}
    addresses = {-1: 0x4026fe, 0: 0x402e18, 1: 0x4e18ad, 2: 0x4a9019}
    digest = hashlib.sha256()
    last_was_blend = False
    cached_before_request = None

    def check(label):
        check_poses(('mixed', label))
        check_effects(('mixed', label))
        state = State()
        assert lib.bk_actor_pose_state(pose, C.byref(state))
        vm.clip = vm.clips[0]
        assert bytes(state) == bytes(State(*vm.state())), (label, 'exact clock')
        digest.update(bytes(state))

    for step in range(steps):
        mode = [-1, -1, -1, -1, -1, 0, 1, 2, 1, 1, 0, -1][step % 12]
        seconds = C.c_float([0, .05, .1, 0, .05, 0, .125, .5, 0, 0, 0, 2][step % 12]).value
        hidden = 7 if step % 19 in [14, 15] else 0
        vm.call(0x423a99, struct.pack('<2I', vm.root(0), hidden))
        assert lib.bk_actor_forest_visibility(forest, root, hidden, error), error.value
        if step % 12 == 3:
            cached_before_request = vm.floats(animation + 0x74, 1)[0]
            state = State()
            assert lib.bk_actor_pose_state(pose, C.byref(state))
            selected = next((i for i in active if i != state.slot), active[0])
            vm.call(0x401b0a, struct.pack('<2I', vm.clips[0], selected))
            assert lib.bk_actor_pose_request_mode(pose, selected, 0, error), error.value
            counters['requests'] += 1
        if step % 12 == 5 and cached_before_request is not None:
            state = State()
            assert lib.bk_actor_pose_state(pose, C.byref(state))
            descriptor = vm.clips[0] + 0x190 + state.slot * 156
            vm.vector(descriptor + 0x44, [0])
            vm.vector(descriptor + 0x60, [cached_before_request])
            assert lib.bk_actor_pose_set_clock(pose, state.slot, 0,
                                                cached_before_request, error), error.value
            counters['clock_edits'] += 1
        if step % 12 == 10:
            material = lib.bk_material_pose_material(materials, 0).contents
            values = bytearray(C.string_at(C.addressof(material) + Material.diffuse.offset, 68))
            struct.pack_into('<f', values, 0, .125 + (step % 4) * .125)
            source = vm.alloc(bytes(values))
            vm.call(0x43041a, struct.pack('<2I', vm.material_objects[0][0], source))
            edit = Edit(0, material.id, Values.from_buffer_copy(values))
            assert lib.bk_material_pose_values(materials, C.byref(edit), 1, error), error.value
            counters['material_edits'] += 1
        cached = vm.floats(animation + 0x74, 1)[0]
        vm.effect_calls.clear()
        vm.call(addresses[mode], struct.pack('<If', vm.clips[0], seconds))
        if mode == -1:
            assert lib.bk_ending_selected_assets_advance(owner, 0, seconds, error), error.value
        else:
            assert lib.bk_ending_selected_assets_advance_plain(owner, seconds, mode, error), error.value
        if hidden:
            assert not vm.effect_calls, (step, 'hidden model submitted effects')
        samples = [call for call in vm.effect_calls if call[0] in [0x4097d6, 0x409a94]]
        if samples:
            assert len(samples) == 1
            sample = samples[0]
            if last_was_blend and sample[0] == 0x4097d6 and sample[2] == cached:
                counters['held_plain_after_blend'] += 1
            last_was_blend = sample[0] == 0x409a94
            counters['blend_samples'] += last_was_blend
        check((step, mode))
        counters['frames'] += 1
        counters['hidden_frames'] += bool(hidden)
        counters['zero_seconds'] += seconds == 0
        modes[mode] += 1
    vm.call(0x423a99, struct.pack('<2I', vm.root(0), 0))
    assert lib.bk_actor_forest_visibility(forest, root, 0, error), error.value
    for seconds, mode in [(-1, 0), (float('nan'), 0), (float('inf'), 0), (0, 9)]:
        assert not lib.bk_ending_selected_assets_advance_plain(owner, seconds, mode, error)
        assert lib.bk_ending_selected_assets_pose(owner, 0) == pose
        check(('rejected', str(seconds), mode))
        counters['rejections'] += 1
    assert counters['blend_samples'] and counters['held_plain_after_blend'], counters
    return dict(**counters, by_mode={str(k): v for k, v in modes.items()},
                state_sha256=digest.hexdigest(), native_sampling=True,
                scope=__doc__)
