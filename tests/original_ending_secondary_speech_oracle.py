"""Original47D9EE retained filename, live volume and signed-byte Play flags.

Only Windows string imports and audio load/play are substituted. Native
control, stack-local filename lifetime, slot addressing, flag conversion and
post-load volume read execute. Audio callbacks observe/mutate live owners;
injected failures compare the executed prefix. This is not PCM validation.
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
from model_binding import ROOT, library

I, U, P = C.c_int32, C.c_uint32, C.c_void_p
Names = (C.c_char * 32) * 2
Load = C.CFUNCTYPE(I, P, U, C.c_char_p, P)
Play = C.CFUNCTYPE(I, P, U, I, I, P)


class Ops(C.Structure):
    _fields_ = [('context', P), ('load', Load), ('play', Play)]


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f2f0, 0x300d010)
        self.word(0x53f214, 0x300d020)
        for address in [0x300d010, 0x300d020, 0x4e0956, 0x4ad2bf]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def string(self, address):
        return bytes(self.u.mem_read(address, 64)).split(b'\0')[0]

    def snapshot(self):
        return (bytes(self.u.mem_read(0x722224, 32)) +
                bytes(self.u.mem_read(0x722344, 32)),
                struct.unpack('<i', self.u.mem_read(0xbe9a08, 4))[0])

    def hook(self, u, address, _size, _ctx):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, first, second, third, fourth = struct.unpack('<5I', u.mem_read(sp, 20))
        pop, result = 4, 0
        if address == 0x300d010:
            fmt = self.string(second)
            assert fmt == b'PH%d03%02d.wav', fmt
            text = fmt % (I(third).value, I(fourth).value)
            u.mem_write(first, text + b'\0')
            result = len(text)
        elif address == 0x300d020:
            u.mem_write(first, self.string(second) + b'\0')
            result, pop = first, 12
        else:
            if address == 0x4e0956:
                event = ('load', second, self.string(first))
            else:
                assert first == (0x300a000 + self.slot * 32 if self.present else 0)
                event = ('play', self.slot, I(second).value, I(third).value)
            self.trace.append((event, self.snapshot()))
            if len(self.trace) == self.fail_at:
                self.failed = True
                u.emu_stop()
                return
            if address == 0x4e0956:
                self.word(0x722334 + self.slot * 0x120,
                          0x300a000 + self.slot * 32 if self.present else 0)
            if len(self.trace) == self.mutate_at:
                self.word(0xbe9a08, -1931)
                u.mem_write(0x722224 + self.slot * 0x120, b'live-change.wav\0')
            result = 0x13579
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_ESP, sp + pop)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, group, cue, slot, flags, names, volume, fail_at, mutate_at, present):
        self.slot, self.fail_at, self.mutate_at = slot, fail_at, mutate_at
        self.present, self.failed, self.trace = present, False, []
        self.u.mem_write(0x721b3c, bytes([group]))
        self.word(0xbe9a08, volume)
        for i in range(2):
            self.u.mem_write(0x722224 + i * 0x120, names[32 * i:32 * i + 32])
            self.word(0x722334 + i * 0x120, 0x300c000 + i * 32)
        try:
            self.call(0x47d9ee, struct.pack('<iII', cue, slot, flags))
        except AssertionError:
            assert self.failed and self.u.reg_read(UC_X86_REG_EIP) in (0x4e0956, 0x4ad2bf)
        else:
            assert not self.failed
        return self.snapshot()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--report', type=Path,
                        default=ROOT / 'local/original-ending-secondary-speech-oracle.json')
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    n, lib = Native(exe), library()
    lib.bk_ending_secondary_speech.argtypes = [U, I, U, U, C.POINTER(C.c_char * 32),
                                               C.POINTER(I), C.POINTER(Ops), P]
    rng = random.Random(0x47d9ee)
    error = C.create_string_buffer(256)
    names = Names()
    volume = I()
    trace, callback_errors = [], []
    fail_at = mutate_at = slot = 0

    def effect(event):
        trace.append((event, (bytes(names), volume.value)))
        if len(trace) == fail_at:
            return 0
        if len(trace) == mutate_at:
            volume.value = -1931
            C.memmove(C.addressof(names) + slot * 32, b'live-change.wav\0', 16)
        return 1

    @Load
    def load(_ctx, index, name, _error):
        try:
            return effect(('load', index, name))
        except Exception as exc:
            callback_errors.append(repr(exc))
            return 0

    @Play
    def play(_ctx, index, flags, gain, _error):
        try:
            return effect(('play', index, flags, gain))
        except Exception as exc:
            callback_errors.append(repr(exc))
            return 0

    ops = Ops(None, load, play)
    calls = prefixes = mutations = 0
    flags_seen, slots_seen = set(), set()
    for case in range(3000):
        group, slot = case % 5, (case // 5) % 2
        cue = rng.choice(list(range(1, 19)) + [-2147483648, -9, -1, 0, 99, 100, 2147483647])
        flags = rng.choice([0, 1, 2, 127, 128, 255, 256, 257, 0xffffffff,
                            0x80000000, rng.getrandbits(32)])
        fail_at = [-1, -1, -1, 1, 2][case % 5]
        mutate_at = [-1, 1, 2][case % 3]
        raw = rng.randbytes(64)
        C.memmove(names, raw, 64)
        volume.value = rng.choice([-10000, -991, 0, 100, -2147483648, 2147483647])
        expected = n.run(group, cue, slot, flags, bytes(names), volume.value,
                         fail_at, mutate_at, case % 7 != 0)
        trace.clear()
        result = lib.bk_ending_secondary_speech(group, cue, slot, flags, names,
                                                C.byref(volume), C.byref(ops), error)
        assert not callback_errors, callback_errors
        assert bool(result) == (not n.failed), (case, result, n.failed, error.value)
        assert trace == n.trace, (case, trace, n.trace)
        assert (bytes(names), volume.value) == expected, (case, expected)
        calls += len(trace)
        prefixes += n.failed
        mutations += mutate_at > 0 and mutate_at <= len(trace) and mutate_at != fail_at
        slots_seen.add((group, slot))
        for event, _ in trace:
            if event[0] == 'play':
                flags_seen.add(event[2])
    assert len(slots_seen) == 10 and {-128, -1, 0, 1, 127}.issubset(flags_seen)

    invalid = missing_services = 0
    for group, index in [(5, 0), (0xffffffff, 1), (0, 2), (4, 0xffffffff)]:
        before = (bytes(names), volume.value)
        trace.clear()
        assert not lib.bk_ending_secondary_speech(group, 1, index, 0, names,
                                                  C.byref(volume), C.byref(ops), error)
        assert not trace and (bytes(names), volume.value) == before
        invalid += 1
    for missing in ('load', 'play'):
        empty = Ops(None, Load() if missing == 'load' else load,
                    Play() if missing == 'play' else play)
        fail_at = mutate_at = -1
        slot = 0
        trace.clear()
        assert not lib.bk_ending_secondary_speech(1, 18, 0, 0, names,
                                                  C.byref(volume), C.byref(empty), error)
        assert bytes(names[0]).split(b'\0')[0] == b'PH20318.wav'
        assert len(trace) == (missing == 'play')
        assert (b'missing ' + missing.encode() + b' service') in error.value
        missing_services += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), frames=3000,
                  observed_calls=calls, failed_prefixes=prefixes, callback_mutations=mutations,
                  group_slots=sorted(slots_seen), signed_flags=sorted(flags_seen),
                  invalid_inputs=invalid, missing_services=missing_services, max_error=0,
                  substitutions=['Windows wsprintfA/lstrcpyA', '4e0956 load', '4ad2bf Play'],
                  scope=__doc__)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
