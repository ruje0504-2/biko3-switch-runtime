"""Original mixed clock/effect execution on the live third-stage asset owners.

The caller supplies separately decoded actors and runs the existing original
loader oracle. This mixin adds real MATA loading/sampling to that same machine;
ANIM and MORP continue through the original instructions already installed by
the asset oracle. Only allocation, material-ID lookup and mesh locking are
boundaries. Requests, clock edits and visibility changes below are explicit
test inputs, not a natural game/controller walkthrough.
"""
from __future__ import annotations

import ctypes as C
import hashlib
import math
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

from clip_binding import State
from model_binding import Material
from original_fixed_actor_oracle import Visibility
from original_material_animation_oracle import Edit, Values


ADDRESSES = {-2: 0x4021a1, -1: 0x4026fe, 0: 0x402e18, 1: 0x4e18ad, 2: 0x4a9019}


def bind(lib):
    pointer, integer, number = C.c_void_p, C.c_uint32, C.c_float
    for name, args, result in [
        ('bk_ending_tertiary_assets_advance', [pointer, integer, number, pointer], C.c_int),
        ('bk_ending_tertiary_assets_advance_plain', [pointer, integer, number, C.c_int, pointer], C.c_int),
        ('bk_ending_tertiary_assets_material_animation', [pointer, integer], pointer),
        ('bk_actor_pose_set_clock', [pointer, integer, number, number, pointer], C.c_int),
        ('bk_actor_pose_request_active', [pointer, integer, pointer], C.c_int),
        ('bk_actor_pose_request', [pointer, integer, pointer], C.c_int),
    ]:
        function = getattr(lib, name)
        function.argtypes, function.restype = args, result


