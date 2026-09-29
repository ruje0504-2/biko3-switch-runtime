"""Original4968CB+49717C and4974D1, including live aliases and failure prefixes.

Prediction fields and manual descriptor writes are actual native memory.
The native timer, comparisons, tables, signed byte arithmetic and selector
execute unchanged. Audio leaves, ten-tick requests and RNG are controlled
fixtures. Native uninitialized cue reads stop at their first use and must
match explicit portable failure; no stack-garbage equivalence is claimed.
Real media/actor sampling is a separate check, not implied by this oracle.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_frame_oracle import State as Frame, FIELDS as FRAME_FIELDS, BYTES
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR, Call as AudioCall
from original_ending_auxiliary_tick_oracle import Prediction
from original_ending_tertiary_control_oracle import field_view

I, U, F, B, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_void_p
PRIMARY = 0x3001000
SOUNDS = [0x300c000 + i*0x100 for i in range(6)]


class Cycle(C.Structure):
    _fields_ = [('slow', I), ('sampled', I), ('speech_blocked', C.c_int8),
                ('speech_elapsed', F), ('countdown', C.c_int8)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('event', C.POINTER(B)), ('cycle', C.POINTER(Cycle)),
                ('scale', C.POINTER(F))] + [(name, C.POINTER(I)) for name in
                ['previous_sound', 'voice_volume', 'effect_volume']]


Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Predict = C.CFUNCTYPE(I, P, U, C.POINTER(Prediction), P)
Audio = C.CFUNCTYPE(I, P, C.POINTER(AudioCall), C.POINTER(I), P)
Random = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Write = C.CFUNCTYPE(I, P, U, I, I, P)
Request = C.CFUNCTYPE(I, P, U, P)


class CycleOps(C.Structure):
    _fields_ = [('context', P), ('active', Active), ('prediction', Predict),
                ('audio', Audio), ('random', Random)]


class ManualOps(C.Structure):
    _fields_ = [('context', P), ('active', Active), ('write', Write),
                ('request_ten', Request), ('audio', Audio)]


SCALARS = [('event', 0x721b3d), ('scale', 0x55469c),
           ('previous_sound', 0x5546a0), ('voice_volume', 0xbe9a08),
           ('effect_volume', 0xbe9a10)]
CYCLE_FIELDS = [('slow', 0x6ea38c), ('sampled', 0x6ea390),
                ('speech_blocked', 0x6ea394), ('speech_elapsed', 0x6ea398),
                ('countdown', 0x55696e)]
OWNERS = ['frame', 'auxiliary', 'cycle', 'active', 'predictions', 'chains',
          'present', 'playing', 'status_error', 'last_cue', 'last_bank',
          'play_counts', 'pause_counts', 'randoms', 'random_index'] + [n for n, _ in SCALARS]
WRITES = dict(zip([0x497533, 0x497542, 0x497552, 0x497562, 0x497571, 0x497581,
                  0x497591, 0x4975a0, 0x497685, 0x497694, 0x4976d2, 0x4976e1,
                  0x49771c, 0x49772b],
                 [(6,0),(7,0),(10,0),(11,0),(13,0),(14,0),(17,0),(18,0),
                  (6,1),(7,1),(10,1),(11,1),(17,1),(18,1)]))
CUE_WRITES = [0x496c48,0x496c79,0x496ca8,0x496cd6,0x496d7e,0x496daf,
              0x496ddd,0x496e0c,0x496e9b,0x496ecc,0x496efa,0x496f28]
CUE_READS = [0x496fd1, 0x497015, 0x49702e]


def views(f):
    for name, address in FRAME_FIELDS + BYTES:
        yield field_view(f.frame, name, address)
    for name, address in [('camera_values', 0x721e14), ('camera_table', 0x709fcc)]:
        yield field_view(f.frame, name, address)
    for (name, _), address in zip(Auxiliary._fields_, ADDR):
        yield field_view(f.auxiliary, name, address)
    for name, address in CYCLE_FIELDS:
        yield field_view(f.cycle, name, address)
    for name, address in SCALARS:
        owner = getattr(f, name)
        yield address, C.addressof(owner), C.sizeof(owner)
    yield PRIMARY + 0x140, C.addressof(f.active), 4
    for i in range(32):
        for field, offset in [('duration',0x1e0),('end',0x1e8),('rate',0x1ec),('source',0x1f0)]:
            yield field_view(f.predictions[i], field, PRIMARY + offset + i*156)
        yield PRIMARY + 0x200 + i*156, C.addressof(f.chains) + i*4, 4


class Fixture:
    def clone(self):
        f = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(f, name, type(value).from_buffer_copy(value))
        f.seconds, f.proposed = self.seconds, self.proposed
        f.mutate_at, f.mutations = self.mutate_at, self.mutations
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in OWNERS)

    def effect(self, event, index):
        if event[0] == 'random':
            self.random_index.value += 1
        elif event[0] == 'write':
            self.chains[event[1]] = event[3]
        elif event[0] == 'request_ten':
            self.active.value = event[1]
        elif event[0] == 'audio':
            _, kind, slot, cue, bank, flags, volume = event
            if kind == 1 and self.present[slot]:
                self.playing[slot] = 0
                self.pause_counts[slot] += 1
            elif kind in [2, 4]:
                if kind == 4:
                    self.present[slot] = 1
                    self.last_cue.value, self.last_bank.value = cue, bank
                if self.present[slot]:
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
        for pointer in SOUNDS:
            self.word(pointer, 0x300d000)
        self.word(0x300d024, 0x300d100)
        self.word(0x300d048, 0x300d180)
        for address in [*WRITES, *CUE_WRITES, *CUE_READS, 0x496f75, 0x497633,
                        0x534a34, 0x4ad34a, 0x4ad2bf, 0x49490a, 0x401b0a,
                        0x300d100, 0x300d180]:
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
        sp, bp = u.reg_read(UC_X86_REG_ESP), u.reg_read(UC_X86_REG_EBP)
        self.sync(True)
        f = self.f
        if address in WRITES:
            slot, value = WRITES[address]
            self.emit(('write', slot, 0, value))
            return
        if address in [0x496f75, 0x497633]:
            slot = 0 if address == 0x496f75 else self.integer(bp - 4) + 2
            self.emit(('audio', 0, slot, 0, 0, 0, 0))
            return
        ret = self.integer(sp)
        args = struct.unpack('<4I', u.mem_read(sp + 4, 16))
        result, cleanup = 0, 4
        if address == 0x300d100:
            slot = SOUNDS.index(args[0])
            self.word(args[1], int(f.playing[slot]))
            result, cleanup = (-1 if f.status_error[slot] else 0), 12
        else:
            if address == 0x534a34:
                result = f.randoms[f.random_index.value]
                event = ('random', result)
            elif address == 0x401b0a:
                assert args[0] == PRIMARY
                event = ('request_ten', args[1])
            elif address == 0x4ad34a:
                assert ret in [0x496bf9, 0x496d2e, 0x496e4c]
                slot = 2 if ret == 0x496bf9 else f.previous_sound.value + 2
                assert 0 <= slot < 6
                assert args[0] == (SOUNDS[slot] if f.present[slot] else 0)
                event = ('audio', 1, slot, 0, 0, 0, 0)
            elif address == 0x4ad2bf:
                slot = {0x496c10:2, 0x496d46:3, 0x496e63:4}[ret]
                assert args[0] == (SOUNDS[slot] if f.present[slot] else 0)
                event = ('audio', 2, slot, 0, 0, I(args[1]).value, I(args[2]).value)
            elif address == 0x49490a:
                event = ('audio', 4, args[2], I(args[0]).value, I(args[1]).value,
                         I(args[3]).value, f.voice_volume.value)
            elif address == 0x300d180:
                slot, cleanup = SOUNDS.index(args[0]), 8
                event = ('audio', 1, slot, 0, 0, 0, 0)
            else:
                raise AssertionError(hex(address))
            if not self.emit(event): return
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, kind, fail_at=0):
        self.f, self.trace = original.clone(), []
        self.fail_at, self.failed, self.undefined_cue, self.have_cue = fail_at, False, False, False
        self.sync(False)
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack-128, bytes([0xcd])*128)
        self.u.mem_write(self.stack, struct.pack('<2I', self.stop, self.f.proposed & 0xffffffff))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x4968cb if kind == 'cycle' else 0x4974d1, self.stop, count=100000)
        assert self.failed or self.undefined_cue or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace, I(self.u.reg_read(UC_X86_REG_EAX)).value, self.undefined_cue


def portable(lib, original, kind, fail_at=0, missing=None):
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
        out[0] = f.active.value
        return 1
    @decorate(Predict)
    def prediction(_, slot, out, _e):
        out[0] = f.predictions[slot]
        return 1
    @decorate(Audio)
    def audio(_, call, playing, _e):
        a = call.contents
        assert a.slot < 6
        playing[0] = int(bool(f.present[a.slot] and f.playing[a.slot] and not f.status_error[a.slot]))
        event = ('audio', a.operation, a.slot, a.cue, a.bank, a.flags, a.volume)
        return emit(event)
    @decorate(Random)
    def draw_random(_, out, _e):
        out[0] = f.randoms[f.random_index.value]
        return emit(('random', out[0]))
    @decorate(Write)
    def write(_, slot, field, value, _e):
        assert field == 0 and slot < 32
        return emit(('write', slot, field, value))
    @decorate(Request)
    def request_ten(_, slot, _e):
        return emit(('request_ten', slot))
    error, accepted = C.create_string_buffer(256), I(-123)
    if kind == 'cycle':
        ops = CycleOps(None, active, prediction, audio, draw_random)
        if missing is not None: setattr(ops, missing, dict(CycleOps._fields_)[missing]())
        b = Bindings()
        for name, typ in Bindings._fields_:
            setattr(b, name, C.cast(C.byref(getattr(f, name)), typ))
        ok = lib.bk_ending_selected_cycle_step(C.byref(b), f.seconds, C.byref(ops), error)
    else:
        ops = ManualOps(None, active, write, request_ten, audio)
        if missing is not None: setattr(ops, missing, dict(ManualOps._fields_)[missing]())
        ok = lib.bk_ending_selected_manual(C.byref(f.auxiliary), f.proposed,
                                          C.byref(ops), C.byref(accepted), error)
    assert not errors, errors
    return bool(ok), f, trace, accepted.value, error.value.decode()


def fixture(rng, case):
    f = Fixture()
    f.frame, f.auxiliary, f.cycle = Frame(), Auxiliary(), Cycle()
    f.frame.group, f.frame.camera_cached, f.frame.camera_event = case % 5, 27, 99
    f.event = B(rng.choice([2, 3, 4, 7, 8, 9, 255]))
    f.auxiliary.variant = rng.randrange(2)
    f.auxiliary.gate = rng.choice([1, 1, 1, 2, 3, 4])
    f.auxiliary.selection = rng.choice([0, 1, 2, -1, 0x7fffffff])
    f.auxiliary.base, f.auxiliary.index = rng.choice([36, 90, 0x7fffffff, -1]), -7
    f.auxiliary.progress = rng.choice([.4, .59, .6, .79, .8, .95, float('nan')])
    f.cycle.slow, f.cycle.sampled = rng.choice([0, 1, -1]), rng.choice([0, 0, 0, 1, -1])
    f.cycle.speech_blocked = rng.choice([-128, -1, 0, 0, 0, 1, 1, 2, 127])
    f.cycle.speech_elapsed = rng.choice([0, 5.99999, 6, 6.000001, 7])
    f.cycle.countdown = rng.choice([-128, -1, 0, 1, 1, 1, 2, 8, 127])
    f.active = I(rng.choice([1, 2, 3, 4, 6, 7, 7, 10, 11, 11, 13, 17]))
    f.predictions = (Prediction*32)(*[Prediction(rng.choice([0,1,73,100,0x7fffffff]),
        100, rng.choice([0, 98.9, 99, 100, 101]), rng.choice([0, .1, 1, -1])) for _ in range(32)])
    f.chains = (I*32)(*[rng.randrange(-1,3) for _ in range(32)])
    f.scale, f.previous_sound = F(rng.choice([.01, .02, 0, .037])), I(rng.choice([-2,-1,0,1,2,3]))
    f.voice_volume, f.effect_volume = I(rng.randrange(-10000, 1)), I(rng.randrange(-10000, 1))
    f.present = (U*6)(*[rng.choice([0,1,1]) for _ in range(6)])
    f.playing = (U*6)(*[rng.randrange(2) for _ in range(6)])
    f.status_error = (U*6)(*[int(rng.randrange(10)==0) for _ in range(6)])
    f.last_cue, f.last_bank = I(-1), I(-1)
    f.play_counts, f.pause_counts = (U*6)(), (U*6)()
    f.randoms = (I*16)(*[rng.choice([-0x80000000,-11,-8,-1,0,1,8,12345,0x7fffffff]) for _ in range(16)])
    f.random_index = U()
    f.seconds = F(rng.choice([0, .000001, 1/60, .5, 1, 6])).value
    f.proposed = rng.choice([0, 0, 1, 2, 3, -1, 4, 0x7fffffff, -0x80000000])
    f.mutate_at, f.mutations = 0, {}
    if case % 3 == 0:
        f.mutate_at = case % 13 + 1
        f.mutations = {'frame.group': (f.frame.group + 3) % 5,
                       'auxiliary.variant': 1-f.auxiliary.variant,
                       'auxiliary.progress': .81, 'auxiliary.selection': 2,
                       'auxiliary.base': 90, 'cycle.slow': 1,
                       'active.value': rng.choice([4,6,7,10,11,13]),
                       'voice_volume.value': -937, 'effect_volume.value': -211,
                       'scale.value': .02}
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=12000)
    parser.add_argument('--output', type=Path, default=ROOT/'local/original-ending-selected-auxiliary.json')
    args = parser.parse_args()
    if args.cases < 1: parser.error('cases must be positive')
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_selected_cycle_initial.argtypes = []
    lib.bk_ending_selected_cycle_initial.restype = Cycle
    lib.bk_ending_selected_cycle_step.argtypes = [C.POINTER(Bindings), F, C.POINTER(CycleOps), P]
    lib.bk_ending_selected_cycle_step.restype = I
    lib.bk_ending_selected_manual.argtypes = [C.POINTER(Auxiliary), I, C.POINTER(ManualOps), C.POINTER(I), P]
    lib.bk_ending_selected_manual.restype = I
    lib.bk_ending_selected_cue.argtypes = [U,U,C.POINTER(I)]
    lib.bk_ending_selected_cue.restype = I
    initial = lib.bk_ending_selected_cycle_initial()
    for name, address in CYCLE_FIELDS:
        typ = dict(Cycle._fields_)[name]
        assert bytes(typ(getattr(initial,name))) == bytes(native.u.mem_read(address,C.sizeof(typ)))
    for variant in range(2):
        for index in range(30):
            cue = I(-1)
            assert lib.bk_ending_selected_cue(variant,index,C.byref(cue))
            assert cue.value == I(native.integer(0x5546ac+variant*120+index*4)).value
    rng, digest = random.Random(0x4968cb), hashlib.sha256()
    rows = []
    for kind in ['cycle','manual']:
        calls = failures = mutations = undefined = accepted_count = 0
        operations, proposals, banks = set(), set(), set()
        for case in range(args.cases):
            original = fixture(rng, case)
            want, expected, native_result, uninitialized = native.run(original,kind)
            ok, got, trace, accepted, error = portable(lib,original,kind)
            if uninitialized:
                assert not ok and error == 'selected auxiliary: original cue is uninitialized for active clip', (kind,case,error)
                undefined += 1
            else:
                assert ok, (kind,case,error)
                if kind == 'manual':
                    assert accepted == native_result, (case,accepted,native_result)
                    accepted_count += accepted != 0
            compare(want,got,expected,trace,(kind,case))
            calls += len(trace)
            mutations += 0 < original.mutate_at <= len(trace)
            proposals.add(original.proposed)
            for event, _ in trace:
                operations.add(event[0])
                if event[0]=='audio' and event[1]==4: banks.add(event[4])
            digest.update(b''.join(got.snapshot()))
            if trace and case % 11 == 0:
                fail_at=case % len(trace)+1
                want,expected,_,_=native.run(original,kind,fail_at)
                ok,got,prefix,_,_=portable(lib,original,kind,fail_at)
                assert not ok
                compare(want,got,expected,prefix,(kind,case,'failure'))
                failures += 1
        rows.append(dict(kind=kind,frames=args.cases,calls=calls,failure_prefixes=failures,
                         live_mutations=mutations,undefined_cue_rejections=undefined,
                         accepted=accepted_count,operations=sorted(operations),banks=sorted(banks),
                         proposals=sorted(proposals)))
    rejected = 0
    for seconds in [float('nan'),float('inf'),-1]:
        original=fixture(rng,0)
        original.seconds=seconds
        ok,got,trace,_,error=portable(lib,original,'cycle')
        assert not ok and not trace and got.snapshot()==original.snapshot(),error
        rejected+=1
    for variant,index in [(2,0),(0,30),(0xffffffff,0),(0,0xffffffff)]:
        out=I(-123)
        assert not lib.bk_ending_selected_cue(variant,index,C.byref(out)) and out.value==-123
        rejected+=1
    if args.cases>=12000:
        assert rows[0]['operations']==['audio','random'] and rows[0]['undefined_cue_rejections']>0
        assert {3,4,5,6} <= set(rows[0]['banks'])
        assert rows[1]['operations']==['audio','request_ten','write'] and rows[1]['accepted']>0
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),checks=rows,
                bounded_rejections=rejected,cue_table_words=60,state_sha256=digest.hexdigest(),
                max_error=0,scope=__doc__,full_loader=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS selected auxiliary',args.cases,'cycle and manual frames',
          sum(r['calls'] for r in rows),'calls',sum(r['failure_prefixes'] for r in rows),
          'failure prefixes',sum(r['undefined_cue_rejections'] for r in rows),
          'undefined cue rejections',digest.hexdigest(),flush=True)


if __name__=='__main__':
    main()
