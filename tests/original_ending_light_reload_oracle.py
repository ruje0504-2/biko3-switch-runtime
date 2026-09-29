"""Native4a4140/4a435a/4a4438 across actual normal/secondary model reload.

Original425f40 executes its retained enable write and D3D LightEnable call.
Device calls are observed; actual scene owners provide model identity, names,
light data and caches. Old/new models have explicit initial enable0 fixtures.
This verifies registry reset/classification/ambient and retained-model enables,
not driver light-slot allocation, rasterization or the whole story flow.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from model_binding import ROOT
from playback_binding import library
from original_lighting_pass_oracle import Native, Input, Command
from original_lighting_oracle import Oracle
from original_ending_lighting_oracle import Source, World, Lighting, bind as lighting_bind
from original_ending_background_reload_oracle import bind as reload_bind, State, Presets, I


class NativeReload(Native):
    enable = 0x300d300
    registry = 0x300e000

    def __init__(self, exe):
        self.commands = []
        self.ambient = 0
        self.enables = {}
        super().__init__(exe)
        self.word(self.table + 0xb0, self.enable)
        self.u.hook_add(UC_HOOK_CODE, self.hook, begin=self.enable, end=self.enable)

    def hook(self, u, address, size, user):
        if address == 0x425f40:
            return  # Execute original write+70 and real device-call dispatch.
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, *args = struct.unpack('<4I', u.mem_read(sp, 16))
        if address == self.enable:
            assert args[0] == self.device
            self.enables[args[1]] = args[2]
            u.reg_write(UC_X86_REG_EAX, 0)
            u.reg_write(UC_X86_REG_ESP, sp + 16)
            u.reg_write(UC_X86_REG_EIP, ret)
            return
        if address == self.renderstate:
            self.ambient = args[2]
        super().hook(u, address, size, user)

    def install(self, flat):
        self.word(0x645614, self.registry)
        self.word(0x645618, len(flat))
        self.u.mem_write(self.registry, bytes(0x900))
        self.u.mem_write(self.key, b'BK3_L\0')
        for i, light in enumerate(flat):
            self.word(self.registry + i * 0x88 + 0x80, light['pointer'])
            self.word(self.registry + i * 0x88 + 0x84, 0x3ef)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-light-reload.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    lib = library()
    reload_bind(lib)
    lighting_bind(lib)
    for name in ['reset', 'select_bk3_l', 'ambient']:
        fn = getattr(lib, 'bk_scene_light_registry_' + name)
        fn.argtypes, fn.restype = [C.c_void_p], C.c_int
    lib.bk_scene_light_registry_inherit.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    err = C.create_string_buffer(256)
    store = lib.bk_resources_create(err)
    assert store, err.value
    for pack in ['bk3_08', 'bk3_09', 'bk3_04', 'bk3_03', 'fambom']:
        assert lib.bk_resources_mount(store, pack.encode(),
                    str(args.data / (pack + '.pp')).encode(), err), err.value
    profiles = snapshots = enable_calls = 0
    native_light = Oracle(exe)
    records = []
    try:
        for group in range(5):
            for variant in range(2):
                owners, registries, environments = [], [], []
                try:
                    vm = NativeReload(exe)
                    state, presets, random = State(), Presets(), C.c_uint32(123)
                    state.pose.world[:] = state.matrix[:] = I
                    clocks = (C.c_uint32 * 4)(100, 110, 120, 130)
                    normal = lib.bk_ending_normal_assets_create(store, group, variant,
                        clocks, C.byref(random), C.byref(state), C.byref(presets), err)
                    assert normal, err.value
                    owners.append(('normal', normal))
                    assert lib.bk_ending_normal_assets_load_background(normal, store, err), err.value
                    background = lib.bk_ending_normal_assets_background(normal)
                    secondary = lib.bk_ending_secondary_assets_create_reloaded(store,
                        group, variant, background, clocks, C.byref(random),
                        C.byref(state), C.byref(presets), err)
                    assert secondary, err.value
                    owners.append(('secondary', secondary))
                    models, source_sets = {}, []
                    light_identity = 0
                    for kind, owner in owners:
                        sources = []
                        order = [0, 4] if kind == 'normal' else ([0, 3] if group == 1 else [3, 0])
                        for actor in order:
                            pose = getattr(lib, f'bk_ending_{kind}_assets_pose')(owner, actor)
                            model = lib.bk_actor_pose_model(pose)
                            identity = C.addressof(model.contents)
                            if identity not in models:
                                count = C.c_size_t()
                                matrix = lib.bk_actor_pose_world(pose, C.byref(count))
                                world = (C.c_float * count.value)(*matrix[:count.value])
                                env = lib.bk_model_environment_create(model, world, err)
                                assert env, err.value
                                environments.append(env)
                                raw = C.string_at(model.contents.source, model.contents.source_size)
                                offset = next(chunk.offset for chunk in
                                    model.contents.chunks[:model.contents.chunk_count] if chunk.tag == b'LIGH')
                                lights = []
                                for i, light in enumerate(env.contents.lights[:env.contents.light_count]):
                                    index = light_identity
                                    light_identity += 1
                                    assert light_identity <= 32
                                    pointer = vm.light + index * 0x100
                                    frame, parent = 0x3004000 + index * 0x200, 0x300a000 + index * 0x100
                                    vm.u.mem_write(pointer, bytes(0x100))
                                    vm.word(pointer + 4, 0x3ef)
                                    vm.word(pointer + 0x6c, 1)
                                    vm.word(pointer + 0x70, 0)
                                    vm.word(pointer + 0x74, index)
                                    vm.word(pointer + 0xe0, frame)
                                    vm.word(frame + 0x22c, parent)
                                    vm.u.mem_write(pointer + 8, light.name + b'\0')
                                    vm.u.mem_write(pointer + 0x7c, bytes(light.diffuse))
                                    parent_index = model.contents.frames[light.frame_index].parent_index
                                    vm.u.mem_write(parent + 8, model.contents.frames[parent_index].name + b'\0')
                                    payload = raw[offset + i * 172 + 68:offset + (i + 1) * 172]
                                    submitted, _ = native_light.light_state(payload,
                                        world[light.frame_index * 16:(light.frame_index + 1) * 16])
                                    expected = struct.unpack('<I25f', submitted) if submitted else None
                                    lights.append(dict(pointer=pointer, index=index, kind=light.type,
                                                       expected=expected))
                                models[identity] = dict(model=model, world=world, lights=lights)
                            sources.append(models[identity])
                        source_sets.append(sources)
                    old, new = source_sets
                    for sources in source_sets:
                        value = (Source * len(sources))(*[Source(source['model'],
                            World(source['world'], len(source['world']))) for source in sources])
                        registry = lib.bk_scene_light_registry_create(value, len(sources), err)
                        assert registry, err.value
                        registries.append(registry)
                    old_flat = [light for source in old for light in source['lights']]
                    new_flat = [light for source in new for light in source['lights']]
                    assert lib.bk_scene_light_registry_command(registries[0], C.byref(Command(0, 0, 0x123456)))
                    vm.call(vm.renderstate, struct.pack('<3I', vm.device, 139, 0x123456))
                    # Explicit changed old enables; selecting at most one light
                    # per model stays within hardware-independent shader caps.
                    old_index = 0
                    for source in old:
                        eligible = [i for i, light in enumerate(source['lights']) if light['kind'] in [1, 2]]
                        selected = eligible[(group + variant) % len(eligible)] if eligible else -1
                        for index, light in enumerate(source['lights']):
                            enabled = int(index == selected)
                            assert lib.bk_scene_light_registry_command(registries[0],
                                                C.byref(Command(1, old_index, enabled)))
                            vm.call(0x425f40, struct.pack('<II', light['pointer'], enabled))
                            old_index += 1
                            enable_calls += 1
                    assert lib.bk_scene_light_registry_inherit(registries[1], registries[0], err), err.value
                    caches = (World * len(new))(*[World(source['world'], len(source['world'])) for source in new])

                    def check(registry_count, label):
                        nonlocal snapshots
                        inp, out = Input(), Lighting()
                        assert lib.bk_scene_light_registry_input(registries[1], C.byref(inp))
                        assert inp.light_count == registry_count == struct.unpack('<I', vm.u.mem_read(0x705738, 4))[0]
                        assert lib.bk_scene_light_registry_values(registries[1], caches, len(new), C.byref(out), err), err.value
                        assert [round(v * 255) for v in out.ambient] == [(vm.ambient >> shift) & 255 for shift in [16, 8, 0]], label
                        selected = [light for light in new_flat if light['kind'] in [1, 2] and
                                    struct.unpack('<I', vm.u.mem_read(light['pointer'] + 0x70, 4))[0]]
                        assert out.point_count == sum(light['kind'] == 1 for light in selected), label
                        assert out.spot_count == sum(light['kind'] == 2 for light in selected), label
                        point_index = spot_index = 0
                        for light in selected:
                            kind, *value = light['expected']
                            if kind == 1:
                                point = out.points[point_index]
                                point_index += 1
                            else:
                                assert kind == 2
                                spot = out.spots[spot_index]
                                point = spot.point
                                spot_index += 1
                                assert list(spot.direction) == value[15:18], label
                                assert [spot.falloff, spot.theta, spot.phi] == [value[19], value[23], value[24]], label
                            assert list(point.position) == value[12:15], label
                            assert list(point.diffuse) == value[:3], label
                            assert list(point.specular) == value[4:7], label
                            assert list(point.ambient) == value[8:11], label
                            assert [point.range, point.attenuation0, point.attenuation1,
                                    point.attenuation2] == [value[18], *value[20:23]], label
                        assert [light.group for light in inp.lights[:registry_count]] == list(
                            struct.unpack('<' + 'i' * registry_count, vm.u.mem_read(0x7056f8, 4 * registry_count)))
                        assert [light.ambient_rank for light in inp.lights[:registry_count]] == list(
                            struct.unpack('<' + 'i' * registry_count, vm.u.mem_read(0x70573c, 4 * registry_count)))
                        snapshots += 1

                    vm.install(old_flat)
                    vm.call(0x4a4140, b'')
                    assert lib.bk_scene_light_registry_reset(registries[1])
                    check(0, 'reset retains device state')
                    vm.install(new_flat)
                    vm.call(0x4a435a, struct.pack('<I', vm.key))
                    assert lib.bk_scene_light_registry_select_bk3_l(registries[1])
                    check(len(new_flat), 'classify retains device state')
                    vm.call(0x4a4438, b'')
                    assert lib.bk_scene_light_registry_ambient(registries[1])
                    check(len(new_flat), 'ambient update retains enable state')
                    for index, light in enumerate(new_flat):
                        # Toggle each actual light, checking its native cached
                        # enable as well as the resulting composed light count.
                        for enabled in [1, 0]:
                            assert lib.bk_scene_light_registry_command(registries[1],
                                                    C.byref(Command(1, index, enabled)))
                            vm.call(0x425f40, struct.pack('<II', light['pointer'], enabled))
                            enable_calls += 1
                            check(len(new_flat), (index, enabled))
                    records.append(dict(group=group, variant=variant,
                        old_lights=len(old_flat), new_lights=len(new_flat), shared_background=group != 1))
                    profiles += 1
                    print('PASS light reload', group, variant, flush=True)
                finally:
                    for registry in registries:
                        lib.bk_scene_light_registry_destroy(registry)
                    for env in environments:
                        lib.bk_model_environment_destroy(env)
                    for kind, owner in reversed(owners):
                        getattr(lib, f'bk_ending_{kind}_assets_destroy')(owner)
    finally:
        lib.bk_resources_destroy(store)
    report = dict(passed=True, exe_sha256=digest, profiles=profiles,
                  snapshots=snapshots, enable_calls=enable_calls, records=records, scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS light reload', profiles, 'profiles', snapshots, 'snapshots', enable_calls, 'native enables')


if __name__ == '__main__':
    main()