class MixedEffectsReference:
    """Mixin before the existing tertiary-asset Reference in the MRO."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.material_ids = {}
        self.material_groups = {}
        self.material_objects = {}
        self.material_undefined = {}
        self.effect_calls = []
        self.uc.hook_add(UC_HOOK_CODE, self.material_lookup, begin=0x415ed8, end=0x415ed8)
        self.uc.hook_add(UC_HOOK_CODE, self.material_setter, begin=0x43041a, end=0x43041a)
        for address in [0x4097d6, 0x409a94, 0x4300e0, 0x433a7d, 0x433cbe]:
            self.uc.hook_add(UC_HOOK_CODE, self.observe_effect, begin=address, end=address)

    def material_lookup(self, machine, _address, _size, _context):
        stack = machine.reg_read(UC_X86_REG_ESP)
        machine.reg_write(UC_X86_REG_EAX, self.material_ids.get(self.read(stack + 4), 0))
        machine.reg_write(UC_X86_REG_ESP, stack + 4)
        machine.reg_write(UC_X86_REG_EIP, self.read(stack))

    def material_setter(self, machine, _address, _size, _context):
        stack = machine.reg_read(UC_X86_REG_ESP)
        material = self.read(stack + 4)
        # Native interpolated specular/emissive W are uninitialized. Keep the
        # established portable-zero policy separate from defined-field error.
        self.material_undefined[material] = self.read(stack) in [0x42f921, 0x42fb71]

    def observe_effect(self, machine, address, _size, _context):
        stack = machine.reg_read(UC_X86_REG_ESP)
        group = self.read(stack + 4)
        count = 3 if address in [0x409a94, 0x433cbe] else 1
        self.effect_calls.append((address, group, *self.floats(stack + 8, count)))

    def setup_materials(self, models, raws, physical):
        for role, actor in physical.items():
            if role >= 3:
                continue
            model = models[actor].contents
            objects = []
            self.material_ids = {}
            for material in model.materials[:model.material_count]:
                target = self.alloc(bytes(0xd0))
                source = self.alloc(C.string_at(C.addressof(material) + Material.diffuse.offset, 68))
                self.call(0x43041a, struct.pack('<2I', target, source))
                self.word(target + 4, 0x3f6)
                self.word(target + 0x6c, 1)
                self.material_ids[material.id] = target
                objects.append(target)
            self.material_objects[role] = objects
            chunk = next((c for c in model.chunks[:model.chunk_count] if c.tag == b'MATA'), None)
            if chunk is None:
                continue
            payload = raws[actor][chunk.offset:chunk.offset + chunk.size]
            assert len(payload) == chunk.size
            source, output = self.alloc(payload), self.alloc(bytes(4))
            self.word(self.stack - 0x164, source)
            self.word(self.stack - 0x148, output)
            self.uc.reg_write(UC_X86_REG_EBP, self.stack)
            self.uc.reg_write(UC_X86_REG_ESP, self.stack - 0x2000)
            self.uc.reg_write(UC_X86_REG_FPCW, 0x037f)
            self.uc.emu_start(0x419c86, 0x419e03, count=10000000)
            assert self.uc.reg_read(UC_X86_REG_EIP) == 0x419e03
            assert self.read(self.stack - 0x164) == source + len(payload)
            group = self.read(output)
            assert group
            self.material_groups[role] = group
            self.word(self.models[actor] + 0x158, group)


def verify(lib, vm, owner, models, clips, physical, steps, check_poses, error):
    """Exercise production scene effect owners; never manually sample C effects."""
    assert steps >= 24
    roles = [role for role in physical if role < 3]
    counters = dict(frames=0, vertices=0, material_fields=0, undefined_w_fields=0,
                    hidden_frames=0, zero_seconds=0, requests=0, clock_edits=0,
                    material_edits=0, blend_samples=0, held_plain_after_blend=0,
                    fixed_to_seconds=0, rejections=0, terminal_owner_failures=0)
    by_mode = {str(mode): 0 for mode in ADDRESSES}
    digest, worst = hashlib.sha256(), 0.0
    # An auxiliary component failure invalidates the shared BOM owner. Keep
    # borrowed addresses only to audit its applied prefix before destruction;
    # do not treat an invalidated owner as available for further animation.
    borrowed = {role: (
        lib.bk_ending_tertiary_assets_morph(owner, role),
        lib.bk_ending_tertiary_assets_material_animation(owner, role),
        lib.bk_ending_tertiary_assets_materials(owner, role)) for role in roles}
    terminal_failure = False

    def equal(actual, expected, label):
        nonlocal worst
        assert len(actual) == len(expected), label
        for index, (got, want) in enumerate(zip(actual, expected)):
            if got == want:
                continue
            delta = abs(got - want) / max(1, abs(want))
            worst = max(worst, delta)
            assert math.isfinite(delta) and delta < 3e-5, (label, index, got, want, delta)

    def check(label):
        check_poses(('mixed-effects', label))
        for role in roles:
            actor = physical[role]
            pose = lib.bk_ending_tertiary_assets_pose(owner, role)
            state = State()
            assert lib.bk_actor_pose_state(pose, C.byref(state))
            vm.clip = vm.clips[actor]
            assert bytes(state) == bytes(State(*vm.state())), (label, role, 'clock')
            digest.update(bytes(state))
            morph, animation, materials = borrowed[role]
            actual_owners = (
                lib.bk_ending_tertiary_assets_morph(owner, role),
                lib.bk_ending_tertiary_assets_material_animation(owner, role),
                lib.bk_ending_tertiary_assets_materials(owner, role))
            assert actual_owners == ((None, None, None) if terminal_failure and role else borrowed[role]), (label, role, 'owner lifetime')
            native_morph = vm.aux_effects.get(actor)
            assert bool(morph) == bool(native_morph), (label, role, 'morph owner')
            if native_morph:
                meshes, group = native_morph
                time = lib.bk_morph_group_time(morph)
                assert time == vm.floats(group + 0x74, 1)[0], (label, role, 'morph cache')
                digest.update(struct.pack('<f', time))
                for submesh, target in meshes.items():
                    count, vertices = vm.meshes[target]
                    mesh = lib.bk_morph_group_mesh(morph, submesh)
                    assert mesh
                    got = C.string_at(lib.bk_morph_mesh_vertices(mesh), count * 60)
                    want = bytes(vm.uc.mem_read(vertices, count * 60))
                    if got != want:
                        for vertex in range(count):
                            offset = vertex * 60
                            equal(struct.unpack_from('<9f', got, offset),
                                  struct.unpack_from('<9f', want, offset),
                                  (label, role, submesh, vertex))
                            assert got[offset + 36:offset + 60] == want[offset + 36:offset + 60]
                    digest.update(got)
                    counters['vertices'] += count
            assert bool(animation) == (role in vm.material_groups)
            if animation:
                time = lib.bk_material_animation_time(animation)
                assert time == vm.floats(vm.material_groups[role] + 0x74, 1)[0]
                digest.update(struct.pack('<f', time))
            for index, target in enumerate(vm.material_objects[role]):
                material = lib.bk_material_pose_material(materials, index).contents
                raw = C.string_at(C.addressof(material) + Material.diffuse.offset, 68)
                got, want = struct.unpack('<17f', raw), vm.floats(target + 0x70, 17)
                for field in range(17):
                    if vm.material_undefined[target] and field in [11, 15]:
                        assert got[field] == 0, (label, role, index, field)
                        counters['undefined_w_fields'] += 1
                    else:
                        equal([got[field]], [want[field]], (label, role, index, field))
                        counters['material_fields'] += 1
                digest.update(raw)

    check('loaded')
    for role in roles:
        actor = physical[role]
        pose = lib.bk_ending_tertiary_assets_pose(owner, role)
        native_clip = vm.clips[actor]
        root = vm.roots[actor]
        native_root = vm.frame_bases[actor] + root * 0x400
        active = [slot for slot in range(128) if lib.bk_clip_definition(clips[actor], slot).contents.active]
        assert active
        previous_mode, last_was_blend = None, False
        cached_before_request = None
        for step in range(steps):
            # Each cycle includes both repeated zero-dt calls and a request
            # into a blend followed by a plain sample at its old cached time.
            mode = [-1, -1, -1, -1, -1, 0, 1, 2, -2, -2, 0, -1][step % 12]
            if role == 0 and mode == -2:
                mode = 1
            seconds = C.c_float([0, .05, .1, 0, .05, 0, .125, .5, 0, 0, 0, 2][step % 12]).value
            hidden = 7 if step % 19 in [14, 15] else 0
            visibility = Visibility(root, hidden)
            vm.call(0x423a99, struct.pack('<2I', native_root, hidden))
            assert lib.bk_actor_pose_visibility(pose, C.byref(visibility), 1, error), error.value
            if step % 12 == 3:
                anim = vm.read(vm.models[actor] + 0x148)
                cached_before_request = vm.floats(anim + 0x74, 1)[0] if anim else 0
                state = State()
                assert lib.bk_actor_pose_state(pose, C.byref(state))
                slot = next((s for s in active if s != state.slot), active[0])
                vm.call(0x401b0a, struct.pack('<2I', native_clip, slot))
                assert lib.bk_actor_pose_request(pose, slot, error), error.value
                counters['requests'] += 1
            if step % 12 == 5 and cached_before_request is not None:
                state = State()
                assert lib.bk_actor_pose_state(pose, C.byref(state))
                offset = native_clip + 0x190 + state.slot * 156
                vm.vector(offset + 0x44, [0])
                vm.vector(offset + 0x60, [cached_before_request])
                assert lib.bk_actor_pose_set_clock(pose, state.slot, 0, cached_before_request, error), error.value
                counters['clock_edits'] += 1
            if step % 12 == 10 and role in vm.material_groups:
                materials = lib.bk_ending_tertiary_assets_materials(owner, role)
                material = lib.bk_material_pose_material(materials, 0).contents
                values = bytearray(C.string_at(C.addressof(material) + Material.diffuse.offset, 68))
                struct.pack_into('<f', values, 0, .125 + (step % 4) * .125)
                edit = Edit(0, material.id, Values.from_buffer_copy(values))
                source = vm.alloc(bytes(values))
                vm.call(0x43041a, struct.pack('<2I', vm.material_objects[role][0], source))
                assert lib.bk_material_pose_values(materials, C.byref(edit), 1, error), error.value
                counters['material_edits'] += 1
            anim = vm.read(vm.models[actor] + 0x148)
            cached = vm.floats(anim + 0x74, 1)[0] if anim else 0
            vm.effect_calls.clear()
            args = struct.pack('<I', native_clip) if mode == -2 else struct.pack('<If', native_clip, seconds)
            vm.call(ADDRESSES[mode], args)
            if mode == -2:
                bom = lib.bk_ending_tertiary_assets_bom(owner)
                assert bom and lib.bk_bom_dual_assets_advance_frame(bom, role, error), error.value
            elif mode == -1:
                assert lib.bk_ending_tertiary_assets_advance(owner, role, seconds, error), error.value
            else:
                assert lib.bk_ending_tertiary_assets_advance_plain(owner, role, seconds, mode, error), error.value
            if hidden:
                assert not vm.effect_calls, (role, step, 'hidden effects')
            anim_calls = [call for call in vm.effect_calls if call[0] in [0x4097d6, 0x409a94]]
            if anim_calls:
                assert len(anim_calls) == 1
                sample = anim_calls[0]
                if last_was_blend and sample[0] == 0x4097d6 and sample[2] == cached:
                    counters['held_plain_after_blend'] += 1
                last_was_blend = sample[0] == 0x409a94
                counters['blend_samples'] += last_was_blend
            counters['fixed_to_seconds'] += previous_mode == -2 and mode != -2
            previous_mode = mode
            check((role, step, mode))
            counters['frames'] += 1
            counters['hidden_frames'] += bool(hidden)
            counters['zero_seconds'] += mode != -2 and seconds == 0
            by_mode[str(mode)] += 1
            # Primary argument rejection preserves the owner. Auxiliary mode
            # validation also occurs before entering its component lifecycle.
            if step == steps - 1:
                visibility = Visibility(root, 0)
                vm.call(0x423a99, struct.pack('<2I', native_root, 0))
                assert lib.bk_actor_pose_visibility(pose, C.byref(visibility), 1, error)
                invalid = [(-1, 0), (float('nan'), 0), (float('inf'), 0)] if role == 0 else [(0, 9)]
                for bad, bad_mode in invalid:
                    assert not lib.bk_ending_tertiary_assets_advance_plain(owner, role, bad, bad_mode, error)
                    check((role, 'rejected', str(bad), bad_mode))
                    counters['rejections'] += 1
    if len(roles) > 1:
        assert not lib.bk_ending_tertiary_assets_advance_plain(owner, roles[-1], -1, 0, error)
        terminal_failure = True
        bom = lib.bk_ending_tertiary_assets_bom(owner)
        assert lib.bk_bom_dual_assets_count(bom) == 0
        assert not lib.bk_bom_dual_assets_advance_frame(bom, roles[-1], error)
        check('terminal component failure preserves borrowed data, invalidates owner')
        counters['terminal_owner_failures'] += 1
    assert counters['blend_samples'] and counters['held_plain_after_blend'], counters
    return dict(**counters, by_mode=by_mode, max_normalized_error=worst,
                state_sha256=digest.hexdigest(), native_sampling=True,
                natural_controller_or_device_validation=False)
