"""Original player-wall projection/side/intersection math, without function hooks."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library


Vec = C.c_float * 3
class Segment(C.Structure):
    _fields_ = [('start', Vec), ('end', Vec), ('distance', C.c_float)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe')
    args = p.parse_args()
    exe = open(args.exe, 'rb').read()
    u, lib = machine(exe), library()
    stack, stop, dest = 0x2008000, 0x300f000, 0x3000200
    for name, types in [
        ('bk_proximity_interior_xz', [C.POINTER(C.c_int), Vec, Vec, Vec, C.c_float]),
        ('bk_proximity_project_xz', [C.POINTER(C.c_int), Vec, Vec, Vec, Vec, C.c_float]),
        ('bk_sight_line_side', [C.POINTER(C.c_int), Vec, Vec, Vec]),
        ('bk_sight_crossing_point', [C.POINTER(C.c_int), Vec, C.POINTER(Segment), C.POINTER(Segment)])]:
        getattr(lib, name).argtypes = types
        getattr(lib, name).restype = C.c_int
    def native(address, payload):
        u.mem_write(stack, struct.pack('<I', stop) + payload)
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == stop
        return u.reg_read(UC_X86_REG_EAX) & 255
    rng = random.Random(0x4b6bb4)
    worst = 0.0
    nonfinite = intersections = projection_misses = 0
    for case in range(12000):
        def vector():
            return Vec(*(rng.uniform(-500, 500) for _ in range(3)))
        a, b = Segment(vector(), vector(), 0), Segment(vector(), vector(), 0)
        point = vector()
        radius = C.c_float(rng.choice([0, 1, 4, 15, rng.uniform(0, 1000)])).value
        if case % 12 == 0:
            a.end[:] = a.start[:]
        elif case % 12 == 1:
            a.end[0] = a.start[0]
        elif case % 12 == 2:
            a.end[2] = a.start[2]
        elif case % 12 == 3:
            a = Segment(Vec(-10, 0, 0), Vec(10, 0, 0), 0)
            b = Segment(Vec(0, 0, -10), Vec(0, 0, 10), 0)
        elif case % 12 == 4:
            point[:] = a.start[:]
        elif case % 12 == 5:
            point[:] = a.end[:]
        out = Vec(77, 88, 99)
        payload = bytes(a) + bytes(point) + bytes(4) + struct.pack('<f', radius)
        u.mem_write(dest, bytes(out))
        expected = native(0x4b6bb4, payload + struct.pack('<I', dest))
        projection = struct.unpack('<3f', u.mem_read(dest, 12))
        hit = C.c_int(123)
        assert lib.bk_proximity_project_xz(C.byref(hit), out, a.start, a.end, point, radius)
        assert hit.value == expected, (case, 'project', hit.value, expected)
        projection_misses += not expected
        for actual, wanted in zip(out, projection):
            delta = abs(actual - wanted) / max(1, abs(wanted))
            worst = max(worst, delta)
            assert delta < 2e-6, (case, 'projection coordinate', actual, wanted)
        expected = native(0x4ae68d, payload)
        assert lib.bk_proximity_interior_xz(C.byref(hit), a.start, a.end, point, radius)
        assert hit.value == expected
        expected = native(0x4ae1d7, bytes(a) + bytes(point) + bytes(4))
        assert lib.bk_sight_line_side(C.byref(hit), a.start, a.end, point)
        assert hit.value == expected, (case, 'side', hit.value, expected)
        out = Vec(77, 88, 99)
        u.mem_write(dest, bytes(out))
        expected = native(0x4ae8ff, bytes(a) + bytes(b))
        native(0x4aed7a, bytes(a) + bytes(b) + struct.pack('<I', dest))
        intersection = struct.unpack('<3f', u.mem_read(dest, 12))
        hit.value = 123
        ok = lib.bk_sight_crossing_point(C.byref(hit), out, C.byref(a), C.byref(b))
        if not all(math.isfinite(v) for v in intersection):
            assert not ok and tuple(out) == (77, 88, 99) and hit.value == 123
            nonfinite += 1
        else:
            assert ok and hit.value == expected, (case, 'cross point', tuple(out), intersection)
            intersections += expected
            for actual, wanted in zip(out, intersection):
                delta = abs(actual - wanted) / max(1, abs(wanted))
                worst = max(worst, delta)
                assert delta < 2e-6, (case, 'intersection coordinate', actual, wanted)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  native_functions=['4ae68d', '4b6bb4', '4ae1d7', '4aed7a', '4ae8ff'],
                  cases=12000, native_calls=60000, projection_misses=projection_misses,
                  finite_intersections=intersections, native_nonfinite_rejected=nonfinite,
                  max_relative_error=worst, hooks=[],
                  scope='Player wall dependency math only. Native nonfinite slope intersections explicitly rejected; full wall response still pending.')
    (ROOT/'local/original-edge-math-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
