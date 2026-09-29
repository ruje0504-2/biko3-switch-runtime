"""Original48D8E9 selected parent, raw tables and ordered live service boundaries.

The native four-gate parent and four opening substates execute unchanged.
Direct camera/node/descriptor reads use independent native memory. Observe
every child boundary, including FOV-before-camera, load-before-Play, signed
pointer conversion, byte key results, retained tables and failed prefixes.
Children, actual media and actor sampling remain explicit fixtures here;
this is not the48E75B action,4949DC setup, a complete loader or Switch test.
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
from original_ending_frame_oracle import State as Frame, Input, FIELDS, BYTES
from original_ending_control_oracle import State as Control, STATE_ADDR, FLAG_ADDR
from original_ending_auxiliary_oracle import State as Auxiliary, ADDR, Call as AudioCall
from original_menu_camera_oracle import State as Camera, I as IDENTITY
from original_ending_preset_oracle import Presets
from original_ending_tertiary_control_oracle import field_view

I, U, F, B, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_int8, C.c_void_p
PAIR, CONFIG, PREFIX = I * 2, I * 6, I * 3
PRIMARY = 0x3001000
NODES = [0x3006000 + i * 0x100 for i in range(39)]
SOUNDS = [0x300c000 + i * 0x100 for i in range(6)]
EXE_SHA256 = 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'


class State(C.Structure):
    _fields_ = [('fov', F), ('replay_elapsed', F), ('replay_after', I),
                ('manual_mode', I), ('expression_override', I)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('camera', C.POINTER(Camera)),
                ('presets', C.POINTER(Presets)), ('substate', C.POINTER(B)),
                ('mode', C.POINTER(I)), ('choice', C.POINTER(I)), ('reset_c', C.POINTER(I)),
                ('configuration', C.POINTER(CONFIG)), ('camera_words', C.POINTER(I)),
                ('group_prefix', C.POINTER(PREFIX)), ('voice_latches', C.POINTER(I)),
                ('inputs', C.POINTER(I)), ('processed', C.POINTER(I)),
                ('selected', C.POINTER(I)), ('open', C.POINTER(I)),
                ('gauge_y', C.POINTER(F)), ('scale', C.POINTER(F)),
                ('plain_scheduled', C.POINTER(I)), ('previous_flow', C.POINTER(S)),
                ('targets', C.POINTER(PAIR)), ('alternate', C.POINTER(I)),
                ('voice_volume', C.POINTER(I)), ('effect_volume', C.POINTER(I)),
                ('speech_name', P)]


Key = C.CFUNCTYPE(I, P, U, U, C.POINTER(U), P)
RawKey = C.CFUNCTYPE(I, P, U, C.POINTER(U), P)
Audio = C.CFUNCTYPE(I, P, C.POINTER(AudioCall), C.POINTER(I), P)
Load = C.CFUNCTYPE(I, P, U, C.c_char_p, P)
Expression = C.CFUNCTYPE(I, P, I, I, I, P)
Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Request = C.CFUNCTYPE(I, P, U, P)
Write = C.CFUNCTYPE(I, P, U, I, I, P)
Target = C.CFUNCTYPE(I, P, U, C.POINTER(F), P)
CameraCall = C.CFUNCTYPE(I, P, I, I, C.POINTER(U), U, C.POINTER(U), P)
Manual = C.CFUNCTYPE(I, P, I, C.POINTER(I), P)
Pick = C.CFUNCTYPE(I, P, C.POINTER(F), C.POINTER(I), P)
Choose = C.CFUNCTYPE(I, P, C.POINTER(I), C.POINTER(I), P)
Begin = C.CFUNCTYPE(I, P, P)
Action = C.CFUNCTYPE(I, P, C.POINTER(Input), F, P)
Random = C.CFUNCTYPE(I, P, C.POINTER(I), P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('key', Key), ('raw_key', RawKey), ('audio', Audio),
                ('load', Load), ('expression', Expression), ('active', Active),
                ('request', Request), ('write', Write), ('target', Target),
                ('camera', CameraCall), ('manual', Manual), ('pick', Pick),
                ('choose', Choose), ('begin', Begin), ('action', Action), ('random', Random)]


STATE_FIELDS = [('fov', 0x556968), ('replay_elapsed', 0x6ea360),
                ('replay_after', 0x6ddebc), ('manual_mode', 0x6ddec4),
                ('expression_override', 0x6dde98)]
EXTRA = [('substate', 0x6ea358), ('mode', 0x6dde94), ('choice', 0x722104),
         ('reset_c', 0x6ddea8), ('configuration', 0x6e9fa8), ('camera_words', 0x6ea028),
         ('voice_latches', 0x6ea2c0), ('inputs', 0x6ea170), ('processed', 0x6ea178),
         ('selected', 0x721ed8), ('open', 0x72210c), ('gauge_y', 0x721e24),
         ('scale', 0x721ad0), ('plain_scheduled', 0xb53c38), ('previous_flow', 0x721ad4),
         ('targets', 0x721f90), ('alternate', 0x6afd38), ('voice_volume', 0xbe9a08),
         ('effect_volume', 0xbe9a10), ('speech_name', 0x722224)]
OWNERS = ['state', 'frame', 'control', 'auxiliary', 'camera', 'presets',
          'group_prefix', 'input', 'active', 'nodes', 'descriptors',
          'present', 'playing', 'status_error', 'loaded', 'random_index'] + [n for n, _ in EXTRA]
WRITES = {0x48e5cd: (5, 0, 1), 0x48e5dd: (6, 1, 7), 0x48e5ed: (7, 1, 6),
          0x48e5fc: (10, 1, 11), 0x48e60c: (11, 1, 10)}
STATUS = {0x48df59: 0, 0x48dfdf: 0, 0x48e326: 1}


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
    yield 0x71b37c, C.addressof(f.presets), C.sizeof(f.presets)
    for name, address in STATE_FIELDS:
        yield field_view(f.state, name, address)
    for name, address in EXTRA:
        value = getattr(f, name)
        yield address, C.addressof(value), C.sizeof(value)
    for i in range(5):
        yield 0x6ea180 + i * 64, C.addressof(f.group_prefix[i]), 12
    yield PRIMARY + 0x140, C.addressof(f.active), 4
    for i in range(32):
        yield PRIMARY + 0x200 + i * 156, C.addressof(f.descriptors[i]), 8
    for i, node in enumerate(NODES):
        yield node + 0xf0, C.addressof(f.nodes) + i * 12, 12


class Fixture:
    def clone(self):
        f = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(f, name, type(value).from_buffer_copy(value))
        for name in ['seconds', 'keys', 'raw_keys', 'pick_result', 'chosen',
                     'camera_result', 'accepted', 'randoms', 'mutate_at', 'mutations']:
            setattr(f, name, getattr(self, name))
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in OWNERS)

    def effect(self, event, index):
        kind = event[0]
        if kind == 'audio':
            _, op, slot, _cue, _bank, _flags, _volume = event
            if op in [3, 4]:
                self.present[slot] = 1
            if op in [2, 3, 4]:
                self.playing[slot] = self.present[slot]
        elif kind == 'load':
            self.present[event[1]] = self.loaded[event[1]]
            self.playing[event[1]] = 0
        elif kind == 'expression':
            self.auxiliary.expression_a, self.auxiliary.expression_b = event[1:3]
        elif kind == 'request':
            self.active.value = event[1]
        elif kind == 'write':
            self.descriptors[event[1]][event[2]] = event[3]
        elif kind == 'camera':
            self.camera.matrix[12] = 17 + index
            self.camera.pitch = F(self.camera.pitch - .125).value
        elif kind == 'manual' and self.accepted:
            self.auxiliary.selection = event[1]
        elif kind == 'pick':
            self.frame.camera_cached = self.pick_result[1]
        elif kind == 'choose':
            self.frame.camera_event = 12
        elif kind == 'begin':
            self.frame.camera_event = 23
        elif kind == 'action':
            self.frame.camera_cached = 17
        elif kind == 'random':
            self.random_index.value += 1
        if self.mutate_at == index:
            for name, value in self.mutations.items():
                owner_name, field = name.split('.')
                owner = getattr(self, owner_name)
                if field.isdigit(): owner[int(field)] = value
                else: setattr(owner, field, value)

    def key(self, code, mode):
        bit = [0, 1, 0x5a, 0x33450].index(code) + mode * 4
        return 0xa50100 | (128 if self.keys & (1 << bit) else 0)


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.word(0x721b28, PRIMARY)
        self.word(0x300d024, 0x300d100)
        self.word(0x53f2f0, 0x300d200)
        self.word(0x53f214, 0x300d240)
        self.word(0x53f2ec, 0x300d280)
        for node, pointer in enumerate(NODES): self.word(0x721ef4 + node * 4, pointer)
        for pointer in SOUNDS: self.word(pointer, 0x300d000)
        for address in [*WRITES, *STATUS, 0x42cf0e, 0x4dfb96, 0x4e0b7e,
                        0x4e0ecb, 0x4e0956, 0x4ad2bf, 0x4946b4, 0x49490a,
                        0x4974d1, 0x47c334, 0x47cb41, 0x4949dc, 0x4018c8,
                        0x48e75b, 0x4b76c2, 0x534a34, 0x300d100,
                        0x300d200, 0x300d240, 0x300d280]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def cstring(self, address):
        data = bytearray()
        for i in range(260):
            value = self.u.mem_read(address + i, 1)[0]
            if not value: return bytes(data)
            data.append(value)
        raise AssertionError('unterminated native string')

    def sync(self, read):
        for address, pointer, size in views(self.f):
            if read: C.memmove(pointer, bytes(self.u.mem_read(address, size)), size)
            else: self.u.mem_write(address, C.string_at(pointer, size))
        if not read:
            for i, sound in enumerate(SOUNDS):
                self.word(0x722334 + i * 0x120, sound if self.f.present[i] else 0)

    def emit(self, event):
        self.trace.append((event, self.f.snapshot()))
        if self.fail_at == len(self.trace):
            self.interrupted = True
            self.u.emu_stop()
            return False
        self.f.effect(event, len(self.trace))
        self.sync(False)
        return True

    def hook(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.integer(sp)
        args = struct.unpack('<6I', u.mem_read(sp + 4, 24))
        self.sync(True)
        f = self.f
        result, cleanup = 0, 4
        if address in WRITES:
            self.emit(('write', *WRITES[address]))
            return  # The native descriptor store still executes.
        if address in STATUS:
            self.emit(('audio', 0, STATUS[address], 0, 0, 0, 0))
            return  # Run the original presence/HRESULT/playing-bit sequence.
        if address == 0x42cf0e:
            f.camera.fov = struct.unpack('<f', u.mem_read(sp + 4, 4))[0]
        elif address == 0x300d100:
            slot = SOUNDS.index(args[0])
            self.word(args[1], int(f.playing[slot]))
            result, cleanup = (-1 if f.status_error[slot] else 0), 12
        elif address == 0x300d200:
            text = self.cstring(args[1]) % I(args[2]).value
            u.mem_write(args[0], text + b'\0')
            result = len(text)
        elif address == 0x300d240:
            u.mem_write(args[0], self.cstring(args[1]) + b'\0')
            result, cleanup = args[0], 12
        else:
            if address == 0x4dfb96:
                event = ('expression', *[I(v).value for v in args[:3]])
            elif address == 0x4e0956:
                event = ('load', args[1], self.cstring(args[0]).decode('ascii'))
            elif address in [0x4e0b7e, 0x4e0ecb]:
                assert args[0] == 0x71af38
                event = (('camera', 0) if address == 0x4e0b7e else
                         ('camera', 1, I(args[1]).value, *args[2:6]))
                result = f.camera_result
            elif address == 0x4ad2bf:
                slot = 5 if ret == 0x48e588 else 0
                assert args[0] == (SOUNDS[slot] if f.present[slot] else 0)
                event = ('audio', 2, slot, 0, 0, I(args[1]).value, I(args[2]).value)
            elif address == 0x4946b4:
                event = ('audio', 3, args[1], I(args[0]).value, 0, args[2] & 255, f.voice_volume.value)
            elif address == 0x49490a:
                event = ('audio', 4, args[2], I(args[0]).value, I(args[1]).value,
                         args[3] & 255, f.voice_volume.value)
            elif address == 0x4974d1:
                event, result = ('manual', I(args[0]).value), f.accepted
            elif address == 0x47c334:
                assert args[2] == 4
                event = ('pick', *struct.unpack('<2f', u.mem_read(sp + 4, 8)))
                result = f.pick_result[0]
            elif address == 0x47cb41:
                event = ('choose', *[I(v).value for v in args[:2]])
                result = f.chosen
            elif address == 0x4949dc:
                event = ('begin',)
            elif address == 0x4018c8:
                assert args[0] == PRIMARY
                event = ('request', args[1])
            elif address == 0x48e75b:
                assert self.integer(sp + 48) == 0x70d370
                event = ('action', bytes(u.mem_read(sp + 4, 44)), f.seconds)
            elif address == 0x4b76c2:
                assert args[2] == 0
                result = f.key(args[0], args[1])
                event = ('key', args[0], args[1], result)
            elif address == 0x300d280:
                result = f.raw_keys[[0x43, 0x56, 0x20].index(args[0])]
                event, cleanup = ('raw_key', args[0], result), 8
            elif address == 0x534a34:
                result = f.randoms[f.random_index.value]
                event = ('random', result)
            else: raise AssertionError(hex(address))
            if not self.emit(event): return
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture, fail_at=0):
        self.f = fixture.clone()
        self.trace, self.fail_at, self.interrupted = [], fail_at, False
        self.sync(False)
        self.u.mem_write(0x733700, struct.pack('<f', self.f.seconds))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop) + bytes(self.f.input)
                         + struct.pack('<I', 0x70d370))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(0x48d8e9, self.stop, count=100000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True)
        return self.f, self.trace


def portable(lib, original, fail_at=0, missing=None):
    f = original.clone()
    trace, callbacks, errors, used = [], [], [], set()
    def emit(event):
        trace.append((event, f.snapshot()))
        if fail_at == len(trace): return 0
        f.effect(event, len(trace))
        return 1
    def decorate(typ):
        def wrap(fn):
            def checked(*args):
                used.add(fn.__name__)
                try: return fn(*args)
                except Exception as exc:
                    errors.append(repr(exc))
                    return 0
            callback = typ(checked)
            callbacks.append(callback)
            return callback
        return wrap
    @decorate(Key)
    def key(_, code, mode, out, _e):
        out[0] = f.key(code, mode)
        return emit(('key', code, mode, out[0]))
    @decorate(RawKey)
    def raw_key(_, code, out, _e):
        out[0] = f.raw_keys[[0x43, 0x56, 0x20].index(code)]
        return emit(('raw_key', code, out[0]))
    @decorate(Audio)
    def audio(_, call, out, _e):
        c = call.contents
        out[0] = bool(f.present[c.slot] and f.playing[c.slot] and not f.status_error[c.slot])
        return emit(('audio', *[getattr(c, n) for n, _ in AudioCall._fields_]))
    @decorate(Load)
    def load(_, slot, name, _e):
        return emit(('load', slot, name.decode('ascii')))
    @decorate(Expression)
    def expression(_, a, b, mode, _e): return emit(('expression', a, b, mode))
    @decorate(Active)
    def active(_, out, _e):
        out[0] = f.active.value
        return 1
    @decorate(Request)
    def request(_, slot, _e): return emit(('request', slot))
    @decorate(Write)
    def write(_, slot, field, value, _e): return emit(('write', slot, field, value))
    @decorate(Target)
    def target(_, node, out, _e):
        assert node in [0, 5, 13]
        for i in range(3): out[i] = f.nodes[node * 3 + i]
        return 1
    @decorate(CameraCall)
    def camera(_, mode, choice, offset, extra, out, _e):
        out[0] = f.camera_result
        return emit(('camera', 0) if mode == 0 else ('camera', mode, choice, *offset[:3], extra))
    @decorate(Manual)
    def manual(_, proposed, out, _e):
        out[0] = f.accepted
        return emit(('manual', proposed))
    @decorate(Pick)
    def pick(_, point, out, _e):
        out[0] = f.pick_result[0]
        return emit(('pick', *point[:2]))
    @decorate(Choose)
    def choose(_, point, out, _e):
        out[0] = f.chosen
        return emit(('choose', *point[:2]))
    @decorate(Begin)
    def begin(_, _e): return emit(('begin',))
    @decorate(Action)
    def action(_, inputs, seconds, _e): return emit(('action', bytes(inputs.contents), seconds))
    @decorate(Random)
    def random_value(_, out, _e):
        out[0] = f.randoms[f.random_index.value]
        return emit(('random', out[0]))
    ops = Ops(None, key, raw_key, audio, load, expression, active, request, write,
              target, camera, manual, pick, choose, begin, action, random_value)
    if missing:
        setattr(ops, missing, dict(Ops._fields_)[missing]())
    bindings = Bindings()
    for name, typ in Bindings._fields_:
        setattr(bindings, name, C.cast(C.byref(getattr(f, name)), typ))
    error = C.create_string_buffer(256)
    ok = lib.bk_ending_selected_control_step(C.byref(f.state), C.byref(bindings),
            C.byref(f.input), f.seconds, C.byref(ops), error)
    assert not errors, errors
    if 'random_value' in used:
        used.remove('random_value')
        used.add('random')
    return bool(ok), f, trace, error.value.decode(), used


def fixture(rng, case):
    f = Fixture()
    f.frame, f.control, f.auxiliary, f.camera = Frame(), Control(), Auxiliary(), Camera()
    f.state = State(rng.choice([.19999998, .2, .25, 1]), rng.choice([0, 9.999, 10, 30]),
                    rng.choice([-1, 0, 10, 30]), rng.choice([0, 3, 5]), -7)
    f.presets = Presets()
    C.memmove(C.byref(f.presets), bytes((F * 24)(*[i * 1.25 - 8 for i in range(24)])), 96)
    f.input = Input((U * 11)(*[rng.getrandbits(32) for _ in range(11)]))
    f.input.words[6:8] = [rng.choice([0, 0, 0, 1, 0xffffffff]) for _ in range(2)]
    f.input.words[9:11] = [rng.choice([0, 37, 1920, -1, -317, -2147483648, 2147483647]) & 0xffffffff for _ in range(2)]
    f.active = I(rng.choice([1, 1, 4, 4, 4, 5, 6, 7, 10, 11]))
    f.nodes = (F * 117)(*[i * .75 - 19.5 for i in range(117)])
    f.descriptors = (PAIR * 32)(*[PAIR(17 + i, 31 - i) for i in range(32)])
    f.configuration = (CONFIG * 5)(*[CONFIG(*range(6)) for _ in range(5)])
    f.camera_words = (I * 75)(*range(75))
    f.group_prefix = (PREFIX * 5)(*[PREFIX(0, i + 19, i + 21) for i in range(5)])
    f.voice_latches = (I * 20)(*[0 if i % 4 == 0 else i + 11 for i in range(20)])
    f.inputs, f.processed = PAIR(7, 8), PAIR(9, 10)
    f.targets = (PAIR * 39)(*[PAIR(i * 9 - 100, i * 17 - 300) for i in range(39)])
    f.alternate = PAIR(-37, 208)
    f.speech_name = (C.c_char * 32).from_buffer_copy(b'previous.wav\0' + b'\x35' * 19)
    for name in ['substate', 'mode', 'choice', 'reset_c', 'selected', 'open',
                 'gauge_y', 'scale', 'plain_scheduled', 'previous_flow',
                 'voice_volume', 'effect_volume']:
        typ = dict(Bindings._fields_)[name]._type_
        setattr(f, name, typ(rng.randrange(3)))
    f.substate.value = case % 4
    f.mode.value = rng.choice([0, 1, 2, 2, 3, 4])
    f.open.value = rng.choice([0, 0, 1])
    f.selected.value = rng.choice([-1, 0, 1, 2, 3, 4, 5])
    f.previous_flow.value = rng.choice([8, 24, 24, -1])
    f.voice_volume.value, f.effect_volume.value = -713, -411
    f.scale.value, f.gauge_y.value = rng.choice([.375, .5, .75, 1]), 374.5
    f.frame.group = case // 16 % 5
    f.frame.camera_mode = rng.choice([0, 0, 1, 2, 4])
    f.frame.camera_clip = rng.randrange(3)
    f.frame.camera_cached = rng.randrange(39)
    f.frame.camera_manual = rng.choice([0, 1, 1, 255])
    f.frame.auxiliary_mode = rng.choice([0, 0, 0, 1])
    f.frame.camera_values[:] = [rng.getrandbits(32) for _ in range(3)]
    for row in f.frame.camera_table: row[:] = [rng.getrandbits(32) for _ in range(4)]
    f.control.target_choice = rng.randrange(3)
    f.control.toggles[:] = [rng.choice([0, 1, 255]) for _ in range(8)]
    f.control.saved_camera[:] = [17] * 16
    f.auxiliary.gate = 4 if case % 16 < 4 else 1
    if case % 16 == 14: f.auxiliary.gate = 2
    if case % 16 == 15: f.auxiliary.gate = 3
    f.auxiliary.variant = (case // 5) % 2
    f.auxiliary.selection = rng.choice([0, 1, 2, 0x7fffffff, -1])
    f.auxiliary.pending = rng.choice([-1, 0, 1, 1, 2, 3])
    f.auxiliary.index, f.auxiliary.base = -1, -7
    f.auxiliary.progress = rng.choice([0, .39999998, .4, .40001, .59999996, .6, .79999995, .8, 1])
    f.camera.pose.world[:] = f.camera.matrix[:] = IDENTITY
    f.camera.pitch, f.camera.fov = 7.25, .7
    f.keys = rng.getrandbits(12) if case % 5 else 0
    f.raw_keys = [rng.choice([0xaaaa0001, 0x55557fff, 0xcccc8000, 0x3333ffff]) for _ in range(3)]
    f.present = (I * 6)(*[rng.choice([0, 1, 1]) for _ in range(6)])
    f.playing = (I * 6)(*[rng.choice([0, 0, 1]) for _ in range(6)])
    f.status_error = (I * 6)(*[rng.choice([0, 0, 1]) for _ in range(6)])
    f.loaded = (I * 6)(*[rng.randrange(2) for _ in range(6)])
    f.random_index = I()
    f.randoms = [rng.choice([-2147483648, -11, -1, 0, 6, 32767, 2147483647]) for _ in range(16)]
    f.pick_result = (rng.choice([0, 1, 1, 2]), rng.randrange(39))
    f.chosen, f.accepted = rng.choice([-1, 0, 1]), rng.choice([0, 1, -1])
    f.camera_result = rng.choice([0, 1, 256, 0xa500ff])
    f.seconds = F(rng.choice([0, .000001, 1/60, .5, .99999994, 1, 1.00000012, 2])).value
    f.mutate_at, f.mutations = 0, {}
    return f


def compare(want, got, expected_trace, trace, label):
    events, expected_events = [e for e, _ in trace], [e for e, _ in expected_trace]
    assert events == expected_events, (label, 'calls', events, expected_events)
    for i, ((_, have), (_, expected)) in enumerate(zip(trace, expected_trace)):
        assert have == expected, (label, 'callback', i, events[i],
                [(OWNERS[j], g.hex(), w.hex()) for j, (g, w) in enumerate(zip(have, expected)) if g != w][:4])
    have, expected = got.snapshot(), want.snapshot()
    assert have == expected, (label, 'final',
            [(OWNERS[j], g.hex(), w.hex()) for j, (g, w) in enumerate(zip(have, expected)) if g != w][:4])


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=16000)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-ending-selected-control.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == EXE_SHA256
    lib, native = library(), Native(exe)
    lib.bk_ending_selected_control_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
                                                   C.POINTER(Input), F, C.POINTER(Ops), P]
    lib.bk_ending_selected_control_step.restype = I
    lib.bk_ending_selected_control_initial.argtypes = []
    lib.bk_ending_selected_control_initial.restype = State
    assert bytes(lib.bk_ending_selected_control_initial()) == b''.join(
        bytes(native.u.mem_read(address, 4)) for _, address in STATE_FIELDS)
    constants = {hex(a): struct.unpack('<f', native.u.mem_read(a, 4))[0]
                 for a in [0x53f3e4, 0x53f3ac, 0x53f3cc, 0x53f3a4, 0x53f410, 0x53f494]}
    assert list(constants.values()) == [F(v).value for v in [.01, 1, .2, .1, 2, .4]]
    rng, digest = random.Random(0x48d8e9), hashlib.sha256()
    calls = failures = mutations = rejected = 0
    operations, gates, openings, variants, missing_cases = set(), set(), set(), set(), {}
    for case in range(args.cases):
        original = fixture(rng, case)
        if case % 31 == 0:
            original.auxiliary.progress = rng.choice([float('nan'), float('inf'), -float('inf')])
        if case % 97 == 0:
            original.auxiliary.gate = rng.choice([0, -1, -2147483648, 2147483647])
        if case % 7 == 0:
            original.mutate_at = 1 + case % 7
            original.mutations = {
                'frame.group': (original.frame.group + 2) % 5,
                'frame.camera_clip': 2, 'frame.camera_mode': 0,
                'auxiliary.variant': 1 - original.auxiliary.variant,
                'auxiliary.pending': [0, 1, 2, 3, -1][case % 5],
                'voice_volume.value': -907, 'plain_scheduled.value': 1,
                'control.target_choice': (original.control.target_choice + 1) % 3,
                'nodes.0': -55.125}
        want, expected_trace = native.run(original)
        ok, got, trace, error, used = portable(lib, original)
        assert ok, (case, error, [v[0] for v in trace])
        compare(want, got, expected_trace, trace, case)
        for name in used:
            missing_cases.setdefault(name, original.clone())
        operations.update(e[0] for e, _ in trace)
        gates.add(original.auxiliary.gate)
        variants.add(original.auxiliary.variant)
        if original.auxiliary.gate == 4: openings.add(original.substate.value)
        calls += len(trace)
        mutations += 0 < original.mutate_at <= len(trace)
        for blob in got.snapshot(): digest.update(blob)
        if case % 17 == 0 and trace:
            fail_at = 1 + case % len(trace)
            want_prefix, expected_prefix = native.run(original, fail_at)
            ok, got_prefix, prefix, _, _ = portable(lib, original, fail_at)
            assert not ok
            compare(want_prefix, got_prefix, expected_prefix, prefix, (case, 'prefix'))
            failures += 1
    native_edge_cases = 0
    for substate, variant in [(s, 0) for s in [4, 127, 128, 255]] + [
            (0, v) for v in [-2147483648, -1, 2, 2147483647]]:
        original = fixture(rng, 0)
        original.auxiliary.gate, original.substate.value = 4, substate
        original.auxiliary.variant, original.auxiliary.selection = variant, 2147483647
        want, expected_trace = native.run(original)
        ok, got, trace, error, _ = portable(lib, original)
        assert ok, error
        compare(want, got, expected_trace, trace, ('native edge', substate, variant))
        native_edge_cases += 1
    for name, original in missing_cases.items():
        ok, _, _, error, _ = portable(lib, original, missing=name)
        assert not ok and 'missing' in error, (name, error)
    for value in [float('nan'), float('inf'), -1.0]:
        original = fixture(rng, 0)
        original.seconds = value
        ok, got, trace, error, _ = portable(lib, original)
        assert not ok and not trace and got.snapshot() == original.snapshot(), error
        rejected += 1
    for field, value in [('group', 255), ('camera_clip', -1), ('target_choice', 3)]:
        original = fixture(rng, 3)
        original.auxiliary.gate, original.substate.value = 4, 3
        if field == 'target_choice': original.control.target_choice = value
        else: setattr(original.frame, field, value)
        ok, _, _, error, _ = portable(lib, original)
        assert not ok and 'outside' in error, (field, error)
        rejected += 1
    if args.cases >= 16000:
        assert openings == {0, 1, 2, 3} and variants == {0, 1}
        assert {1, 2, 3, 4} <= gates
        assert set(missing_cases) == {n for n, _ in Ops._fields_[1:]}, missing_cases.keys()
        assert mutations > 400 and calls > 20000 and failures > 400
    report = dict(passed=True, exe_sha256=EXE_SHA256, frames=args.cases, calls=calls,
                  failure_prefixes=failures, live_mutations=mutations,
                  bounded_rejections=rejected, missing_services=len(missing_cases),
                  native_edge_cases=native_edge_cases,
                  operations=sorted(operations), gates=sorted(gates), openings=sorted(openings),
                  variants=sorted(variants), constants=constants, table_words=210,
                  state_sha256=digest.hexdigest(), max_error=0, scope=__doc__,
                  full_loader=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS selected control', args.cases, 'frames', calls, 'calls', failures,
          'failure prefixes', mutations, 'live mutations', rejected, 'rejections',
          len(missing_cases), 'missing services', digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
