"""Native PCM level and shared voice envelope; only DirectSound is replaced."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

class Envelope(C.Structure):
    _fields_ = [('target', C.c_float), ('smoothed', C.c_float)]

class Native:
    stack, stop, obj, vtable = 0x2008000, 0x300f000, 0x3000000, 0x3000100
    first, second, result, level = 0x3002000, 0x3003000, 0x3004000, 0x3004010
    def __init__(self, exe):
        self.u = machine(exe)
        self.word(self.obj, self.vtable)
        self.functions = {}
        for i, (offset, name, argc) in enumerate([(0x24, 'status', 2), (0xc, 'caps', 2), (0x10, 'position', 3), (0x2c, 'lock', 8), (0x4c, 'unlock', 5)]):
            stub = 0x300e000 + i * 16
            self.functions[stub] = (name, argc)
            self.word(self.vtable + offset, stub)
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=stub, end=stub)
        self.u.mem_write(self.stop, b'\xd9\x1d' + struct.pack('<I', self.result))
    def word(self, ptr, value):
        self.u.mem_write(ptr, struct.pack('<I', value & 0xffffffff))
    def hook(self, u, address, size, user):
        name, argc = self.functions[address]
        sp = u.reg_read(UC_X86_REG_ESP)
        values = struct.unpack('<' + 'I' * (argc + 1), u.mem_read(sp, (argc + 1) * 4))
        ret, args = values[0], values[1:]
        assert args[0] == self.obj
        result = 0
        if name == 'status':
            self.word(args[1], 0 if self.failure == 'stopped' else 1)
            result = 1 if self.failure == 'status' else 0
        elif name == 'caps':
            self.word(args[1] + 8, 4096)
        elif name == 'position':
            self.word(args[1], 4096 - 442 if self.failure == 'tail' else 100)
        elif name == 'lock':
            assert args[1:3] == (100, 442)
            self.word(args[3], self.first); self.word(args[4], self.split)
            self.word(args[5], self.second); self.word(args[6], 442 - self.split)
            result = 1 if self.failure == 'lock' else 0
        elif name == 'unlock':
            assert args[1:] == (self.first, 0, self.second, 0)
            self.unlocks += 1
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_ESP, sp + (argc + 1) * 4)
        u.reg_write(UC_X86_REG_EIP, ret)
    def call(self, address, *args, floating=False):
        self.u.mem_write(self.stack, struct.pack('<' + 'I' * (len(args) + 1), self.stop, *args))
        self.u.reg_write(UC_X86_REG_ESP, self.stack)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(address, self.stop, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.stop
        if floating:
            self.u.emu_start(self.stop, self.stop + 6, count=1)
            return bytes(self.u.mem_read(self.result, 4))
        return self.u.reg_read(UC_X86_REG_EAX)

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path); args = ap.parse_args()
    exe = args.exe.read_bytes(); native = Native(exe); lib = library(); rng = random.Random(0x4af18f)
    lib.bk_voice_pcm_level.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.POINTER(C.c_int32), C.c_void_p]
    lib.bk_voice_pcm_level.restype = C.c_int
    lib.bk_voice_envelope_step.argtypes = [C.POINTER(Envelope), C.c_int, C.c_int32, C.c_float, C.POINTER(C.c_float), C.c_void_p]
    lib.bk_voice_envelope_step.restype = C.c_int
    error = C.create_string_buffer(256); cases = pcm_cases = inactive = rejects = 0
    state = Envelope(); native.u.mem_write(0x708878, bytes(state))
    for case in range(6000):
        native.failure = ['ok', 'ok', 'ok', 'ok', 'ok', 'ok', 'null', 'stopped', 'status', 'tail', 'lock'][case % 11]
        native.split = [0, 1, 2, 219, 220, 439, 440, 441, 442, rng.randrange(443)][case % 10]
        a = struct.pack('<221h', *[rng.choice([-32768, 0, 0, 1, 32767, rng.randrange(-32768, 32768)]) for _ in range(221)])
        b = struct.pack('<221h', *[rng.randrange(-32768, 32768) for _ in range(221)])
        native.u.mem_write(native.first, a); native.u.mem_write(native.second, b)
        native.unlocks = 0
        sound = 0 if native.failure == 'null' else native.obj
        available = native.call(0x4aef4e, sound, native.level)
        level = struct.unpack('<i', native.u.mem_read(native.level, 4))[0]
        if available:
            actual = C.c_int32(-1)
            assert lib.bk_voice_pcm_level(a, native.split, b, 442 - native.split, C.byref(actual), error), error.value
            assert actual.value == level, (case, native.split, actual.value, level)
            assert native.unlocks == 1; pcm_cases += 1
        else:
            assert level == 0 and native.unlocks == 0; inactive += 1
        if case % 7 == 0:
            state = Envelope(rng.uniform(-10, 30), rng.choice([-9, 0, 1, 9, 30, rng.uniform(-10, 30)]))
            native.u.mem_write(0x708878, bytes(state))
        seconds = C.c_float([0, 1/60, .1, .9, 10, rng.random()][case % 6]).value
        native.u.mem_write(0x733700, struct.pack('<f', seconds))
        wanted = native.call(0x4af18f, sound, floating=True)
        out = C.c_float(999)
        assert lib.bk_voice_envelope_step(C.byref(state), available, level, seconds, C.byref(out), error), error.value
        assert bytes(out) == wanted and bytes(state) == bytes(native.u.mem_read(0x708878, 8)), (case, list(struct.unpack('<3f', bytes(state) + bytes(out))), list(struct.unpack('<3f', bytes(native.u.mem_read(0x708878, 8)) + wanted)))
        if case % 25 == 0:
            held = bytes(state); previous = bytes(out)
            assert not lib.bk_voice_envelope_step(C.byref(state), 1, level, float('nan'), C.byref(out), error)
            assert bytes(state) == held and bytes(out) == previous; rejects += 1
        cases += 1
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), cases=cases, pcm_windows=pcm_cases, inactive_or_failed_sources=inactive, atomic_rejections=rejects, max_error=0, native_functions=['0x4aef4e', '0x4af18f'], hooks=['DirectSound status/caps/play cursor/lock/unlock only'], x87_control_word='0x037f', scope='Actual native signed16 PCM window summation and shared mouth-drive envelope, including split buffers, skipped final sample, negative sample extreme and stopped/failed/tail sources. Synthetic PCM at explicit playback cursors; no audio decoding/playback or morph rendering.')
    (ROOT/'local/original-voice-envelope-oracle.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS', cases, 'envelopes;', pcm_cases, 'PCM windows;', inactive, 'inactive sources;', rejects, 'atomic rejections')

if __name__ == '__main__': main()
