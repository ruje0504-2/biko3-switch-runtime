"""Complete original4dd280,4df32c and the two parent filename builders.

Runs original control instructions, RNG, timing comparisons/writes, preference
table, string selectors and buffer-presence branches. Project/warp, model
requests/find, expression/eyes, audio and recoil have explicit observing and
mutating fixture services. Their numeric implementations have separate tests;
this validates composition and failure prefixes, not natural gameplay or GPU.
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
from original_ending_normal_control_oracle import (
    Bindings as ParentBindings, Ops as ParentOps, Key, Status, Slot, Load, Play,
    Actor, f32_neighbors)
from model_binding import ROOT, library

I, U, F, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_int8, C.c_void_p


class State(C.Structure):
    _fields_ = [('clip', I), ('increment', I)]


class Timing(C.Structure):
    _fields_ = [('source', F), ('end', F)]


class Bindings(C.Structure):
    _fields_ = [('normal', ParentBindings), ('contact', C.POINTER(I)),
                ('side', C.POINTER(S)), ('targets', C.POINTER(I * 2)),
                ('reference', C.POINTER(U)), ('node', C.POINTER(U)),
                ('flip', C.POINTER(I)), ('effect_volume', C.POINTER(I))]


Project = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
TimingCall = C.CFUNCTYPE(I, P, I, C.POINTER(Timing), P)
Source = C.CFUNCTYPE(I, P, I, F, P)
Find = C.CFUNCTYPE(I, P, C.c_char_p, C.POINTER(U), P)
Warp = C.CFUNCTYPE(I, P, F, F, P)
Recoil = C.CFUNCTYPE(I, P, U, F, I, U, I, C.POINTER(I), P)


class Ops(C.Structure):
    _fields_ = [('normal', ParentOps), ('project', Project),
                ('timing', TimingCall), ('source', Source), ('instant', Actor),
                ('find', Find), ('warp', Warp), ('recoil', Recoil)]


EXTRA = [('state', 0x709db0), ('increment', 0x709dac), ('ready', 0x719b0c),
         ('target', 0x719444), ('inputs', 0x709c70), ('name', 0x722224),
         ('seed', 0x58edd8), ('contact', 0x721ed4), ('side', 0x719b4c),
         ('targets', 0x721f90), ('reference', 0x719b28), ('node', 0x719b2c),
         ('flip', 0x714fa8), ('effect_volume', 0xbe9a10), ('volume', 0xbe9a08)]
SHARED = ['frame', 'control', 'aux', 'state', 'ready', 'target', 'inputs',
          'name', 'seed', 'contact', 'side', 'targets', 'reference', 'node',
          'flip', 'effect_volume', 'volume', 'timings', 'input']
NODE_NAMES = [b'A_nip_L', b'L_nip', b'A_nip_R', b'R_nip',
              b'A_siri_L', b'L_siri', b'A_siri_R', b'R_siri']


class Fixture:
    def clone(self):
        out = type(self)()
        for name in SHARED + ['projection']:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'loaded', 'hr']:
            setattr(out, name, list(getattr(self, name)))
        for name in ['requested', 'seconds', 'key_result', 'actor_exists',
                     'recoil_result', 'missing_nodes', 'mutate_at', 'mutated_pending']:
            setattr(out, name, getattr(self, name))
        return out

    def snapshot(self):
        return tuple(bytes(getattr(self, n)) for n in SHARED) + (
            tuple(self.present), tuple(self.playing))

    def find(self, name):
        index = NODE_NAMES.index(name)
        return 0 if self.missing_nodes & (1 << index) else 0x100 + index

    def effect(self, event, number):
        if event[0] == 'project':
            self.targets[event[1]][:] = self.projection
        elif event[0] == 'load':
            self.present[event[1]] = self.loaded[event[1]]
            self.playing[event[1]] = False
        elif event[0] == 'play':
            self.playing[event[1]] = bool(self.present[event[1]])
        elif event[0] == 'pause':
            self.playing[event[1]] = False
        if number == self.mutate_at:
            self.frame.group = (self.frame.group + 1) % 5
            self.frame.camera_cached = 9
            self.state.clip = (self.state.clip + 3) % 120
            self.aux.progress = .59
            self.aux.pending = self.mutated_pending
            self.control.toggles[7] ^= 1
            self.volume.value = -1931
            self.effect_volume.value = -971


class Native(Base):
    primary, auxiliary = 0x3000000, 0x3007000

    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f2f0, 0x300d010)
        self.word(0x53f214, 0x300d020)
        self.word(0x300b024, 0x300d100)
        self.word(0x300b048, 0x300d110)
        for slot in range(6):
            self.word(0x300a000 + 32 * slot, 0x300b000)
        for address in [0x300d010, 0x300d020, 0x300d100, 0x300d110,
                        0x42d4b6, 0x4b768d, 0x4e0956, 0x4ad2bf, 0x4dfb96,
                        0x4018c8, 0x401d24, 0x423a99, 0x425904, 0x4b76c2,
                        0x49b900, 0x49b28f]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
        self.u.hook_add(UC_HOOK_CODE, self.observe_voice,
                        begin=0x4e0032, end=0x4e0032)

    def observe_voice(self, uc, _address, _size, _context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        mode = struct.unpack('<i', uc.mem_read(sp + 8, 4))[0]
        target = struct.unpack('<i', uc.mem_read(0x721ed0, 4))[0]
        if target == 1 and mode not in (0, 1):
            #4e0032 leaves its cue local unwritten for this combination.
            # Keep executing it as evidence; do not assign the residual
            # stack value a portable meaning or filter away the test case.
            self.undefined_voice = (len(self.trace), self.read().snapshot())

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def string(self, address):
        return bytes(self.u.mem_read(address, 64)).split(b'\0')[0]

    def spans(self, s):
        for name, address in FIELDS + BYTES:
            typ = dict(Frame._fields_)[name]
            yield address, C.addressof(s.frame) + getattr(Frame, name).offset, C.sizeof(typ)
        yield 0x721e14, C.addressof(s.frame.camera_values), 12
        yield 0x709fcc, C.addressof(s.frame.camera_table), 80
        yield 0x7220f8, C.addressof(s.control.toggles), 8
        for (name, typ), address in zip(Auxiliary._fields_, ADDR):
            yield address, C.addressof(s.aux) + getattr(Auxiliary, name).offset, C.sizeof(typ)
        for name, address in EXTRA:
            if name == 'increment':
                yield address, C.addressof(s.state) + State.increment.offset, 4
            elif name == 'state':
                yield address, C.addressof(s.state) + State.clip.offset, 4
            else:
                value = getattr(s, name)
                yield address, C.addressof(value), C.sizeof(value)
        for slot, timing in enumerate(s.timings):
            yield self.primary + slot * 0x9c + 0x1f0, C.addressof(timing) + Timing.source.offset, 4
            yield self.primary + slot * 0x9c + 0x1e8, C.addressof(timing) + Timing.end.offset, 4

    def write(self, s):
        for address, source, size in self.spans(s):
            self.u.mem_write(address, C.string_at(source, size))
        self.word(0x721b28, self.primary)
        self.word(0x721b2c, self.auxiliary if s.actor_exists else 0)
        self.word(self.primary + 0x160, 0x3006000)
        self.word(0x3006014, 0x3006100)
        self.word(self.auxiliary + 0x160, 0x3007200)
        self.word(0x3007214, 0x3007300)
        for index in range(39):
            self.word(0x721ef4 + index * 4, 0x3008000 + index * 64)
        for slot in range(6):
            self.word(0x722334 + slot * 0x120,
                      0x300a000 + slot * 32 if s.present[slot] else 0)

    def read(self):
        s = self.current.clone()
        for address, target, size in self.spans(s):
            C.memmove(target, bytes(self.u.mem_read(address, size)), size)
        return s

    def hook(self, uc, address, _size, _context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        words = struct.unpack('<7I', uc.mem_read(sp, 28))
        ret, first, second, third = words[:4]
        pop, result = 4, 0
        if address == 0x300d010:
            fmt = self.string(second)
            count = fmt.count(b'%')
            assert 1 <= count <= 3, fmt
            args = struct.unpack('<' + 'i' * count, uc.mem_read(sp + 12, 4 * count))
            name = fmt % args
            uc.mem_write(first, name + b'\0')
            result = len(name)
        elif address == 0x300d020:
            uc.mem_write(first, self.string(second) + b'\0')
            result, pop = first, 12
        else:
            s = self.read()
            write_after = None
            if address == 0x42d4b6:
                index = (second - 0x721f90) // 8
                assert 0 <= index < 39 and first == 0x3008000 + index * 64 + 0xc0
                event = ('project', index)
            elif address == 0x4b768d:
                event = ('warp', tuple(struct.unpack('<2f', uc.mem_read(sp + 4, 8))))
            elif address == 0x4e0956:
                assert second in (0, 1)
                event = ('load', second, self.string(first))
                self.last_loaded = second
            elif address == 0x4ad2bf:
                slot = ((first - 0x300a000) // 32 if first else
                        5 if ret == 0x4dd72c else self.last_loaded)
                assert slot in (0, 1, 5)
                event = ('play', slot, I(second).value, I(third).value)
            elif address in (0x300d100, 0x300d110):
                slot = (first - 0x300a000) // 32
                assert slot in (0, 1, 5) and s.present[slot]
                if address == 0x300d100:
                    event = ('status', slot)
                    uc.mem_write(second, struct.pack('<I', 1 if s.playing[slot] else 2))
                    result, pop = s.hr[slot] & 0xffffffff, 12
                else:
                    event, pop = ('pause', slot), 8
            elif address == 0x4dfb96:
                assert third == 1
                s.aux.expression_a, s.aux.expression_b = I(first).value, I(second).value
                event = ('eyes', third)
            elif address in (0x4018c8, 0x401d24):
                assert first in (self.primary, self.auxiliary)
                event = ('instant' if address == 0x401d24 else 'request',
                         int(first == self.auxiliary), I(second).value)
            elif address == 0x423a99:
                assert first == 0x3007300
                event = ('root', 1, I(second).value)
            elif address == 0x425904:
                assert first == 0x3006100 and third in (0x719b28, 0x719b2c)
                name = self.string(second)
                event = ('find', name)
                write_after = ('reference' if third == 0x719b28 else 'node', s.find(name))
            elif address == 0x4b76c2:
                assert (first, second, third) == (0, 0, 0)
                event, result = ('key', 0, 0), s.key_result
            elif address in (0x49b900, 0x49b28f):
                kind = int(address == 0x49b900)
                assert first == kind and second == 0x714fac
                event = ('recoil', kind, struct.unpack('<f', struct.pack('<I', third))[0],
                         I(words[4]).value, words[5], I(words[6]).value)
                result = s.recoil_result
            else:
                raise AssertionError(hex(address))
            self.trace.append((event, s.snapshot()))
            s.effect(event, len(self.trace))
            if write_after:
                getattr(s, write_after[0]).value = write_after[1]
            self.current = s
            self.write(s)
        uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        uc.reg_write(UC_X86_REG_ESP, sp + pop)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture):
        self.current = fixture.clone()
        self.write(self.current)
        self.trace, self.last_loaded, self.undefined_voice = [], 0, None
        self.u.mem_write(0x733700, struct.pack('<f', fixture.seconds))
        self.call(0x4dd280, struct.pack('<i', fixture.requested) + bytes(fixture.input))
        return self.u.reg_read(UC_X86_REG_EAX), self.trace, self.read().snapshot()


def portable(lib, fixture, fail_at=0):
    s, trace, error = fixture.clone(), [], C.create_string_buffer(256)
    result = U(99)

    def commit(event):
        trace.append((event, s.snapshot()))
        if len(trace) == fail_at:
            return 0
        s.effect(event, len(trace))
        return 1

    @Key
    def key(_c, code, mode, out, _e):
        assert (code, mode) == (0, 0)
        out[0] = s.key_result
        return commit(('key', code, mode))

    @Status
    def present(_c, slot, out, _e):
        out[0] = s.present[slot]
        return 1

    @Status
    def status(_c, slot, out, _e):
        out[0] = bool(s.playing[slot] and not s.hr[slot])
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

    @Project
    def project(_c, index, out, _e):
        ok = commit(('project', index))
        if ok:
            out[0], out[1] = s.projection
        return ok

    @TimingCall
    def timing(_c, clip, out, _e):
        assert 0 <= clip < 128
        out[0] = s.timings[clip]
        return 1

    @Source
    def source(_c, clip, value, _e):
        assert 0 <= clip < 128
        s.timings[clip].source = value
        return 1

    @Actor
    def instant(_c, actor, clip, _e):
        return commit(('instant', actor, clip))

    @Find
    def find(_c, name, out, _e):
        result = s.find(name)
        ok = commit(('find', name))
        if ok:
            out[0] = result
        return ok

    @Warp
    def warp(_c, x, y, _e):
        return commit(('warp', (x, y)))

    @Recoil
    def recoil(_c, kind, degrees, flip, ms, reset, out, _e):
        out[0] = s.recoil_result
        return commit(('recoil', kind, degrees, flip, ms, reset))

    parent = ParentBindings(frame=C.pointer(s.frame), control=C.pointer(s.control),
                            auxiliary=C.pointer(s.aux), ready=C.pointer(s.ready),
                            target=C.pointer(s.target), inputs=s.inputs,
                            name=C.addressof(s.name), random=C.pointer(s.seed),
                            volume=C.pointer(s.volume))
    b = Bindings(parent, C.pointer(s.contact), C.pointer(s.side), s.targets,
                 C.pointer(s.reference), C.pointer(s.node), C.pointer(s.flip),
                 C.pointer(s.effect_volume))
    parent_ops = ParentOps(key=key, present=present, status=status, pause=pause,
                          load=load, play=play, eyes=eyes, request=request,
                          actor_present=actor_present, root_flag=root)
    ops = Ops(parent_ops, project, timing, source, instant, find, warp, recoil)
    ok = lib.bk_ending_normal_action_step(C.byref(s.state), C.byref(b),
        s.requested, C.byref(s.input), s.seconds, C.byref(ops), C.byref(result), error)
    return ok, result.value, trace, s.snapshot(), error.value


def fixture(rng, case):
    s = Fixture()
    s.frame, s.control, s.aux, s.state = Frame(), Control(), Auxiliary(), State()
    s.frame.phase, s.frame.group, s.frame.state_721ee0 = 1, case % 5, 3
    choices = [1, 11, 12, 9, 10, 26, 27, 5]
    s.frame.camera_cached = choices[(case // 9) % 8]
    s.frame.camera_mode, s.frame.camera_request = 4, 7
    s.control.toggles[:] = [rng.choice([0, 1, 255]) for _ in range(8)]
    s.aux.progress = F(rng.choice([0, .1, .19, .2, .39, .4, .59, .6, .9])).value
    s.aux.pending = rng.choice([0, 3, 3, 4, 5, 6, 6, 7, 9])
    s.aux.index, s.aux.expression_a, s.aux.expression_b = 11, 17, 19
    s.state.clip, s.state.increment = rng.randrange(90), rng.randrange(5)
    s.ready = I([0, 1, 2, 3, 4, 5, 6, 7, -1][case % 9])
    s.target, s.contact, s.side = I(rng.randrange(7)), I(rng.randrange(7) * 2), S(7)
    s.inputs = (I * 14)(*[rng.randrange(3) for _ in range(14)])
    s.name = (C.c_char * 32).from_buffer_copy(b'prior.wav\0' + b'Q' * 22)
    s.seed = U(rng.getrandbits(32))
    s.targets = ((I * 2) * 39)(*[(I * 2)(rng.randrange(-400, 400),
                                       rng.randrange(-400, 400)) for _ in range(39)])
    s.projection = (I * 2)(rng.choice([-2147483648, -400, 0, 200, 2147483647]),
                           rng.choice([-2147483648, -400, 0, 200, 2147483647]))
    s.reference, s.node = U(0x119), U(0x11a)
    s.flip, s.effect_volume, s.volume = I(rng.choice([-1, 0, 1])), I(-400), I(-900)
    s.timings = (Timing * 128)()
    for i, t in enumerate(s.timings):
        t.end = F(i * 3.25 + .5).value
        t.source = F(t.end + rng.choice([-2, -1, -.5, 0, 1, 10])).value
    s.input = Input()
    for i in range(11):
        s.input.words[i] = rng.getrandbits(32)
    s.input.words[6], s.input.words[7] = [
        rng.choice([0, 0, 1, 2, 3, 5, 9, 17, 1000, 0xffffffff, 0x80000000])
        for _ in range(2)]
    s.requested, s.seconds = rng.randrange(100), F(rng.choice([0, 1 / 60, .1, 1, 30])).value
    s.key_result = rng.choice([0x400, 0x401, 0xff])
    s.actor_exists, s.recoil_result = rng.randrange(2), rng.choice([0, 1, 256])
    s.missing_nodes = rng.randrange(256)
    s.present = [rng.choice([0, 1, 1]) for _ in range(6)]
    s.playing = [bool(s.present[i] and rng.randrange(2)) for i in range(6)]
    s.loaded = [rng.choice([0, 1, 1]) for _ in range(6)]
    s.hr = [rng.choice([0, 0, -1]) for _ in range(6)]
    s.mutate_at = rng.randrange(1, 11) if case % 4 == 0 else 0
    s.mutated_pending = rng.choice([3, 4, 5, 6, 7, 9])
    return s


def compare(case, actual, expected):
    assert len(actual) == len(expected), (case, 'call count',
        [x[0] for x in actual], [x[0] for x in expected])
    for step, (a, z) in enumerate(zip(actual, expected)):
        assert a[0] == z[0], (case, step, 'event', a[0], z[0])
        snapshot_equal((case, step, a[0]), a[1], z[1])


def snapshot_equal(label, a, z):
    for index, (av, zv) in enumerate(zip(a, z)):
        assert av == zv, (label, SHARED[index] if index < len(SHARED) else index,
                         av, zv)


def selectors(lib, native):
    lib.bk_ending_sound_loop_name.argtypes = [U, I, F, P, P]
    lib.bk_ending_sound_action_name.argtypes = [U, I, F, P, P]
    lib.bk_ending_normal_increment.argtypes = [C.POINTER(U), F, C.POINTER(I), P]
    lib.bk_ending_normal_preference.argtypes = [U, U]
    error = C.create_string_buffer(256)
    rng = random.Random(0x4df32c)
    fixture_ = fixture(rng, 0)
    native.current, native.trace = fixture_, []
    native.write(fixture_)
    values = [-1, 0, .1, 1, 100]
    for value in [.19, .2, .39, .4, .59, .6]:
        values.extend(f32_neighbors(value))
    names, deltas = 0, 0
    for case in range(4600):
        progress = F(values[case % len(values)]).value
        seed, result = U(rng.getrandbits(32)), I(99)
        native.word(0x58edd8, seed.value)
        native.u.mem_write(0x721e20, struct.pack('<f', progress))
        native.call(0x4df32c, b'')
        expected_seed = struct.unpack('<I', native.u.mem_read(0x58edd8, 4))[0]
        expected_result = native.u.reg_read(UC_X86_REG_EAX)
        assert lib.bk_ending_normal_increment(C.byref(seed), progress, C.byref(result), error)
        assert (seed.value, result.value) == (expected_seed, expected_result)
        deltas += 1
        group, target = case % 5, [1, 11, 12, 9, 10, 26, 27, 5][case % 8]
        native.word(0x721e00, 1)
        native.u.mem_write(0x721b3c, bytes([group]))
        native.word(0x721ed0, target)
        for kind in (0, 1):
            output = C.create_string_buffer(b'Q' * 32, 33)
            if not kind and progress < F(.19).value:
                assert not lib.bk_ending_sound_loop_name(group, 1, progress, output, error)
                assert output.raw == b'Q' * 32 + b'\0'
                continue
            native.u.mem_write(0x300e000, b'Q' * 33)
            native.call(0x4dfca9 if kind else 0x4dfbbd, struct.pack('<I', 0x300e000))
            expected = native.string(0x300e000)
            fn = lib.bk_ending_sound_action_name if kind else lib.bk_ending_sound_loop_name
            assert fn(group, target if kind else 1, progress, output, error), error.value
            assert output.value == expected, (kind, group, target, progress, output.value, expected)
            assert native.string(0x722224) == expected
            names += 1
    for group in range(5):
        for index in range(14):
            original = struct.unpack('<i', native.u.mem_read(0x57551c + group * 56 + index * 4, 4))[0]
            assert lib.bk_ending_normal_preference(group, index) == original
    return names, deltas


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--cases', type=int, default=7200)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native, lib = Native(exe), library()
    lib.bk_ending_normal_action_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
        I, C.POINTER(Input), F, C.POINTER(Ops), C.POINTER(U), P]
    names, deltas = selectors(lib, native)
    rng = random.Random(0x4dd280)
    states, events, completions, mutations, failures, rejects = {}, {}, 0, 0, 0, 0
    undefined = []
    for case in range(args.cases):
        s = fixture(rng, case)
        result, expected, final = native.run(s)
        ok, returned, actual, got, error = portable(lib, s)
        state = str(s.ready.value)
        states[state] = states.get(state, 0) + 1
        if native.undefined_voice is not None:
            count, prefix = native.undefined_voice
            assert not ok and error == b'ending sound: undefined normal voice selection', (case, error)
            compare((case, 'undefined original cue'), actual, expected[:count])
            snapshot_equal((case, 'undefined original cue'), got, prefix)
            undefined.append(case)
            continue
        assert ok, (case, error)
        assert returned == result, (case, 'result', returned, result)
        compare(case, actual, expected)
        snapshot_equal((case, 'final'), got, final)
        for event, _ in expected:
            events[event[0]] = events.get(event[0], 0) + 1
        completions += bool(result)
        mutations += bool(0 < s.mutate_at <= len(expected))
        if case % 13 == 0:
            stop = 1 + case % len(expected)
            ok, _, trace, snapshot, _ = portable(lib, s, stop)
            assert not ok
            compare((case, 'failure'), trace, expected[:stop])
            snapshot_equal((case, 'failure writes'), snapshot, expected[stop - 1][1])
            failures += 1
        if case % 100 == 0:
            for bad in [-1, math.inf, math.nan]:
                s.seconds = bad
                ok, _, trace, snapshot, _ = portable(lib, s)
                assert not ok and not trace
                snapshot_equal((case, 'invalid time'), snapshot, s.snapshot())
                rejects += 1
    report = dict(passed=True, exe_sha256=digest, frames=args.cases,
                  states=states, service_calls=events, completions=completions,
                  callback_mutations=mutations, failure_prefixes=failures,
                  invalid_time_rejections=rejects, filename_cases=names,
                  action_increment_cases=deltas, preference_words=70,
                  native_uninitialized_voice_cases=undefined,
                  defined_frames=args.cases-len(undefined), scope=__doc__)
    (ROOT / 'local/original-ending-normal-action-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
