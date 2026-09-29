"""Complete4965b9 automatic auxiliary cycle against original instructions.

The original branch, look-ahead, byte countdown, index arithmetic, expression
setter and source stores execute. Configured selection, voice, eye selection
and RNG are observing leaves. Read/write service failures are injected at the
corresponding native instruction boundaries; they prove retained prefixes,
not native I/O failures. Resource/scheduler behavior is verified separately.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Machine
from original_ending_frame_oracle import State as Frame, FIELDS, BYTES
from original_ending_auxiliary_oracle import (
    State, ADDR, Ops, Active, Write, Request, Audio, Eyes,
)
from model_binding import ROOT, library

I, P = C.c_int32, C.c_void_p


class Prediction(C.Structure):
    _fields_ = [('duration', I), ('end', C.c_float), ('source', C.c_float),
                ('rate', C.c_float)]


class Cycle(C.Structure):
    _fields_ = [('scale', C.c_float), ('countdown', C.c_int8)]


Predict = C.CFUNCTYPE(I, P, C.c_uint, C.POINTER(Prediction), P)
Random = C.CFUNCTYPE(I, P, C.POINTER(I), P)


class TickOps(C.Structure):
    _fields_ = [('auxiliary', Ops), ('prediction', Predict), ('random', Random)]


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def value(raw):
    return struct.unpack('<f', struct.pack('<I', raw))[0]


READS = {0x4965de: 'active', 0x496770: 'active',
         0x4965f0: 'prediction', 0x496782: 'prediction'}
WRITES = {0x49674e: 6, 0x496765: 7, 0x49689d: 10, 0x4968b4: 11}


class Native(Machine):
    def __init__(self, exe):
        super().__init__(exe)
        for address in [*READS, *WRITES, 0x4018c8, 0x4946b4, 0x4a07a9, 0x534a34]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, raw):
        self.u.mem_write(address, struct.pack('<I', raw & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<i', self.u.mem_read(address, 4))[0]

    def snapshot(self):
        s, f = State(), Frame()
        for (name, _), address in zip(State._fields_, ADDR):
            C.memmove(C.addressof(s) + getattr(State, name).offset,
                      bytes(self.u.mem_read(address, 4)), 4)
        for name, address in FIELDS:
            setattr(f, name, self.integer(address))
        for name, address in BYTES:
            setattr(f, name, self.u.mem_read(address, 1)[0])
        actor = bytes(self.u.mem_read(self.actor, 0x5100))
        rows = tuple(struct.unpack_from('<I', actor, 0x190 + i * 156 + off)[0]
                     for i in range(128) for off in (0x54, 0x60, 0x70, 0x74))
        return (bytes(s), bytes(f), self.integer(self.actor + 0x140), rows,
                self.eye, bytes(self.u.mem_read(0x55469c, 4)),
                self.u.mem_read(0x55696d, 1)[0], self.integer(0xbe9a08))

    def emit(self, event):
        self.trace.append((event, self.snapshot()))
        if event[0] == self.data['mutation'] and not self.mutated:
            for name, raw in self.data['changes'].items():
                if name in ('group', 'countdown'):
                    self.u.mem_write(0x721b3c if name == 'group' else 0x55696d,
                                     bytes([raw & 255]))
                elif name == 'phase':
                    self.word(0x721e00, raw)
                elif name == 'voice':
                    self.word(0xbe9a08, raw)
                elif name == 'scale':
                    self.word(0x55469c, bits(raw))
                else:
                    index = [field for field, _ in State._fields_].index(name)
                    self.word(ADDR[index], raw)
            self.mutated = True
        if len(self.trace) - 1 == self.data['failure']:
            self.failed = True
            self.u.reg_write(UC_X86_REG_EIP, self.stop)
            return False
        return True

    def hook(self, u, address, size, unused):
        if address in READS:
            kind = READS[address]
            event = (kind,) if kind == 'active' else (kind, self.integer(self.actor + 0x140))
            self.emit(event)
            return
        if address in WRITES:
            self.emit(('write', WRITES[address], 2, 0))
            return
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, *args = struct.unpack('<5I', u.mem_read(sp, 20))
        result = 0
        if address == 0x4018c8:
            assert args[0] == self.actor
            if not self.emit(('request', args[1])):
                return
            self.word(self.actor + 0x140, args[1])
        elif address == 0x4946b4:
            if not self.emit(('audio', 3, args[1], args[0], 0, args[2],
                              self.integer(0xbe9a08))):
                return
        elif address == 0x4a07a9:
            if not self.emit(('eyes', args[1])):
                return
            if self.data['eye_present']:
                self.eye = 77
        elif address == 0x534a34:
            if not self.emit(('random',)):
                return
            result = self.data['random']
        else:
            raise AssertionError(hex(address))
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, data):
        self.data, self.trace = data, []
        self.failed = self.mutated = False
        self.eye = 31
        self.word(0x721b28, self.actor)
        self.u.mem_write(self.actor, bytes(0x5100))
        self.word(self.actor + 0x140, data['slot'])
        for index, row in enumerate(data['rows']):
            for raw, off in zip(row, (0x54, 0x60, 0x70, 0x74)):
                self.word(self.actor + 0x190 + index * 156 + off, raw)
        if 0 <= data['slot'] < 128:
            base = self.actor + 0x190 + data['slot'] * 156
            prediction = data['prediction']
            self.word(base + 0x50, prediction.duration)
            self.word(base + 0x58, bits(prediction.end))
            self.word(base + 0x5c, bits(prediction.rate))
        for (name, _), address in zip(State._fields_, ADDR):
            offset = getattr(State, name).offset
            self.u.mem_write(address, bytes(data['state'])[offset:offset + 4])
        for name, address in FIELDS:
            self.word(address, getattr(data['frame'], name))
        for name, address in BYTES:
            self.u.mem_write(address, bytes([getattr(data['frame'], name)]))
        self.word(0x55469c, bits(data['cycle'].scale))
        self.u.mem_write(0x55696d, bytes([data['cycle'].countdown & 255]))
        self.word(0xbe9a08, data['voice'])
        self.word(0x733700, bits(data['seconds']))
        self.call(0x4965b9, b'')
        return not self.failed, self.trace, self.snapshot(), self.mutated


def portable(lib, data):
    s, f = State.from_buffer_copy(data['state']), Frame.from_buffer_copy(data['frame'])
    cycle, voice = Cycle.from_buffer_copy(data['cycle']), I(data['voice'])
    rows, slot, eye = [row[:] for row in data['rows']], [data['slot']], [31]
    trace, mutated = [], [False]

    def snapshot():
        return (bytes(s), bytes(f), slot[0], tuple(v for row in rows for v in row),
                eye[0], struct.pack('<f', cycle.scale), cycle.countdown & 255, voice.value)

    def emit(event):
        trace.append((event, snapshot()))
        if event[0] == data['mutation'] and not mutated[0]:
            for name, raw in data['changes'].items():
                if name in ('group', 'phase'):
                    setattr(f, name, raw)
                elif name in ('scale', 'countdown'):
                    setattr(cycle, name, raw)
                elif name == 'voice':
                    voice.value = raw
                else:
                    setattr(s, name, raw)
            mutated[0] = True
        return int(len(trace) - 1 != data['failure'])

    @Active
    def active(_, out, err):
        if not emit(('active',)):
            return 0
        out[0] = slot[0]
        return 1

    @Predict
    def predict(_, index, out, err):
        if not emit(('prediction', index)):
            return 0
        out[0] = Prediction.from_buffer_copy(data['prediction'])
        out[0].source = value(rows[index][1])
        return 1

    @Write
    def write(_, index, kind, raw, err):
        if not emit(('write', index, kind, raw)):
            return 0
        rows[index][1] = rows[index][0]
        return 1

    @Request
    def request(_, index, err):
        if not emit(('request', index)):
            return 0
        slot[0] = index
        return 1

    @Random
    def random_value(_, out, err):
        if not emit(('random',)):
            return 0
        out[0] = data['random']
        return 1

    @Audio
    def audio(_, call, out, err):
        c = call.contents
        out[0] = 0
        return emit(('audio', c.operation, c.slot, c.cue, c.bank, c.flags, c.volume))

    @Eyes
    def eyes(_, index, err):
        if not emit(('eyes', index)):
            return 0
        if data['eye_present']:
            eye[0] = 77
        return 1

    ops = TickOps(Ops(None, active, write, request, audio, eyes), predict, random_value)
    error = C.create_string_buffer(256)
    result = lib.bk_ending_auxiliary_tick(C.byref(s), C.byref(f), C.byref(cycle),
            data['seconds'], C.byref(voice), C.byref(ops), error)
    return bool(result), trace, snapshot(), mutated[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe, lib = args.exe.read_bytes(), library()
    native = Native(exe)
    lib.bk_ending_auxiliary_tick.argtypes = [C.POINTER(State), C.POINTER(Frame),
            C.POINTER(Cycle), C.c_float, C.POINTER(I), C.POINTER(TickOps), P]
    lib.bk_ending_auxiliary_cycle_initial.restype = Cycle
    initial = lib.bk_ending_auxiliary_cycle_initial()
    assert struct.pack('<f', initial.scale) == bytes(native.u.mem_read(0x55469c, 4))
    assert initial.countdown == struct.unpack('<b', native.u.mem_read(0x55696d, 1))[0]
    rng = random.Random(0x4965b9)
    frames, services, failures, mutations, rewinds = 12000, 0, 0, 0, 0
    selected, countdowns = {13: 0, 14: 0}, set()
    for case in range(frames):
        s = State(0, case % 2, case % 10, rng.choice([-2147483648, 2147483647, 7]),
                  -9, 3, 0 if case % 2 == 0 else rng.choice([1, -1, 7]), .75, 11, 12)
        f = Frame()
        f.group, f.phase, f.camera_cached, f.camera_event = case % 5, (case % 7) + 1, 77, 88
        slot = (11 if s.direction else 7) if case % 5 else rng.choice([-1, 0, 3, 7, 11, 13, 14, 127])
        countdown = case % 256 - 128
        if case % 3 == 0:
            countdown = rng.choice([-128, -127, -1, 0, 1, 2, 10, 127])
        countdowns.add(countdown)
        cycle = Cycle(rng.choice([.02, .01, 0, -.02, math.nan, math.inf]), countdown)
        seconds = C.c_float(rng.choice([0, 1 / 60, .001, .05, .5, 1])).value
        prediction = Prediction(rng.choice([0, 1, 15, 400, 2147483647, -3]),
                rng.uniform(-100, 500), 0, rng.choice([0, .1, .5, 1, -1, math.nan, math.inf]))
        prediction.source = rng.choice([prediction.end, prediction.end + 1,
                prediction.end - 1, 0, rng.uniform(-100, 500)])
        rows = [[bits(rng.uniform(0, 100)), bits(rng.uniform(100, 300)),
                 rng.randrange(2), rng.randrange(128)] for _ in range(128)]
        if 0 <= slot < 128:
            rows[slot][1] = bits(prediction.source)
        data = dict(state=s, frame=f, cycle=cycle, slot=slot, rows=rows,
                prediction=prediction, seconds=seconds, voice=-800,
                random=rng.choice([rng.randrange(32768), -1, -2147483648, 2147483647]),
                eye_present=case % 3 != 0,
                failure=rng.randrange(9) if case % 7 == 0 else -1,
                mutation=rng.choice(['request', 'random', 'audio', 'eyes']) if case % 3 == 0 else '',
                changes=dict(base=-2147483648, selection=2147483647,
                    direction=27, group=4, phase=6, voice=-999,
                    countdown=-117, scale=.01))
        expected, actual = native.run(data), portable(lib, data)
        if actual != expected:
            for index, (a, b) in enumerate(zip(actual[1], expected[1])):
                if a != b:
                    raise AssertionError((case, 'service', index, a, b))
            raise AssertionError((case, 'result', actual, expected))
        services += len(actual[1])
        failures += not actual[0]
        mutations += actual[3]
        for event, _ in actual[1]:
            if event[0] == 'request':
                selected[event[1]] += 1
            elif event[0] == 'write':
                rewinds += 1
    assert len(countdowns) == 256 and all(selected.values()) and failures and mutations
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
            frames=frames, service_calls=services, failed_prefixes=failures,
            mutable_callbacks=mutations, selected=selected, source_rewinds=rewinds,
            distinct_countdowns=len(countdowns), byte_exact=True, max_error=0,
            scope=__doc__)
    (ROOT / 'local/original-ending-auxiliary-tick-oracle.json').write_text(
            json.dumps(report, indent=2) + '\n')
    print('PASS', json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
