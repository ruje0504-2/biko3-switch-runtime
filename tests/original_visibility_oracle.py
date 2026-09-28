"""Compare original sight cone and scene edge occlusion without math hooks."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

Vector = C.c_float*3
Triangle = Vector*3
class Segment(C.Structure):
    _fields_ = [('start', Vector), ('end', Vector), ('distance', C.c_float)]
class Sight(C.Structure):
    _fields_ = [('actor_position', Vector), ('player_position', Vector), ('head_distance', C.c_float), ('facing', C.c_float), ('actor_kind', C.c_int32), ('player_action', C.c_int32), ('short_range_action', C.c_int32)]
class Native:
    stack, stop = 0x2008000, 0x300f000
    def __init__(self, exe): self.u = machine(exe)
    def call(self, address, data, limit=100000):
        self.u.mem_write(self.stack, struct.pack('<I', self.stop)+data)
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(address, self.stop, count=limit)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.stop
        return self.u.reg_read(UC_X86_REG_EAX)&255
    def crossing(self, a, b): return self.call(0x4ae8ff, bytes(a)+bytes(b))
    def triangle(self, triangle, segment):
        return self.call(0x518cd3, b''.join(bytes(v)+bytes(4) for v in triangle)+bytes(segment)+struct.pack('<ff', 23, 67))
    def cone(self, inp):
        self.u.mem_write(0x729084, bytes(inp.actor_position))
        self.u.mem_write(0x71b7ac, bytes(inp.player_position))
        self.u.mem_write(0x728df4, struct.pack('<i', inp.actor_kind))
        self.u.mem_write(0x71b520, struct.pack('<i', inp.player_action))
        self.u.mem_write(0x71b544, struct.pack('<i', inp.short_range_action))
        ray = Segment(inp.actor_position, inp.player_position, inp.head_distance)
        return self.call(0x518bb0, bytes(ray)+struct.pack('<ff', 123, inp.facing))

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x518bb0)
    lib.bk_sight_crossing.argtypes = [C.POINTER(C.c_int), C.POINTER(Segment), C.POINTER(Segment)]
    lib.bk_sight_triangle.argtypes = [C.POINTER(C.c_int), C.POINTER(Vector), C.POINTER(Segment)]
    lib.bk_sight_cone.argtypes = [C.POINTER(C.c_int), C.POINTER(Sight)]
    counts, hits = [0]*3, [0]*3
    def vector(): return Vector(*(rng.uniform(-200, 200) for _ in range(3)))
    for case in range(14000):
        a, b = Segment(vector(), vector(), 0), Segment(vector(), vector(), 0)
        if case%7 == 0: b.start = a.start
        if case%7 == 1: b.end = a.end
        if case%7 == 2: a.end = a.start
        if case%7 == 3: b = Segment(a.end, a.start, 0)
        if case%7 == 4:
            a.start, a.end = Vector(0, 0, 0), Vector(0, 0, 10)
            b.start, b.end = Vector(0, 2, 5), Vector(0, 3, 15)
        expected, result = native.crossing(a, b), C.c_int(-1)
        assert lib.bk_sight_crossing(C.byref(result), C.byref(a), C.byref(b))
        assert result.value == expected, ('cross', case, result.value, expected)
        counts[0] += 1; hits[0] += result.value
        triangle = Triangle(vector(), vector(), vector())
        if case%4 == 0:
            # Equality and one-ULP height separation at the sight endpoints.
            for p in triangle: p[1] = 20
            a.start[1] = rng.choice([19, 20, 20.000002]); a.end[1] = a.start[1]
        expected = native.triangle(triangle, a)
        assert lib.bk_sight_triangle(C.byref(result), triangle, C.byref(a))
        assert result.value == expected, ('triangle', case, result.value, expected)
        counts[1] += 1; hits[1] += result.value
        actor, player = vector(), vector()
        if case%2 == 0:
            actor = Vector(0, 0, 0)
            player = rng.choice([Vector(0, 0, 1), Vector(1, 20, 0), Vector(-1, -20, 0), Vector(0, 20.000002, -1), Vector(0, 0, 0)])
        inp = Sight(actor, player, rng.choice([0, 40, 40.000004, 100, 100.00001]), rng.choice([-360, -90, 0, 30, 60, 90, 180, 270, 300, 360, rng.uniform(-400, 400)]), rng.choice([-1, 0, 1, 2, 3, 4, 5]), rng.choice([0, 1]), 1)
        expected = native.cone(inp)
        assert lib.bk_sight_cone(C.byref(result), C.byref(inp))
        assert result.value == expected, ('cone', case, result.value, expected)
        counts[2] += 1; hits[2] += result.value
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=dict(zip(['segment', 'triangle', 'cone'], counts)), hits=hits, native_functions=['0x4ae8ff', '0x518cd3', '0x518bb0', '0x4aeba9'], hooks=[], x87_control_word='0x037f', scope='Inclusive XZ crossing except parallel/collinear, triangle edge height-range occlusion, original cone thresholds and unwrapped heading difference; caller-supplied cached head distance and facing. Scene loading/actor stage/ground adjustment excluded.')
    (ROOT/'local/original-visibility-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', counts, 'visibility cases; hits', hits)
if __name__ == '__main__': main()
