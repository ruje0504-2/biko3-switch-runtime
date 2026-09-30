"""Execute the full48CC18 and49717C against the portable gallery effect.

The native thresholds, branching, timer, latches and tables run unchanged.
Only audio leaves and RNG are fixtures. Compare every observable prefix and
retained byte, including live callback mutations. Stop at the first native
uninitialized cue read; the portable side must explicitly reject it there.
This is not a complete phase8 scene or hardware test.
"""
from __future__ import annotations
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_frame_oracle import State as Frame, FIELDS as FRAME_FIELDS, BYTES
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR, Call as AudioCall
from original_ending_tertiary_control_oracle import field_view

I, U, F, B, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_int8, C.c_void_p
PRIMARY = 0x3001000
SOUNDS = [0x300c000 + i*0x100 for i in range(6)]


class State(C.Structure):
    _fields_ = [('alternating', S), ('sampled', I), ('speech_blocked', S), ('speech_elapsed', F)]


class Timing(C.Structure):
    _fields_ = [('start', F), ('end', F), ('source', F)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('event', C.POINTER(B)), ('state', C.POINTER(State)),
                ('voice_volume', C.POINTER(I)), ('effect_volume', C.POINTER(I))]


Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
ReadTiming = C.CFUNCTYPE(I, P, U, C.POINTER(Timing), P)
Audio = C.CFUNCTYPE(I, P, C.POINTER(AudioCall), C.POINTER(I), P)
Random = C.CFUNCTYPE(I, P, C.POINTER(I), P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('active', Active), ('timing', ReadTiming),
                ('audio', Audio), ('random', Random)]


FIELDS = [('alternating', 0x6dde70), ('sampled', 0x6dde78),
          ('speech_blocked', 0x6dde7c), ('speech_elapsed', 0x6dde80)]
SCALARS = [('event', 0x721b3d), ('voice_volume', 0xbe9a08), ('effect_volume', 0xbe9a10)]
OWNERS = ['frame', 'auxiliary', 'state', 'active', 'timings', 'present', 'playing',
          'status_error', 'last_cue', 'last_bank', 'play_counts', 'randoms',
          'random_index'] + [n for n, _ in SCALARS]
CUE_WRITES = [0x48d51c, 0x48d54d, 0x48d57b, 0x48d5a9]
CUE_READS = [0x48d648, 0x48d68c, 0x48d6a5]


def views(f):
    for name, address in FRAME_FIELDS + BYTES:
        yield field_view(f.frame, name, address)
    for name, address in [('camera_values', 0x721e14), ('camera_table', 0x709fcc)]:
        yield field_view(f.frame, name, address)
    for (name, _), address in zip(Auxiliary._fields_, ADDR):
        yield field_view(f.auxiliary, name, address)
    for name, address in FIELDS:
        yield field_view(f.state, name, address)
    for name, address in SCALARS:
        owner = getattr(f, name)
        yield address, C.addressof(owner), C.sizeof(owner)
    yield PRIMARY + 0x140, C.addressof(f.active), 4
    for i in range(32):
        for name, offset in [('start', 0x1e4), ('end', 0x1e8), ('source', 0x1f0)]:
            yield field_view(f.timings[i], name, PRIMARY + offset + i*156)


