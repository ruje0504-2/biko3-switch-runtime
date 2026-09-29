"""Original495469/4952C8 with unmodified distance, sqrt and absolute value.

Compare the complete active descriptor, retained direction/setting words and
sequences of radial/drag edits. No instruction hooks or replacement numerical
children. The fixed30 radial offset, float motion ABI, duration coefficients,
ignored Y input, integer wrapping and preserved elapsed are exercised.
This verifies CPU services, not the48E75B parent, scene binding or hardware.
"""
from __future__ import annotations
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn.x86_const import UC_X86_REG_FPSW, UC_X86_REG_FPTAG
from original_prop_route_oracle import Native
from model_binding import library

EXE_SHA256 = 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
I, F, P = C.c_int32, C.c_float, C.c_void_p
XY, Motion = I * 2, F * 2


class Clip(C.Structure):
    _fields_ = [('duration', I)] + [(name, F) for name in
                    ['start', 'end', 'source', 'rate', 'elapsed']]


def bind(lib):
    lib.bk_ending_selected_motion_pointer.argtypes = [C.POINTER(Clip)] + [C.POINTER(I)] * 3 + [P]
    lib.bk_ending_selected_motion_pointer.restype = I
    lib.bk_ending_selected_motion_drag.argtypes = [C.POINTER(Clip), I, I, C.POINTER(F), P]
    lib.bk_ending_selected_motion_drag.restype = I


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--samples', type=int, default=16000)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.samples < 1:
        parser.error('samples must be positive')
    raw = args.exe.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == EXE_SHA256
    native, lib = Native(raw), library()
    bind(lib)
    u, error = native.u, C.create_string_buffer(256)
    rng, digest = random.Random(0x4952c8), hashlib.sha256()
    descriptor = native.actor + 0x190 + 3 * 0x9c
    mapping = [('duration', 0x50), ('start', 0x54), ('end', 0x58),
               ('source', 0x60), ('rate', 0x5c), ('elapsed', 0x44)]
    counters = dict(pointer=0, drag=0, sequence=0, zero_anchors=0,
                    wrapped_coordinates=0, radial_offset=0, ignored_y=0,
                    overshoots=0, unchanged_descriptor_fields=0, rejected=0)
    durations, settings, reversals = set(), set(), set()

    def wi(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def put(c):
        u.mem_write(descriptor, b'\xa5' * 0x9c)
        for name, offset in mapping:
            u.mem_write(descriptor + offset, struct.pack(
                '<i' if name == 'duration' else '<f', getattr(c, name)))
        return bytes(u.mem_read(descriptor, 0x9c))

    def check(c, before, label):
        actual = bytes(u.mem_read(descriptor, 0x9c))
        wanted = Clip(*[struct.unpack('<i' if name == 'duration' else '<f',
                                      actual[offset:offset + 4])[0]
                         for name, offset in mapping])
        assert bytes(c) == bytes(wanted), (label, bytes(c).hex(), bytes(wanted).hex(),
                                          c.source, wanted.source)
        unchanged = bytearray(before)
        unchanged[0x60:0x64] = actual[0x60:0x64]
        assert actual == unchanged, ('native unrelated descriptor write', label)
        digest.update(bytes(c))
        counters['unchanged_descriptor_fields'] += 1

    def call(address, data):
        #4952C8 leaves a discarded float return on the native x87 stack.
        u.reg_write(UC_X86_REG_FPSW, 0)
        u.reg_write(UC_X86_REG_FPTAG, 0xffff)
        native.call(address, data)

    def pointer(c, target, menu, cursor, label):
        before = put(c)
        u.mem_write(0x6afd38, bytes(target))
        u.mem_write(0x7220c8, bytes(menu))
        inputs = (C.c_uint32 * 11)()
        inputs[9:11] = [value & 0xffffffff for value in cursor]
        call(0x495469, struct.pack('<I', native.actor) + bytes(inputs))
        assert lib.bk_ending_selected_motion_pointer(C.byref(c), target, menu, cursor, error), error.value
        check(c, before, label)
        counters['pointer'] += 1
        counters['radial_offset'] += c.source != c.start and c.start != 30

    def drag(c, setting, reverse, motion, label):
        before = put(c)
        wi(0xb53c38, setting)
        wi(0x6ea314, reverse)
        call(0x4952c8, struct.pack('<I', native.actor) + bytes(motion))
        assert lib.bk_ending_selected_motion_drag(C.byref(c), setting, reverse, motion, error), error.value
        check(c, before, label)
        assert bytes(u.mem_read(0xb53c38, 4)) == struct.pack('<i', setting)
        assert bytes(u.mem_read(0x6ea314, 4)) == struct.pack('<i', reverse)
        counters['drag'] += 1
        counters['overshoots'] += c.source > c.end
        durations.add(c.duration)
        settings.add(setting)
        reversals.add(reverse)

    wi(native.actor + 0x140, 3)
    constants = {hex(a): struct.unpack('<f', u.mem_read(a, 4))[0]
                 for a in [0x53f3f8, 0x53f3b4, 0x53f4fc]}
    assert list(constants.values()) == [.5, 60., 30.]
    for case in range(args.samples):
        duration = [-2147483648, -1, 0, 1, 10, 15, 20, 30, 40, 60, 80, 120,
                    400, 2147483647][case % 14]
        c = Clip(duration, rng.uniform(-500, 500), rng.uniform(-500, 500),
                 rng.uniform(-500, 500), rng.uniform(-20, 20), rng.uniform(-50, 100))
        target, menu, cursor = [XY(rng.randrange(-2000, 2001),
                                    rng.randrange(-2000, 2001)) for _ in range(3)]
        if case % 7 == 0:
            target[:] = menu[:]
            counters['zero_anchors'] += 1
        if case % 11 == 0:
            cursor[:] = menu[:]
        if case % 13 == 0:
            cursor[:] = target[:]
        if case % 17 == 0:
            for point in [target, menu, cursor]:
                point[:] = [rng.choice([-2147483648, -16777217, 16777217, 2147483647])
                            for _ in range(2)]
            counters['wrapped_coordinates'] += 1
        pointer(c, target, menu, cursor, ('pointer', case))
        setting = [0, 1, -1, 17][case % 4]
        reverse = [0, 1, -1, 17][case // 4 % 4]
        motion = Motion(rng.uniform(-17000, 17000), rng.uniform(-17000, 17000))
        if case % 9 == 0:
            motion[:] = [0, 0]
        if case % 12 == 0:
            motion[1] = float('nan')
            assert setting == 0
            counters['ignored_y'] += 1
        drag(c, setting, reverse, motion, ('drag', case))

    # Alternate the two operations on retained clocks. Pointer coordinates
    #stay within their anchor to exercise the fixed30 path with start!=30.
    clocks = [Clip(15, 5, 85, 31, 1.7, 13), Clip(80, 70, 120, 80, .25, 29)]
    for case in range(1600):
        c = clocks[case % 2]
        if case % 3 == 0:
            pointer(c, XY(160, 180), XY(10, 10), XY(20, 20), ('sequence-pointer', case))
        else:
            drag(c, case % 2, case // 2 % 2, Motion((case % 101) - 50.25, .375),
                 ('sequence-drag', case))
        counters['sequence'] += 1

    def reject(function, c, *parameters):
        before = bytes(c)
        assert not function(C.byref(c), *parameters, error)
        assert bytes(c) == before and error.value
        counters['rejected'] += 1

    for value in [float('nan'), float('inf'), -float('inf')]:
        for field in ['start', 'end', 'source', 'rate']:
            c = Clip(40, 0, 100, 50, 2, 17)
            setattr(c, field, value)
            reject(lib.bk_ending_selected_motion_drag, c, 0, 0, Motion(1, 2))
        reject(lib.bk_ending_selected_motion_drag, Clip(40, 0, 100, 50, 2, 17),
               0, 0, Motion(value, 2))
        reject(lib.bk_ending_selected_motion_drag, Clip(40, 0, 100, 50, 2, 17),
               1, 0, Motion(1, value))
        for field in ['start', 'end']:
            c = Clip(40, 0, 100, 50, 2, 17)
            setattr(c, field, value)
            reject(lib.bk_ending_selected_motion_pointer, c, XY(1, 1), XY(0, 0), XY(0, 0))
    reject(lib.bk_ending_selected_motion_drag, Clip(40, -3e38, 3e38, 50, 3e38, 17),
           1, 0, Motion(3e38, 3e38))
    reject(lib.bk_ending_selected_motion_pointer, Clip(40, -3e38, 3e38, 50, 2, 17),
           XY(1, 1), XY(0, 0), XY(0, 0))
    for missing in range(3):
        values = [XY(1, 1), XY(0, 0), XY(0, 0)]
        values[missing] = None
        reject(lib.bk_ending_selected_motion_pointer, Clip(40, 0, 100, 50, 2, 17), *values)
    reject(lib.bk_ending_selected_motion_drag, Clip(40, 0, 100, 50, 2, 17), 0, 0, None)
    assert not lib.bk_ending_selected_motion_pointer(None, XY(), XY(), XY(), error)
    assert not lib.bk_ending_selected_motion_drag(None, 0, 0, Motion(), error)
    counters['rejected'] += 2
    report = dict(passed=True, exe_sha256=EXE_SHA256, counters=counters,
                  durations=sorted(durations), settings=sorted(settings), reversals=sorted(reversals),
                  constants=constants, max_error=0, state_sha256=digest.hexdigest(),
                  hooks=[], scope=__doc__, parent_or_scene_validation=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS selected motion', json.dumps(counters, sort_keys=True), digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
