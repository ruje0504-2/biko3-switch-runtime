"""Original third-ending tables and4a7d10 on actual Japanese material owners.

429c6b registry search/string comparison and4a7d10 alpha branching execute
unmodified. Registry order is an explicit fresh/retained-background fixture;
reference counting and material get/set calls are observing boundaries. This
does not verify original full registry construction, GPU output or a loader.
"""
from __future__ import annotations
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct

import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native
from original_ending_tertiary_assets_oracle import bind, Config, CameraState, Presets
from model_binding import Material, ROOT
from playback_binding import library


class Registry(Native):
    base, names, lookup = 0x6000000, 0x6020000, 0x6030000

    def __init__(self, raw):
        super().__init__(raw)
        self.u.mem_map(self.base, 0x100000)
        self.values = {}
        for address in [0x42a0a5, 0x42a0cd, 0x430553, 0x43041a]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def install(self, entries):
        assert len(entries) < 900
        self.values = {}
        rows = []
        for index, (name, value) in enumerate(entries):
            handle = self.names + index * 32
            assert len(name) < 128 and len(value) == 68
            self.values[handle] = value
            rows.append(name.ljust(128, b'\0') + struct.pack('<2I', handle, 0x3ee))
        self.u.mem_write(self.base, b''.join(rows))
        self.u.mem_write(0x645614, struct.pack('<2I', self.base, len(rows)))

    def hook(self, uc, address, size, context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        ret, handle, buffer = struct.unpack('<3I', uc.mem_read(sp, 12))
        assert handle in self.values, (hex(address), hex(handle))
        if address == 0x430553:
            uc.mem_write(buffer, self.values[handle])
        elif address == 0x43041a:
            self.values[handle] = bytes(uc.mem_read(buffer, 68))
        uc.reg_write(UC_X86_REG_EAX, handle)
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def alpha(self, name, hidden, alpha):
        self.u.mem_write(self.lookup, name + b'\0')
        self.call(0x4a7d10, struct.pack('<IIf', self.lookup, hidden, alpha))
        return [self.values[self.names + i * 32] for i in range(len(self.values))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--groups', default='0,1,2,3,4')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    groups = [int(x) for x in args.groups.split(',')]
    assert groups and len(set(groups)) == len(groups) and all(0 <= x < 5 for x in groups)
    raw = args.exe.read_bytes()
    native, lib = Registry(raw), library()
    bind(lib)
    error = C.create_string_buffer(256)
    lib.bk_ending_tertiary_material_name.argtypes = [C.c_int, C.c_uint, C.c_uint]
    lib.bk_ending_tertiary_material_name.restype = C.c_char_p
    lib.bk_ending_tertiary_initial_targets.restype = C.POINTER(C.c_int32)
    lib.bk_ending_tertiary_assets_material_alpha.argtypes = [C.c_void_p, C.c_char_p,
                                                            C.c_uint32, C.c_float, C.c_void_p]
    lib.bk_material_pose_material.argtypes = [C.c_void_p, C.c_uint32]
    lib.bk_material_pose_material.restype = C.POINTER(Material)
    lib.bk_menu_camera_dialogue.argtypes = [C.POINTER(CameraState)]
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    tables = [(0x54ae00, 2), (0x548f88, 6), (0x54b828, 4)]
    digest = hashlib.sha256()
    names, table_words, invalid, profiles, calls, matrices = 0, 0, 0, 0, 0, 0
    for table, (address, width) in enumerate(tables):
        for group in range(5):
            for slot in range(width):
                expected = pe.get_data(address-base+(group*width+slot)*260, 260).split(b'\0')[0]
                actual = lib.bk_ending_tertiary_material_name(table, group, slot)
                assert actual == expected, (table, group, slot, actual, expected)
                if table == 1:
                    #4d2320 initializes the same six names from a separate
                    #table before registering a fresh outer background.
                    loader_name = pe.get_data(0x5736a4-base+(group*6+slot)*260, 260).split(b'\0')[0]
                    assert loader_name == expected, (group, slot, 'loader material table')
                digest.update(actual + b'\0')
                names += 1
        for group, slot in [(5, 0), (0xffffffff, 0), (0, width), (4, 0xffffffff)]:
            assert lib.bk_ending_tertiary_material_name(table, group, slot) is None
            invalid += 1
    for table in [-1, 3, 2147483647]:
        assert lib.bk_ending_tertiary_material_name(table, 0, 0) is None
        invalid += 1
    assert bytes(C.cast(lib.bk_ending_tertiary_initial_targets(), C.POINTER(C.c_int32 * 5)).contents) == pe.get_data(0x54ccb4-base, 20)
    for group in range(5):
        for variant in range(2):
            config = Config()
            assert lib.bk_ending_tertiary_config(C.byref(config), group, variant)
            expected = pe.get_data(0x56f7f4-base+(group*2+variant)*320, 320)
            assert bytes(config.actions) == expected, (group, variant, 'action table')
            # Separate views used by the picker and parent controller.
            assert list(config.actions)[5:20] == list(struct.unpack_from('<15i', expected, 20))
            digest.update(expected)
            table_words += 80
    store = lib.bk_resources_create(error)
    assert store, error.value
    archives = ['bk3_11', 'fambom', 'bk3_04', 'bk3_03']
    for name in archives:
        assert lib.bk_resources_mount(store, name.encode(), str(args.data/(name+'.pp')).encode(), error), error.value
    duplicates = 0
    try:
        for group in groups:
            for variant in range(2):
                for reload in range(2):
                    assets = background = None
                    try:
                        camera, presets = CameraState(), Presets()
                        assert lib.bk_menu_camera_dialogue(C.byref(camera))
                        clocks, seed = (C.c_uint32 * 4)(100, 100, 100, 100), C.c_uint32(501 + group)
                        if reload:
                            background = lib.bk_ending_background_assets_create(store,
                                f'm{group+1:02}_{90+1-variant}.xan'.encode(), error)
                            assert background, error.value
                            assets = lib.bk_ending_tertiary_assets_create_reloaded(store, group,
                                variant, background, clocks, C.byref(seed), C.byref(camera), C.byref(presets), error)
                        else:
                            assets = lib.bk_ending_tertiary_assets_create(store, group,
                                variant, clocks, C.byref(seed), C.byref(camera), C.byref(presets), error)
                        assert assets and lib.bk_ending_tertiary_assets_load_background(assets, store, error), error.value
                        owners, entries = [], []
                        for role in ([5, 0, 1, 2] if reload else [0, 1, 2, 5]):
                            pose = lib.bk_ending_tertiary_assets_pose(assets, role)
                            if not pose:
                                continue
                            model = lib.bk_actor_pose_model(pose).contents
                            materials = lib.bk_ending_tertiary_assets_materials(assets, role)
                            assert materials
                            for index in range(model.material_count):
                                current = lib.bk_material_pose_material(materials, index).contents
                                owners.append((materials, index, bytes(current)))
                                entries.append((model.materials[index].name,
                                    C.string_at(C.addressof(current)+Material.diffuse.offset, 68)))
                        duplicates += len(entries) - len({name for name, _ in entries})
                        native.install(entries)
                        queries = [lib.bk_ending_tertiary_material_name(table, group, slot)
                                   for table, (_, width) in enumerate(tables) for slot in range(width)]
                        actual_names = sorted({name for name, _ in entries if name})
                        queries += actual_names[:10]
                        queries += [name.swapcase() for name in actual_names[:10]]
                        queries += [b'', b'absent-material-for-oracle']
                        for index, name in enumerate(queries):
                            hidden, alpha = [0, 0, 1, 255][index % 4], [0., .2, 1.][index % 3]
                            expected = native.alpha(name, hidden, alpha)
                            assert lib.bk_ending_tertiary_assets_material_alpha(assets, name,
                                hidden, alpha, error), (group, variant, reload, name, error.value)
                            for (materials, slot, original), values in zip(owners, expected):
                                actual = bytes(lib.bk_material_pose_material(materials, slot).contents)
                                wanted = original[:Material.diffuse.offset] + values + original[Material.diffuse.offset+68:]
                                assert actual == wanted, (group, variant, reload, name, slot)
                                digest.update(actual)
                                matrices += 1
                            calls += 1
                        before = [bytes(lib.bk_material_pose_material(p, i).contents) for p, i, _ in owners]
                        assert not lib.bk_ending_tertiary_assets_material_alpha(assets, None, 0, 1, error)
                        assert not lib.bk_ending_tertiary_assets_material_alpha(None, b'', 0, 1, error)
                        assert before == [bytes(lib.bk_material_pose_material(p, i).contents) for p, i, _ in owners]
                        invalid += 2
                        profiles += 1
                    finally:
                        lib.bk_ending_tertiary_assets_destroy(assets)
                        lib.bk_ending_background_assets_destroy(background)
    finally:
        lib.bk_resources_destroy(store)
    report = dict(passed=True, exe_sha256=hashlib.sha256(raw).hexdigest(),
                  groups=groups, material_names=names, action_words=table_words,
                  initial_targets=5, profiles=profiles, original_calls=calls,
                  material_snapshots=matrices, duplicate_names=duplicates,
                  rejections=invalid, state_sha256=digest.hexdigest(),
                  hooks=['42a0a5','42a0cd','430553','43041a'], scope=__doc__,
                  gpu_validation=False, complete_scene=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS tertiary materials', names, table_words, profiles, calls,
          matrices, duplicates, invalid, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
