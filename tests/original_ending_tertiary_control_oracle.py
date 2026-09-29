"""Original476720 parent vs portable rules, including ordered live callbacks.

The original six-state parent, branch tables, floating arithmetic and CRT RNG
execute unchanged. Resource/audio/camera/input/action services are explicit
observing boundaries, not implementations of the third ending's loader or
children. Record lanes and callback-prefix state are checked independently.
No playable entry, GPU result, persistent unlock or Switch acceptance implied.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                              UC_X86_REG_ESP, UC_X86_REG_FPCW)
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_frame_oracle import State as Frame, Input, FIELDS, BYTES
from original_ending_control_oracle import State as Control, STATE_ADDR, FLAG_ADDR
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR
from original_menu_camera_oracle import State as Camera, I as IDENTITY

I, U, F, B, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_int8, C.c_void_p
PAIR = I * 2
ACTORS = [0x3003000, 0x3004000, 0x3005000]
ROOTS = [0x3009000, 0x3009100, 0x3009200]
SOUNDS = [0x300a000, 0x300a020]
NODE = 0x3002000
RECORDS = 0xb550b0


class State(C.Structure):
    _fields_ = [('last_cue', I), ('replay_elapsed', F), ('replay_after', I)]


class Record(C.Structure):
    _fields_ = [('retained', (U * 10000) * 2), ('actions', I * 10000), ('count', I)]


class Records(C.Structure):
    _fields_ = [('groups', Record * 5)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('camera', C.POINTER(Camera)),
                ('records', C.POINTER(Records)), ('dispatch', C.POINTER(I)),
                ('face_mode', C.POINTER(I)), ('part_mode', C.POINTER(I)),
                ('latches', C.POINTER(I)), ('return_ready', C.POINTER(I)),
                ('unavailable', C.POINTER(I)), ('movement_ready', C.POINTER(I)),
                ('substate', C.POINTER(B)), ('expression_override', C.POINTER(I)),
                ('pending_effect', C.POINTER(I)), ('fov', C.POINTER(F)),
                ('open', C.POINTER(I)), ('previous', C.POINTER(S)),
                ('finish_setting', C.POINTER(S)), ('actions', C.POINTER(I)),
                ('targets', C.POINTER(PAIR)), ('initial_targets', C.POINTER(I)),
                ('voice_volume', C.POINTER(I)), ('effect_volume', C.POINTER(I)),
                ('seed', C.POINTER(U))]


Key = C.CFUNCTYPE(I, P, U, U, C.POINTER(U), P)
Status = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
Voice = C.CFUNCTYPE(I, P, I, U, I, I, P)
Play = C.CFUNCTYPE(I, P, U, I, P)
Effect = C.CFUNCTYPE(I, P, I, P)
Expression = C.CFUNCTYPE(I, P, I, I, I, P)
Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Source = C.CFUNCTYPE(I, P, U, C.POINTER(F), P)
Request = C.CFUNCTYPE(I, P, U, I, P)
Hidden = C.CFUNCTYPE(I, P, U, I, P)
Appearance = C.CFUNCTYPE(I, P, I, U, U, I, P)
Target = C.CFUNCTYPE(I, P, U, C.POINTER(F), P)
CameraCall = C.CFUNCTYPE(I, P, I, I, C.POINTER(U), U, C.POINTER(U), P)
Pick = C.CFUNCTYPE(I, P, C.POINTER(F), C.POINTER(I), P)
Choose = C.CFUNCTYPE(I, P, C.POINTER(I), C.POINTER(I), P)
Begin = C.CFUNCTYPE(I, P, P)
Action = C.CFUNCTYPE(I, P, C.POINTER(Input), F, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('key', Key), ('present', Status), ('status', Status),
                ('voice', Voice), ('play', Play), ('effect', Effect),
                ('expression', Expression), ('active', Active), ('source', Source),
                ('request', Request), ('hidden', Hidden), ('appearance', Appearance),
                ('target', Target), ('camera', CameraCall), ('pick', Pick),
                ('choose', Choose), ('begin', Begin), ('action', Action)]


EXTRA = [('dispatch', 0x721ee8), ('face_mode', 0x721dfc), ('part_mode', 0x6a3c20),
         ('latches', 0x6afcfc), ('return_ready', 0x6afd08), ('unavailable', 0x6afd0c),
         ('movement_ready', 0x6afd14), ('substate', 0x6afd18),
         ('expression_override', 0x6a3c24), ('pending_effect', 0x54ccc8),
         ('fov', 0x54ccd0), ('open', 0x72210c), ('previous', 0x721ad4),
         ('finish_setting', 0x71bcdc), ('actions', 0x709dcc), ('targets', 0x721f90),
         ('initial_targets', 0x54ccb4), ('voice_volume', 0xbe9a08),
         ('effect_volume', 0xbe9a10), ('seed', 0x58edd8), ('node', NODE + 0xf0),
         ('source', ACTORS[0] + 0x808)]
OWNERS = ['frame', 'control', 'auxiliary', 'camera', 'state', 'records', 'input',
          'active', 'hidden'] + [name for name, _ in EXTRA]


def field_view(owner, name, address):
    typ = dict(type(owner)._fields_)[name]
    return address, C.addressof(owner) + getattr(type(owner), name).offset, C.sizeof(typ)


def views(f):
    for name, address in FIELDS + BYTES:
        yield field_view(f.frame, name, address)
    for name, address in [('camera_values', 0x721e14), ('camera_table', 0x709fcc)]:
        yield field_view(f.frame, name, address)
    for (name, _), address in zip(Control._fields_[:7], STATE_ADDR):
        yield field_view(f.control, name, address)
    for name, address in [('targets', 0x70c8d8), ('saved_camera', 0x71b41c),
                          ('variant', 0x721b3d), ('toggles', 0x7220f8),
                          ('pause_selection', 0xbeeb7d)]:
        yield field_view(f.control, name, address)
    for i, address in enumerate(FLAG_ADDR):
        yield address, C.addressof(f.control) + Control.pause_flags.offset + i, 1
    for (name, _), address in zip(Auxiliary._fields_, ADDR):
        yield field_view(f.auxiliary, name, address)
    for name, address in [('yaw', 0x71b364), ('pitch', 0x71b368), ('radius', 0x71b36c),
                          ('height', 0x71b370), ('matrix', 0x71b3dc)]:
        yield field_view(f.camera, name, address)
    for name, address in [('last_cue', 0x6afd1c), ('replay_elapsed', 0x6afd20),
                          ('replay_after', 0x54ccd4)]:
        yield field_view(f.state, name, address)
    for name, address in EXTRA:
        owner = getattr(f, name)
        yield address, C.addressof(owner), C.sizeof(owner)
    for i, actor in enumerate(ACTORS):
        yield actor + 0x140, C.addressof(f.active) + i * 4, 4


class Fixture:
    def clone(self):
        result = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(result, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'hr', 'loaded']:
            setattr(result, name, list(getattr(self, name)))
        for name in ['keys', 'pick_result', 'chosen', 'camera_result', 'seconds',
                     'mutate_at', 'mutations']:
            setattr(result, name, getattr(self, name))
        return result

    def snapshot(self):
        scalars = tuple(C.string_at(pointer, size) for _, pointer, size in views(self))
        records = tuple((r.count, tuple(r.actions[:16]), tuple(r.actions[-2:]))
                        for r in self.records.groups)
        return scalars + (struct.pack('<f', self.camera.fov), bytes(self.hidden),
                          tuple(self.present), tuple(self.playing), records)

    def effect(self, event, index):
        kind = event[0]
        if kind == 'voice':
            slot = event[2]
            self.present[slot] = self.loaded[slot]
            self.playing[slot] = False
        elif kind == 'play':
            self.playing[event[1]] = self.present[event[1]]
        elif kind == 'expression':
            self.auxiliary.expression_a, self.auxiliary.expression_b = event[1:3]
        elif kind == 'request':
            self.active[event[1]] = event[2]
        elif kind == 'hidden':
            self.hidden[event[1]] = event[2]
        elif kind == 'camera':
            self.camera.matrix[12] = 17 + index
            self.camera.pitch = F(self.camera.pitch - .125).value
        elif kind == 'pick':
            self.frame.camera_cached = self.pick_result[1]
        elif kind == 'choose':
            self.frame.camera_event = 12
        elif kind == 'begin':
            self.frame.camera_event = 23
        elif kind == 'action':
            self.frame.camera_cached = 17
        if self.mutate_at == index:
            for name, value in self.mutations.items():
                owner_name, field = name.split('.')
                owner = getattr(self, owner_name)
                if field.isdigit():
                    owner[int(field)] = value
                else:
                    setattr(owner, field, value)


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.addresses = [0x42cf0e, 0x4dfb96, 0x479739, 0x4ad2bf, 0x4e0b7e,
                          0x4e0ecb, 0x4dac12, 0x47cb41, 0x478eab, 0x47811c,
                          0x4018c8, 0x423a99, 0x4a7d10, 0x4b76c2, 0x300d100]
        for address in self.addresses:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
        self.word(0x300b024, 0x300d100)
        for sound in SOUNDS:
            self.word(sound, 0x300b000)
        for i, actor in enumerate(ACTORS):
            self.word(0x721b28 + i * 4, actor)
            self.word(actor + 0x160, 0x3008000 + i * 64)
            self.word(0x3008014 + i * 64, ROOTS[i])
        self.word(0x721f08, NODE)
        self.word(0x725034, 0x300a040)

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
        if read:
            for i, r in enumerate(self.f.records.groups):
                base = RECORDS + i * C.sizeof(Record)
                r.count = self.integer(base + Record.count.offset)
                r.actions[:16] = struct.unpack('<16i', self.u.mem_read(base + Record.actions.offset, 64))
                r.actions[-2:] = struct.unpack('<2i', self.u.mem_read(base + Record.actions.offset + 9998*4, 8))
        else:
            for i, sound in enumerate(SOUNDS):
                self.word(0x722334 + i * 0x120, sound if self.f.present[i] else 0)

    def hook(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.integer(sp)
        args = struct.unpack('<6I', u.mem_read(sp + 4, 24))
        self.sync(True)
        f = self.f
        result = cleanup = 0
        if address == 0x42cf0e:
            f.camera.fov = struct.unpack('<f', u.mem_read(sp + 4, 4))[0]
            u.reg_write(UC_X86_REG_EAX, 0)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, ret)
            return
        if address == 0x4dfb96:
            event = ('expression', *[I(v).value for v in args[:3]])
        elif address == 0x479739:
            event = ('voice', I(args[0]).value, args[1], I(args[2]).value, I(args[3]).value)
        elif address == 0x4ad2bf:
            assert args[1] == 0
            if args[0] == 0x300a040:
                event = ('effect', I(args[2]).value)
            else:
                # NULL is an actual observed Play argument when a load failed.
                slot = SOUNDS.index(args[0]) if args[0] in SOUNDS else self.last_voice_slot
                event = ('play', slot, I(args[2]).value)
        elif address in [0x4e0b7e, 0x4e0ecb]:
            assert args[0] == 0x71af38
            mode = 0 if address == 0x4e0b7e else 1
            event = ('camera', mode) if mode == 0 else ('camera', mode, I(args[1]).value, *args[2:6])
            result = f.camera_result
        elif address == 0x4dac12:
            event = ('pick', *struct.unpack('<2f', u.mem_read(sp + 4, 8)))
            result = f.pick_result[0]
        elif address == 0x47cb41:
            event = ('choose', *[I(v).value for v in args[:2]])
            result = f.chosen
        elif address == 0x478eab:
            event = ('begin',)
        elif address == 0x47811c:
            forwarded = self.integer(sp + 48)
            assert forwarded == 0x70d370, hex(forwarded)
            event = ('action', bytes(u.mem_read(sp + 4, 44)), f.seconds)
        elif address == 0x4018c8:
            event = ('request', ACTORS.index(args[0]), I(args[1]).value)
        elif address == 0x423a99:
            event = ('hidden', ROOTS.index(args[0]), I(args[1]).value)
        elif address == 0x4a7d10:
            assert args[2] == 0x3f800000
            if 0x54ae00 <= args[0] < 0x54ae00 + 5*2*260:
                table, ordinal, width = 0, (args[0]-0x54ae00)//260, 2
            else:
                assert 0x548f88 <= args[0] < 0x548f88+5*6*260, hex(args[0])
                table, ordinal, width = 1, (args[0]-0x548f88)//260, 6
            event = ('appearance', table, ordinal//width, ordinal%width, I(args[1]).value)
        elif address == 0x4b76c2:
            assert args[2] == 0
            bit = [0, 1, 0x5a, 0x33450].index(args[0]) + args[1]*4
            result = 0xa50100 | (128 if f.keys & (1 << bit) else 0)
            event = ('key', args[0], args[1], result)
        elif address == 0x300d100:
            slot = SOUNDS.index(args[0])
            event = ('status', slot)
            self.word(args[1], int(f.playing[slot]))
            result = f.hr[slot]
            cleanup = 8
        else:
            raise AssertionError(hex(address))
        self.trace.append((event, f.snapshot()))
        if self.fail_at == len(self.trace):
            self.interrupted = True
            u.emu_stop()
            return
        if event[0] == 'voice':
            self.last_voice_slot = event[2]
        f.effect(event, len(self.trace))
        self.sync(False)
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture, fail_at=0):
        self.f = fixture.clone()
        self.trace = []
        self.fail_at, self.interrupted, self.last_voice_slot = fail_at, False, 0
        self.sync(False)
        self.u.mem_write(RECORDS, bytes(self.f.records))
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop) + bytes(self.f.input) + struct.pack('<I', 0x70d370))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x476720, self.stop, count=100000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        C.memmove(C.addressof(self.f.records), bytes(self.u.mem_read(RECORDS, C.sizeof(Records))), C.sizeof(Records))
        return self.f, self.trace


def portable(lib, original, fail_at=0):
    f = original.clone()
    trace, callbacks, callback_errors = [], [], []
    def emit(event):
        trace.append((event, f.snapshot()))
        if fail_at == len(trace):
            return 0
        f.effect(event, len(trace))
        return 1
    def decorate(typ):
        def wrapped(fn):
            def checked(*args):
                try:
                    return fn(*args)
                except Exception as exc:
                    callback_errors.append(repr(exc))
                    return 0
            cb = typ(checked)
            callbacks.append(cb)
            return cb
        return wrapped
    @decorate(Key)
    def key(_, code, mode, out, _e):
        bit = [0, 1, 0x5a, 0x33450].index(code) + mode*4
        out[0] = 0xa50100 | (128 if f.keys & (1 << bit) else 0)
        return emit(('key', code, mode, out[0]))
    @decorate(Status)
    def present(_, slot, out, _e):
        out[0] = f.present[slot]
        return 1
    @decorate(Status)
    def status(_, slot, out, _e):
        out[0] = bool(f.playing[slot] and not f.hr[slot])
        return emit(('status', slot))
    @decorate(Voice)
    def voice(_, cue, slot, bank, select, _e):
        return emit(('voice', cue, slot, bank, select))
    @decorate(Play)
    def play(_, slot, volume, _e):
        return emit(('play', slot, volume))
    @decorate(Effect)
    def effect(_, volume, _e):
        return emit(('effect', volume))
    @decorate(Expression)
    def expression(_, a, b, mode, _e):
        return emit(('expression', a, b, mode))
    @decorate(Active)
    def active(_, out, _e):
        out[0] = f.active[0]
        return 1
    @decorate(Source)
    def source(_, slot, out, _e):
        assert slot == 10
        out[0] = f.source.value
        return 1
    @decorate(Request)
    def request(_, actor, clip, _e):
        return emit(('request', actor, clip))
    @decorate(Hidden)
    def hidden(_, actor, value, _e):
        ok = emit(('hidden', actor, value))
        return ok
    @decorate(Appearance)
    def appearance(_, table, group, slot, hidden, _e):
        return emit(('appearance', table, group, slot, hidden))
    @decorate(Target)
    def target(_, node, out, _e):
        assert node == 5
        for i in range(3):
            out[i] = f.node[i]
        return 1
    @decorate(CameraCall)
    def camera(_, mode, choice, offsets, extra, out, _e):
        out[0] = f.camera_result
        event = ('camera', mode) if mode == 0 else ('camera', mode, choice, *offsets[:3], extra)
        return emit(event)
    @decorate(Pick)
    def pick(_, point, out, _e):
        out[0] = f.pick_result[0]
        return emit(('pick', *point[:2]))
    @decorate(Choose)
    def choose(_, point, out, _e):
        out[0] = f.chosen
        return emit(('choose', *point[:2]))
    @decorate(Begin)
    def begin(_, _e):
        return emit(('begin',))
    @decorate(Action)
    def action(_, inputs, seconds, _e):
        return emit(('action', bytes(inputs.contents), seconds))
    ops = Ops(None, key, present, status, voice, play, effect, expression, active,
              source, request, hidden, appearance, target, camera, pick, choose, begin, action)
    bindings = Bindings()
    for name, pointer_type in Bindings._fields_:
        value = getattr(f, name)
        setattr(bindings, name, C.cast(C.byref(value), pointer_type))
    error = C.create_string_buffer(256)
    ok = lib.bk_ending_tertiary_control_step(C.byref(f.state), C.byref(bindings),
             C.byref(f.input), f.seconds, C.byref(ops), error)
    assert not callback_errors, callback_errors
    return bool(ok), f, trace, error.value.decode()


def fixture(rng, case):
    f = Fixture()
    f.frame, f.control, f.auxiliary, f.camera = Frame(), Control(), Auxiliary(), Camera()
    f.state = State(rng.randrange(9), rng.choice([0, 29.999, 30, 50]), rng.choice([0, 30, 50]))
    f.records = Records.from_buffer_copy(b'\x35' * C.sizeof(Records))
    for r in f.records.groups:
        r.count = rng.choice([0, 2, 8, 9999])
    f.input = Input((U*11)(*[rng.getrandbits(32) for _ in range(11)]))
    f.input.words[9:11] = [I(rng.randrange(-500, 2000)).value & 0xffffffff for _ in range(2)]
    f.active = (I*3)(rng.choice([1, 2, 3, 4, 4, 4, 5, 6, 7, 8, 9, 10, 11]), 1, 1)
    f.hidden = (I*3)(0, 1, 1)
    scalar_types = dict(Bindings._fields_)
    for name, _ in EXTRA:
        if name in ['actions', 'targets', 'initial_targets', 'latches', 'unavailable', 'node', 'source']:
            continue
        typ = scalar_types[name]._type_
        setattr(f, name, typ(rng.randrange(3)))
    f.latches = (I*3)(*[rng.randrange(2) for _ in range(3)])
    f.unavailable = (I*2)(0, 0)
    f.actions = (I*80)(*[rng.randrange(39) for _ in range(80)])
    f.actions[0], f.actions[5], f.actions[10] = 12, 13, 14
    f.targets = (PAIR*39)(*[PAIR(i*9-100, i*17-300) for i in range(39)])
    f.initial_targets = (I*5)(12, 13, 14, 15, 16)
    f.node = (F*3)(3.25, -17.75, 31.5)
    f.source = F(rng.choice([0, 10, 369.999, 370, 370.001, 1000]))
    f.seed.value = rng.getrandbits(32)
    f.voice_volume.value, f.effect_volume.value = -713, -411
    f.dispatch.value = case % 6
    f.substate.value = rng.choice([0, 0, 1, 2, 3, 255])
    f.open.value = rng.choice([0, 0, 1])
    f.pending_effect.value = rng.choice([-1, 0, 5, 13])
    f.previous.value = rng.choice([8, 8, 24, -1])
    f.finish_setting.value = rng.choice([0, 1, 2])
    f.frame.group = case//6 % 5
    f.frame.camera_mode = rng.choice([0, 0, 1, 2, 4])
    f.frame.camera_clip = rng.randrange(3)
    f.frame.camera_cached = rng.choice([6, 12, 13, 14])
    f.frame.transition_action = rng.choice([0, 7, 8, 49])
    f.frame.camera_values[:] = [rng.getrandbits(32) for _ in range(3)]
    for row in f.frame.camera_table:
        row[:] = [rng.getrandbits(32) for _ in range(4)]
    f.control.target_choice = 2
    f.control.toggles[:] = [rng.choice([0, 1, 255]) for _ in range(8)]
    f.control.saved_camera[:] = [17]*16
    f.auxiliary.pending = rng.choice([-1, 0, 1, 2, 3, 4])
    f.auxiliary.index = -1
    f.auxiliary.progress = rng.choice([0, .189999, .19, .199999, .2, .25, .389999, .39, .399999, .4, 1])
    f.camera.pose.world[:] = f.camera.matrix[:] = IDENTITY
    f.camera.pitch, f.camera.fov = 7.25, .7
    f.fov.value = rng.choice([.19, .2, .25, 1])
    f.keys = rng.getrandbits(12) if case % 3 else 0
    f.present = [rng.choice([True, True, False]) for _ in range(2)]
    f.playing = [rng.choice([True, False, False]) for _ in range(2)]
    f.hr = [rng.choice([0, 0, -1]) for _ in range(2)]
    f.loaded = [True, True]
    f.pick_result = (rng.choice([0, 1, 1, 2]), rng.choice([12, 13, 14]))
    if f.frame.group == 0 and case % 13 == 0:
        f.pick_result = (1, 6)
        f.auxiliary.progress = .75
    f.chosen = rng.choice([-1, 0, 1])
    f.camera_result = rng.choice([0, 1, 256, 255])
    f.seconds = F(rng.choice([0, .000001, 1/60, .5, 1, 2])).value
    f.mutate_at, f.mutations = 0, {}
    return f


def compare(want, got, wanted_trace, got_trace, label):
    events = [event for event, _ in got_trace]
    expected_events = [event for event, _ in wanted_trace]
    assert events == expected_events, (label, 'calls', events, expected_events)
    for i, ((_, have), (_, expected)) in enumerate(zip(got_trace, wanted_trace)):
        assert have == expected, (label, 'callback snapshot', i, events[i],
             [(j, g, w) for j, (g, w) in enumerate(zip(have, expected)) if g != w][:5])
    have, expected = got.snapshot(), want.snapshot()
    assert have == expected, (label, 'final',
         [(j, g, w) for j, (g, w) in enumerate(zip(have, expected)) if g != w][:5])
    assert bytes(got.records) == bytes(want.records), (label, 'complete record lanes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=12000)
    parser.add_argument('--output', type=Path, default=ROOT/'local/original-ending-tertiary-control.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_tertiary_control_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
                                                  C.POINTER(Input), F, C.POINTER(Ops), P]
    lib.bk_ending_tertiary_control_initial.restype = State
    assert C.sizeof(Record) == 0x1d4c4
    assert bytes(lib.bk_ending_tertiary_control_initial()) == b''.join(
        bytes(native.u.mem_read(a, 4)) for a in [0x6afd1c, 0x6afd20, 0x54ccd4])
    constants = {hex(a): struct.unpack('<f', native.u.mem_read(a, 4))[0]
                 for a in [0x53f488, 0x53f48c, 0x53f490, 0x53f494]}
    assert list(constants.values()) == [F(370).value, F(.19).value, F(.39).value, F(.4).value], constants
    dispatch = list(struct.unpack('<6I', native.u.mem_read(0x4780f4, 24)))
    assert dispatch == [0x476a42, 0x476c39, 0x4780b6, 0x477e32, 0x476777, 0x477e52], [hex(v) for v in dispatch]
    rng = random.Random(0x476720)
    operations, calls, failures, mutations, rejected = set(), 0, 0, 0, 0
    written_actions = set()
    digest = hashlib.sha256()
    for case in range(args.cases):
        original = fixture(rng, case)
        if case % 23 == 0:
            original.auxiliary.progress = rng.choice([float('nan'), float('inf'), -float('inf')])
            original.source.value = rng.choice([float('nan'), float('inf'), -float('inf')])
            # Keep the undefined native hover-cue local out of equivalence cases.
            original.pick_result = (1, 12)
        if case % 7 == 0:
            original.mutate_at = 1 + case % 7
            original.mutations = {
                'frame.group': (original.frame.group + 2) % 5,
                'frame.camera_cached': 13,
                'frame.camera_clip': 2,
                'frame.camera_mode': 4,
                'auxiliary.progress': .75,
                'auxiliary.pending': [1, 2, 3, 4, -1][case % 5],
                'previous.value': 8 if original.previous.value != 8 else 24,
                'voice_volume.value': -907,
                'node.0': -55.125,
                'seed.value': 0x7f123456,
            }
        want, wanted_trace = native.run(original)
        ok, got, trace, error = portable(lib, original)
        assert ok, (case, error, [v[0] for v in trace])
        compare(want, got, wanted_trace, trace, case)
        operations.update(event[0] for event, _ in trace)
        calls += len(trace)
        mutations += 0 < original.mutate_at <= len(trace)
        for before, after in zip(original.records.groups, got.records.groups):
            written_actions.update(after.actions[before.count:after.count])
        digest.update(bytes(got.state)+bytes(got.frame)+bytes(got.auxiliary)+bytes(got.records))
        if case % 19 == 0 and trace:
            fail_at = 1 + case % len(trace)
            want_prefix, expected_prefix = native.run(original, fail_at)
            ok, got_prefix, prefix, _ = portable(lib, original, fail_at)
            assert not ok
            compare(want_prefix, got_prefix, expected_prefix, prefix, (case, 'prefix'))
            failures += 1
    for group in range(5):
        for count in [-1, 10000, 0x7fffffff]:
            original = fixture(rng, 5)
            original.frame.group = group
            original.dispatch.value = 5
            original.previous.value = 8
            original.finish_setting.value = 0
            original.frame.transition_action = 0
            original.records.groups[group].count = count
            before = bytes(original.records)
            ok, got, trace, error = portable(lib, original)
            assert not ok and 'record write outside' in error, (group, count, error)
            assert bytes(got.records) == before
            rejected += 1
    for blocked in [0, 1]:
        original = fixture(rng, 1)
        original.active[0] = 4
        original.open.value = 0
        original.frame.camera_mode = 0
        original.present[1] = False
        original.auxiliary.pending = 0
        original.pending_effect.value = -1
        original.pick_result = (1, original.actions[5 if blocked == 0 else 10])
        original.unavailable[blocked] = 1
        ok, _, _, error = portable(lib, original)
        assert not ok and 'uninitialized local' in error, error
        rejected += 1
    for value in [float('nan'), float('inf'), -1.0]:
        original = fixture(rng, 0)
        original.seconds = value
        ok, got, trace, error = portable(lib, original)
        assert not ok and not trace and got.snapshot() == original.snapshot(), error
        rejected += 1
    if args.cases >= 12000:
        assert operations == {'voice', 'play', 'effect', 'expression', 'request', 'hidden',
                              'appearance', 'camera', 'pick', 'choose', 'begin', 'action',
                              'key', 'status'}, operations
        assert written_actions == {12, 13, 15, 16, 17, 20}, written_actions
        assert mutations >= 300, mutations
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  frames=args.cases, calls=calls, failure_prefixes=failures,
                  live_mutations=mutations, bounded_rejections=rejected,
                  written_actions=sorted(written_actions),
                  operations=sorted(operations), state_sha256=digest.hexdigest(),
                  constants=constants, max_error=0, scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS tertiary parent:', args.cases, 'frames;', calls, 'calls;', failures,
          'failure prefixes;', mutations, 'live mutations;', rejected,
          'rejections; state', digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
