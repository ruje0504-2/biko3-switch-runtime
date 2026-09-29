"""Original47811c/478eab rules with observing, live scene-service boundaries.

Native branches, replay RNG, counters, shared writes and direct auxiliary
rewinds execute unchanged. Manual/drag input, circle/menu geometry, expression,
model and audio services are explicit fixture boundaries. This checks those
callers and their failure prefixes, not a playable third ending or rendering.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                              UC_X86_REG_ESP, UC_X86_REG_FPCW)
import original_ending_tertiary_control_oracle as parent
from model_binding import library
from original_matrix_oracle import machine

I, U, F, P = parent.I, parent.U, parent.F, parent.P
PAIR, ACTORS, ROOTS, SOUNDS = parent.PAIR, parent.ACTORS, parent.ROOTS, parent.SOUNDS


class State(C.Structure):
    _fields_ = [('replay_elapsed', F), ('replay_after', I)]


class Bindings(C.Structure):
    _fields_ = [('control', parent.Bindings), ('gauge_y', C.POINTER(F)),
                ('scale', C.POINTER(F)), ('choices', C.POINTER(I))]


EffectSlot = C.CFUNCTYPE(I, P, U, I, P)
Material = C.CFUNCTYPE(I, P, I, U, U, I, F, P)
Manual = C.CFUNCTYPE(I, P, C.POINTER(parent.Input), C.POINTER(I), P)
Drag = C.CFUNCTYPE(I, P, U, C.POINTER(U), U, P)
Hit = C.CFUNCTYPE(I, P, U, C.POINTER(I), C.POINTER(I), P)
Rewind = C.CFUNCTYPE(I, P, U, U, P)
Refresh = C.CFUNCTYPE(I, P, U, P)
Place = C.CFUNCTYPE(I, P, I, I, C.POINTER(I), P)


class Ops(C.Structure):
    _fields_ = [('control', parent.Ops), ('effect_slot', EffectSlot),
                ('material', Material), ('manual', Manual), ('drag', Drag),
                ('hit', Hit), ('rewind', Rewind), ('refresh', Refresh),
                ('place_menu', Place)]


EXTRA = [('gauge_y', 0x721e24), ('scale', 0x721ad0), ('choices', 0x7220e4),
         ('points', 0x7220c8), ('menu_width', 0x7389e8)]
MORE = ['action_state', 'aux_times'] + [name for name, _ in EXTRA]


def views(f):
    yield from parent.views(f)
    for name, address in [('replay_elapsed', 0x6afd24), ('replay_after', 0x54ccd8)]:
        yield parent.field_view(f.action_state, name, address)
    for name, address in EXTRA:
        value = getattr(f, name)
        yield address, C.addressof(value), C.sizeof(value)
    for actor, offset, index in [(1, 0x628, 0), (2, 0x760, 1)]:
        yield ACTORS[actor] + offset, C.addressof(f.aux_times[index]), 4
        yield ACTORS[actor] + offset + 12, C.addressof(f.aux_times[index]) + 4, 4


class Fixture(parent.Fixture):
    def clone(self):
        out = Fixture()
        for name in parent.OWNERS + MORE:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'hr', 'loaded']:
            setattr(out, name, list(getattr(self, name)))
        for name in ['keys', 'key_result', 'pick_result', 'chosen', 'camera_result',
                     'seconds', 'mutate_at', 'mutations', 'hits']:
            setattr(out, name, getattr(self, name))
        return out

    def snapshot(self):
        return super().snapshot() + tuple(bytes(getattr(self, name)) for name in MORE)


def fixture(rng, case):
    f = parent.fixture(rng, case)
    f.__class__ = Fixture
    f.action_state = State(rng.choice([0, 9.999, 10, 29.999, 30, 51]),
                            rng.choice([0, 10, 30, 50]))
    f.gauge_y = F(rng.choice([0, 100, 320, 480, 720]))
    f.scale = F(rng.choice([.25, .5, .75, 1, 1.5]))
    f.choices = (I * 3)(*[rng.choice([-1, 0, 1, 2, 3, 17]) for _ in range(3)])
    f.points = (PAIR * 3)(PAIR(100, 150), PAIR(300, 350), PAIR(500, 550))
    f.menu_width = F(rng.choice([1, 31.5, 64, 128, 213.3]))
    f.aux_times = ((F * 2) * 2)((F * 2)(151.25, 158.5), (F * 2)(234.75, 249.5))
    f.frame.group = case % 5
    f.part_mode.value = [0, 1, 0, 1, -1, 2][case // 5 % 6]
    f.active[0] = [2, 5, 7, 9, 4, 11][case // 30 % 6]
    f.frame.camera_cached = rng.choice([0, 6, 12, 13, 14, 38])
    f.return_ready.value = rng.randrange(2)
    f.movement_ready.value = rng.choice([0, 1, 255])
    f.unavailable[:] = [rng.randrange(2), rng.randrange(2)]
    f.auxiliary.progress = rng.choice([0, .189999, .19, .199999, .2, .399999, .4, 1])
    f.hits = rng.randrange(8)
    f.key_result = rng.choice([0, 0x100, 0x123400, 1, 0x80, 0x123480, 0xffffffff])
    f.chosen = rng.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10])
    f.seconds = F(rng.choice([0, 1e-7, 1/60, 1/30, .25, 1, 10])).value
    for r in f.records.groups:
        r.count = rng.choice([-2147483648, -1, 0, 1, 3, 9999, 2147483647])
    f.input.words[6:8] = [struct.unpack('<I', struct.pack('<f', v))[0]
                          for v in [rng.uniform(-30, 30), rng.uniform(-30, 30)]]
    f.mutate_at = -1
    f.mutations = {}
    return f


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.float_return = 0x300e000
        self.u.mem_write(self.float_return, b'\xd9\xee\xc3')  # fldz; ret
        for address in [0x4dfb96, 0x479739, 0x4ad2bf, 0x4018c8, 0x423a99,
                        0x4a7d10, 0x4b76c2, 0x479bc9, 0x479cc2, 0x4a777e,
                        0x402e18, 0x47cb41, 0x495125]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
        for i, actor in enumerate(ACTORS):
            self.word(0x721b28 + i * 4, actor)
            self.word(actor + 0x160, 0x3008000 + i * 64)
            self.word(0x3008014 + i * 64, ROOTS[i])
        self.effects = {0x300a100 + i * 16: i for i in range(40)}
        for address, slot in self.effects.items():
            self.word(0x722574 + slot * 0x120, address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def sync(self, read):
        for address, pointer, size in views(self.f):
            if read:
                C.memmove(pointer, bytes(self.u.mem_read(address, size)), size)
            else:
                self.u.mem_write(address, C.string_at(pointer, size))
        for i, r in enumerate(self.f.records.groups):
            address = parent.RECORDS + i * C.sizeof(parent.Record) + parent.Record.count.offset
            if read:
                r.count = I(self.integer(address)).value
            else:
                self.word(address, r.count)
        if not read:
            for i, sound in enumerate(SOUNDS):
                self.word(0x722334 + i * 0x120, sound if self.f.present[i] else 0)

    def hook(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.integer(sp)
        args = struct.unpack('<6I', u.mem_read(sp + 4, 24))
        self.sync(True)
        f = self.f
        result = 0
        if address == 0x4dfb96:
            event = ('expression', *[I(v).value for v in args[:3]])
        elif address == 0x479739:
            event = ('voice', I(args[0]).value, args[1], I(args[2]).value, I(args[3]).value)
        elif address == 0x4ad2bf:
            assert args[1] == 0
            if args[0] in self.effects:
                event = ('effect_slot', self.effects[args[0]], I(args[2]).value)
            else:
                assert args[0] in [0, SOUNDS[0]], hex(args[0])
                event = ('play', 0, I(args[2]).value)
        elif address == 0x4018c8:
            event = ('request', ACTORS.index(args[0]), I(args[1]).value)
        elif address == 0x423a99:
            event = ('hidden', ROOTS.index(args[0]), I(args[1]).value)
        elif address == 0x4a7d10:
            if 0x54ae00 <= args[0] < 0x54ae00 + 5*2*260:
                table, ordinal, width = 0, (args[0]-0x54ae00)//260, 2
            elif 0x548f88 <= args[0] < 0x548f88+5*6*260:
                table, ordinal, width = 1, (args[0]-0x548f88)//260, 6
            else:
                assert 0x54b828 <= args[0] < 0x54b828+5*4*260, hex(args[0])
                table, ordinal, width = 2, (args[0]-0x54b828)//260, 4
            alpha = struct.unpack('<f', struct.pack('<I', args[2]))[0]
            event = ('material', table, ordinal//width, ordinal%width, I(args[1]).value, alpha)
        elif address == 0x4b76c2:
            assert args[:3] == (0, 3, 0)
            result = f.key_result
            event = ('key', 0, 3, result)
        elif address == 0x479bc9:
            assert args[0] == ACTORS[0]
            event = ('manual', bytes(u.mem_read(sp + 8, 44)),
                     *struct.unpack('<2i', u.mem_read(sp + 52, 8)))
        elif address == 0x479cc2:
            event = ('drag', ACTORS.index(args[0]), args[1], args[2], args[3])
        elif address == 0x4a777e:
            index = self.hit_count
            self.hit_count += 1
            got = struct.unpack('<5f', u.mem_read(sp + 4, 20))
            pointer = [I(v).value for v in f.input.words[9:11]]
            want = tuple(F(v).value for v in [*f.points[index],
                         float(f.menu_width.value)/2, *pointer])
            assert got == want, ('hit geometry forwarding', got, want)
            self.word(args[5], 0)
            result = int(bool(f.hits & (1 << index)))
            event = ('hit', index, *pointer, result)
        elif address == 0x402e18:
            assert args[1] == 0
            event = ('refresh', ACTORS.index(args[0]))
        elif address == 0x47cb41:
            result = f.chosen
            event = ('choose', I(args[0]).value, I(args[1]).value)
        elif address == 0x495125:
            event = ('place_menu', *[I(v).value for v in args[:4]])
        else:
            raise AssertionError(hex(address))
        self.trace.append((event, f.snapshot()))
        if self.fail_at == len(self.trace):
            self.interrupted = True
            u.emu_stop()
            return
        f.effect(event, len(self.trace))
        self.sync(False)
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        if address == 0x479cc2:
            u.reg_write(UC_X86_REG_EIP, self.float_return)
        else:
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, begin=False, fail_at=0):
        self.f = original.clone()
        self.trace, self.hit_count = [], 0
        self.fail_at, self.interrupted = fail_at, False
        self.sync(False)
        self.u.mem_write(parent.RECORDS, bytes(self.f.records))
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop) + bytes(self.f.input))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x478eab if begin else 0x47811c, self.stop, count=100000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace


def portable(lib, original, begin=False, fail_at=0):
    f = original.clone()
    trace, callbacks, errors = [], [], []
    def emit(event):
        trace.append((event, f.snapshot()))
        if fail_at == len(trace):
            return 0
        f.effect(event, len(trace))
        return 1
    def decorate(typ):
        def wrap(fn):
            def checked(*args):
                try:
                    return fn(*args)
                except Exception as exc:
                    errors.append(repr(exc))
                    return 0
            value = typ(checked)
            callbacks.append(value)
            return value
        return wrap
    @decorate(parent.Key)
    def key(_, code, mode, out, _e):
        out[0] = f.key_result
        return emit(('key', code, mode, out[0]))
    @decorate(parent.Voice)
    def voice(_, cue, slot, bank, select, _e):
        return emit(('voice', cue, slot, bank, select))
    @decorate(parent.Play)
    def play(_, slot, volume, _e):
        return emit(('play', slot, volume))
    @decorate(parent.Expression)
    def expression(_, x, y, mode, _e):
        return emit(('expression', x, y, mode))
    @decorate(parent.Active)
    def active(_, out, _e):
        out[0] = f.active[0]
        return 1
    @decorate(parent.Request)
    def request(_, actor, clip, _e):
        return emit(('request', actor, clip))
    @decorate(parent.Hidden)
    def hidden(_, actor, value, _e):
        return emit(('hidden', actor, value))
    @decorate(parent.Choose)
    def choose(_, point, out, _e):
        out[0] = f.chosen
        return emit(('choose', *point[:2]))
    @decorate(EffectSlot)
    def effect_slot(_, slot, volume, _e):
        return emit(('effect_slot', slot, volume))
    @decorate(Material)
    def material(_, table, group, slot, hidden, alpha, _e):
        return emit(('material', table, group, slot, hidden, alpha))
    @decorate(Manual)
    def manual(_, inputs, point, _e):
        return emit(('manual', bytes(inputs.contents), *point[:2]))
    @decorate(Drag)
    def drag(_, actor, motion, mode, _e):
        return emit(('drag', actor, *motion[:2], mode))
    @decorate(Hit)
    def hit(_, index, pointer, out, _e):
        out[0] = int(bool(f.hits & (1 << index)))
        return emit(('hit', index, *pointer[:2], out[0]))
    @decorate(Rewind)
    def rewind(_, actor, clip, _e):
        assert (actor, clip) in [(1, 7), (2, 9)]
        f.aux_times[actor-1][1] = f.aux_times[actor-1][0]
        return 1
    @decorate(Refresh)
    def refresh(_, actor, _e):
        return emit(('refresh', actor))
    @decorate(Place)
    def place(_, angle, step, point, _e):
        return emit(('place_menu', angle, step, *point[:2]))
    parent_ops = parent.Ops()
    for name in ['key', 'voice', 'play', 'expression', 'active', 'request', 'hidden', 'choose']:
        setattr(parent_ops, name, locals()[name])
    ops = Ops(parent_ops, effect_slot, material, manual, drag, hit, rewind, refresh, place)
    shared = parent.Bindings()
    for name, typ in parent.Bindings._fields_:
        setattr(shared, name, C.cast(C.byref(getattr(f, name)), typ))
    bindings = Bindings(shared, C.pointer(f.gauge_y), C.pointer(f.scale), f.choices)
    error = C.create_string_buffer(256)
    if begin:
        ok = lib.bk_ending_tertiary_action_begin(C.byref(bindings), C.byref(ops), error)
    else:
        ok = lib.bk_ending_tertiary_action_step(C.byref(f.action_state), C.byref(bindings),
                   C.byref(f.input), f.seconds, C.byref(ops), error)
    assert not errors, errors
    return bool(ok), f, trace, error.value.decode()


def compare(want, got, expected, actual, label):
    events = [item[0] for item in expected]
    observed = [item[0] for item in actual]
    assert events == observed, (label, 'calls', events, observed)
    for index, ((event, left), (_, right)) in enumerate(zip(expected, actual)):
        if left != right:
            diff = [(i, a, b) for i, (a, b) in enumerate(zip(left, right)) if a != b]
            raise AssertionError((label, 'prefix', index, event, diff[:4]))
    left, right = want.snapshot(), got.snapshot()
    if left != right:
        diff = [(i, a, b) for i, (a, b) in enumerate(zip(left, right)) if a != b]
        raise AssertionError((label, 'final', diff[:4]))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--samples', type=int, default=12000)
    ap.add_argument('--begins', type=int, default=6000)
    ap.add_argument('--output', '--report', dest='report', type=Path)
    args = ap.parse_args()
    assert args.samples > 0 and args.begins > 0
    digest = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native, lib = Native(args.exe.read_bytes()), library()
    lib.bk_ending_tertiary_action_begin.argtypes = [C.POINTER(Bindings), C.POINTER(Ops), P]
    lib.bk_ending_tertiary_action_begin.restype = I
    lib.bk_ending_tertiary_action_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
                                                   C.POINTER(parent.Input), F, C.POINTER(Ops), P]
    lib.bk_ending_tertiary_action_step.restype = I
    rng, state_hash = random.Random(0x47811c), hashlib.sha256()
    calls = prefixes = mutations = 0
    coverage = {}
    for begin, samples in [(False, args.samples), (True, args.begins)]:
        for case in range(samples):
            f = fixture(rng, case)
            if case % 7 == 0:
                f.mutate_at = 1
                f.mutations = {'voice_volume.value': -1731, 'effect_volume.value': -923,
                               'frame.camera_cached': 13, 'auxiliary.progress': .25,
                               'part_mode.value': 1 - (f.part_mode.value & 1)}
                # The effect index and record pointer were captured on entry.
                # Group0's effect local is originally uninitialized, so only
                # mutate other entry groups in the equivalence population.
                if f.frame.group:
                    f.mutations['frame.group'] = (f.frame.group + 2) % 5
                mutations += 1
            want, expected = native.run(f, begin)
            ok, got, actual, error = portable(lib, f, begin)
            assert ok, ('portable rejected', begin, case, error)
            compare(want, got, expected, actual, (begin, case))
            calls += len(actual)
            for event, _ in actual:
                coverage[event[0]] = coverage.get(event[0], 0) + 1
            state_hash.update(repr((got.snapshot(), actual)).encode())
            if expected and case % 13 == 0:
                stop = rng.randrange(1, len(expected) + 1)
                want, expected = native.run(f, begin, stop)
                ok, got, actual, error = portable(lib, f, begin, stop)
                assert native.interrupted and not ok, ('failure not observed', begin, case, stop)
                compare(want, got, expected, actual, (begin, case, 'failure', stop))
                prefixes += 1
        print('Checked tertiary', 'begin' if begin else 'action', samples, flush=True)
    # Invalid projected access is rejected at its actual use. A menu choice
    # prefix has already been written, while action replay may already play.
    rejections = 0
    for index in [-1, 39, 2147483647]:
        for begin in [False, True]:
            f = fixture(rng, 0)
            f.part_mode.value = 0
            f.frame.camera_cached = index
            f.active[0] = 2
            ok, result, trace, error = portable(lib, f, begin)
            assert not ok and 'target' in error
            if begin:
                assert list(result.choices) == [0, -1, -1]
            rejections += 1
    report = {'passed': True, 'exe_sha256': digest, 'action_samples': args.samples,
              'begin_samples': args.begins, 'calls': calls, 'failure_prefixes': prefixes,
              'live_mutations': mutations, 'explicit_rejections': rejections,
              'coverage': coverage, 'state_sha256': state_hash.hexdigest(),
              'scope': __doc__, 'scene_children': False, 'gpu_or_device_validation': False}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS tertiary action/begin TOTAL', args.samples, args.begins, calls,
          prefixes, mutations, rejections, state_hash.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