class Fixture:
    def clone(self):
        f = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(f, name, type(value).from_buffer_copy(value))
        f.seconds, f.mutate_at, f.mutations = self.seconds, self.mutate_at, self.mutations
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in OWNERS)

    def effect(self, event, index):
        if event[0] == 'random': self.random_index.value += 1
        elif event[0] == 'audio':
            _, kind, slot, cue, bank, _flags, _volume = event
            if kind == 4:
                self.present[slot] = 1
                self.last_cue.value, self.last_bank.value = cue, bank
            if kind in [2, 4] and self.present[slot]:
                self.playing[slot] = 1
                self.play_counts[slot] += 1
        if index == self.mutate_at:
            for path, value in self.mutations.items():
                name, field = path.split('.')
                owner = getattr(self, name)
                if field.isdigit(): owner[int(field)] = value
                else: setattr(owner, field, value)


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.word(0x721b28, PRIMARY)
        for pointer in SOUNDS: self.word(pointer, 0x300d000)
        self.word(0x300d024, 0x300d100)
        for address in [*CUE_WRITES, *CUE_READS, 0x48d5ac,
                        0x534a34, 0x4ad2bf, 0x49490a, 0x300d100]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def sync(self, read):
        for address, data, size in views(self.f):
            if read: C.memmove(data, bytes(self.u.mem_read(address, size)), size)
            else: self.u.mem_write(address, C.string_at(data, size))
        if not read:
            for i, pointer in enumerate(SOUNDS):
                self.word(0x722334 + i*0x120, pointer if self.f.present[i] else 0)

    def emit(self, event):
        self.trace.append((event, self.f.snapshot()))
        if self.fail_at == len(self.trace):
            self.failed = True
            self.u.emu_stop()
            return False
        self.f.effect(event, len(self.trace))
        self.sync(False)
        return True

    def hook(self, u, address, _size, _context):
        if address in CUE_WRITES:
            self.have_cue = True
            return
        if address in CUE_READS:
            if not self.have_cue:
                self.undefined_cue = True
                u.emu_stop()
            return
        self.sync(True)
        f = self.f
        if address == 0x48d5ac:
            self.emit(('audio', 0, 0, 0, 0, 0, 0))
            return
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.integer(sp)
        args = struct.unpack('<4I', u.mem_read(sp+4, 16))
        result, cleanup = 0, 4
        if address == 0x300d100:
            slot = SOUNDS.index(args[0])
            self.word(args[1], int(f.playing[slot]))
            result, cleanup = (-1 if f.status_error[slot] else 0), 12
        else:
            if address == 0x534a34:
                result = f.randoms[f.random_index.value]
                event = ('random', result)
            elif address == 0x4ad2bf:
                assert ret == 0x48d4dd
                assert args[0] == (SOUNDS[4] if f.present[4] else 0)
                event = ('audio', 2, 4, 0, 0, I(args[1]).value, I(args[2]).value)
            elif address == 0x49490a:
                event = ('audio', 4, args[2], I(args[0]).value, I(args[1]).value,
                         I(args[3]).value, f.voice_volume.value)
            else: raise AssertionError(hex(address))
            if not self.emit(event): return
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp+cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, fail_at=0):
        self.f, self.trace = original.clone(), []
        self.fail_at, self.failed, self.undefined_cue, self.have_cue = fail_at, False, False, False
        self.sync(False)
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack-128, bytes([0xcd])*128)
        self.word(self.stack, self.stop)
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x48cc18, self.stop, count=100000)
        assert self.failed or self.undefined_cue or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace, self.undefined_cue


def portable(lib, original, fail_at=0, missing=None, query_fail=None):
    f, trace, callbacks, errors = original.clone(), [], [], []
    def emit(event):
        trace.append((event, f.snapshot()))
        if fail_at == len(trace): return 0
        f.effect(event, len(trace))
        return 1
    def decorate(typ):
        def wrapper(fn):
            def checked(*args):
                try: return fn(*args)
                except Exception as exc:
                    errors.append(repr(exc))
                    return 0
            callback = typ(checked)
            callbacks.append(callback)
            return callback
        return wrapper
    @decorate(Active)
    def active(_, out, _e):
        if query_fail == 'active': return 0
        out[0] = f.active.value
        return 1
    @decorate(ReadTiming)
    def timing(_, slot, out, _e):
        if query_fail == slot: return 0
        out[0] = f.timings[slot]
        return 1
    @decorate(Audio)
    def audio(_, call, playing, _e):
        a = call.contents
        assert a.slot < 6
        playing[0] = int(bool(f.present[a.slot] and f.playing[a.slot] and not f.status_error[a.slot]))
        return emit(('audio', a.operation, a.slot, a.cue, a.bank, a.flags, a.volume))
    @decorate(Random)
    def draw_random(_, out, _e):
        out[0] = f.randoms[f.random_index.value]
        return emit(('random', out[0]))
    ops = Ops(None, active, timing, audio, draw_random)
    if missing is not None: setattr(ops, missing, dict(Ops._fields_)[missing]())
    b = Bindings()
    for name, typ in Bindings._fields_:
        setattr(b, name, C.cast(C.byref(getattr(f, name)), typ))
    error = C.create_string_buffer(256)
    ok = lib.bk_ending_gallery_effect_step(C.byref(b), f.seconds, C.byref(ops), error)
    assert not errors, errors
    return bool(ok), f, trace, error.value.decode()


