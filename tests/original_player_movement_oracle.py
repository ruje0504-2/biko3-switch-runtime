"""Execute original4c0126 through4c0ba4, before matrix/collision/scene effects.

Only device key/mouse queries and prior interaction decision4c20ee are inputs.
Movement, trig, action branching, angular bounds and original order execute x86.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library


class State(C.Structure):
    _fields_ = [('position', C.c_float * 3), ('previous', C.c_float * 3),
                ('velocity', C.c_float * 3), ('yaw', C.c_float), ('pitch', C.c_float),
                ('turn', C.c_float * 2), ('acceleration', C.c_float),
                ('action', C.c_int32), ('move_latch', C.c_int32),
                ('interaction_mode', C.c_int8)]


class Input(C.Structure):
    _fields_ = [('seconds', C.c_float), ('look', C.c_float * 2),
                ('buttons', C.c_uint32), ('controls_allowed', C.c_int),
                ('active_clip', C.c_int32), ('actions', C.c_int32 * 21)]


FIELDS = [('position', 0x29c, '3f'), ('previous', 0x2c8, '3f'),
          ('velocity', 0x2b4, '3f'), ('yaw', 0x2ac, 'f'), ('pitch', 0x2b0, 'f'),
          ('turn', 0x2c0, '2f'), ('acceleration', 0x2d4, 'f'), ('action', 0x10, 'i'),
          ('interaction_mode', 0x579, 'b')]
ALIASES = [[(0, 1, 0), (0, 2, 0), (0x26, 2, 0), (0x30d40, 2, 0)],
           [(1, 2, 0), (0x28, 2, 0), (0x30d41, 2, 0)],
           [(0x25, 2, 0), (0x30d42, 2, 0)],
           [(0x27, 2, 0), (0x30d43, 2, 0)],
           [(0x20, 2, 0), (0x33450, 2, 0)], [(0, 1, 2)]]


class Native:
    actor, model, stack, stop = 0x3001000, 0x3003000, 0x2008000, 0x4c0ba4
    def __init__(self, exe):
        self.u = machine(exe)
        for address in (0x4b757e, 0x4b76c2, 0x4c20ee):
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def hook(self, u, address, size, data):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        result = 0
        if address == 0x4b757e:
            a, b = struct.unpack('<2I', u.mem_read(sp + 4, 8))
            u.mem_write(a, struct.pack('<f', self.input.look[0]))
            u.mem_write(b, struct.pack('<f', self.input.look[1]))
        elif address == 0x4b76c2:
            key = struct.unpack('<3I', u.mem_read(sp + 4, 12))
            result = key in self.keys
        else:
            result = self.input.controls_allowed
        u.reg_write(UC_X86_REG_EAX, int(result))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)

    def step(self, state, input_, rng):
        self.input = input_
        self.keys = {rng.choice(aliases) for bit, aliases in enumerate(ALIASES)
                     if input_.buttons & (1 << bit)}
        u = self.u
        u.mem_write(self.actor, bytes(0x900))
        u.mem_write(self.actor, struct.pack('<I', self.model))
        u.mem_write(self.model + 0x140, struct.pack('<i', input_.active_clip))
        u.mem_write(self.actor + 0x14, bytes(input_.actions))
        for name, offset, fmt in FIELDS:
            value = getattr(state, name)
            values = list(value) if isinstance(value, C.Array) else [value]
            u.mem_write(self.actor + offset, struct.pack('<' + fmt, *values))
        u.mem_write(0x7099ec, struct.pack('<i', state.move_latch))
        u.mem_write(0x733700, struct.pack('<f', input_.seconds))
        u.mem_write(self.stack, struct.pack('<3I', 0x300f000, 0x3000000, self.actor))
        u.reg_write(UC_X86_REG_ESP, self.stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x4c0126, self.stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == self.stop
        output = State()
        for name, offset, fmt in FIELDS:
            values = struct.unpack('<' + fmt, u.mem_read(self.actor + offset, struct.calcsize(fmt)))
            field = getattr(output, name)
            if isinstance(field, C.Array):
                field[:] = values
            else:
                setattr(output, name, values[0])
        output.move_latch = struct.unpack('<i', u.mem_read(0x7099ec, 4))[0]
        return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_player_movement.argtypes = [C.POINTER(State), C.POINTER(Input), C.c_void_p]
    lib.bk_player_movement.restype = C.c_int
    rng = random.Random(0x4c0126)
    error = C.create_string_buffer(256)
    worst = 0.0
    rejects = 0
    bits = set()
    for case in range(16000):
        if case % 8 == 0:
            state = State()
            state.position[:] = [rng.uniform(-2000, 2000) for _ in range(3)]
            state.previous[:] = [rng.uniform(-2000, 2000) for _ in range(3)]
            state.velocity[:] = [rng.uniform(-10, 10) for _ in range(3)]
            state.turn[:] = [rng.uniform(-10, 10), rng.uniform(-10, 10)]
            state.yaw = rng.choice([-720, -360, -.001, 0, 90, 359.999, 360, 720, rng.uniform(-500, 500)])
            state.pitch = rng.choice([-100, -80, 0, 80, 100, rng.uniform(-100, 100)])
            state.acceleration = rng.uniform(-1, 2)
        inp = Input()
        inp.seconds = rng.choice([0, .001, 1/60, .1, 1, 2])
        inp.look[:] = [rng.uniform(-1000, 1000), rng.uniform(-1000, 1000)]
        inp.buttons = case % 64
        bits.add(inp.buttons)
        inp.controls_allowed = int(case % 11 != 0)
        inp.active_clip = rng.choice([0, 1, 4, -1, 23])
        # Include aliases and actions not in the movement table.
        inp.actions[:] = [100 + i for i in range(21)] if case % 3 else [rng.randrange(10) for _ in range(21)]
        state.action = rng.choice(list(inp.actions) + [-1, 999])
        state.move_latch = rng.choice([-1, 0, 1, 2])
        state.interaction_mode = rng.choice([-1, 0, 1, 2, 5, 6])
        expected = native.step(state, inp, rng)
        assert lib.bk_player_movement(C.byref(state), C.byref(inp), error), error.value
        for name, _, _ in FIELDS:
            a, b = getattr(state, name), getattr(expected, name)
            if isinstance(a, C.Array):
                pairs = zip(a, b)
            else:
                pairs = [(a, b)]
            for actual, wanted in pairs:
                delta = abs(actual - wanted) / max(1, abs(wanted))
                worst = max(worst, delta)
                assert math.isfinite(delta) and delta < 2e-6, (case, name, actual, wanted, inp.buttons)
        assert state.move_latch == expected.move_latch
        if case % 100 == 0:
            held = bytes(state)
            inp.seconds = float('nan')
            assert not lib.bk_player_movement(C.byref(state), C.byref(inp), error)
            assert bytes(state) == held
            rejects += 1
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  native_range='4c0126..4c0ba4 (before matrix/collision)',
                  samples=16000, button_combinations=len(bits), rejects=rejects,
                  max_relative_error=worst,
                  hooks=['device mouse4b757e', 'device key4b76c2', 'prior interaction decision4c20ee'],
                  scope='Movement/action/turn state only. No UI effects, collision, camera or gameplay claim.')
    (ROOT/'local/original-player-movement-oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
