"""Original4db608 state4, including all five opening substates.

Camera and audio leaves are explicit observing/mutating services. Original
branches, float stores, matrix/target copies, string formatting and contact
name selection execute. Compare ordered leaf calls and every live snapshot;
this is not a natural ending or a GPU/Switch validation.
"""
import argparse
import ctypes as C
import hashlib
import json
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
from model_binding import ROOT, library

I, U, F, P = C.c_int32, C.c_uint32, C.c_float, C.c_void_p


class Retained(C.Structure):
    _fields_ = [('fov', F)]


class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('camera', C.POINTER(Camera)),
                ('substate', C.POINTER(I)), ('wait', C.POINTER(I)),
                ('next', C.POINTER(I)), ('previous', C.POINTER(C.c_int8)),
                ('volume', C.POINTER(I)), ('name', P),
                ('retained', C.POINTER(Retained))]


CameraCall = C.CFUNCTYPE(I, P, I, I, C.POINTER(U), U, C.POINTER(U), P)
Target = C.CFUNCTYPE(I, P, C.POINTER(F), P)
Status = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
Load = C.CFUNCTYPE(I, P, U, C.c_char_p, P)
Play = C.CFUNCTYPE(I, P, U, I, P)
Name = C.CFUNCTYPE(I, P, P, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('camera', CameraCall), ('target', Target),
                ('present', Status), ('status', Status), ('load', Load),
                ('play', Play), ('voice_name', Name)]


def bits(value):
    return struct.pack('<f', value)


