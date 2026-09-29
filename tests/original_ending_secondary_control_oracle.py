"""Original47A5D0 all-state parent, native circles/RNG and ordered live state.

Camera, speech, picking, menu construction and actor requests are observing
services with deterministic state changes. The original parent, circles,
floating comparisons, copies and CRT randomness execute unmodified. This
does not claim the child47D9EE/47C334/47CB41/47CD36 implementations or a
production/natural ending. Resource effects have a separate native oracle.
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
from original_menu_camera_oracle import State as Camera, I as IDENTITY
from original_ending_opening_oracle import CameraCall, Target, Status
from model_binding import ROOT, library

I, U, F, B, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_int8, C.c_void_p
PAIR = I * 2
ACTOR, NODE = 0x3003000, 0x3002000
EXE_HASH = 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'


class State(C.Structure):
    _fields_ = [('remaining', I), ('alternate', I), ('fov', F),
                ('automatic', I), ('chance', I), ('saved_camera', F * 4),
                ('saved_toggle', B)]


class Timing(C.Structure):
    _fields_ = [('end', F), ('source', F)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('camera', C.POINTER(Camera)),
                ('substate', C.POINTER(B)), ('latches', C.POINTER(I)),
                ('rate', C.POINTER(F)), ('next_mode', C.POINTER(I)),
                ('previous', C.POINTER(S)), ('open', C.POINTER(I)),
                ('targets', C.POINTER(PAIR)), ('points', C.POINTER(PAIR)),
                ('choices', C.POINTER(I)), ('alternate_point', C.POINTER(I)),
                ('width', C.POINTER(F)), ('seed', C.POINTER(U))]


Key = C.CFUNCTYPE(I, P, U, U, C.POINTER(U), P)
Voice = C.CFUNCTYPE(I, P, U, U, I, P)
Eyes = C.CFUNCTYPE(I, P, U, P)
Request = C.CFUNCTYPE(I, P, I, P)
Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Time = C.CFUNCTYPE(I, P, I, C.POINTER(Timing), P)
Pick = C.CFUNCTYPE(I, P, C.POINTER(F), I, C.POINTER(I), P)
Choose = C.CFUNCTYPE(I, P, C.POINTER(I), C.POINTER(I), P)
Action = C.CFUNCTYPE(I, P, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('key', Key), ('present', Status), ('status', Status),
                ('voice', Voice), ('eyes', Eyes), ('request', Request),
                ('restart', Request), ('active', Active), ('timing', Time),
                ('target', Target), ('camera', CameraCall), ('pick', Pick),
                ('choose', Choose), ('action', Action)]


STATE_ADDR = [0x54e284, 0x54e288, 0x54e28c, 0x6afd40, 0x6afd34,
              0x6bbe1c, 0x6afd30]
EXTRA = [('substate', 0x6bbe34), ('latches', 0x6bbe2c), ('rate', 0x54cd04),
         ('next_mode', 0x719b20), ('previous', 0x721ad4), ('open', 0x72210c),
         ('targets', 0x721f90), ('points', 0x7220c8), ('choices', 0x7220e4),
         ('alternate_point', 0x6afd38), ('width', 0x7389e8), ('seed', 0x58edd8),
         ('target', NODE + 0xf0), ('active', ACTOR + 0x140)]
SHARED = ['frame', 'control', 'aux', 'camera', 'state', 'input', 'timings'] + [n for n, _ in EXTRA]


class Fixture:
    def clone(self):
        out = Fixture()
        for name in SHARED:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'loaded', 'hr']:
            setattr(out, name, list(getattr(self, name)))
        for name in ['key_mask', 'pick', 'chosen', 'menu', 'camera_result',
                     'mutate_at', 'seconds']:
            setattr(out, name, getattr(self, name))
        return out

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in SHARED) + (
            tuple(self.present), tuple(self.playing))

    def key(self, code, mode):
        bit = [0, 1, 0x5a, 0x33450].index(code) + mode * 4
        return 0x800100 | (1 if self.key_mask & (1 << bit) else 0)

    def effect(self, event, number):
        kind = event[0]
        if kind == 'voice':
            slot = event[2]
            self.present[slot] = self.loaded[slot]
            self.playing[slot] = bool(self.present[slot])
        elif kind in ('request', 'restart'):
            self.active.value = event[1]
        elif kind == 'camera':
            self.camera.matrix[12] = 31 + number
            self.camera.yaw = F(self.camera.yaw + 3.25).value
        elif kind == 'pick':
            self.frame.camera_cached = self.pick[1]
        elif kind == 'choose':
            self.frame.camera_event = self.menu
        elif kind == 'action':
            self.points[0][:] = [-113, 117]
        if self.mutate_at == number:
            self.frame.group = (self.frame.group + 1) % 5
            self.frame.camera_clip = (self.frame.camera_clip + 1) % 3
            self.previous.value = 8 if self.previous.value == 24 else 24
            self.target[:] = [19.25, -6.5, 31.75]
            self.control.toggles[7] ^= 1
            self.control.toggles[1] = 17
            self.rate.value = .7
            self.active.value = (self.active.value + 3) % 17
            self.aux.pending = [0, 2, 3, 10][number % 4]
            self.camera.pitch = -13.25


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x300b024, 0x300d100)
        for slot in range(2):
            self.word(0x300a000 + slot * 32, 0x300b000)
        for address in [0x42cf0e, 0x4e0b7e, 0x4e0ecb, 0x300d100, 0x4b76c2,
                        0x47d9ee, 0x4dfb96, 0x4018c8, 0x401f71,
                        0x47c334, 0x47cb41, 0x47cd36]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def spans(self, s):
        for name, address in FIELDS + BYTES:
            typ = dict(Frame._fields_)[name]
            yield address, C.addressof(s.frame) + getattr(Frame, name).offset, C.sizeof(typ)
        for address, value in [(0x721e14, s.frame.camera_values),
                               (0x709fcc, s.frame.camera_table),
                               (0x71b41c, s.control.saved_camera),
                               (0x7220f8, s.control.toggles),
                               (0x71b3dc, s.camera.matrix)]:
            yield address, C.addressof(value), C.sizeof(value)
        for name, address in [('yaw', 0x71b364), ('pitch', 0x71b368),
                              ('radius', 0x71b36c), ('height', 0x71b370)]:
            yield address, C.addressof(s.camera) + getattr(Camera, name).offset, 4
        for (name, typ), address in zip(Auxiliary._fields_, ADDR):
            yield address, C.addressof(s.aux) + getattr(Auxiliary, name).offset, C.sizeof(typ)
        for (name, typ), address in zip(State._fields_, STATE_ADDR):
            yield address, C.addressof(s.state) + getattr(State, name).offset, C.sizeof(typ)
        for name, address in EXTRA:
            value = getattr(s, name)
            yield address, C.addressof(value), C.sizeof(value)
        for slot, timing in enumerate(s.timings):
            yield ACTOR + 0x1e8 + slot * 156, C.addressof(timing) + Timing.end.offset, 4
            yield ACTOR + 0x1f0 + slot * 156, C.addressof(timing) + Timing.source.offset, 4

    def write(self, s):
        for address, source, count in self.spans(s):
            self.u.mem_write(address, C.string_at(source, count))
        for slot, present in enumerate(s.present):
            self.word(0x722334 + slot * 0x120,
                      0x300a000 + slot * 32 if present else 0)
        self.word(0x721b28, ACTOR)
        self.word(0x721ef4, NODE)

    def read(self):
        s = self.current.clone()
        for address, destination, count in self.spans(s):
            C.memmove(destination, bytes(self.u.mem_read(address, count)), count)
        return s

    def hook(self, uc, address, size, context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        words = struct.unpack('<7I', uc.mem_read(sp, 28))
        ret, first, second, third = words[:4]
        result, pop, event = 0, 4, None
        s = self.read()
        if address == 0x42cf0e:
            s.camera.fov = struct.unpack('<f', uc.mem_read(sp + 4, 4))[0]
        elif address in (0x4e0b7e, 0x4e0ecb):
            assert first == 0x71af38
            kind = int(address == 0x4e0ecb)
            event = ('camera', kind, I(second).value if kind else 0,
                     tuple(words[3:6]) if kind else (0, 0, 0), words[6] if kind else 0)
            result = s.camera_result
        elif address == 0x4b76c2:
            assert third == 0
            event, result = ('key', first, second), s.key(first, second)
        elif address == 0x300d100:
            slot = (first - 0x300a000) // 32
            assert slot in (0, 1) and s.present[slot]
            event = ('status', slot)
            self.word(second, 1 if s.playing[slot] else 2)
            result, pop = s.hr[slot] & 0xffffffff, 12
        elif address == 0x47d9ee:
            event = ('voice', first, second, I(third).value)
        elif address == 0x4dfb96:
            assert third == 1
            s.aux.expression_a, s.aux.expression_b = I(first).value, I(second).value
            event = ('eyes', third)
        elif address in (0x4018c8, 0x401f71):
            assert first == ACTOR
            event = ('request' if address == 0x4018c8 else 'restart', I(second).value)
        elif address == 0x47c334:
            event = ('pick', tuple(struct.unpack('<2f', uc.mem_read(sp + 4, 8))), I(third).value)
            result = s.pick[0]
        elif address == 0x47cb41:
            event, result = ('choose', (I(first).value, I(second).value)), s.chosen
        elif address == 0x47cd36:
            event = ('action',)
        else:
            raise AssertionError(hex(address))
        if event is not None:
            self.trace.append((event, s.snapshot()))
            s.effect(event, len(self.trace))
        self.current = s
        self.write(s)
        uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        uc.reg_write(UC_X86_REG_ESP, sp + pop)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture):
        self.current, self.trace = fixture.clone(), []
        self.write(self.current)
        self.u.mem_write(0x733700, struct.pack('<f', fixture.seconds))
        self.call(0x47a5d0, bytes(fixture.input))
        return self.trace, self.read().snapshot()


def bind(lib):
    lib.bk_ending_secondary_control_step.argtypes = [C.POINTER(State),
        C.POINTER(Bindings), C.POINTER(Input), F, C.POINTER(Ops), P]
    lib.bk_ending_secondary_control_initial.restype = State
    lib.bk_ending_secondary_control_camera.argtypes = [U, U]
    lib.bk_ending_secondary_control_camera.restype = C.POINTER(F)


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
    def key(_c, code, mode, out, _e):
        out[0] = s.key(code, mode)
        return commit(('key', code, mode))

    @Status
    def present(_c, slot, out, _e):
        out[0] = s.present[slot]
        return 1

    @Status
    def status(_c, slot, out, _e):
        out[0] = bool(s.playing[slot] and not s.hr[slot])
        return commit(('status', slot))

    @Voice
    def voice(_c, cue, slot, flags, _e):
        return commit(('voice', cue, slot, flags))

    @Eyes
    def eyes(_c, slot, _e):
        return commit(('eyes', slot))

    @Request
    def request(_c, clip, _e):
        return commit(('request', clip))

    @Request
    def restart(_c, clip, _e):
        return commit(('restart', clip))

    @Active
    def active(_c, out, _e):
        out[0] = s.active.value
        return 1

    @Time
    def timing(_c, slot, out, _e):
        if not 0 <= slot < 128:
            return 0
        out[0] = s.timings[slot]
        return 1

    @Target
    def target(_c, out, _e):
        for index in range(3):
            out[index] = s.target[index]
        return 1

    @CameraCall
    def camera(_c, kind, choice, offset, extra, out, _e):
        out[0] = s.camera_result
        return commit(('camera', kind, choice if kind else 0,
                       tuple(offset[:3]) if kind else (0, 0, 0), extra if kind else 0))

    @Pick
    def pick(_c, point, mode, out, _e):
        out[0] = s.pick[0]
        return commit(('pick', tuple(point[:2]), mode))

    @Choose
    def choose(_c, point, out, _e):
        out[0] = s.chosen
        return commit(('choose', tuple(point[:2])))

    @Action
    def action(_c, _e):
        return commit(('action',))

    bindings = Bindings(C.pointer(s.frame), C.pointer(s.control), C.pointer(s.aux),
        C.pointer(s.camera), C.pointer(s.substate), s.latches, C.pointer(s.rate),
        C.pointer(s.next_mode), C.pointer(s.previous), C.pointer(s.open),
        s.targets, s.points, s.choices, s.alternate_point, C.pointer(s.width), C.pointer(s.seed))
    ops = Ops(None, key, present, status, voice, eyes, request, restart,
              active, timing, target, camera, pick, choose, action)
    ok = lib.bk_ending_secondary_control_step(C.byref(s.state), C.byref(bindings),
        C.byref(s.input), s.seconds, C.byref(ops), error)
    return ok, trace, s.snapshot(), error.value


def fixture(rng, case):
    s = Fixture()
    s.frame, s.control, s.aux, s.camera = Frame(), Control(), Auxiliary(), Camera()
    s.state, s.input = State(), Input()
    s.substate, s.latches = B(rng.choice([0, 1, 2, 3, 4, 5, 7, 255])), (I * 2)(0, 1)
    s.rate = F(rng.choice([.3, .4, .6, .9]))
    s.next_mode, s.previous, s.open = I(9), S(rng.choice([0, 8, 24])), I(rng.randrange(2))
    s.targets, s.points, s.choices = (PAIR * 39)(), (PAIR * 3)(), (I * 3)()
    s.alternate_point, s.width = PAIR(777, -333), F(rng.choice([0, 2, 40, 256]))
    s.seed, s.target, s.active = U(rng.getrandbits(32)), (F * 3)(1.25, -3.5, 79.25), I(rng.randrange(17))
    s.timings = (Timing * 128)()
    for slot, timing in enumerate(s.timings):
        timing.end = 100 + slot * 2
        timing.source = rng.choice([0, timing.end - 1, timing.end, timing.end + 2])
    for i, point in enumerate(s.targets):
        point[:] = [i * 17 - 80, i * 31 + 200]
    for i, point in enumerate(s.points):
        point[:] = [100 + i * 200, 300]
        s.choices[i] = rng.randrange(7) - 1
    s.frame.phase, s.frame.group, s.frame.state_721ee4 = 2, case % 5, case % 9
    s.frame.camera_mode, s.frame.camera_clip = rng.choice([0, 0, 1, 4]), rng.randrange(3)
    s.frame.camera_cached, s.frame.camera_event = rng.randrange(39), rng.randrange(4)
    s.frame.transition_action, s.frame.curtain_wanted = 37, 0
    for row in s.frame.camera_table:
        row[:] = [rng.getrandbits(32) for _ in range(4)]
    for i in range(8):
        s.control.toggles[i] = rng.choice([0, 1, 255])
    s.camera.pose.world[:], s.camera.matrix[:] = IDENTITY, IDENTITY
    s.camera.yaw, s.camera.pitch, s.camera.radius, s.camera.height = 32.5, -16.25, 20, 4
    s.camera.fov = .4
    s.control.saved_camera[:] = [float(i) for i in range(16)]
    s.aux.expression_a, s.aux.expression_b, s.aux.index = 17, 19, 53
    s.aux.pending, s.aux.gate = rng.choice([0, 1, 2, 3, 10, 11]), 71
    s.state.remaining = rng.choice([-0x80000000, -1, 0, 1, 2, 15, 0x7fffffff])
    s.state.alternate, s.state.automatic, s.state.chance = rng.randrange(2), rng.randrange(4), 31
    s.state.fov = rng.choice([.1, .2, .21, .3, 1, 2])
    s.state.saved_camera[:], s.state.saved_toggle = [27.25, -8.5, 77.5, 2.25], 13
    s.present = [rng.choice([0, 1, 1]), rng.randrange(2)]
    s.playing = [bool(s.present[i] and rng.randrange(2)) for i in range(2)]
    s.loaded, s.hr = [rng.randrange(2) for _ in range(2)], [rng.choice([0, 0, -1]) for _ in range(2)]
    s.key_mask = rng.randrange(1 << 16)
    s.pick, s.chosen, s.menu = (rng.choice([0, 1, 2, 256]), rng.randrange(39)), rng.choice([-1, 0, 1]), rng.choice([1, 2])
    s.camera_result, s.mutate_at = rng.choice([0, 1, 256, 257]), rng.randrange(1, 6) if case % 4 == 0 else 0
    s.seconds = F(rng.choice([0, 1 / 60, .1, .25, 1, 30])).value
    s.input.words[:] = [rng.getrandbits(32) for _ in range(11)]
    if s.frame.state_721ee4 == 3:
        index = (case // 9) % 4
        s.key_mask |= 1 << 12
        if index < 3:
            s.input.words[9], s.input.words[10] = s.points[index][:]
            s.input.words[9] += rng.choice([0, 0, 0, int(s.width.value / 2), max(0, int(s.width.value / 2) - 1)])
        s.frame.camera_event = 1 + (case // 36) % 2
        if s.frame.camera_event == 2:
            s.choices[:] = [rng.choice([4, 5]), rng.choice([4, 5]), 6]
    elif s.frame.state_721ee4 == 4:
        s.substate.value = (case // 9) % 8
    elif s.frame.state_721ee4 == 5:
        s.active.value = 12
        s.state.remaining = rng.choice([0, 1, 2, 20])
    elif s.frame.state_721ee4 == 6:
        s.substate.value = (case // 9) % 4
        s.active.value = rng.choice([13, 14, 15, 16])
    elif s.frame.state_721ee4 == 7:
        s.state.remaining = rng.choice([-1, 0, 1, 2])
        s.active.value = rng.choice([13, 14, 15])
    return s


def compare(case, expected, got, label):
    assert len(expected) == len(got), (case, label, 'length')
    for index, (a, b) in enumerate(zip(expected, got)):
        assert a == b, (case, label, SHARED[index] if index < len(SHARED) else index, a, b)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=15000)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == EXE_HASH
    native, lib = Native(exe), library()
    bind(lib)
    initial = lib.bk_ending_secondary_control_initial()
    for (name, typ), address in zip(State._fields_, STATE_ADDR):
        assert C.string_at(C.addressof(initial) + getattr(State, name).offset, C.sizeof(typ)) == bytes(native.u.mem_read(address, C.sizeof(typ))), name
    for group in range(5):
        for row in range(3):
            values = lib.bk_ending_secondary_control_camera(group, row)
            assert C.string_at(values, 16) == bytes(native.u.mem_read(0x54e194 + group * 48 + row * 16, 16))
    assert not lib.bk_ending_secondary_control_camera(5, 0)
    assert not lib.bk_ending_secondary_control_camera(0, 3)
    rng = random.Random(0x47a5d0)
    branches, services, mutations, failures, rejections = {}, {}, 0, 0, 0
    for case in range(args.cases):
        s = fixture(rng, case)
        expected, final = native.run(s)
        ok, actual, result, error = portable(lib, s)
        assert ok, (case, error)
        assert len(expected) == len(actual), (case, 'call count', [v[0] for v in expected], [v[0] for v in actual])
        for index, (want, got) in enumerate(zip(expected, actual)):
            assert want[0] == got[0], (case, index, 'event', want[0], got[0])
            compare(case, want[1], got[1], (index, want[0]))
        compare(case, final, result, 'final')
        state = str(s.frame.state_721ee4)
        branches[state] = branches.get(state, 0) + 1
        for event, _ in expected:
            services[event[0]] = services.get(event[0], 0) + 1
        mutations += 0 < s.mutate_at <= len(expected)
        if expected and case % 7 == 0:
            stop = 1 + case % len(expected)
            ok, prefix, stopped, _ = portable(lib, s, stop)
            assert not ok and prefix == expected[:stop], (case, 'failure call prefix')
            compare(case, expected[stop - 1][1], stopped, 'failure state prefix')
            failures += 1
        if case % 100 == 0:
            for invalid in [-1, math.nan, math.inf]:
                s.seconds = invalid
                ok, calls, stopped, _ = portable(lib, s)
                assert not ok and not calls and stopped == s.snapshot()
                rejections += 1
        if case and case % 3000 == 0:
            print('PASS secondary parent cases', case, flush=True)
    report = dict(passed=True, exe_sha256=EXE_HASH, frames=args.cases,
                  branches=branches, service_calls=services,
                  callback_mutations=mutations, failure_prefixes=failures,
                  invalid_time_rejections=rejections, max_error=0, scope=__doc__)
    (ROOT / 'local/original-ending-secondary-control.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS', json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
