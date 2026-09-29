"""Original4db608 parent control, live state and ordered service boundaries.

All parent instructions execute. Opening/state4 and4dd280 are separately
required children; this oracle observes their dispatch, not their internals.
Audio, target picking, actor requests, manual camera and expression/eye leaves
are explicit deterministic fixtures with callback mutations. This is neither
a natural ending nor a production/GPU/Switch validation.
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
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame, Input, FIELDS, BYTES
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR
from model_binding import ROOT, library

I, U, F, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_int8, C.c_void_p


class State(C.Structure):
    _fields_ = [(n, I) for n in ['kind', 'column', 'clip', 'hover', 'delay']] + [
        ('elapsed', F), ('manual_choice', S), ('manual_action', S)]


class Record(C.Structure):
    _fields_ = [('retained', (U * 10000) * 2), ('actions', I * 10000), ('count', I)]


class Records(C.Structure):
    _fields_ = [('groups', Record * 5)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary))] + [
        (n, C.POINTER(I)) for n in ['substate', 'wait', 'latches', 'ready',
            'target', 'inputs', 'processed', 'next', 'open', 'volume']] + [
        ('previous', C.POINTER(S)), ('actions', C.POINTER(I)), ('name', P),
        ('random', C.POINTER(U)), ('records', C.POINTER(Records))]


Key = C.CFUNCTYPE(I, P, U, U, C.POINTER(U), P)
RawKey = C.CFUNCTYPE(I, P, U, C.POINTER(U), P)
Target = C.CFUNCTYPE(I, P, C.POINTER(F), C.POINTER(I), P)
Status = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
Slot = C.CFUNCTYPE(I, P, U, P)
Load = C.CFUNCTYPE(I, P, U, C.c_char_p, P)
Play = C.CFUNCTYPE(I, P, U, I, I, P)
Name = C.CFUNCTYPE(I, P, I, P, P)
Actor = C.CFUNCTYPE(I, P, U, I, P)
Manual = C.CFUNCTYPE(I, P, P)
Action = C.CFUNCTYPE(I, P, I, C.POINTER(Input), P)
Opening = C.CFUNCTYPE(I, P, F, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('key', Key), ('raw_key', RawKey),
                ('target', Target), ('present', Status), ('status', Status),
                ('pause', Slot), ('load', Load), ('play', Play), ('name', Name),
                ('eyes', Slot), ('request', Actor), ('actor_present', Status),
                ('root_flag', Actor), ('manual_camera', Manual),
                ('action', Action), ('opening', Opening)]


STATE_ADDR = [0x721e10, 0x7220f0, 0x57563c, 0x719c54, 0x575640,
              0x719c58, 0x575634, 0x719b60]
EXTRA_ADDR = [('substate', 0x719b50), ('wait', 0x719b24),
              ('latches', 0x719b54), ('ready', 0x719b0c), ('target', 0x719444),
              ('inputs', 0x709c70), ('processed', 0x719b64), ('next', 0x719b20),
              ('open', 0x72210c), ('volume', 0xbe9a08), ('previous', 0x721ad4),
              ('actions', 0x709db8), ('name', 0x722224), ('seed', 0x58edd8)]
NAMES = [b'fixture-loop.wav', b'fixture-action.wav']
SHARED = ['frame', 'control', 'aux', 'state', 'input'] + [n for n, _ in EXTRA_ADDR]


class Fixture:
    def clone(self):
        out = Fixture()
        for name in SHARED + ['records']:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'hr', 'loaded']:
            setattr(out, name, list(getattr(self, name)))
        for name in ['pick', 'key_mask', 'raw_mask', 'actor_exists',
                     'mutate_at', 'seconds']:
            setattr(out, name, getattr(self, name))
        return out

    def snapshot(self):
        # Original helper's complete10000-word lanes have their own tests.
        # This caller suite uses counts0..4 and checks all five lane prefixes,
        # the count0 predecessor, and the immutable table/input/other fields.
        records = tuple((r.retained[1][9999], tuple(r.actions[:8]), r.count)
                        for r in self.records.groups)
        return tuple(bytes(getattr(self, n)) for n in SHARED) + (
            records, tuple(self.present), tuple(self.playing))

    def key(self, code, mode):
        bit = ({0: 0, 1: 1, 0x5a: 2, 0x33450: 3}[code] + mode * 4)
        return 0x400 if not self.key_mask & (1 << bit) else 0x401

    def raw_key(self, code):
        bit = [0x41, 0x53, 0x44, 0x46, 0x42, 0x58, 0x43, 0x56, 0x20].index(code)
        return 0x10001 | (0x8000 if self.raw_mask & (1 << bit) else 0)

    def change(self, number):
        if number != self.mutate_at:
            return
        self.frame.group = (self.frame.group + 1) % 5
        self.previous.value = 8 if self.previous.value == 24 else 24
        self.frame.camera_cached = 5
        self.volume.value = -1931
        self.aux.progress = .59
        self.state.clip += 3
        self.control.toggles[7] ^= 1

    def effect(self, event, number):
        if event[0] == 'load':
            self.present[event[1]] = self.loaded[event[1]]
            self.playing[event[1]] = False
        elif event[0] == 'play':
            self.playing[event[1]] = self.present[event[1]]
        elif event[0] == 'pause':
            self.playing[event[1]] = False
        elif event[0] == 'target':
            self.frame.camera_cached, self.state.kind, self.state.column = self.pick[1:]
        elif event[0] == 'opening':
            self.substate.value += 1
        elif event[0] == 'action':
            self.ready.value = (self.ready.value + 1) % 7
        self.change(number)


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f2f0, 0x300d010)
        self.word(0x53f214, 0x300d020)
        self.word(0x53f2ec, 0x300d030)
        self.word(0x300b024, 0x300d100)
        self.word(0x300b048, 0x300d110)
        for slot in range(2):
            self.word(0x300a000 + slot * 32, 0x300b000)
        for address in [0x300d010, 0x300d020, 0x300d030, 0x300d100, 0x300d110,
                        0x4b76c2, 0x4dac12, 0x4e0956, 0x4ad2bf, 0x4dfbbd,
                        0x4dfca9, 0x4dfb96, 0x4018c8, 0x423a99, 0x4e252b,
                        0x4dd280, 0x4db68f]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def string(self, address):
        return bytes(self.u.mem_read(address, 260)).split(b'\0')[0]

    def spans(self, s):
        for name, address in FIELDS + BYTES:
            typ = dict(Frame._fields_)[name]
            yield address, C.addressof(s.frame) + getattr(Frame, name).offset, C.sizeof(typ)
        yield 0x721e14, C.addressof(s.frame.camera_values), 12
        yield 0x709fcc, C.addressof(s.frame.camera_table), 80
        yield 0x7220f8, C.addressof(s.control.toggles), 8
        for (name, typ), address in zip(Auxiliary._fields_, ADDR):
            yield address, C.addressof(s.aux) + getattr(Auxiliary, name).offset, C.sizeof(typ)
        for (name, typ), address in zip(State._fields_, STATE_ADDR):
            yield address, C.addressof(s.state) + getattr(State, name).offset, C.sizeof(typ)
        for name, address in EXTRA_ADDR:
            value = getattr(s, name)
            yield address, C.addressof(value), C.sizeof(value)
        for group, r in enumerate(s.records.groups):
            base = 0xb550b0 + group * 0x1d4c4
            yield base + 79996, C.addressof(r.retained[1]) + 9999 * 4, 4
            yield base + 80000, C.addressof(r.actions), 32
            yield base + 120000, C.addressof(r) + Record.count.offset, 4

    def write(self, s):
        for address, source, count in self.spans(s):
            self.u.mem_write(address, C.string_at(source, count))
        for slot in range(2):
            self.word(0x722334 + slot * 0x120,
                      0x300a000 + slot * 32 if s.present[slot] else 0)
        self.word(0x721b28, 0x3003000)
        self.word(0x721b2c, 0x3004000 if s.actor_exists else 0)
        self.word(0x3004160, 0x3005000)
        self.word(0x3005014, 0x3006000)

    def read(self):
        s = self.current.clone()
        for address, target, count in self.spans(s):
            C.memmove(target, bytes(self.u.mem_read(address, count)), count)
        return s

    def hook(self, uc, address, _size, _context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        words = struct.unpack('<13I', uc.mem_read(sp, 52))
        ret, first, second, third = words[:4]
        result, pop, event = 0, 4, None
        if address == 0x300d010:
            fmt = self.string(second)
            count = fmt.count(b'%')
            assert count in (1, 2, 3), fmt
            arguments = struct.unpack('<' + 'i' * count,
                                      uc.mem_read(sp + 12, count * 4))
            name = fmt % arguments
            uc.mem_write(first, name + b'\0')
            result = len(name)
        elif address == 0x300d020:
            uc.mem_write(first, self.string(second) + b'\0')
            result, pop = first, 12
        else:
            s = self.read()
            if address == 0x300d030:
                event, result, pop = ('raw', first), s.raw_key(first), 8
            elif address == 0x4b76c2:
                assert third == 0
                event, result = ('key', first, second), s.key(first, second)
            elif address == 0x4dac12:
                event = ('target', tuple(struct.unpack('<2f', uc.mem_read(sp + 4, 8))))
                result = s.pick[0]
            elif address == 0x4e0956:
                event = ('load', second, self.string(first))
            elif address == 0x4ad2bf:
                slot = (first - 0x300a000) // 32 if first else int(ret == 0x4dcc7d)
                assert slot in (0, 1)
                event = ('play', slot, I(second).value, I(third).value)
            elif address in (0x300d100, 0x300d110):
                slot = (first - 0x300a000) // 32
                assert slot in (0, 1) and s.present[slot]
                if address == 0x300d100:
                    event = ('status', slot)
                    uc.mem_write(second, struct.pack('<I', 1 if s.playing[slot] else 2))
                    result, pop = s.hr[slot] & 0xffffffff, 12
                else:
                    event, pop = ('pause', slot), 8
            elif address in (0x4dfbbd, 0x4dfca9):
                kind = int(address == 0x4dfca9)
                event = ('name', kind)
                uc.mem_write(first, NAMES[kind] + b'\0')
            elif address == 0x4dfb96:
                # The already recovered expression/eye leaf is the service
                # boundary. Its preceding scalar writes are still observed.
                assert third == 1
                s.aux.expression_a, s.aux.expression_b = I(first).value, I(second).value
                event = ('eyes', third)
            elif address == 0x4018c8:
                assert first in (0x3003000, 0x3004000)
                event = ('request', int(first == 0x3004000), I(second).value)
            elif address == 0x423a99:
                assert first == 0x3006000
                event = ('root', 1, I(second).value)
            elif address == 0x4e252b:
                event = ('manual',)
            elif address == 0x4dd280:
                event = ('action', I(first).value, bytes(uc.mem_read(sp + 8, 44)))
            elif address == 0x4db68f:
                event = ('opening', struct.pack('<f', s.seconds))
            else:
                raise AssertionError(hex(address))
            self.trace.append((event, s.snapshot()))
            s.effect(event, len(self.trace))
            self.current = s
            self.write(s)
        if address == 0x4db68f:
            uc.reg_write(UC_X86_REG_EIP, 0x4dd24e)
        else:
            uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
            uc.reg_write(UC_X86_REG_ESP, sp + pop)
            uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture):
        self.current = fixture.clone()
        self.write(self.current)
        self.u.mem_write(0x733700, struct.pack('<f', fixture.seconds))
        self.trace = []
        self.call(0x4db608, bytes(fixture.input))
        return self.trace, self.read().snapshot()


def portable(lib, fixture, fail_at=0):
    s, trace = fixture.clone(), []
    error = C.create_string_buffer(256)

    def commit(event):
        trace.append((event, s.snapshot()))
        if len(trace) == fail_at:
            return 0
        s.effect(event, len(trace))
        return 1

    @Key
    def key(_c, code, mode, result, _e):
        result[0] = s.key(code, mode)
        return commit(('key', code, mode))

    @RawKey
    def raw(_c, code, result, _e):
        result[0] = s.raw_key(code)
        return commit(('raw', code))

    @Target
    def target(_c, xy, result, _e):
        result[0] = s.pick[0]
        return commit(('target', tuple(xy[:2])))

    @Status
    def present(_c, slot, out, _e):
        out[0] = s.present[slot]
        return 1

    @Status
    def status(_c, slot, out, _e):
        out[0] = s.playing[slot] if not s.hr[slot] else 0
        return commit(('status', slot))

    @Slot
    def pause(_c, slot, _e):
        return commit(('pause', slot))

    @Load
    def load(_c, slot, name, _e):
        return commit(('load', slot, name))

    @Play
    def play(_c, slot, flags, volume, _e):
        return commit(('play', slot, flags, volume))

    @Name
    def name(_c, kind, out, _e):
        C.memmove(out, NAMES[kind] + b'\0', len(NAMES[kind]) + 1)
        return commit(('name', kind))

    @Slot
    def eyes(_c, slot, _e):
        return commit(('eyes', slot))

    @Actor
    def request(_c, actor, clip, _e):
        return commit(('request', actor, clip))

    @Status
    def actor_present(_c, actor, out, _e):
        assert actor == 1
        out[0] = s.actor_exists
        return 1

    @Actor
    def root(_c, actor, flag, _e):
        return commit(('root', actor, flag))

    @Manual
    def manual(_c, _e):
        return commit(('manual',))

    @Action
    def action(_c, clip, inp, _e):
        return commit(('action', clip, bytes(inp.contents)))

    @Opening
    def opening(_c, seconds, _e):
        return commit(('opening', struct.pack('<f', seconds)))

    b = Bindings(C.pointer(s.frame), C.pointer(s.control), C.pointer(s.aux),
                 C.pointer(s.substate), C.pointer(s.wait), s.latches,
                 C.pointer(s.ready), C.pointer(s.target), s.inputs, s.processed,
                 C.pointer(s.next), C.pointer(s.open), C.pointer(s.volume),
                 C.pointer(s.previous), s.actions, C.addressof(s.name),
                 C.pointer(s.seed), C.pointer(s.records))
    ops = Ops(None, key, raw, target, present, status, pause, load, play, name,
              eyes, request, actor_present, root, manual, action, opening)
    ok = lib.bk_ending_normal_control_step(C.byref(s.state), C.byref(b),
                                           C.byref(s.input), s.seconds,
                                           C.byref(ops), error)
    return ok, trace, s.snapshot(), error.value


def f32_neighbors(value):
    word = struct.unpack('<I', struct.pack('<f', value))[0]
    return [struct.unpack('<f', struct.pack('<I', word + offset))[0]
            for offset in [-1, 0, 1]]


def fixture(rng, case):
    s = Fixture()
    s.frame, s.control, s.aux = Frame(), Control(), Auxiliary()
    s.state, s.input, s.records = State(), Input(), Records()
    s.frame.phase, s.frame.group = 1, case % 5
    states = [0, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 5, 6, -1]
    s.frame.state_721ee0 = states[case % len(states)]
    s.frame.camera_mode = rng.choice([0, 0, 0, 1, 2, 4])
    s.frame.camera_manual = rng.choice([0, 0, 1, 1, 255])
    s.frame.camera_cached = rng.choice([-1, 1, 5, 6, 9, 10, 11, 12, 26, 27])
    s.frame.transition_action, s.frame.curtain_wanted = 37, 0
    for i in range(8):
        s.control.toggles[i] = rng.choice([0, 1, 255])
    values = [0, .1, 1, 2]
    for value in [.19, .2, .39, .4, .49, .59, .6, .71, .79]:
        values.extend(f32_neighbors(value))
    s.aux.progress = values[case % len(values)]
    s.aux.pending = rng.choice([0, 0, 1, 2, 3, 9, 10, 11, 12, 13])
    s.aux.index, s.aux.selection = 9, 2
    s.aux.expression_a, s.aux.expression_b = 17, 19
    s.state.kind, s.state.column = rng.randrange(3), rng.randrange(5)
    s.state.clip, s.state.hover, s.state.delay = 31, rng.randrange(12), 20
    s.state.elapsed = rng.choice([0, 19.9, 20, 49.9])
    s.state.manual_choice, s.state.manual_action = rng.randrange(-1, 8), 7
    for name in ['substate', 'wait', 'ready', 'target', 'next', 'open', 'volume']:
        setattr(s, name, I(rng.randrange(2)))
    s.substate.value, s.ready.value, s.target.value, s.volume.value = 3, 6, 77, -900
    s.wait.value = rng.choice([0, 0, 0, 1])
    s.previous = S(rng.choice([0, 8, 8, 24]))
    s.latches = (I * 3)(*[rng.randrange(2) for _ in range(3)])
    s.inputs = (I * 14)(*[rng.randrange(10) for _ in range(14)])
    s.processed = (I * 14)(*[rng.randrange(10) for _ in range(14)])
    s.actions = (I * 80)(*[rng.randrange(1, 128) if rng.randrange(5) else -1
                          for _ in range(80)])
    s.name = (C.c_char * 32).from_buffer_copy(b'prior.wav\0' + b'Q' * 22)
    s.seed = U(rng.getrandbits(32))
    for r in s.records.groups:
        r.retained[1][9999] = rng.choice([0, 10, 12, 13, 14, 0xffffffff])
        r.actions[:8] = [rng.choice([0, 10, 12, 13, 14, -1]) for _ in range(8)]
        r.count = rng.randrange(5)
    s.present = [rng.choice([0, 1, 1]), rng.choice([0, 0, 1])]
    s.playing = [bool(s.present[i] and rng.randrange(2)) for i in range(2)]
    s.loaded = [rng.choice([0, 1, 1]), rng.choice([0, 1, 1])]
    s.hr = [rng.choice([0, 0, -1]), rng.choice([0, 0, -1])]
    s.key_mask, s.raw_mask = rng.randrange(1 << 12), rng.randrange(1 << 9)
    s.actor_exists = rng.randrange(2)
    s.pick = (rng.choice([0, 1, 256]),
              [1, 5, 6, 9, 10, 11, 12, 26, 27, 0][case % 10],
              rng.randrange(3), rng.randrange(5))
    s.mutate_at = rng.randrange(1, 8) if case % 4 == 0 else 0
    s.seconds = F(rng.choice([0, 1 / 60, .1, 1, 30])).value
    for i in range(11):
        s.input.words[i] = rng.getrandbits(32)
    # Directed idle cases guarantee real target/action and both stride paths.
    if case % 14 in (2, 3, 4):
        s.aux.pending, s.frame.camera_mode, s.wait.value = 1, 0, 0
        s.present[1], s.playing[1], s.next.value = 0, False, 1
        s.latches[:], s.open.value = [1, 1, 1], 0
        s.pick = (1, s.pick[1], s.pick[2], s.pick[3])
        s.key_mask |= 1 << 4
        s.actions[s.pick[2] * 15 + s.pick[3] * 5 + 4] = 37
    # Directed no-hover cases exercise stopped speech fallback and wait gates.
    if case % 14 == 5:
        s.pick = (0, -1, 0, 0)
        s.key_mask = 0
        s.next.value = 1
    return s


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--cases', type=int, default=7000)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native, lib = Native(exe), library()
    lib.bk_ending_normal_control_step.argtypes = [C.POINTER(State),
        C.POINTER(Bindings), C.POINTER(Input), F, C.POINTER(Ops), P]
    lib.bk_ending_normal_control_initial.restype = State
    initial = lib.bk_ending_normal_control_initial()
    for (name, typ), address in zip(State._fields_, STATE_ADDR):
        expected = typ.from_buffer_copy(bytes(native.u.mem_read(address, C.sizeof(typ)))).value
        assert getattr(initial, name) == expected, (name, getattr(initial, name), expected)
    rng = random.Random(0x4db608)
    branches, events, mutations, failures, rejects = {}, {}, 0, 0, 0
    for case in range(args.cases):
        s = fixture(rng, case)
        expected, final = native.run(s)
        ok, actual, got, error = portable(lib, s)
        assert ok, (case, error)
        assert len(actual) == len(expected), (case, 'call count',
            [v[0] for v in actual], [v[0] for v in expected])
        for step, (a, z) in enumerate(zip(actual, expected)):
            assert a[0] == z[0], (case, step, 'event', a[0], z[0])
            for n, (av, zv) in enumerate(zip(a[1], z[1])):
                assert av == zv, (case, step, a[0], SHARED[n] if n < len(SHARED) else n, av, zv)
        for n, (av, zv) in enumerate(zip(got, final)):
            assert av == zv, (case, 'final', SHARED[n] if n < len(SHARED) else n, av, zv)
        key = str(s.frame.state_721ee0)
        branches[key] = branches.get(key, 0) + 1
        for event, _ in expected:
            events[event[0]] = events.get(event[0], 0) + 1
        mutations += bool(0 < s.mutate_at <= len(expected))
        if expected and case % 11 == 0:
            stop = 1 + case % len(expected)
            ok, actual, stopped, _ = portable(lib, s, stop)
            assert not ok and actual == expected[:stop], (case, 'failed prefix')
            assert stopped == expected[stop - 1][1], (case, 'failed writes')
            failures += 1
        if case % 100 == 0:
            for bad in [-1, math.inf, math.nan]:
                s.seconds = bad
                ok, actual, stopped, _ = portable(lib, s)
                assert not ok and not actual and stopped == s.snapshot()
                rejects += 1
    report = dict(passed=True, exe_sha256=digest, frames=args.cases,
                  branches=branches, service_calls=events, callback_mutations=mutations,
                  failed_prefixes=failures, invalid_time_rejections=rejects,
                  scope=__doc__)
    (ROOT / 'local/original-ending-normal-control-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