def fixture(rng, case):
    f = Fixture()
    f.frame, f.auxiliary, f.state = Frame(), Auxiliary(), State()
    f.frame.group = rng.choice([0, 1, 2, 3, 4, 5, 128, 255])
    f.event = B(rng.choice([0, 2, 3, 4, 7, 8, 9, 255]))
    f.auxiliary.variant, f.auxiliary.selection = rng.randrange(2), rng.randrange(4)
    f.state.alternating = rng.choice([-128, -1, 0, 0, 1, 2, 127])
    f.state.sampled = rng.choice([0, 0, 0, 1, -1])
    f.state.speech_blocked = rng.choice([-128, -1, 0, 0, 0, 1, 1, 2, 127])
    f.state.speech_elapsed = rng.choice([0, 5.99999, 6, 6.000001, 7])
    f.active = I(rng.choice([1, 2, 3, 4, 6, 7, 10, 11, 13, 17, 18]))
    f.timings = (Timing*32)(*[Timing(0, rng.choice([100, 380, 400]),
        rng.choice([0, 98.9, 99, 100, 101, 379.99997, 380, 399.99997, 400, 401, float('nan')])) for _ in range(32)])
    f.voice_volume, f.effect_volume = I(rng.randrange(-10000, 1)), I(rng.randrange(-10000, 1))
    f.present = (U*6)(*[rng.choice([0, 1, 1]) for _ in range(6)])
    f.playing = (U*6)(*[rng.randrange(2) for _ in range(6)])
    f.status_error = (U*6)(*[int(rng.randrange(10) == 0) for _ in range(6)])
    f.last_cue, f.last_bank, f.play_counts = I(-1), I(-1), (U*6)()
    f.randoms = (I*16)(*[rng.choice([-0x80000000, -11, -8, -1, 0, 1, 8, 12345, 0x7fffffff]) for _ in range(16)])
    f.random_index = U()
    f.seconds = F(rng.choice([0, .000001, 1/60, .5, 1, 6])).value
    f.mutate_at, f.mutations = 0, {}
    if case % 3 == 0:
        f.mutate_at = case % 5 + 1
        f.mutations = {'frame.group': (f.frame.group+3) % 5,
                       'auxiliary.variant': 1-f.auxiliary.variant,
                       'auxiliary.selection': 2, 'event.value': 8,
                       'active.value': rng.choice([4, 6, 7, 10, 11, 17]),
                       'state.sampled': -1, 'state.speech_blocked': 0,
                       'voice_volume.value': -937, 'effect_volume.value': -211}
    return f


def compare(want, got, expected, actual, label):
    assert [e for e, _ in actual] == [e for e, _ in expected], (
        label, 'calls', [e for e, _ in actual], [e for e, _ in expected])
    for i, ((event, a), (_, b)) in enumerate(zip(actual, expected)):
        assert a == b, (label, 'snapshot', i, event,
            [(OWNERS[j], x, y) for j, (x, y) in enumerate(zip(a, b)) if x != y][:3])
    assert got.snapshot() == want.snapshot(), (label, 'final',
        [(OWNERS[j], x, y) for j, (x, y) in enumerate(zip(got.snapshot(), want.snapshot())) if x != y][:3])


