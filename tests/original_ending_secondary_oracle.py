"""Fixed-EXE4D00FA configuration and unhooked cached-target arithmetic.

This does not claim the resource loader, phase2 controllers or GPU rendering.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path

from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine


class Config(C.Structure):
    _fields_ = [('primary', C.c_char * 32), ('face', C.c_char * 32),
                ('visible_nodes', (C.c_char * 32) * 3),
                ('position', C.c_float * 3), ('yaw', C.c_int32),
                ('camera_yaw', C.c_int32), ('expression_a', C.c_int32),
                ('expression_b', C.c_int32),
                ('camera_table', (C.c_uint32 * 4) * 5)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'local/original-ending-secondary.json')
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    native, lib = machine(raw), library()
    config = lib.bk_ending_secondary_config
    config.argtypes = [C.POINTER(Config), C.c_uint]
    targets = lib.bk_ending_secondary_targets
    targets.argtypes = [C.POINTER(C.c_float)] * 3 + [C.c_void_p]
    error = C.create_string_buffer(256)
    profiles = []
    for group in range(5):
        out = Config()
        assert config(C.byref(out), group)
        assert out.primary == f'h{group + 1:02}_02.xan'.encode()
        face_format = bytes(native.mem_read(0x575aec, 32)).split(b'\0')[0]
        assert out.face == face_format % (group + 1)
        assert bytes(out.position) == bytes(native.mem_read(0x570d6c + group * 120, 12))
        assert out.yaw == struct.unpack('<i', native.mem_read(0x570c9c + group * 40, 4))[0]
        assert out.camera_yaw == struct.unpack('<i', native.mem_read(0x570c24 + group * 24, 4))[0]
        assert bytes(out.camera_table) == bytes(native.mem_read(0x571008, 80))
        for slot in range(3):
            name = bytes(native.mem_read(0x564b10 + (group * 30 + 3 + slot) * 260, 260))
            assert out.visible_nodes[slot].value == name.split(b'\0')[0]
        profiles.append(dict(group=group, primary=out.primary.decode(),
                             face=out.face.decode(), position=list(out.position),
                             camera_yaw=out.camera_yaw))
    sentinel = Config.from_buffer_copy(b'\x35' * C.sizeof(Config))
    assert not config(C.byref(sentinel), 5)
    assert bytes(sentinel) == b'\x35' * C.sizeof(Config)
    assert not config(None, 0)

    node, anchor = 0x3000000, 0x3000400
    native.mem_write(0x721ef4, struct.pack('<I', node))
    native.mem_write(0x719b40, struct.pack('<I', anchor))
    rng = random.Random(0x4d00fa)
    vectors = [[5, 17, 29, 11, 3, -7], [0.0, -0.0, 0.0, -0.0, 0.0, -0.0],
               [3.4028234663852886e38] * 3 + [-3.4028234663852886e38] * 3,
               [1.401298464324817e-45] * 3 + [-1.401298464324817e-45] * 3]
    while len(vectors) < 4096:
        values = struct.unpack('<6f', struct.pack('<6I', *[rng.getrandbits(32) for _ in range(6)]))
        if all(math.isfinite(value) for value in values):
            vectors.append(values)
    for case, values in enumerate(vectors):
        inputs = (C.c_float * 6)(*values)
        native.mem_write(node + 0xf0, bytes(inputs)[:12])
        native.mem_write(anchor + 0xf0, bytes(inputs)[12:])
        native.reg_write(UC_X86_REG_FPCW, 0x037f)
        native.emu_start(0x4d0add, 0x4d0b9d, count=1000)
        assert native.reg_read(UC_X86_REG_EIP) == 0x4d0b9d
        expected = bytes(native.mem_read(0x70c8d8, 36))
        second = C.cast(C.byref(inputs, 12), C.POINTER(C.c_float))
        out = (C.c_float * 9)(*[99] * 9)
        assert targets(out, inputs, second, error), error.value
        assert bytes(out) == expected, (case, list(out), struct.unpack('<9f', expected))
        # Both source ranges may lie inside the destination.
        alias = (C.c_float * 9)(*values, 91, 92, 93)
        assert targets(alias, alias, C.cast(C.byref(alias, 12), C.POINTER(C.c_float)), error)
        assert bytes(alias) == expected
    rejected = 0
    for index in range(6):
        for value in [float('nan'), float('inf'), -float('inf')]:
            inputs = (C.c_float * 6)(1, 2, 3, 4, 5, 6)
            inputs[index] = value
            out = (C.c_float * 9)(*[99] * 9)
            held = bytes(out)
            assert not targets(out, inputs, C.cast(C.byref(inputs, 12), C.POINTER(C.c_float)), error)
            assert bytes(out) == held
            rejected += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(raw).hexdigest(),
                  profiles=profiles, native_target_samples=len(vectors),
                  alias_samples=len(vectors), atomic_nonfinite_rejections=rejected,
                  native_segment='0x4d0add..0x4d0b9d', max_error=0,
                  scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS secondary configuration: 5 profiles;', len(vectors),
          'unhooked targets and aliases;', rejected, 'atomic rejections')


if __name__ == '__main__':
    main()
