"""Pinned original4D1025 dispatch/table arguments and background branch.

The original argument-selection/placement/camera-yaw/phase branches execute
unchanged. The background comparison stops before its real loading callback;
this test alone does not establish assets, effects, media or playable phase5/6.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP
from model_binding import ROOT, library
from original_matrix_oracle import machine


class Config(C.Structure):
    _fields_ = [('pack', C.c_char * 8), ('primary', C.c_char * 32),
                ('face', C.c_char * 32), ('visible_nodes', (C.c_char * 32) * 3),
                ('position', C.c_float * 3), ('yaw', C.c_int32),
                ('camera_yaw', C.c_int32), ('expression_a', C.c_int32),
                ('expression_b', C.c_int32), ('expression_mode', C.c_int32),
                ('event', C.c_uint32), ('phase', C.c_uint32),
                ('camera_table', (C.c_uint32 * 4) * 5)]


def bind(lib):
    lib.bk_ending_selected_config.argtypes = [C.POINTER(Config), C.c_uint, C.c_uint, C.c_uint]
    lib.bk_ending_selected_config.restype = C.c_int
    lib.bk_ending_selected_replaces_background.argtypes = [C.c_uint, C.c_uint, C.c_int32]
    lib.bk_ending_selected_replaces_background.restype = C.c_int


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-selected-config.json')
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native, lib = machine(raw), library()
    bind(lib)
    base = 0x200c000
    digest, profiles = hashlib.sha256(), []

    def write(address, value):
        native.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def read(address):
        return struct.unpack('<I', native.mem_read(address, 4))[0]

    def fragment(first, last):
        native.reg_write(UC_X86_REG_EBP, base)
        native.reg_write(UC_X86_REG_ESP, base - 0x1000)
        native.emu_start(first, last, count=100000)
        assert native.reg_read(UC_X86_REG_EIP) == last, hex(native.reg_read(UC_X86_REG_EIP))

    for group in range(5):
        for variant in range(2):
            for selection in range(3):
                config = Config()
                assert lib.bk_ending_selected_config(C.byref(config), group, variant, selection)
                native.mem_write(0x721b3c, bytes([group, 0x7f]))
                write(0x721e04, variant)
                write(0x7220f4, 0xcccccccc)
                write(base + 8, selection)
                fragment(0x4d152c, 0x4d1693)
                stack = native.reg_read(UC_X86_REG_ESP)
                table, event, suffix, yaw = struct.unpack('<IIIi', native.mem_read(stack, 16))
                assert config.event == event == native.mem_read(0x721b3d, 1)[0]
                assert read(0x7220f4) == selection
                assert table == 0x571058 + group * 16
                assert config.yaw == yaw
                assert bytes(config.position) == bytes(native.mem_read(stack + 16, 12))
                assert bytes(config.camera_table) == bytes(native.mem_read(table, 80))
                pack = bytes(native.mem_read(0x575c78 if variant else 0x575c6c, 32)).split(b'\0')[0]
                assert config.pack == pack.lstrip(b'\\')[:-3]
                face_format = bytes(native.mem_read(0x575c8c, 32)).split(b'\0')[0]
                assert config.face == face_format % (group + 1, suffix)
                assert config.primary == f'h{group + 1:02}_{suffix:02}.xan'.encode()
                for slot in range(3):
                    address = 0x564b10 + (group * 30 + event * 3 + slot) * 260
                    expected = bytes(native.mem_read(address, 260)).split(b'\0')[0]
                    assert config.visible_nodes[slot].value == expected
                fragment(0x4d17ea, 0x4d1820)
                assert config.camera_yaw == struct.unpack('<i', native.mem_read(0x71b354, 4))[0]
                fragment(0x4d1c07, 0x4d1c26)
                assert config.phase == read(0x721e00)
                # Actual expression/eye behavior is verified by asset oracles;
                # these three push-immediates establish this loader's inputs.
                assert bytes(native.mem_read(0x4d2099, 6)) == b'\x6a\x01\x6a\x03\x6a\x06'
                assert (config.expression_a, config.expression_b, config.expression_mode) == (6, 3, 1)
                digest.update(bytes(config))
                profiles.append({'group':group, 'variant':variant, 'selection':selection,
                                 'event':event, 'primary':config.primary.decode(), 'pack':config.pack.decode()})

    def stop_before_load(u, _address, _size, _context):
        u.emu_stop()

    native.hook_add(UC_HOOK_CODE, stop_before_load, begin=0x4d1c94, end=0x4d1c94)
    rng = random.Random(0x4d1025)
    selections = [-0x80000000, -1, 0, 1, 2, 3, 4, 5, 0x7fffffff]
    selections += [rng.randrange(-0x80000000, 0x80000000) for _ in range(128)]
    branches = 0
    for group in range(5):
        native.mem_write(0x721b3c, bytes([group]))
        for variant in range(2):
            write(0x721e00, 5 + variant)
            for selected in selections:
                write(0x721ed8, selected)
                native.reg_write(UC_X86_REG_EBP, base)
                native.reg_write(UC_X86_REG_ESP, base - 0x1000)
                native.emu_start(0x4d1c64, 0x4d1cc1, count=1000)
                stop = native.reg_read(UC_X86_REG_EIP)
                assert stop in [0x4d1c94, 0x4d1cc1]
                got = lib.bk_ending_selected_replaces_background(group, variant, selected)
                assert got == int(stop == 0x4d1c94), (group, variant, selected)
                digest.update(struct.pack('<IIiI', group, variant, selected, got))
                branches += 1
    rejected = 0
    for group, variant, selection in [(5, 0, 0), (0xffffffff, 0, 0), (0, 2, 0),
                                       (0, 0xffffffff, 0), (0, 0, 3), (0, 0, 0xffffffff)]:
        sentinel = Config.from_buffer_copy(b'\x35' * C.sizeof(Config))
        assert not lib.bk_ending_selected_config(C.byref(sentinel), group, variant, selection)
        assert bytes(sentinel) == b'\x35' * C.sizeof(Config)
        rejected += 1
    assert not lib.bk_ending_selected_config(None, 0, 0, 0)
    assert lib.bk_ending_selected_replaces_background(5, 0, 0) == -1
    assert lib.bk_ending_selected_replaces_background(0, 2, 0) == -1
    rejected += 3
    result = {'passed':True, 'exe_sha256':hashlib.sha256(raw).hexdigest(),
              'profiles':profiles, 'native_background_branches':branches,
              'invalid_rejections':rejected, 'max_error':0, 'sha256':digest.hexdigest(),
              'scope':__doc__}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS selected configuration profiles{len(profiles)} branches{branches} rejects{rejected} error0 sha256={digest.hexdigest()}')


if __name__ == '__main__':
    main()