def main():
    faulthandler.enable()
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path)
    p.add_argument('--cases', type=int, default=12000)
    p.add_argument('--output', type=Path, default=ROOT/'local/original-ending-gallery-effect.json')
    args = p.parse_args()
    if args.cases < 1: p.error('--cases must be positive')
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_gallery_effect_initial.argtypes = []
    lib.bk_ending_gallery_effect_initial.restype = State
    lib.bk_ending_gallery_effect_step.argtypes = [C.POINTER(Bindings), F, C.POINTER(Ops), P]
    lib.bk_ending_gallery_effect_step.restype = I
    initial = lib.bk_ending_gallery_effect_initial()
    for name, address in FIELDS:
        typ = dict(State._fields_)[name]
        assert bytes(typ(getattr(initial, name))) == bytes(native.u.mem_read(address, C.sizeof(typ)))
    assert native.u.mem_read(0x54f8f4, 240) == native.u.mem_read(0x5546ac, 240)
    rng, digest = random.Random(0x48cc18), hashlib.sha256()
    stats = dict(random_frames=0, boundary_frames=0, retained_frames=0,
                 calls=0, live_mutations=0, failure_prefixes=0, undefined_cue_rejections=0)
    banks, coverage = set(), set()

    def check(original, label, failure=False):
        want, expected, uninitialized = native.run(original)
        ok, got, trace, error = portable(lib, original)
        if uninitialized:
            assert not ok and error == 'gallery effect: original cue is uninitialized for active clip', (label,error)
            stats['undefined_cue_rejections'] += 1
        else: assert ok, (label, error)
        compare(want, got, expected, trace, label)
        stats['calls'] += len(trace)
        stats['live_mutations'] += 0 < original.mutate_at <= len(trace)
        for event, _ in trace:
            if event[0] == 'audio' and event[1] == 4: banks.add(event[4])
        digest.update(b''.join(got.snapshot()))
        if failure:
            for fail_at in range(1, len(trace)+1):
                want, expected, _ = native.run(original, fail_at)
                ok, failed, prefix, _ = portable(lib, original, fail_at)
                assert not ok
                compare(want, failed, expected, prefix, (label, 'failure', fail_at))
                stats['failure_prefixes'] += 1
        return got

    for case in range(args.cases):
        check(fixture(rng, case), ('random', case), case % 11 == 0)
        stats['random_frames'] += 1
    # Isolate every possible comparison slot. Other slots stay far from end;
    # this catches accidentally omitted alternatives and wrong near operators.
    for group in range(5):
        for event in [0, 2, 3, 4, 7, 8, 9, 255]:
            for slot in [6, 7, 10, 11, 17, 18]:
                for end, source in [(100, 98.99999), (100, 99), (100, 100),
                                    (100, float('nan')), (float('nan'), 0)]:
                    f = fixture(rng, 1)
                    f.frame.group, f.event.value, f.active.value = group, event, 6
                    f.state, f.auxiliary.variant = State(), 1
                    f.timings = (Timing*32)(*[Timing(0, 100, 0) for _ in range(32)])
                    f.timings[slot].end, f.timings[slot].source = end, source
                    check(f, ('threshold', group, event, slot, end, source))
                    stats['boundary_frames'] += 1
    for group, event in [(0, 2), (2, 8)]:
        for active in [4, 6, 7, 10, 11, 17, 18]:
            for alternating in [-1, 0, 1, 2]:
                for sampled in [-1, 0, 1]:
                    for a, b in [(379.99997, 399.99997), (380, 399.99997),
                                 (379.99997, 400), (380, 400), (float('nan'), 400),
                                 (380, float('nan'))]:
                        f = fixture(rng, 1)
                        f.frame.group, f.event.value, f.active.value = group, event, active
                        f.state = State(alternating, sampled, 0, 0)
                        f.timings[17].source, f.timings[18].source = a, b
                        check(f, ('alternating', group, active, alternating, sampled, a, b))
                        stats['boundary_frames'] += 1
    # Carry latches/timer across frames; reset only the fixture's external
    # audio completion and descriptor inputs, never the effect state.
    for group in range(5):
        f = fixture(rng, 1)
        f.frame.group, f.event.value, f.active.value = group, 0, 6
        f.state, f.auxiliary.variant, f.seconds = State(), 1, F(1/60).value
        for tick in range(800):
            f.random_index.value = 0
            f.playing[0] = 0
            for timing in f.timings: timing.end, timing.source = 100, tick % 102
            f = check(f, ('retained', group, tick))
            stats['retained_frames'] += 1
            coverage.add((group, f.state.sampled, f.state.speech_blocked))
    rejected = 0
    for seconds in [float('nan'), float('inf'), -1]:
        f = fixture(rng, 1)
        f.seconds = seconds
        ok, got, trace, _ = portable(lib, f)
        assert not ok and not trace and got.snapshot() == f.snapshot()
        rejected += 1
    baseline = fixture(rng, 1)
    baseline.frame.group, baseline.event.value, baseline.active.value = 0, 2, 6
    baseline.state, baseline.auxiliary.variant = State(), 1
    baseline.timings[6] = Timing(0, 100, 100)
    for member in ['active', 'timing', 'audio', 'random']:
        ok, _, _, error = portable(lib, baseline, missing=member)
        assert not ok and error == 'gallery effect: missing '+member+' service', (member,error)
        rejected += 1
    for query in ['active', 6]:
        ok, got, trace, _ = portable(lib, baseline, query_fail=query)
        assert not ok and not trace and got.snapshot() == baseline.snapshot()
        rejected += 1
    for variant in [-1, 2, 0x7fffffff]:
        f = baseline.clone()
        f.auxiliary.variant = variant
        ok, _, trace, error = portable(lib, f)
        assert not ok and error == 'gallery effect: voice table outside original two rows'
        assert [x[0][0] for x in trace] == ['audio', 'random']
        rejected += 1
    if args.cases >= 12000:
        assert {3, 4, 5, 6} <= banks and stats['undefined_cue_rejections'] > 0
        assert stats['live_mutations'] > 0 and stats['failure_prefixes'] > 0
        assert all((group, 0, 0) in coverage and (group, 1, 1) in coverage for group in range(5))
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), **stats,
                  banks=sorted(banks), bounded_service_rejections=rejected,
                  cue_table_words=60, state_sha256=digest.hexdigest(), max_error=0,
                  scope=__doc__, full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('PASS gallery effect:', stats, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