class Fixture:
    def clone(self):
        out = Fixture()
        for name in ['frame', 'control', 'aux', 'camera', 'retained', 'substate',
                     'wait', 'next', 'previous', 'volume', 'name', 'target']:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        for name in ['present', 'playing', 'hr', 'loaded']:
            setattr(out, name, list(getattr(self, name)))
        out.result, out.mutate_at = self.result, self.mutate_at
        return out

    def snapshot(self):
        return tuple(bytes(getattr(self, name)) for name in [
            'frame', 'control', 'aux', 'camera', 'retained', 'substate',
            'wait', 'next', 'previous', 'volume', 'name', 'target']) + (
                tuple(self.present), tuple(self.playing))

    def change(self, number):
        if number == self.mutate_at:
            self.next.value = 1 - bool(self.next.value)
            self.frame.group = (self.frame.group + 1) % 5
            self.frame.camera_clip = (self.frame.camera_clip + 1) % 3
            self.volume.value = -1731
            self.target[:] = [19.25, -6.5, 31.75]
            self.control.toggles[7] ^= 1


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f2f0, 0x300d010)  # wsprintfA
        self.word(0x53f214, 0x300d020)  # lstrcpyA
        self.word(0x300b024, 0x300d100)
        for slot in range(2):
            self.word(0x300a000 + slot * 32, 0x300b000)
        for address in [0x42cf0e, 0x4e0b7e, 0x4e0ecb, 0x4e0956, 0x4ad2bf,
                        0x300d010, 0x300d020, 0x300d100]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def string(self, address):
        return bytes(self.u.mem_read(address, 260)).split(b'\0')[0]

    def write(self, s):
        for name, address in FIELDS:
            self.word(address, getattr(s.frame, name))
        for name, address in BYTES:
            self.u.mem_write(address, bytes([getattr(s.frame, name)]))
        for address, value in [(0x721e14, s.frame.camera_values),
                               (0x709fcc, s.frame.camera_table),
                               (0x71b41c, s.control.saved_camera),
                               (0x7220f8, s.control.toggles),
                               (0x71b3dc, s.camera.matrix),
                               (0x575644, s.retained), (0x719b50, s.substate),
                               (0x719b24, s.wait), (0x719b20, s.next),
                               (0x721ad4, s.previous), (0xbe9a08, s.volume),
                               (0x722224, s.name), (0x30020f0, s.target)]:
            self.u.mem_write(address, bytes(value))
        for (name, _), address in zip(Auxiliary._fields_, ADDR):
            off = getattr(Auxiliary, name).offset
            self.u.mem_write(address, bytes(s.aux)[off:off + 4])
        for slot, present in enumerate(s.present):
            self.word(0x722334 + slot * 0x120,
                      0x300a000 + slot * 32 if present else 0)
        self.word(0x721f08, 0x3002000)

    def read(self):
        s = self.current.clone()
        for name, address in FIELDS:
            setattr(s.frame, name, struct.unpack('<I', self.u.mem_read(address, 4))[0])
        for name, address in BYTES:
            setattr(s.frame, name, self.u.mem_read(address, 1)[0])
        for obj, field, address, count in [
            (s.frame, 'camera_values', 0x721e14, 12),
            (s.frame, 'camera_table', 0x709fcc, 80),
            (s.control, 'saved_camera', 0x71b41c, 64),
            (s.control, 'toggles', 0x7220f8, 8),
            (s.camera, 'matrix', 0x71b3dc, 64)]:
            C.memmove(C.addressof(obj) + getattr(type(obj), field).offset,
                      bytes(self.u.mem_read(address, count)), count)
        for name, address in [('retained', 0x575644), ('substate', 0x719b50),
                              ('wait', 0x719b24), ('next', 0x719b20),
                              ('previous', 0x721ad4), ('volume', 0xbe9a08),
                              ('name', 0x722224), ('target', 0x30020f0)]:
            value = getattr(s, name)
            C.memmove(C.addressof(value), bytes(self.u.mem_read(address, C.sizeof(value))), C.sizeof(value))
        for (name, _), address in zip(Auxiliary._fields_, ADDR):
            C.memmove(C.addressof(s.aux) + getattr(Auxiliary, name).offset,
                      bytes(self.u.mem_read(address, 4)), 4)
        return s

    def hook(self, uc, address, _size, _context):
        sp = uc.reg_read(UC_X86_REG_ESP)
        words = struct.unpack('<7I', uc.mem_read(sp, 28))
        ret, first, second, third = words[:4]
        result, pop, event = 0, 4, None
        s = self.read()
        if address == 0x42cf0e:
            s.camera.fov = struct.unpack('<f', uc.mem_read(sp + 4, 4))[0]
        elif address == 0x300d010:
            fmt = self.string(second)
            assert fmt in (b'PH%d0001.wav', b'PH%d%02d%02d.wav'), fmt
            count = fmt.count(b'%')
            arguments = struct.unpack('<' + 'i' * count,
                                      uc.mem_read(sp + 12, count * 4))
            name = fmt % arguments
            uc.mem_write(first, name + b'\0')
            result = len(name)
        elif address == 0x300d020:
            uc.mem_write(first, self.string(second) + b'\0')
            result, pop = first, 12
            # Only this string write changes modeled state in this hook.
            s = self.read()
        elif address in (0x4e0b7e, 0x4e0ecb):
            assert first == 0x71af38
            kind = int(address == 0x4e0ecb)
            event = ('camera', kind, second if kind else 0,
                     tuple(words[3:6]) if kind else (0, 0, 0),
                     words[6] if kind else 0)
            result = s.result
        elif address == 0x4e0956:
            event = ('load', second, self.string(first))
            self.last_slot = second
        elif address == 0x4ad2bf:
            slot = (first - 0x300a000) // 32 if first else self.last_slot
            assert slot in (0, 1) and second == 0
            event = ('play', slot, C.c_int32(third).value)
        elif address == 0x300d100:
            slot = (first - 0x300a000) // 32
            assert slot in (0, 1) and s.present[slot]
            event = ('status', slot)
            uc.mem_write(second, struct.pack('<I', 1 if s.playing[slot] else 2))
            result, pop = s.hr[slot] & 0xffffffff, 12
        else:
            raise AssertionError(hex(address))
        if event:
            self.trace.append((event, s.snapshot()))
            if event[0] == 'camera':
                s.camera.matrix[12] = 31 + len(self.trace)
            elif event[0] == 'load':
                s.present[event[1]] = s.loaded[event[1]]
                s.playing[event[1]] = False
            elif event[0] == 'play':
                s.playing[event[1]] = s.present[event[1]]
            s.change(len(self.trace))
        self.current = s
        self.write(s)
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_ESP, sp + pop)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, fixture, seconds):
        self.current = fixture.clone()
        self.write(self.current)
        self.u.mem_write(0x733700, bits(seconds))
        self.trace, self.last_slot = [], 0
        self.call(0x4db608, bytes(Input()))
        return self.trace, self.read().snapshot()


