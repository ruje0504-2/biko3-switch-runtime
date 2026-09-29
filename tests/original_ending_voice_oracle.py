"""Complete original4ad5a4 and4ad363, with only DirectSound leaves replaced.
Two independently changing status results, split PCM, odd cursor alignment,
near-end rejection, lock failures, integer quantization and retained smoothing
are compared byte-for-byte. Synthetic source buffers; real mixer cursor
binding is verified separately by the ending presentation resource probe.
Decoded PCM boundary checks also run4ad363 against bounded442-byte locks,
including the original DWORD underflow and every cursor of a442-byte ring.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_voice_envelope_oracle import Native as Base, Envelope
from model_binding import ROOT, library

class Native(Base):
    def hook(self, u, address, size, user):
        name, argc = self.functions[address]
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, *args = struct.unpack('<'+'I'*(argc+1), u.mem_read(sp, (argc+1)*4))
        assert args[0] == self.obj
        hr = 0
        if name == 'status':
            playing, hr = self.statuses[self.status_index]
            self.status_index += 1
            self.word(args[1], playing)
        elif name == 'caps':
            self.word(args[1]+8, getattr(self, 'buffer_bytes', 4096))
        elif name == 'position':
            self.word(args[1], self.position)
        elif name == 'lock':
            assert args[1:3] == [self.position & ~1, 442]
            self.word(args[3], self.first)
            self.word(args[4], self.split)
            self.word(args[5], self.second)
            self.word(args[6], 442-self.split)
            hr = self.lock_error
        elif name == 'unlock':
            assert args[1:] == [self.first, 0, self.second, 0]
            self.unlocks += 1
        u.reg_write(UC_X86_REG_EAX, hr & 0xffffffff)
        u.reg_write(UC_X86_REG_ESP, sp+(argc+1)*4)
        u.reg_write(UC_X86_REG_EIP, ret)

def pcm_boundaries(native, lib, err):
    lib.bk_ending_voice_pcm_level.argtypes = [C.POINTER(C.c_int16), C.c_size_t,
            C.c_size_t, C.POINTER(C.c_int), C.POINTER(C.c_int32), C.c_void_p]
    lib.bk_ending_voice_pcm_level.restype = C.c_int
    rng = random.Random(0x4ad363)
    cases = windows = wrapped = short_locks = tail_rejections = 0
    for count in (0, 1, 2, 219, 220, 221, 222, 223, 224, 256, 2048):
        values = [rng.randrange(-32768, 32768) for _ in range(count)]
        if count:
            values[0], values[-1] = -32768, 32767
        pcm = (C.c_int16 * count)(*values)
        raw = struct.pack('<' + 'h' * count, *values)
        starts = range(count + 1) if count <= 224 else sorted(
                {0, 1, 2, count - 222, count - 221, count - 220, count - 1, count})
        for source in starts:
            for odd in (0, 1):
                native.buffer_bytes = count * 2
                native.position = source * 2 + odd
                offset = native.position & ~1
                native.split = min(442, max(0, len(raw) - offset))
                # DirectSound leaf policy: at most one complete buffer can
                # be locked, with the suffix and prefix returned separately.
                # The original guard/summing/envelope instructions execute.
                native.lock_error = -1 if len(raw) < 442 or offset >= len(raw) else 0
                first = raw[offset:offset + native.split]
                second = raw[:442 - native.split]
                native.u.mem_write(native.first, first.ljust(442, b'\x5a'))
                native.u.mem_write(native.second, second.ljust(442, b'\xa5'))
                native.statuses, native.status_index, native.unlocks = [(1, 0)], 0, 0
                wanted_sampled = native.call(0x4ad363, native.obj, native.level)
                wanted_level = struct.unpack('<i', native.u.mem_read(native.level, 4))[0]
                sampled, level = C.c_int(-1), C.c_int32(-1)
                assert lib.bk_ending_voice_pcm_level(pcm, count, source,
                        C.byref(sampled), C.byref(level), err), err.value
                assert (sampled.value, level.value) == (wanted_sampled, wanted_level), (
                        count, source, odd, sampled.value, level.value,
                        wanted_sampled, wanted_level)
                assert native.unlocks == wanted_sampled
                state = Envelope(rng.uniform(-10, 64), rng.uniform(-10, 30))
                native.u.mem_write(0x708840, bytes(state))
                native.statuses, native.status_index = [(1, 0), (1, 0)], 0
                expected = native.call(0x4ad5a4, native.obj, floating=True)
                out = C.c_float()
                assert lib.bk_ending_voice_envelope_step(C.byref(state), 1,
                        sampled.value, level.value, C.byref(out), err), err.value
                assert bytes(out) == expected and bytes(state) == bytes(
                        native.u.mem_read(0x708840, 8)), (count, source, odd)
                cases += 1
                windows += wanted_sampled
                wrapped += bool(wanted_sampled and native.split < 442)
                short_locks += count < 221
                tail_rejections += bool(count >= 222 and not wanted_sampled)
    assert wrapped == 440  #220 nonzero cursors, each tested even and odd.
    return dict(cases=cases, sampled_windows=windows, wrapped_442_byte_windows=wrapped,
            short_buffer_lock_rejections=short_locks, tail_or_cursor_rejections=tail_rejections,
            max_error=0, scope='Decoded PCM and full original4ad363/4ad5a4 instructions; DirectSound leaf substitutes enforce442-byte capacity and two-span wrap. No Windows audio driver is executed.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--records', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    lib.bk_ending_voice_envelope_step.argtypes = [C.POINTER(Envelope), C.c_int,
            C.c_int, C.c_int32, C.POINTER(C.c_float), C.c_void_p]
    lib.bk_voice_pcm_level.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p,
            C.c_size_t, C.POINTER(C.c_int32), C.c_void_p]
    rng = random.Random(0x4ad5a4)
    err = C.create_string_buffer(256)
    state = Envelope()
    if args.records:
        raw = args.records.read_bytes()
        assert raw[:8] == b'BK3EP001' and (len(raw)-8) % 496 == 0
        frames = source_bytes = inactive = 0
        for off in range(8, len(raw), 496):
            row = struct.unpack_from('<14I', raw, off)
            group, variant, step, playing, consumed_playing, failed_sample, size, position = row[:8]
            native.buffer_bytes, native.position = size, position
            native.split, native.lock_error = 442, -1 if failed_sample else 0
            native.u.mem_write(native.first, raw[off+56:off+496]+b'\0\0')
            native.u.mem_write(native.second, bytes(442))
            native.u.mem_write(0x708840, struct.pack('<2I', *row[8:10]))
            native.statuses, native.status_index = [(playing, 0), (consumed_playing, 0)], 0
            native.unlocks = 0
            native.call(0x4ad5a4, native.obj, floating=True)
            expected = bytes(native.u.mem_read(0x708840, 8))
            assert expected == struct.pack('<2I', *row[10:12]), (group, variant, step, expected.hex(), row)
            frames += 1
            source_bytes += native.unlocks * 440
            inactive += not playing
        result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                records=str(args.records), records_sha256=hashlib.sha256(raw).hexdigest(),
                frames=frames, sampled_pcm_bytes=source_bytes, inactive=inactive,
                max_error=0, scope='Actual consumed Japanese PCM windows from ending-presentation-probe; original4ad363/4ad5a4 execute with recorded status/cursor/lock boundaries. Independent envelope states and real near-end exclusion compared byte-exact. Not Switch audio verification.')
        report = args.records.with_suffix('.verification.json')
        report.write_text(json.dumps(result, indent=2)+'\n')
        print('PASS', json.dumps(result), flush=True)
        return
    pcm_cases = playing_failures = inactive = rejects = 0
    for case in range(12000):
        present = case % 19 != 0
        statuses = [rng.choice([(1, 0), (1, 0), (0, 0), (1, -1)]) for _ in range(2)]
        native.position = rng.choice([100, 101, 4096-444, 4096-443, 4096-442, 4095])
        native.split = rng.choice([0, 1, 2, 219, 220, 439, 440, 441, 442])
        native.lock_error = 0 if case % 13 else -1
        # Constant low samples cover /512 truncation boundaries as well as
        # loud/random windows, including signed16 minimum and ignored tail.
        a = struct.pack('<221h', *[rng.choice([-32768, 0, 255, 256, 511, 512,
                32767, rng.randrange(-32768, 32768)]) for _ in range(221)])
        b = struct.pack('<221h', *[rng.randrange(-32768, 32768) for _ in range(221)])
        if case % 5 == 0:
            a = struct.pack('<221h', *([rng.randrange(0, 2048)]*220+[32767]))
        native.u.mem_write(native.first, a)
        native.u.mem_write(native.second, b)
        sound = native.obj if present else 0
        native.statuses, native.status_index, native.unlocks = [statuses[1]], 0, 0
        sampled = native.call(0x4ad363, sound, native.level)
        magnitude = struct.unpack('<i', native.u.mem_read(native.level, 4))[0]
        if sampled:
            value = C.c_int32()
            assert lib.bk_voice_pcm_level(a, native.split, b, 442-native.split,
                                          C.byref(value), err), err.value
            assert value.value == magnitude and native.unlocks == 1
            pcm_cases += 1
        else:
            assert magnitude == 0 and native.unlocks == 0
        if case % 7 == 0:
            state = Envelope(rng.uniform(-10, 64), rng.uniform(-10, 30))
        native.u.mem_write(0x708840, bytes(state))
        native.statuses, native.status_index = statuses, 0
        expected = native.call(0x4ad5a4, sound, floating=True)
        playing = int(present and statuses[0][0] == 1 and statuses[0][1] == 0)
        out = C.c_float(99)
        assert lib.bk_ending_voice_envelope_step(C.byref(state), playing, sampled,
                magnitude, C.byref(out), err), err.value
        assert bytes(out) == expected and bytes(state) == bytes(native.u.mem_read(0x708840, 8)), (
                case, playing, sampled, magnitude, bytes(out).hex(), expected.hex())
        inactive += not playing
        playing_failures += bool(playing and not sampled)
        if case % 25 == 0:
            held = bytes(state), bytes(out)
            assert not lib.bk_ending_voice_envelope_step(C.byref(state), 1, 1,
                    65537, C.byref(out), err)
            assert held == (bytes(state), bytes(out))
            rejects += 1
    boundaries = pcm_boundaries(native, lib, err)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
            frames=12000, pcm_windows=pcm_cases, inactive=inactive,
            playing_sample_failures=playing_failures, atomic_rejections=rejects,
            decoded_pcm_boundaries=boundaries,
            max_error=0, scope=__doc__)
    (ROOT/'local/original-ending-voice-oracle.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS', json.dumps(report), flush=True)

if __name__ == '__main__':
    main()
