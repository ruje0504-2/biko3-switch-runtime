"""Pinned48E75B action caller, all eight modes and ordered shared state.

Execute the original parent,495469/4952C8 numerical children, descriptor
stores, recording copies and working-flag writes. Media, configured clip
requests, UI texture updates and camera operations are observing services.
Their callbacks may change live state; later failures preserve the executed
prefix. This does not validate the complete4D1025 scene or Switch hardware.
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
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX,
    UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW,
    UC_X86_REG_FPSW, UC_X86_REG_FPTAG)
import original_ending_selected_control_oracle as parent
from original_ending_selected_motion_oracle import Clip, bind as bind_motion
from original_ending_tertiary_control_oracle import Record, Records, RECORDS
from original_matrix_oracle import machine
from model_binding import ROOT, library

I, U, F, B, S, P = parent.I, parent.U, parent.F, parent.B, parent.S, parent.P
PRIMARY, NODES, SOUNDS = parent.PRIMARY, parent.NODES, parent.SOUNDS
EXE_SHA256 = parent.EXE_SHA256
UI_BASE, UI_STRIDE, UI_COUNT = 0x734058, 0x16c, 80
BYTE_OFFSETS = (0x134, 0x166, 0x167)
ACTION_SERVICES = frozenset([
    'key', 'audio', 'load', 'expression', 'active', 'request', 'write',
    'target', 'camera', 'random_value', 'clip', 'source', 'pointer', 'drag',
    'hit', 'clock', 'stop', 'repeat', 'ui_byte', 'ui_uv_reset', 'ui_fade'])


class State(C.Structure):
    _fields_ = [('end_difference', F), ('clock_sample', U), ('previous_clock', U),
        ('saved_toggle', B), ('replay_variant', S), ('saved_target', U * 3),
        ('drag_result', F), ('saved_camera', U * 4), ('speech_elapsed', F),
        ('speech_ready', S), ('motion_direction', S), ('motion_state', I),
        ('counter', I), ('completion_elapsed', F), ('crossings', I),
        ('previous_motion', F), ('previous_progress', F),
        ('diagnostic_crossings', I), ('diagnostic_ms', I)]


class Bindings(C.Structure):
    _fields_ = [('control', parent.Bindings), ('controller', C.POINTER(parent.State)),
        ('presets', C.POINTER(parent.Presets)), ('selected', C.POINTER(I)),
        ('records', C.POINTER(Records)), ('working', C.POINTER(B * 8))] + [
        (name, C.POINTER(I)) for name in ['face_mode', 'once', 'random_latch',
        'reset_a', 'small_motion', 'large_motion', 'fast_motion', 'reverse',
        'previous_sound', 'stage', 'reset_340', 'reset_354', 'words_318',
        'group_seen']] + [('group_suffix', C.POINTER(I * 12)),
        ('sequence_elapsed', C.POINTER(F)), ('sequence', C.POINTER(B)),
        ('animation_scale', C.POINTER(F))]


ReadClip = C.CFUNCTYPE(I, P, U, C.POINTER(Clip), P)
Source = C.CFUNCTYPE(I, P, U, F, P)
Pointer = C.CFUNCTYPE(I, P, C.POINTER(parent.Input), P)
Drag = C.CFUNCTYPE(I, P, C.POINTER(F), C.POINTER(F), P)
Hit = C.CFUNCTYPE(I, P, U, C.POINTER(I), C.POINTER(I), P)
Clock = C.CFUNCTYPE(I, P, C.POINTER(U), P)
Stop = C.CFUNCTYPE(I, P, U, P)
UiByte = C.CFUNCTYPE(I, P, U, I, B, P)
UiFade = C.CFUNCTYPE(I, P, U, F, P)


class Ops(C.Structure):
    _fields_ = [('control', parent.Ops), ('clip', ReadClip), ('source', Source),
        ('pointer', Pointer), ('drag', Drag), ('hit', Hit), ('choices', C.POINTER(I)),
        ('clock', Clock), ('stop', Stop), ('repeat', Stop), ('ui_byte', UiByte),
        ('ui_uv_reset', Stop), ('ui_fade', UiFade)]


STATE_FIELDS = list(zip([name for name, _ in State._fields_], [
    0x6dde88, 0x6dde8c, 0x6e9fa0, 0x6dde9c, 0x6ddea5, 0x6ddeb0,
    0x6ea154, 0x6ea15c, 0x6ea364, 0x55696c, 0x6ea368, 0x6ea36c,
    0x6ea370, 0x6ea374, 0x6ea378, 0x6ea37c, 0x6ea380, 0x725704, 0x72570c]))
EXTRA = [('face_mode', 0x721dfc), ('once', 0x6ea310), ('random_latch', 0x6ddea0),
    ('reset_a', 0x6ea344), ('small_motion', 0x6dde90), ('large_motion', 0x6ddec8),
    ('fast_motion', 0x6ea16c), ('reverse', 0x6ea314), ('previous_sound', 0x5546a0),
    ('stage', 0x6ea348), ('reset_340', 0x6ea340), ('reset_354', 0x6ea354),
    ('words_318', 0x6ea318), ('sequence_elapsed', 0x6ea350),
    ('sequence', 0x6ea34c), ('sequence_count', 0x6ea34d),
    ('animation_scale', 0x55469c), ('working', 0x721dc6),
    ('choices', 0x7220e4), ('points', 0x7220c8), ('menu_width', 0x7389e8)]
OWNERS = list(dict.fromkeys(parent.OWNERS + ['action_state', 'clips', 'chains',
    'group_seen', 'group_suffix', 'ui_bytes', 'ui_uv', 'ui_alpha', 'records',
    'clock_index'] + [name for name, _ in EXTRA]))
CLIP_FIELDS = [('duration', 0x50), ('start', 0x54), ('end', 0x58),
               ('source', 0x60), ('rate', 0x5c), ('elapsed', 0x44)]
# Entry to each original presence/GetStatus block. Dynamic effect indices
# below are the already multiplied register values at the native CMP.
STATUS_SITES = {0x48efaf: ('reg', UC_X86_REG_ECX), 0x48f046: 5,
    0x48f513: 0, 0x48f59b: 1, 0x48f8b3: 0, 0x48fe43: 0, 0x48feca: 1,
    0x4902ab: 0, 0x49069d: 0, 0x490725: 1, 0x490898: 0, 0x490c29: 0,
    0x490cb0: 1, 0x490eb7: 0, 0x491874: ('reg', UC_X86_REG_EAX),
    0x49190b: 5, 0x49243b: 0, 0x492731: 1, 0x492c60: 5,
    0x492ea4: 1, 0x492ef6: 0, 0x492fdd: ('reg', UC_X86_REG_ECX),
    0x49312d: 0, 0x493366: 0, 0x493705: 1, 0x4939e2: 0,
    0x493bb9: ('reg', UC_X86_REG_ECX)}


def views(f):
    for address, pointer, size in parent.views(f):
        if PRIMARY + 0x200 <= address < PRIMARY + 0x200 + 32 * 156:
            continue
        yield address, pointer, size
    for name, address in STATE_FIELDS:
        yield parent.field_view(f.action_state, name, address)
    for name, address in EXTRA:
        value = getattr(f, name)
        yield address, C.addressof(value), C.sizeof(value)
    for g in range(5):
        yield 0x6ea18c + g * 64, C.addressof(f.group_seen) + g * 4, 4
        yield 0x6ea190 + g * 64, C.addressof(f.group_suffix[g]), 48
    for slot in range(32):
        base = PRIMARY + 0x190 + slot * 156
        for name, offset in CLIP_FIELDS:
            yield parent.field_view(f.clips[slot], name, base + offset)
        yield base + 0x70, C.addressof(f.chains[slot]), 8
    # Other UI slots alias the existing common fade/pause owners above.
    #48E75B touches only these three slots; do not overwrite those aliases
    # with a second independently initialized UI fixture.
    for slot in [52, 53, 54]:
        for n, offset in enumerate(BYTE_OFFSETS):
            yield UI_BASE + slot * UI_STRIDE + offset, C.addressof(f.ui_bytes[slot]) + n, 1


class Fixture(parent.Fixture):
    def clone(self):
        f = Fixture()
        for name in OWNERS:
            value = getattr(self, name)
            setattr(f, name, type(value).from_buffer_copy(value))
        for name in ['seconds', 'keys', 'raw_keys', 'pick_result', 'chosen',
            'camera_result', 'accepted', 'randoms', 'mutate_at', 'mutations',
            'clocks', 'hits']:
            setattr(f, name, getattr(self, name))
        return f

    def snapshot(self):
        # Keep complete recording-lane comparisons without retaining hundreds
        # of kilobytes in each service-prefix trace.
        return tuple(hashlib.sha256(bytes(self.records)).digest() if n == 'records'
                     else bytes(getattr(self, n)) for n in OWNERS)

    def effect(self, event, index):
        kind = event[0]
        if kind in ['request', 'repeat']:
            self.active.value = event[1]
        elif kind == 'audio':
            _, op, slot, _cue, _bank, _flags, _volume = event
            if op in [3, 4]: self.present[slot] = 1
            if op in [2, 3, 4]: self.playing[slot] = self.present[slot]
            if op == 1: self.playing[slot] = 0
        elif kind == 'stop': self.playing[event[1]] = 0
        elif kind == 'load':
            self.present[event[1]] = self.loaded[event[1]]
            self.playing[event[1]] = 0
        elif kind == 'expression':
            self.auxiliary.expression_a, self.auxiliary.expression_b = event[1:3]
        elif kind == 'camera':
            self.camera.matrix[12] = 17 + index
            self.camera.pitch = F(self.camera.pitch - .125).value
        elif kind == 'random': self.random_index.value += 1
        elif kind == 'clock': self.clock_index.value += 1
        elif kind == 'ui_uv_reset': self.ui_uv[event[1]][:] = (0, 0, 1, 1)
        elif kind == 'ui_fade': self.ui_alpha[event[1]] = event[2]
        if self.mutate_at == index:
            for path, value in self.mutations.items():
                owner_name, field = path.split('.')
                owner = getattr(self, owner_name)
                if field.isdigit(): owner[int(field)] = value
                else: setattr(owner, field, value)


def fixture(rng, case):
    f = Fixture()
    baseline = parent.fixture(rng, case)
    f.__dict__.update(baseline.__dict__)
    f.mode.value = case % 8
    # Modes0/3 consume release-mode3, whose key bits begin at bit12.
    f.keys = rng.getrandbits(16)
    f.auxiliary.selection = case // 8 % 3
    f.auxiliary.variant = case // 24 % 2
    f.frame.group = case // 48 % 5
    f.frame.phase = rng.choice([5, 6])
    f.choice.value = rng.randrange(2)
    f.plain_scheduled.value = rng.choice([0, 0, 1, -1])
    f.reset_c.value = rng.choice([0, 1, 9, 10, 11, 19])
    f.active.value = rng.choice([1, 4, 5, 6, 7, 10, 11, 13, 14, 15, 16, 17, 18, 20, 21, 22, 23, 24])
    f.input.words[6:8] = [rng.choice([0, 0, -2147483648, -141, -140, -139,
                                     -1, 1, 139, 140, 141, 2147483647]) & 0xffffffff
                          for _ in range(2)]
    f.input.words[9:11] = [rng.choice([0, 40, -90, 1280, 960, -2147483648,
                                      2147483647]) & 0xffffffff for _ in range(2)]
    f.action_state = State()
    a = f.action_state
    a.speech_ready = rng.choice([0, 1, 2, -1])
    a.speech_elapsed = rng.choice([0, 7.999, 8, 8.001])
    a.motion_direction = rng.choice([0, 1, 2, -128, 127])
    a.counter = rng.choice([-1, 0, 1, 2, 1999, 2000, 4999, 5000, 0x7fffffff])
    a.crossings = rng.choice([0, 5, 6, 7, 99, 100, 0x7fffffff])
    a.replay_variant = case // 8 % 2
    if f.mode.value == 7: a.counter = rng.choice([-1, 0, 1, 2] if a.replay_variant else [-1, 0, 1])
    a.clock_sample = 73
    a.previous_clock = rng.choice([0, 1, 73, 0xfffffffa])
    a.previous_motion, a.previous_progress = rng.choice([-1, 0, 1]), rng.choice([-1, 0, 1])
    a.completion_elapsed = rng.choice([0, 19.999, 20, 25])
    a.saved_toggle = 39
    a.saved_target[:] = [0x40100000, 0x40200000, 0x40300000]
    a.saved_camera[:] = [0x41a00000, 0x41b00000, 0x41c00000, 0x41d00000]
    a.end_difference, a.drag_result = 17, -9
    a.diagnostic_crossings, a.diagnostic_ms = 11, 19
    f.clips = (Clip * 32)()
    for slot, p in enumerate(f.clips):
        p.duration = rng.choice([10, 15, 20, 30, 40, 60, 80, 37])
        p.start, p.end = 30 + slot * 5, 33 + slot * 5
        p.source = rng.choice([p.start, p.start + .25, p.end, p.end + 1])
        p.rate, p.elapsed = rng.choice([.1, .5, 1, 2]), 11.125 + slot
    f.chains = (parent.PAIR * 32)(*[parent.PAIR(0, -1) for _ in range(32)])
    for name in ['face_mode', 'once', 'random_latch', 'reset_a', 'small_motion',
                 'large_motion', 'fast_motion', 'reverse', 'previous_sound',
                 'stage', 'reset_340', 'reset_354']:
        setattr(f, name, I(rng.choice([0, 1, 2])))
    f.small_motion.value, f.large_motion.value = rng.choice([0, 1, 4, 5]), rng.choice([0, 4, 5])
    f.previous_sound.value = rng.choice([-1, 0, 1, 2, 3])
    f.words_318 = (I * 10)(*range(10))
    f.group_seen = (I * 5)(*range(71, 76))
    f.group_suffix = ((I * 12) * 5)(*[(I * 12)(*[300 + g * 12 + i for i in range(12)]) for g in range(5)])
    f.sequence_elapsed, f.sequence, f.sequence_count = F(3.5), B(5), B(219)
    f.animation_scale = F(.01)
    f.working = ((B * 8) * 5)(*[(B * 8)(*[2 + i + g for i in range(8)]) for g in range(5)])
    f.records = Records()
    for g, record in enumerate(f.records.groups):
        for lane in range(2):
            record.retained[lane][:] = [0x81000000 + g * 0x10000 + lane * 0x4000 + i for i in range(10000)]
        record.actions[:] = [g * 10000 + i for i in range(10000)]
        record.count = rng.choice([0, 1, 3, 9999])
    f.ui_bytes = ((B * 3) * UI_COUNT)(*[(B * 3)(23, 29, 31) for _ in range(UI_COUNT)])
    f.ui_uv = ((F * 4) * UI_COUNT)(*[(F * 4)(.1, .2, .3, .4) for _ in range(UI_COUNT)])
    f.ui_alpha = (F * UI_COUNT)(*[.75] * UI_COUNT)
    f.choices = (I * 2)(rng.choice([-1, 0, 1, 2, 3]), rng.choice([-1, 0, 1, 2, 3]))
    f.points = (parent.PAIR * 2)(parent.PAIR(100, 80), parent.PAIR(200, 240))
    f.menu_width = F(24.5)
    f.clocks = [rng.choice([0, 1, 72, 2001, 5001, 0xffffffff]) for _ in range(16)]
    f.clock_index = U()
    f.hits = rng.randrange(4)
    # Direct Stop in cancellation requires a real speech0 buffer. Nullable
    # effect buffers and HRESULT failures remain covered independently.
    f.present[0] = f.loaded[0] = 1
    return f


class Native:
    def __init__(self, exe):
        self.u = machine(exe)
        self.stack, self.stop = 0x2008000, 0x300f000
        self.u.mem_map(0x3100000, 0x10000)
        self.word(0x721b28, PRIMARY)
        for i, node in enumerate(NODES): self.word(0x721ef4 + i * 4, node)
        for sound in SOUNDS: self.word(sound, 0x300d000)
        self.word(0x300d024, 0x300d100)
        self.word(0x300d048, 0x300d140)
        self.word(0x53f2f0, 0x300d200)
        self.word(0x53f214, 0x300d240)
        self.word(0x53f358, 0x300d300)
        self.textures = [0x3100000 + slot * 64 for slot in range(UI_COUNT)]
        for slot, texture in enumerate(self.textures):
            self.word(UI_BASE + slot * UI_STRIDE + 0x100, texture)
        for address in [*STATUS_SITES, 0x42cf0e, 0x4dfb96, 0x4e0ecb,
                0x4e0956, 0x4ad2bf, 0x4ad34a, 0x4946b4, 0x49490a,
                0x4018c8, 0x401f71, 0x4b76c2, 0x534a34, 0x495469,
                0x4952c8, 0x4a777e, 0x43f558, 0x50e479,
                0x300d100, 0x300d140, 0x300d200, 0x300d240, 0x300d300]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def integer(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def cstring(self, address):
        return bytes(self.u.mem_read(address, 260)).split(b'\0', 1)[0]

    def sync(self, read, full_records=False):
        for address, pointer, size in views(self.f):
            if read: C.memmove(pointer, bytes(self.u.mem_read(address, size)), size)
            else: self.u.mem_write(address, C.string_at(pointer, size))
        if full_records or (read and self.entry_mode in [3, 5]):
            if read: C.memmove(C.byref(self.f.records), bytes(self.u.mem_read(RECORDS, C.sizeof(Records))), C.sizeof(Records))
            else: self.u.mem_write(RECORDS, bytes(self.f.records))
        else:
            for g, record in enumerate(self.f.records.groups):
                address = RECORDS + g * C.sizeof(Record) + Record.count.offset
                if read: record.count = I(self.integer(address)).value
                else: self.word(address, record.count)
        if not read:
            for slot, sound in enumerate(SOUNDS):
                self.word(0x722334 + slot * 0x120, sound if self.f.present[slot] else 0)

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
        self.sync(True)
        f = self.f
        if address in STATUS_SITES:
            slot = STATUS_SITES[address]
            if isinstance(slot, tuple):
                value = u.reg_read(slot[1])
                assert value % 0x120 == 0 and value < 3 * 0x120
                slot = 2 + value // 0x120
            self.emit(('audio', 0, slot, 0, 0, 0, 0))
            return  # Run the actual nullable/GetStatus/HRESULT test.
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.integer(sp)
        args = struct.unpack('<6I', u.mem_read(sp + 4, 24))
        result, cleanup, event = 0, 4, None
        if address == 0x300d100:
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
        elif address == 0x42cf0e:
            f.camera.fov = struct.unpack('<f', u.mem_read(sp + 4, 4))[0]
        elif address == 0x4dfb96: event = ('expression', *[I(v).value for v in args[:3]])
        elif address == 0x4e0956: event = ('load', args[1], self.cstring(args[0]).decode('ascii'))
        elif address == 0x4e0ecb:
            assert args[0] == 0x71af38
            event = ('camera', 1, I(args[1]).value, *args[2:6])
            result = f.camera_result
        elif address in [0x4946b4, 0x49490a]:
            event = (('audio', 3, args[1], I(args[0]).value, 0, args[2] & 255, f.voice_volume.value)
                     if address == 0x4946b4 else
                     ('audio', 4, args[2], I(args[0]).value, I(args[1]).value, args[3] & 255, f.voice_volume.value))
        elif address == 0x4ad2bf:
            slot = {0x48e9f1: 0, 0x49125d: 2, 0x4912c5: 3, 0x491315: 4, 0x4928ad: 4}[ret]
            assert args[0] == (SOUNDS[slot] if f.present[slot] else 0)
            event = ('audio', 2, slot, 0, 0, I(args[1]).value, I(args[2]).value)
        elif address == 0x4ad34a:
            slot = 2 if ret == 0x491246 else f.previous_sound.value + 2
            assert 0 <= slot < 6 and args[0] == (SOUNDS[slot] if f.present[slot] else 0)
            event = ('audio', 1, slot, 0, 0, 0, 0)
        elif address == 0x300d140:
            event, cleanup = ('stop', SOUNDS.index(args[0])), 8
        elif address in [0x4018c8, 0x401f71]:
            assert args[0] == PRIMARY
            event = ('request' if address == 0x4018c8 else 'repeat', args[1])
        elif address == 0x4b76c2:
            assert args[2] == 0
            result = f.key(args[0], args[1])
            event = ('key', args[0], args[1], result)
        elif address == 0x534a34:
            result = f.randoms[f.random_index.value]
            event = ('random', result)
        elif address == 0x300d300:
            result = f.clocks[f.clock_index.value]
            event = ('clock', result)
        elif address == 0x495469:
            assert args[0] == PRIMARY
            self.emit(('pointer', bytes(u.mem_read(sp + 8, 44))))
            return  # Execute the original numerical child without substitution.
        elif address == 0x4952c8:
            assert args[0] == PRIMARY
            self.emit(('drag', *struct.unpack('<2f', u.mem_read(sp + 8, 8))))
            return  # Includes the actual x87 float return consumed by48E75B.
        elif address == 0x4a777e:
            slot = self.hit_count
            self.hit_count += 1
            point = [I(v).value for v in f.input.words[9:11]]
            wanted = tuple(F(v).value for v in [*f.points[slot], f.menu_width.value / 2, *point])
            assert struct.unpack('<5f', u.mem_read(sp + 4, 20)) == wanted
            self.word(args[5], 0)
            result = int(bool(f.hits & (1 << slot)))
            event = ('hit', slot, *point, result)
        elif address == 0x43f558:
            slot = self.textures.index(args[0])
            assert args[1:5] == (0, 0, 0x3f800000, 0x3f800000)
            event = ('ui_uv_reset', slot)
        elif address == 0x50e479:
            assert (args[0] - UI_BASE) % UI_STRIDE == 0
            event = ('ui_fade', (args[0] - UI_BASE) // UI_STRIDE,
                     struct.unpack('<f', u.mem_read(sp + 8, 4))[0])
        else: raise AssertionError(hex(address))
        if event is not None and not self.emit(event): return
        u.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp + cleanup)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, original, fail_at=0):
        self.f = original.clone()
        self.entry_mode = original.mode.value
        self.trace, self.fail_at, self.interrupted, self.hit_count = [], fail_at, False, 0
        self.sync(False, True)
        self.u.mem_write(0x733700, bytes(F(original.seconds)))
        self.u.mem_write(self.stack, struct.pack('<I', self.stop) + bytes(original.input)
                         + struct.pack('<I', 0x70d370))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.reg_write(UC_X86_REG_FPSW, 0)
        self.u.reg_write(UC_X86_REG_FPTAG, 0xffff)
        self.u.emu_start(0x48e75b, self.stop, count=300000)
        assert self.interrupted or self.u.reg_read(UC_X86_REG_EIP) == self.stop
        self.sync(True, True)
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
            value = typ(checked)
            callbacks.append(value)
            return value
        return wrap
    @decorate(parent.Key)
    def key(_, code, mode, out, _e):
        out[0] = f.key(code, mode)
        return emit(('key', code, mode, out[0]))
    @decorate(parent.Audio)
    def audio(_, call, out, _e):
        c = call.contents
        assert c.slot < 6
        out[0] = bool(f.present[c.slot] and f.playing[c.slot] and not f.status_error[c.slot])
        return emit(('audio', *[getattr(c, name) for name, _ in parent.AudioCall._fields_]))
    @decorate(parent.Load)
    def load(_, slot, name, _e): return emit(('load', slot, name.decode('ascii')))
    @decorate(parent.Expression)
    def expression(_, a, b, mode, _e): return emit(('expression', a, b, mode))
    @decorate(parent.Active)
    def active(_, out, _e): out[0] = f.active.value; return 1
    @decorate(parent.Request)
    def request(_, slot, _e): return emit(('request', slot))
    @decorate(parent.Write)
    def write(_, slot, field, value, _e):
        assert slot < 32 and field in [0, 1, 2]
        if field == 2: f.clips[slot].source = f.clips[slot].start
        else: f.chains[slot][field] = value
        return 1
    @decorate(parent.Target)
    def target(_, node, out, _e):
        assert node < 39
        for i in range(3): out[i] = f.nodes[node * 3 + i]
        return 1
    @decorate(parent.CameraCall)
    def camera(_, mode, choice, offset, extra, out, _e):
        out[0] = f.camera_result
        return emit(('camera', mode, choice, *offset[:3], extra))
    @decorate(parent.Random)
    def random_value(_, out, _e):
        out[0] = f.randoms[f.random_index.value]
        return emit(('random', out[0]))
    @decorate(ReadClip)
    def clip(_, slot, out, _e):
        if slot >= 32: return 0
        out[0] = f.clips[slot]
        return 1
    @decorate(Source)
    def source(_, slot, value, _e):
        if slot >= 32: return 0
        f.clips[slot].source = value
        return 1
    @decorate(Pointer)
    def pointer(_, inputs, e):
        if not emit(('pointer', bytes(inputs.contents))): return 0
        point = parent.PAIR(*[I(v).value for v in inputs.contents.words[9:11]])
        return lib.bk_ending_selected_motion_pointer(C.byref(f.clips[f.active.value]),
                                                      f.alternate, f.points[0], point, e)
    @decorate(Drag)
    def drag(_, values, out, e):
        if not emit(('drag', *values[:2])): return 0
        out[0] = 0
        return lib.bk_ending_selected_motion_drag(C.byref(f.clips[f.active.value]),
                                                   f.plain_scheduled.value, f.reverse.value, values, e)
    @decorate(Hit)
    def hit(_, slot, point, out, _e):
        assert slot < 2
        out[0] = int(bool(f.hits & (1 << slot)))
        return emit(('hit', slot, *point[:2], out[0]))
    @decorate(Clock)
    def clock(_, out, _e):
        out[0] = f.clocks[f.clock_index.value]
        return emit(('clock', out[0]))
    @decorate(Stop)
    def stop(_, slot, _e):
        if slot >= 6 or not f.present[slot]: return 0
        return emit(('stop', slot))
    @decorate(Stop)
    def repeat(_, slot, _e): return emit(('repeat', slot))
    @decorate(UiByte)
    def ui_byte(_, slot, field, value, _e):
        assert slot < UI_COUNT and field < 3
        f.ui_bytes[slot][field] = value
        return 1
    @decorate(Stop)
    def ui_uv_reset(_, slot, _e): return emit(('ui_uv_reset', slot))
    @decorate(UiFade)
    def ui_fade(_, slot, value, _e): return emit(('ui_fade', slot, value))
    ops = Ops()
    for name in ['key', 'audio', 'load', 'expression', 'active', 'request', 'write', 'target', 'camera']:
        setattr(ops.control, name, locals()[name])
    ops.control.random = random_value
    for name, _ in Ops._fields_[1:]:
        setattr(ops, name, f.choices if name == 'choices' else locals()[name])
    if missing:
        owner, name = (ops.control, missing[8:]) if missing.startswith('control.') else (ops, missing)
        setattr(owner, name, dict(type(owner)._fields_)[name]())
    bindings = Bindings()
    for name, typ in parent.Bindings._fields_:
        setattr(bindings.control, name, C.cast(C.byref(getattr(f, name)), typ))
    for name, typ in Bindings._fields_[1:]:
        setattr(bindings, name, C.cast(C.byref(getattr(f, 'state' if name == 'controller' else name)), typ))
    error = C.create_string_buffer(256)
    ok = lib.bk_ending_selected_action_step(C.byref(f.action_state), C.byref(bindings),
        C.byref(f.input), f.seconds, C.byref(ops), error)
    assert not errors, errors
    return bool(ok), f, trace, error.value.decode(), used


def difference(have, wanted):
    return [(OWNERS[j], [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y][:24])
            for j, (a, b) in enumerate(zip(have, wanted)) if a != b][:6]


def compare(want, got, expected, trace, label):
    events, expected_events = [e for e, _ in trace], [e for e, _ in expected]
    assert events == expected_events, (label, 'calls', events, expected_events)
    for i, ((event, have), (_, wanted)) in enumerate(zip(trace, expected)):
        assert have == wanted, (label, 'prefix', i, event, difference(have, wanted))
    have, wanted = got.snapshot(), want.snapshot()
    assert have == wanted, (label, 'final', difference(have, wanted))
    assert bytes(got.records) == bytes(want.records), (label, 'entire recording lanes')


def bind(lib):
    bind_motion(lib)
    lib.bk_ending_selected_action_step.argtypes = [C.POINTER(State), C.POINTER(Bindings),
        C.POINTER(parent.Input), F, C.POINTER(Ops), P]
    lib.bk_ending_selected_action_step.restype = I
    lib.bk_ending_selected_action_initial.argtypes = []
    lib.bk_ending_selected_action_initial.restype = State


def extra_checks(lib, native, rng, digest, missing_cases):
    counts = dict(native_edges=0, completion_copies=0, captured_descriptors=0,
                  portable_rejections=0, continuous_frames=0, missing_services=0,
                  missing_menu_labels=0)

    def check(original, label):
        want, expected = native.run(original)
        ok, got, trace, error, used = portable(lib, original)
        assert ok, (label, error)
        compare(want, got, expected, trace, label)
        for blob in got.snapshot(): digest.update(blob)
        for name in used: missing_cases.setdefault(name, original.clone())
        return got, trace

    # Native jump-table guard follows the common cooldown, even for a signed
    # group value for which no recording lane may safely be dereferenced.
    for mode in [-2147483648, -1, 8, 127, 2147483647]:
        for elapsed, seconds in [(8, 0), (7.75, .25), (8, .0001)]:
            f = fixture(rng, 0)
            f.mode.value, f.frame.group = mode, 255
            f.action_state.speech_ready, f.action_state.speech_elapsed = 1, elapsed
            f.seconds = F(seconds).value
            got, trace = check(f, ('unknown mode/cooldown', mode, elapsed, seconds))
            assert not trace
            assert got.action_state.speech_ready == (2 if seconds and elapsed == 8 else 1)
            counts['native_edges'] += 1

    for setting in [0, 1]:
        for previous, first, second, counter in [
                (0, 5000, 5011, 0), (0, 5001, 5012, 0),
                (0xfffffffa, 4, 17, 4990), (0xfffffffa, 5, 18, 4990),
                (77, 70, 79, 0), (0, 0, 1, -1)]:
            f = fixture(rng, 2)
            f.mode.value, f.keys, f.active.value = 2, 0, 6
            f.input.words[6:8] = [0, 0]
            f.plain_scheduled.value = setting
            f.action_state.previous_clock, f.action_state.counter = previous, counter
            f.action_state.crossings = 7
            f.clocks[:2] = [first, second]
            f.auxiliary.progress = .2
            f.clips[6].source = f.clips[6].start + .25
            got, trace = check(f, ('clock wrap/window', setting, previous, first, counter))
            assert [event[1] for event, _ in trace if event[0] == 'clock'] == [first, second]
            assert got.action_state.previous_clock == second
            counts['native_edges'] += 1

    # Explicit mode3 release/menu fixtures cover matched and unmatched labels.
    # Unmatched selections still increment without indexing the action array.
    for selection in range(3):
        for label in [-1, 0, 1, 2, 3]:
            f = fixture(rng, 3)
            f.mode.value, f.keys, f.hits = 3, 1 << 12, 1
            f.auxiliary.selection, f.choices[0] = selection, label
            f.previous_flow.value = 8
            f.records.groups[f.frame.group].count = 0
            check(f, ('selection labels', selection, label))
            counts['native_edges'] += 1
    for count in [-2147483648, -1, 0, 10000, 2147483647]:
        f = fixture(rng, 3)
        f.mode.value, f.keys, f.hits = 3, 1 << 12, 1
        f.auxiliary.selection, f.choices[0], f.previous_flow.value = 0, -1, 8
        f.records.groups[f.frame.group].count = count
        got, _ = check(f, ('unmatched wrapping count', count))
        assert got.records.groups[f.frame.group].count == I(count + 1).value
        counts['native_edges'] += 1

    # The expression callback changes active, but mode6 continues reading
    # the descriptor captured before expression to decide its reversal.
    for positive, old, new in [(True, 7, 11), (False, 6, 10)]:
        for at_end in [False, True]:
            f = fixture(rng, 6)
            f.mode.value, f.keys, f.plain_scheduled.value = 6, 0, 0
            f.active.value = old
            f.input.words[6:8] = [1 if positive else 0xffffffff, 0]
            f.clips[old].source = f.clips[old].end if at_end else f.clips[old].start
            f.clips[new].source = f.clips[new].start if at_end else f.clips[new].end
            f.mutate_at, f.mutations = 1, {'active.value': new}
            got, trace = check(f, ('captured descriptor', positive, at_end))
            requests = [event[1] for event, _ in trace if event[0] == 'request']
            assert requests == ([new - 1 if positive else new + 1] if at_end else [])
            assert got.reverse.value == (0 if at_end else 1)
            counts['captured_descriptors'] += 1

    # Copy all40000 bytes of the action lane, including its unused suffix;
    # target recording belongs to the entry group, working flags stay live.
    for g in range(5):
        for variant in range(2):
            for selection in range(3):
                f = fixture(rng, 5)
                f.mode.value, f.stage.value = 5, 2
                f.frame.group, f.frame.camera_mode = g, 0
                f.auxiliary.variant, f.auxiliary.selection = variant, selection
                f.small_motion.value, f.previous_flow.value = 0, 8
                f.action_state.completion_elapsed = 20
                f.playing[1] = 0
                record = f.records.groups[g]
                record.count = [0, 3, 9999][selection]
                if selection == 1:
                    f.mutate_at, f.mutations = 1, {'frame.group': (g + 1) % 5}
                got, _ = check(f, ('complete lane copy', g, variant, selection))
                done = got.records.groups[g]
                assert done.actions[record.count] == 21 and done.count == record.count + 1
                assert bytes(done.retained[variant]) == bytes(done.actions)
                assert bytes(done.retained[1 - variant]) == bytes(record.retained[1 - variant])
                for other in range(5):
                    if other != g:
                        assert bytes(got.records.groups[other]) == bytes(f.records.groups[other])
                assert got.working[got.frame.group][6 + variant] == 1
                assert got.frame.transition_action == 49 and got.frame.curtain_wanted == 1
                assert got.auxiliary.gate == -1
                counts['completion_copies'] += 1

    # These rejection contracts do not execute out-of-bounds native writes.
    for mode in [3, 5]:
        for count in [-2147483648, -1, 10000, 2147483647]:
            f = fixture(rng, mode)
            f.mode.value, f.keys, f.hits = mode, 1 << 12, 1
            f.auxiliary.selection, f.choices[0], f.previous_flow.value = 0, 1, 8
            f.frame.camera_mode, f.small_motion.value, f.stage.value = 0, 0, 2
            f.action_state.completion_elapsed, f.playing[1] = 20, 0
            f.records.groups[f.frame.group].count = count
            ok, got, _, error, _ = portable(lib, f)
            assert not ok and 'recording lane' in error, (mode, count, error)
            assert bytes(got.records) == bytes(f.records)
            assert bytes(got.working) == bytes(f.working)
            if mode == 3:
                assert got.auxiliary.selection == 1 and got.selected.value == 2
            else:
                assert got.frame.transition_action == 49 and got.frame.curtain_wanted == 1
            counts['portable_rejections'] += 1
    for seconds in [-1, float('nan'), float('inf')]:
        f = fixture(rng, 0)
        f.seconds = seconds
        ok, got, trace, _, _ = portable(lib, f)
        assert not ok and not trace and got.snapshot() == f.snapshot()
        counts['portable_rejections'] += 1

    # Retain independent native/portable states across successive ticks.
    # Input/clip time are explicit external fixtures; this is not a claim of
    # natural parent/UI entry into each mode or actual asset sampling.
    for g in range(5):
        for variant in range(2):
            left = fixture(rng, 2)
            left.frame.group, left.auxiliary.variant = g, variant
            left.mode.value, left.keys, left.choice.value = 2, 0, 0
            left.active.value, left.plain_scheduled.value = 6, 0
            left.auxiliary.progress = .97
            left.action_state.counter, left.action_state.crossings = 0, 0
            left.action_state.previous_clock = 1
            left.previous_sound.value = -1
            right = left.clone()
            for tick in range(120):
                for f in [left, right]:
                    f.input.words[6:8] = [(-1 if tick % 2 else 1) & 0xffffffff, 0]
                    f.clocks[:2] = [1 + tick * 17, 2 + tick * 17]
                    f.clock_index.value = f.random_index.value = 0
                    f.playing[:] = [0] * 6
                    f.mutate_at = 0
                    f.seconds = F(1 / 60).value
                want, expected = native.run(left)
                ok, got, trace, error, _ = portable(lib, right)
                assert ok, ('continuous', g, variant, tick, error)
                compare(want, got, expected, trace, ('continuous', g, variant, tick))
                left, right = want, got
                for blob in got.snapshot(): digest.update(blob)
                counts['continuous_frames'] += 1

    control_names = set(dict(parent.Ops._fields_))
    for name, f in missing_cases.items():
        member = 'random' if name == 'random_value' else name
        if member in control_names: member = 'control.' + member
        ok, _, _, error, _ = portable(lib, f, missing=member)
        assert not ok and 'missing' in error, (member, error)
        counts['missing_services'] += 1
    # choices is borrowed data, not a callback. Check it separately from the
    # ten parent and eleven action services, at the first actual label read.
    f = fixture(rng, 3)
    f.mode.value, f.keys, f.hits = 3, 1 << 12, 1
    f.auxiliary.selection = 0
    ok, got, trace, error, _ = portable(lib, f, missing='choices')
    assert not ok and 'missing menu labels' in error, error
    assert [event[0] for event, _ in trace] == ['key', 'hit']
    assert bytes(got.records) == bytes(f.records)
    assert bytes(got.working) == bytes(f.working)
    assert got.auxiliary.selection == f.auxiliary.selection
    counts['missing_menu_labels'] += 1
    return counts


def main():
    faulthandler.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cases', type=int, default=12000)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert args.cases > 0
    raw = args.exe.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == EXE_SHA256
    lib, native = library(), Native(raw)
    bind(lib)
    initial = State()
    for name, address in STATE_FIELDS:
        _, ptr, size = parent.field_view(initial, name, address)
        C.memmove(ptr, bytes(native.u.mem_read(address, size)), size)
    assert bytes(lib.bk_ending_selected_action_initial()) == bytes(initial)
    rng, digest = random.Random(0x48e75b), hashlib.sha256()
    counts = dict(frames=0, calls=0, failure_prefixes=0, live_mutations=0)
    modes, operations, missing_cases = set(), set(), {}
    for case in range(args.cases):
        original = fixture(rng, case)
        if case % 11 == 0:
            original.mutate_at = 1 + case % 5
            original.mutations = {'frame.group': (original.frame.group + 2) % 5,
                'auxiliary.variant': 1 - original.auxiliary.variant,
                'auxiliary.selection': (original.auxiliary.selection + 1) % 3,
                'choice.value': 1 - original.choice.value,
                'voice_volume.value': -977, 'effect_volume.value': -433,
                'plain_scheduled.value': 1 - bool(original.plain_scheduled.value)}
        want, expected = native.run(original)
        ok, got, trace, error, used = portable(lib, original)
        assert ok, (case, error, [event for event, _ in trace])
        compare(want, got, expected, trace, case)
        counts['frames'] += 1
        counts['calls'] += len(trace)
        counts['live_mutations'] += 0 < original.mutate_at <= len(trace)
        modes.add(original.mode.value)
        operations.update(event[0] for event, _ in trace)
        for name in used: missing_cases.setdefault(name, original.clone())
        for blob in got.snapshot(): digest.update(blob)
        if case % 19 == 0 and trace:
            fail_at = 1 + case % len(trace)
            want, expected = native.run(original, fail_at)
            ok, got, trace, error, _ = portable(lib, original, fail_at)
            assert not ok, (case, 'failure not propagated')
            compare(want, got, expected, trace, (case, 'failure'))
            counts['failure_prefixes'] += 1
        if (case + 1) % 1000 == 0:
            print('Checked', case + 1, 'selected-action frames', flush=True)
    counts.update(extra_checks(lib, native, rng, digest, missing_cases))
    if args.cases >= 12000:
        assert modes == set(range(8))
        assert {'pointer', 'drag', 'repeat', 'ui_fade', 'ui_uv_reset', 'hit', 'clock'} <= operations
        assert counts['completion_copies'] == 30 and counts['continuous_frames'] == 1200
        assert set(missing_cases) == ACTION_SERVICES, (
            'missing action callback coverage', sorted(ACTION_SERVICES - set(missing_cases)),
            'unexpected callbacks', sorted(set(missing_cases) - ACTION_SERVICES))
        assert counts['missing_services'] == len(ACTION_SERVICES)
        assert counts['missing_menu_labels'] == 1
    report = dict(passed=True, exe_sha256=EXE_SHA256, counters=counts,
        missing_service_names=sorted(missing_cases),
        modes=sorted(modes), operations=sorted(operations), max_error=0,
        state_sha256=digest.hexdigest(), numerical_children_unmodified=True,
        full_loader=False, gpu_or_device_validation=False, scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS selected action', json.dumps(counts), digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
