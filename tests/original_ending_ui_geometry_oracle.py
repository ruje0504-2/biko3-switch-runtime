"""Original47da89/479f15 line transforms/UV and4a777e strict circle tests.
Original4a7908,472420,acos/sqrt CRT and sprite setters execute unchanged.
Only texture/device IO is substituted. Independent line calls include axes,
coincident anchors, integer-to-float rounding and distinct retained scrolls.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_ending_stage_ui_oracle import Native, Stage, bind, install, same
import original_ending_ui_oracle as m
from original_detached_sprite_oracle import Frame
from model_binding import ROOT, library


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    exe = ap.parse_args().exe.read_bytes()
    n = Native(exe)
    lib = library()
    bind(lib)
    fp, ip = C.POINTER(C.c_float), C.POINTER(C.c_int32)
    lib.bk_ending_ui_line.argtypes = [C.POINTER(Stage), ip, ip, C.c_float,
                                     C.c_float, fp, C.c_void_p]
    lib.bk_ending_ui_circle_hit.argtypes = [fp, C.c_float, fp,
                                           C.POINTER(C.c_int), fp]
    lib.bk_ending_ui_dispatch_sprite.argtypes = [C.POINTER(m.Ui), C.POINTER(Stage),
                                                C.c_uint, C.c_float,
                                                C.POINTER(Frame), C.c_void_p]
    e = C.create_string_buffer(256)
    rng = random.Random(0x479f15)
    f32 = lambda x: C.c_float(x).value
    lines = vertices = circles = hits = wraps = 0
    for profile, width in enumerate([1, 320, 640, 961, 1280, 1920]):
        base, stage = m.Ui(), Stage()
        install(n, base, stage)
        n.load_stage(1, profile % 5, 0, width)
        assert lib.bk_ending_stage_ui_initialize(C.byref(base), C.byref(stage),
                                                1, profile % 5, 0, width, e)
        scrolls = [C.c_float(.001), C.c_float(.998)]
        for t in range(1000):
            mode = t % 2
            addr, scroll_addr = [(0x47da89, 0x6bbe44), (0x479f15, 0x6afd28)][mode]
            anchor = [rng.randrange(-4096, 4096) for _ in range(2)]
            pointer = [rng.randrange(-4096, 4096) for _ in range(2)]
            if t % 5 == 0:
                pointer[0] = anchor[0]
            if t % 7 == 0:
                pointer[1] = anchor[1]
            if t % 13 == 0:
                anchor = [0x7fffffff, -0x80000000]
                pointer = [-0x80000000, 0x7fffffff]
            dt = f32([0, 1/60, .1, 1, 10, 100][t % 6])
            length = f32(400 * f32(width/1280)) if mode == 0 else f32(rng.uniform(0, 600))
            if t % 17 == 0 and mode == 1:
                length = 0
            index = t % 3
            n.u.mem_write(0x7220c8 + index*8, struct.pack('<2i', *anchor))
            n.wf(scroll_addr, scrolls[mode].value)
            n.wf(0x733700, dt)
            args = struct.pack('<3i', index, *pointer)
            if mode:
                args += struct.pack('<f', length)
            n.call(addr, args)
            assert lib.bk_ending_ui_line(C.byref(stage), (C.c_int32*2)(*anchor),
                                         (C.c_int32*2)(*pointer), length, dt,
                                         C.byref(scrolls[mode]), e), e.value
            same(n, base, stage, ('line', profile, t))
            assert scrolls[mode].value == n.rf(scroll_addr)
            wraps += scrolls[mode].value == f32(.001)
            # Capture actual geometry after the original update/draw boundary.
            n.draws = []
            n.call(0x50e6ba, struct.pack('<2I', m.BASE + 63*0x16c, 0))
            frame = Frame()
            assert lib.bk_ending_ui_dispatch_sprite(C.byref(base), C.byref(stage),
                                                    63, dt, C.byref(frame), e)
            assert frame.count == 1 and len(n.draws) == 1
            d = frame.draws[0]
            for j, k in enumerate([0, 1, 2, 3, 0, 2]):
                assert tuple(d.xy[2*k:2*k+2]) == struct.unpack_from('<2f', n.draws[0][2], 32*j)
                vertices += 1
            lines += 1
    print('PASS original line geometry', lines, vertices, flush=True)
    for case in range(16000):
        center = (C.c_float*2)(rng.uniform(-2000, 2000), rng.uniform(-2000, 2000))
        point = (C.c_float*2)(rng.uniform(-2000, 2000), rng.uniform(-2000, 2000))
        radius = f32(rng.uniform(0, 6000))
        if case % 8 < 4:
            center[:] = [0, 0]
            point[:] = [3, 4]
            radius = [5, struct.unpack('<f', struct.pack('<I', 0x40a00001))[0],
                      struct.unpack('<f', struct.pack('<I', 0x409fffff))[0], 0][case % 8]
        if case % 11 == 0:
            point[:] = center[:]
        retained = f32(rng.uniform(-100, 100))
        n.wf(n.actor, retained)
        n.call(0x4a777e, struct.pack('<5fI', *center, radius, *point, n.actor))
        expected_hit = n.u.reg_read(UC_X86_REG_EAX)
        distance, hit = C.c_float(retained), C.c_int(-1)
        assert lib.bk_ending_ui_circle_hit(center, radius, point, C.byref(hit), C.byref(distance))
        assert hit.value == expected_hit and distance.value == n.rf(n.actor), (case, hit.value, expected_hit, distance.value, n.rf(n.actor))
        circles += 1
        hits += hit.value
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  line_calls=lines, vertices=vertices, circle_calls=circles,
                  circle_hits=hits, scroll_at_reset=wraps, max_error=0, scope=__doc__)
    (ROOT/'local/original-ending-ui-geometry-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print(report, flush=True)


if __name__ == '__main__':
    main()
