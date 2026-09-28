"""Original expression requests, mouth and blink control; capture morph calls."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

FIELDS = [('eye', C.c_float, 0xb8), ('mouth', C.c_float, 0xbc), ('blink_phase', C.c_int32, 0xc4),
          ('deadline_ms', C.c_uint32, 0xc8), ('blink_duration_ms', C.c_uint32, 0xcc),
          ('cycles', C.c_int32, 0xd0), ('rapid_count', C.c_int32, 0xd4), ('expression', C.c_int32, 0xd8),
          ('eye_mode', C.c_int32, 0xdc), ('eye_max', C.c_float, 0xe0), ('eye_min', C.c_float, 0xe4),
          ('mouth_mode', C.c_int32, 0xe8), ('mouth_max', C.c_float, 0xec), ('mouth_min', C.c_float, 0xf0),
          ('previous_expression', C.c_int32, 0xf4), ('transition', C.c_float, 0xf8),
          ('transition_start_ms', C.c_uint32, 0xfc), ('transition_rate', C.c_float, 0x100),
          ('previous_eye', C.c_float, 0x104), ('previous_mouth', C.c_float, 0x108),
          ('dirty', C.c_int32, 0x10c), ('eye_count', C.c_uint32, 0x70), ('mouth_count', C.c_uint32, 0x94)]
class State(C.Structure):
    _fields_ = [(name, typ) for name, typ, _ in FIELDS]
class Command(C.Structure):
    _fields_ = [('group', C.c_uint32), ('index', C.c_uint32), ('blend', C.c_int32), ('from_', C.c_float), ('to', C.c_float), ('weight', C.c_float)]
class Commands(C.Structure):
    _fields_ = [('count', C.c_uint32), ('commands', Command * 16)]

class Native:
    stack, stop, face, clock_stub = 0x2008000, 0x300f000, 0x3000000, 0x300e000
    def __init__(self, exe):
        self.u = machine(exe); self.word(0x53f358, self.clock_stub)
        for ptr in [self.clock_stub, 0x4316be, 0x432642]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=ptr, end=ptr)
    def word(self, ptr, value): self.u.mem_write(ptr, struct.pack('<I', value & 0xffffffff))
    def hook(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP); ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        if address == self.clock_stub:
            u.reg_write(UC_X86_REG_EAX, self.now); self.clock_calls += 1
        else:
            ptr = struct.unpack('<I', u.mem_read(sp + 4, 4))[0]
            group, index = (ptr - 0xabc000) // 0x100, ((ptr - 0xabc000) % 0x100) // 4
            assert group in [0, 1] and index < 8
            if address == 0x4316be:
                value = struct.unpack('<f', u.mem_read(sp + 8, 4))[0]
                self.commands.append((group, index, 0, value, value, 0))
            else:
                a, b, weight = struct.unpack('<3f', u.mem_read(sp + 8, 12))
                self.commands.append((group, index, 1, a, b, weight))
        u.reg_write(UC_X86_REG_ESP, sp + 4); u.reg_write(UC_X86_REG_EIP, ret)
    def seed(self, state, random_state):
        self.u.mem_write(self.face, bytes(0x114))
        for name, typ, offset in FIELDS:
            self.u.mem_write(self.face + offset, bytes(typ(getattr(state, name))))
        for group, count in enumerate([state.eye_count, state.mouth_count]):
            for index in range(count):
                self.word(self.face + (0x74 if group == 0 else 0x98) + index * 4, 0xabc000 + group * 0x100 + index * 4)
        self.word(0x58edd8, random_state)
    def call(self, address, *args):
        self.commands = []; self.clock_calls = 0
        self.u.mem_write(self.stack, struct.pack('<' + 'I' * (len(args) + 1), self.stop, *args))
        self.u.reg_write(UC_X86_REG_ESP, self.stack); self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(address, self.stop, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.stop
    def state(self):
        state = State()
        for name, typ, offset in FIELDS:
            setattr(state, name, typ.from_buffer_copy(bytes(self.u.mem_read(self.face + offset, 4))).value)
        return state

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native = Native(exe); lib = library(); rng = random.Random(0x4110ef)
    for name, arguments in [
        ('bk_face_init', [C.POINTER(State), C.c_uint, C.c_uint, C.c_void_p]),
        ('bk_face_request', [C.POINTER(State), C.c_int32, C.c_uint32, C.c_void_p]),
        ('bk_face_eye_range', [C.POINTER(State), C.c_float, C.c_float, C.POINTER(C.c_uint32), C.c_void_p]),
        ('bk_face_mouth_range', [C.POINTER(State), C.c_float, C.c_float, C.c_void_p]),
        ('bk_face_transition_seconds', [C.POINTER(State), C.c_float, C.c_void_p]),
        ('bk_face_mouth', [C.POINTER(State), C.c_float, C.c_uint32, C.POINTER(Commands), C.c_void_p]),
        ('bk_face_blink', [C.POINTER(State), C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(Commands), C.c_void_p])]:
        fn = getattr(lib, name); fn.argtypes = arguments; fn.restype = C.c_int
    error = C.create_string_buffer(256); cases = submissions = rejects = random_changes = settings = 0; worst = 0
    def bits(value): return struct.unpack('<I', struct.pack('<f', value))[0]
    def close(a, b):
        nonlocal worst
        delta = abs(a - b) / max(1, abs(b)); worst = max(worst, delta)
        assert math.isfinite(delta) and delta < 3e-6, (case, stage, a, b, delta)
    def compare(state, output=None):
        nonlocal submissions
        expected = native.state()
        for name, typ, _ in FIELDS:
            a, b = getattr(state, name), getattr(expected, name)
            if typ == C.c_float: close(a, b)
            else: assert a == b, (case, stage, name, a, b)
        if output is not None:
            assert output.count == len(native.commands), (case, stage, output.count, native.commands)
            for i, wanted in enumerate(native.commands):
                c = output.commands[i]
                assert (c.group, c.index, c.blend) == wanted[:3], (case, stage, i, wanted)
                for a, b in zip([c.from_, c.to, c.weight], wanted[3:]): close(a, b)
            submissions += output.count
    for case in range(10000):
        if case % 25 == 0:
            state = State(); assert lib.bk_face_init(C.byref(state), rng.choice([0, 1, 2, 8]), rng.choice([0, 1, 3, 8]), error)
            state.eye = rng.uniform(-2, 12); state.mouth = rng.uniform(-2, 12)
            state.blink_phase = rng.choice([-2, -1, 0, 1, 2]); state.deadline_ms = rng.getrandbits(32)
            state.blink_duration_ms = rng.choice([1, 150, 200, 1000, 0xffffffff])
            state.cycles = rng.choice([-2**31, 0, 1, 30]); state.rapid_count = rng.choice([-2**31, 0, 1, 4])
            state.expression = rng.randrange(-1, 8); state.previous_expression = rng.randrange(-1, 8)
            state.eye_min, state.eye_max = sorted([rng.uniform(-2, 12), rng.uniform(-2, 12)])
            state.mouth_min, state.mouth_max = sorted([rng.uniform(-2, 12), rng.uniform(-2, 12)])
            state.eye_mode = rng.choice([-1, 0, 1, 2, 3]); state.mouth_mode = rng.choice([-1, 0, 1, 2, 3])
            state.transition = rng.choice([-1, 0, .5, 1]); state.transition_start_ms = rng.getrandbits(32)
            state.transition_rate = rng.choice([0, .0005, .01, -.001]); state.previous_eye = rng.uniform(-2, 12); state.previous_mouth = rng.uniform(-2, 12)
            random_state = C.c_uint32(rng.getrandbits(32)); native.seed(state, random_state.value)
        if case % 4 == 0:
            minimum, maximum = [C.c_float(rng.uniform(-2, 12)).value for _ in range(2)]
            stage = 'eye range'; native.call(0x411de1, native.face, bits(minimum), bits(maximum))
            assert lib.bk_face_eye_range(C.byref(state), minimum, maximum, C.byref(random_state), error), error.value; compare(state)
            assert random_state.value == struct.unpack('<I', native.u.mem_read(0x58edd8, 4))[0]
            stage = 'mouth range'; native.call(0x411ea7, native.face, bits(minimum), bits(maximum))
            assert lib.bk_face_mouth_range(C.byref(state), minimum, maximum, error), error.value; compare(state)
            seconds = C.c_float(rng.choice([-1, 0, .009, .01, .02, 1, 2, 10000])).value
            stage = 'transition duration'; native.call(0x411f5d, native.face, bits(seconds))
            assert lib.bk_face_transition_seconds(C.byref(state), seconds, error), error.value; compare(state); settings += 3
        # Consecutive calls retain native global RNG and controller state.
        timestamp = rng.choice([0, 0xffffffff, state.deadline_ms, (state.deadline_ms + 1) & 0xffffffff, (state.deadline_ms - 1) & 0xffffffff, rng.getrandbits(32)])
        native.now = (state.transition_start_ms + rng.choice([0, 1, 1998, 1999, 2000, 2001, 10000])) & 0xffffffff
        stage = 'request'; requested = state.expression if case % 7 else rng.randrange(8)
        native.call(0x410fd8, native.face, requested & 0xffffffff)
        assert lib.bk_face_request(C.byref(state), requested, native.now, error), error.value; compare(state)
        output = Commands(); level = C.c_float(rng.choice([-1, 0, 1, 9, rng.uniform(0, 12)])).value
        stage = 'mouth'; native.now = (native.now + rng.choice([0, 1, 1998, 2000, 2001])) & 0xffffffff
        native.call(0x411985, native.face, struct.unpack('<I', struct.pack('<f', level))[0], timestamp)
        assert lib.bk_face_mouth(C.byref(state), level, native.now, C.byref(output), error), error.value; compare(state, output)
        stage = 'blink'; native.now = (native.now + case % 3) & 0xffffffff
        native.call(0x4110ef, native.face, timestamp)
        old_rng = random_state.value
        assert lib.bk_face_blink(C.byref(state), timestamp, native.now, C.byref(random_state), C.byref(output), error), error.value; compare(state, output)
        assert random_state.value == struct.unpack('<I', native.u.mem_read(0x58edd8, 4))[0], (case, random_state.value)
        random_changes += random_state.value != old_rng
        if case % 100 == 0:
            held, command_before, seed_before = bytes(state), bytes(output), random_state.value
            assert not lib.bk_face_mouth(C.byref(state), float('nan'), native.now, C.byref(output), error)
            assert not lib.bk_face_request(C.byref(state), 2**31-1, native.now, error)
            assert bytes(state) == held and bytes(output) == command_before and random_state.value == seed_before; rejects += 2
        cases += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=cases, native_controller_calls=cases*3, setting_calls=settings, morph_submissions=submissions, rng_changing_steps=random_changes, atomic_rejections=rejects, max_normalized_error=worst, native_functions=['0x410fd8', '0x411985', '0x4110ef', '0x411de1', '0x411ea7', '0x411f5d', '0x411d84', '0x41195d', '0x534a34'], hooks=['timeGetTime', '0x4316be/0x432642 morph submission boundary'], scope='Consecutive explicit expression/mouth/blink state and native range/transition setters, two clock inputs, shared native RNG and ordered per-mesh morph commands. Synthetic scalar/binding fixtures; no MORP vertex math, asset morph-node binding, audio playback or full NPC loop.', x87_control_word='0x037f')
    (ROOT/'local/original-face-controller-oracle.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS', cases, 'steps;', submissions, 'morph commands;', random_changes, 'RNG changing steps; max error', worst)

if __name__ == '__main__': main()
