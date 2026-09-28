"""Replay native projected triangle height with original side/interpolation."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from original_visibility_oracle import Native as SightNative, Vector, Triangle
from model_binding import ROOT, library

class Native(SightNative):
    output = 0x3001000
    def ground(self, point, triangle, height):
        self.u.mem_write(self.output, struct.pack('<f', height))
        args = bytes(point)+bytes(4)+b''.join(bytes(v)+bytes(4) for v in triangle)+struct.pack('<I', self.output)
        hit = self.call(0x4b43be, args)
        return hit, struct.unpack('<f', self.u.mem_read(self.output, 4))[0]

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x4b43be)
    lib.bk_ground_triangle.argtypes = [C.POINTER(C.c_int), C.POINTER(C.c_float), C.POINTER(C.c_float), C.POINTER(Vector)]
    checks = hits = rejected = 0; worst = 0; error_example = None
    for case in range(18000):
        triangle = Triangle(*(Vector(*(rng.uniform(-200, 200) for _ in range(3))) for _ in range(3)))
        if case%6 == 0:
            for p in triangle: p[1] = 5
        if case%6 == 1: triangle[1][0] = triangle[0][0]
        if case%6 == 2: triangle[1] = triangle[0]
        if case%6 == 3:
            triangle = Triangle(Vector(-10, 0, -10), Vector(-10, 10, 10), Vector(10, 20, -10))
        a, b = rng.random(), rng.random()
        if case%2 == 0: a, b = a*.5, b*.5
        point = Vector(*(triangle[0][i]*a+triangle[1][i]*b+triangle[2][i]*(1-a-b) for i in range(3)))
        if case%9 < 3: point = Vector(*triangle[case%9])
        height, hit = C.c_float(123.5), C.c_int(123)
        wanted, wh = native.ground(point, triangle, height.value)
        valid = lib.bk_ground_triangle(C.byref(hit), C.byref(height), point, triangle)
        if not valid:
            assert height.value == 123.5 and hit.value == 123
            # Zero-span edge slices can yield NaN in the native implementation.
            assert wanted == 1 and not math.isfinite(wh), (case, wanted, wh, list(map(list, triangle)), list(point))
            rejected += 1
            continue
        assert hit.value == wanted, (case, hit.value, wanted, list(map(list, triangle)), list(point))
        delta = abs(height.value-wh)/max(1, abs(wh))
        if delta > worst:
            worst = delta; error_example = dict(case=case, actual=height.value, native=wh)
        assert math.isfinite(wh) and delta < 3e-6, (case, height.value, wh, delta, list(map(list, triangle)), list(point))
        checks += 1; hits += hit.value
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=checks, hits=hits, nonfinite_native_rejections=rejected, max_normalized_error=worst, largest_error=error_example, native_functions=['0x4b43be', '0x4ae1d7', '0x4ae4d7'], hooks=[], x87_control_word='0x037f', scope='Triangle side rules and original piecewise projected height, including axes, flat/sloping/degenerate triangles and vertex boundaries. Rejects undefined/nonfinite zero-span native height; mesh selection and actor integration excluded.')
    (ROOT/'local/original-ground-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', checks, 'triangle queries,', hits, 'hits,', rejected, 'nonfinite native rejections; max error', worst)
if __name__ == '__main__': main()