def portable(lib, fixture, seconds, fail_at=0):
    s = fixture.clone()
    trace = []
    error = C.create_string_buffer(256)

    def commit(event):
        trace.append((event, s.snapshot()))
        if len(trace) == fail_at:
            return 0
        if event[0] == 'camera':
            s.camera.matrix[12] = 31 + len(trace)
        elif event[0] == 'load':
            s.present[event[1]] = s.loaded[event[1]]
            s.playing[event[1]] = False
        elif event[0] == 'play':
            s.playing[event[1]] = s.present[event[1]]
        s.change(len(trace))
        return 1

    @CameraCall
    def camera(_context, kind, choice, offset, extra, out, _error):
        out[0] = s.result
        return commit(('camera', kind, choice, tuple(offset[:3]), extra))

    @Target
    def target(_context, out, _error):
        for i in range(3):
            out[i] = s.target[i]
        return 1

    @Status
    def present(_context, slot, out, _error):
        out[0] = s.present[slot]
        return 1

    @Status
    def status(_context, slot, out, _error):
        out[0] = s.playing[slot] and not s.hr[slot]
        return commit(('status', slot))

    @Load
    def load(_context, slot, name, _error):
        return commit(('load', slot, name))

    @Play
    def play(_context, slot, volume, _error):
        return commit(('play', slot, volume))

    @Name
    def name(_context, out, err):
        return lib.bk_ending_sound_contact_voice(s.frame.group, 0, 0, 0, out, err)

    bindings = Bindings(C.pointer(s.frame), C.pointer(s.control), C.pointer(s.aux),
                        C.pointer(s.camera), C.pointer(s.substate), C.pointer(s.wait),
                        C.pointer(s.next), C.pointer(s.previous), C.pointer(s.volume),
                        C.addressof(s.name), C.pointer(s.retained))
    ops = Ops(None, camera, target, present, status, load, play, name)
    ok = lib.bk_ending_opening_step(C.byref(bindings), seconds, C.byref(ops), error)
    return ok, trace, s.snapshot(), error.value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native = Native(exe)
    lib = library()
    lib.bk_ending_opening_step.argtypes = [C.POINTER(Bindings), F, C.POINTER(Ops), P]
    lib.bk_ending_sound_contact_voice.argtypes = [U, I, I, I, P, P]
    rng = random.Random(0x4db608)
    frames, calls, failures, mutations, rejects = 0, 0, 0, 0, 0
    substates = [0] * 5
    for case in range(6000):
        s = Fixture()
        s.frame, s.control, s.aux, s.camera = Frame(), Control(), Auxiliary(), Camera()
        s.frame.phase, s.frame.state_721ee0, s.frame.group = 1, 4, case % 5
        s.frame.camera_mode, s.frame.camera_clip = 5, case % 3
        s.frame.camera_values[:] = [rng.getrandbits(32) for _ in range(3)]
        for row in s.frame.camera_table:
            row[:] = [rng.getrandbits(32) for _ in range(4)]
        s.control.toggles[:] = [rng.choice([0, 1, 2, 255]) for _ in range(8)]
        s.control.saved_camera[:] = [rng.uniform(-100, 100) for _ in range(16)]
        s.aux.index, s.aux.pending, s.aux.expression_a = 17, 19, 7
        s.camera.matrix[:] = s.camera.pose.world[:] = IDENTITY
        s.camera.matrix[12:15] = [9, 17, 29]
        s.camera.fov = .77
        s.retained = Retained(rng.choice([.2, 1, .4, .20000002, -.1, 2]))
        s.substate = I((case // 5) % 5)
        if case % 97 == 0:
            s.substate.value = rng.choice([-1, 5, 1000])
        s.wait = I(rng.choice([0, 1, 2, -1]))
        s.next = I(rng.choice([0, 0, 1, -1]))
        s.previous = C.c_int8(rng.choice([8, 24, 0, -1]))
        s.volume = I(rng.choice([0, -600, -1000, -10000]))
        s.name = (C.c_char * 32).from_buffer_copy(b'previous.wav\0' + b'x' * 19)
        s.target = (F * 3)(12.5, 14.25, -31.75)
        s.present = [bool(rng.randrange(2)) for _ in range(2)]
        s.playing = [bool(rng.randrange(2)) for _ in range(2)]
        s.loaded = [bool(rng.randrange(2)) for _ in range(2)]
        s.hr = [rng.choice([0, 0, -1]) for _ in range(2)]
        s.result = rng.choice([0, 1, 255, 256, 257, 0xffffffff])
        s.mutate_at = rng.randrange(1, 6) if case % 3 == 0 else -1
        seconds = F(rng.choice([0, 1 / 60, .000001, .1, 1, 4, 20])).value
        expected, final = native.run(s, seconds)
        ok, got, actual, error = portable(lib, s, seconds)
        assert ok and got == expected and actual == final, (
            case, s.substate.value, error,
            'calls', [x[0] for x in got], [x[0] for x in expected],
            'snapshot differences',
            [(index, [(i, a, b) for i, (a, b) in enumerate(zip(left, right))
                       if a != b][:12])
             for index, (left, right) in enumerate(zip(actual, final))
             if left != right],
            'call snapshots', [i for i, (a, b) in enumerate(zip(got, expected))
                               if a != b])
        frames += 1
        calls += len(got)
        mutations += 0 < s.mutate_at <= len(got)
        if 0 <= s.substate.value < 5:
            substates[s.substate.value] += 1
        if case < 1000:
            for failure in range(1, len(got) + 1):
                ok, prefix, snapshot, _ = portable(lib, s, seconds, failure)
                assert not ok and prefix == expected[:failure]
                assert snapshot == expected[failure - 1][1]
                failures += 1
        if case % 100 == 0:
            for invalid in [float('nan'), float('inf'), -1]:
                ok, trace, snapshot, _ = portable(lib, s, invalid)
                assert not ok and not trace and snapshot == s.snapshot()
                rejects += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  frames=frames, substates=substates, calls=calls,
                  callback_mutations=mutations, failure_prefixes=failures,
                  invalid_time_rejections=rejects, comparison='byte-exact',
                  scope=__doc__)
    (ROOT / 'local/original-ending-opening-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
