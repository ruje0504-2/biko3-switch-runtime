"""Compare actual cached-head/heading substage 0x4fca12..0x4fcc57."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_visibility_oracle import Segment, Vector
from model_binding import ROOT, library
Matrix = C.c_float*16
class Input(C.Structure):
    _fields_ = [('actor_head_world', Matrix), ('actor_head_local', Matrix), ('torso_local', Matrix), ('player_head', Vector), ('player_position', Vector), ('actor_yaw', C.c_float), ('actor_kind', C.c_int32)]
class State(C.Structure):
    _fields_ = [('sight', Segment), ('bearing', C.c_float), ('facing', C.c_float)]
class Native:
    stack, actor, head, parent, player_head = 0x2008000, 0x3000000, 0x3001000, 0x3002000, 0x3003000
    def __init__(self, exe): self.u = machine(exe)
    def run(self, inp):
        u = self.u
        u.mem_write(self.actor+8, struct.pack('<Ii', self.head, inp.actor_kind))
        u.mem_write(self.actor+0x2ac, struct.pack('<f', inp.actor_yaw))
        u.mem_write(self.head+0x80, bytes(inp.actor_head_local)); u.mem_write(self.head+0xc0, bytes(inp.actor_head_world))
        u.mem_write(self.parent+0x80, bytes(inp.torso_local))
        u.mem_write(self.player_head+0xf0, bytes(inp.player_head))
        u.mem_write(0x71b518, struct.pack('<I', self.player_head)); u.mem_write(0xbf3c70, struct.pack('<I', self.parent))
        u.mem_write(0x71b7ac, bytes(inp.player_position))
        u.mem_write(self.stack+8, struct.pack('<I', self.actor))
        u.reg_write(UC_X86_REG_EBP, self.stack); u.reg_write(UC_X86_REG_ESP, self.stack-0x100)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x4fca12, 0x4fcc57, count=200000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4fcc57
        return State(Segment.from_buffer_copy(u.mem_read(self.actor+0x814, 28)), struct.unpack('<f', u.mem_read(self.actor+0x808, 4))[0], struct.unpack('<f', u.mem_read(self.actor+0x810, 4))[0])

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native, lib, rng = Native(exe), library(), random.Random(0x4fca12)
    lib.bk_npc_head_update.argtypes = [C.POINTER(State), C.POINTER(Input), C.c_void_p]
    lib.bk_npc_head_update.restype = C.c_int
    error = C.create_string_buffer(256); count = rejected = 0; worst = 0
    def matrix():
        m = Matrix(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
        # Include scaled/sheared values accepted by original Euler function;
        # roll asin input stays inside domain, pitch has original clamping.
        m[1] = rng.choice([-1, 0, 1, rng.uniform(-1, 1)])
        m[5] = rng.choice([-1, 0, 1, rng.uniform(-2, 2)])
        m[8] = rng.choice([0, -0.0, 1, -1, rng.uniform(-2, 2)])
        m[9] = rng.uniform(-2, 2)
        m[10] = rng.choice([0, 1, -1, rng.uniform(-2, 2)])
        m[12:15] = [rng.uniform(-500, 500) for _ in range(3)]
        return m
    for case in range(12000):
        inp = Input(matrix(), matrix(), matrix(), Vector(*(rng.uniform(-500,500) for _ in range(3))), Vector(*(rng.uniform(-500,500) for _ in range(3))), rng.choice([0, 180, 360, -360, rng.uniform(-720, 720)]), rng.choice([-1,0,1,2,3,4,5]))
        wanted, state = native.run(inp), State()
        assert lib.bk_npc_head_update(C.byref(state), C.byref(inp), error), error.value
        actual = list(state.sight.start)+list(state.sight.end)+[state.sight.distance, state.bearing, state.facing]
        expected = list(wanted.sight.start)+list(wanted.sight.end)+[wanted.sight.distance, wanted.bearing, wanted.facing]
        for a, b in zip(actual, expected):
            delta = abs(a-b)/max(1,abs(b)); worst = max(worst, delta)
            assert delta < 3e-6, (case, a, b, inp.actor_kind)
        count += 1
        if case%100 == 0:
            old = bytes(state); inp.actor_kind = 1; inp.actor_head_local[1] = 1.0001
            assert not lib.bk_npc_head_update(C.byref(state), C.byref(inp), error)
            assert bytes(state) == old; rejected += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=count, domain_rejections=rejected, max_normalized_error=worst, native_functions=['0x4fca12..0x4fcc57', '0x42ee68', '0x4aeba9'], hooks=[], x87_control_word='0x037f', scope='Actual cached NPC/player head line, distance, head-to-player-body bearing and original actor-kind facing branches. Synthetic supplied matrices include shear/scale and boundaries; actor resource parent selection and scene sequencing not implied.')
    (ROOT/'local/original-npc-head-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', count, 'head updates;', rejected, 'domain rejections; max error', worst)
if __name__ == '__main__': main()
