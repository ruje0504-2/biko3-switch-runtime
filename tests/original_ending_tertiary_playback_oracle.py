"""Original402e18/4e18ad/4a9019 and ordinary4026fe scheduling with real XANs.

All clock, clamp, loop,4025b9/4a8f2d chain and request instructions run unchanged.
ANIM/MATA/MORP are observing submission boundaries, not fabricated animation
implementations. This compares scheduler state, source edits, fixed counters and
ordered sample arguments; actor-local/GPU integration has separate coverage.
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

from unicorn import UC_HOOK_CODE, UcError
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
from original_clip_oracle import Native as ClipNative
from clip_binding import library, State, Sample
from bk3_assets import Archive

I, U, F, P = C.c_int32, C.c_uint32, C.c_float, C.c_void_p
ADDRESSES = {-1: 0x4026fe, 0: 0x402e18, 1: 0x4e18ad, 2: 0x4a9019}


class Timing(C.Structure):
    _fields_ = [('start', F), ('end', F), ('source', F)]


class Prediction(C.Structure):
    _fields_ = [('duration', I), ('end', F), ('source', F), ('rate', F)]


class Native(ClipNative):
    def __init__(self, exe):
        super().__init__(exe)
        # Observe model-level MORP dispatch, not individual track evaluation.
        for a in [0x4300e0, 0x433a7d, 0x433cbe]:
            self.uc.hook_add(UC_HOOK_CODE, self.submit, begin=a, end=a)

    def bind(self, data, authored=False):
        super().bind(data, authored)
        self.word(self.model+0x158, 0x123460)
        self.word(self.model+0x168, 0x123470)

    def submit(self, u, address, _size, _context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp, 4))[0]
        if address == 0x4300e0:
            self.calls.append(('mata', struct.unpack('<f', u.mem_read(sp+8, 4))[0]))
        else:
            channel = 'anim' if address in [0x4097d6, 0x409a94] else 'morph'
            if address in [0x4097d6, 0x433a7d]:
                source = struct.unpack('<f', u.mem_read(sp+8, 4))[0]
                sample = (0, source, source, 0.)
            else:
                sample = (1, *struct.unpack('<3f', u.mem_read(sp+8, 12)))
            self.calls.append((channel, *sample))
        u.reg_write(UC_X86_REG_ESP, sp+4)
        u.reg_write(UC_X86_REG_EIP, ret)


def bind(lib):
    for name, args, result in [
        ('bk_clip_player_create_loaded', [P, P], P),
        ('bk_clip_advance_plain', [P, F, I, C.POINTER(Sample), P], I),
        ('bk_clip_set_clock', [P, U, F, F, P], I),
        ('bk_clip_timing', [P, U, C.POINTER(Timing)], I),
        ('bk_clip_prediction', [P, U, C.POINTER(Prediction)], I),
        ('bk_clip_loops', [P, U, C.POINTER(I)], I),
        ('bk_clip_frame_clock', [P, U, C.POINTER(I), C.POINTER(I)], I),
        ('bk_clip_link', [P, U, C.POINTER(I), C.POINTER(I)], I),
    ]:
        f = getattr(lib, name)
        f.argtypes, f.restype = args, result


def synthetic(index):
    raw = bytearray(0x5190)
    raw[:6] = raw[256:262] = b'test.x'
    for slot in range(3):
        off = 512+0x190+slot*156
        if slot == 1 and index % 4 == 0:
            continue  # unconditional4a8f2d must select this empty descriptor
        loop = (index+slot) % 2
        start, end = (80., 5.) if index % 3 == 0 else (5., 80.)
        if index % 12 == 1 and slot == 1:
            start = end = 20.  # nonzero static target, zero duration
        duration = 0 if start == end else 3+index % 11
        struct.pack_into('<i', raw, off, loop)
        struct.pack_into('<i', raw, off+0x48, min(duration, (index+slot) % 3))
        struct.pack_into('<i', raw, off+0x50, duration)
        struct.pack_into('<2f', raw, off+0x54, start, end)
        struct.pack_into('<i', raw, off+0x68, 1+slot)
        target = slot if index % 5 == 0 else (slot+1) % 3
        struct.pack_into('<3i', raw, off+0x70, int(index % 6 != 5), target, index % 3)
        struct.pack_into('<f', raw, off+0x7c,
                         [0, 1e-7, 1e-6, 2e-6, .5, 2, 8][index % 7])
    return bytes(raw)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('data', type=Path)
    ap.add_argument('--steps', type=int, default=64)
    ap.add_argument('--synthetic', type=int, default=72)
    ap.add_argument('--limit-assets', type=int, default=0)
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    assert args.steps > 0 and args.synthetic >= 0
    exe = args.exe.read_bytes()
    native, lib = Native(exe), library()
    bind(lib)
    error = C.create_string_buffer(256)
    rng, digest = random.Random(0x4a9019), hashlib.sha256()
    frames = edits = switches = table_values = empty_chains = rejections = 0
    by_mode = {m: 0 for m in ADDRESSES}
    records = []

    def state(p):
        value = State()
        assert lib.bk_clip_state(p, C.byref(value))
        expected = State(*native.state())
        assert bytes(value) == bytes(expected), ('state', label,
                    [(name, getattr(value, name), getattr(expected, name))
                     for name, _ in State._fields_ if bytes(F(getattr(value, name))) != bytes(F(getattr(expected, name)))])
        digest.update(bytes(value))
        return value

    def compare_tables(p):
        nonlocal table_values
        for slot in range(128):
            off = native.obj+0x190+slot*156
            raw = bytes(native.uc.mem_read(off, 156))
            timing, pred, loops, interval, counter, chain, target = Timing(), Prediction(), I(), I(), I(), I(), I()
            assert lib.bk_clip_timing(p, slot, C.byref(timing))
            assert lib.bk_clip_prediction(p, slot, C.byref(pred))
            assert lib.bk_clip_loops(p, slot, C.byref(loops))
            assert lib.bk_clip_frame_clock(p, slot, C.byref(interval), C.byref(counter))
            assert lib.bk_clip_link(p, slot, C.byref(chain), C.byref(target))
            want_timing = Timing(*[struct.unpack_from('<f', raw, i)[0] for i in [0x54, 0x58, 0x60]])
            want_pred = Prediction(struct.unpack_from('<i', raw, 0x50)[0],
                                   *[struct.unpack_from('<f', raw, i)[0] for i in [0x58, 0x60, 0x5c]])
            assert bytes(timing) == bytes(want_timing), ('timing', label, slot)
            assert bytes(pred) == bytes(want_pred), ('prediction', label, slot,
                                                     list(bytes(pred)), list(bytes(want_pred)))
            got = (loops.value, interval.value, counter.value, chain.value, target.value)
            want = tuple(struct.unpack_from('<i', raw, i)[0] for i in [0x64, 0x68, 0x6c, 0x70, 0x74])
            assert got == want, ('table', label, slot, got, want)
            digest.update(bytes(timing)+bytes(pred)+struct.pack('<5i', *got))
            table_values += 12

    def run(raw, name, synthetic_case=False):
        nonlocal frames, edits, switches, empty_chains, rejections, label
        definition = lib.bk_clip_set_decode(raw, len(raw), error)
        assert definition, (name, error.value)
        slots = [i for i in range(128) if lib.bk_clip_definition(definition, i).contents.active]
        if synthetic_case:
            slots = [0]
        try:
            for initial in slots:
                for mode in ADDRESSES:
                    native.bind(raw)
                    p = lib.bk_clip_player_create(definition, error)
                    assert p, (name, error.value)
                    try:
                        label = (name, initial, mode, -1)
                        native.select(initial, 1)
                        assert lib.bk_clip_select(p, initial, 1, error), error.value
                        state(p)
                        for step in range(args.steps):
                            label = (name, initial, mode, step)
                            before = state(p)
                            if step % 13 == 5:
                                timing = Timing()
                                assert lib.bk_clip_timing(p, before.slot, C.byref(timing))
                                source = F(rng.choice([timing.start, timing.end,
                                            (timing.start+timing.end)/2, max(0, timing.end-1e-4),
                                            timing.end+1e-4])).value
                                elapsed = F(rng.choice([0, 1, 2.5, 19])).value
                                off = native.obj+0x190+before.slot*156
                                native.uc.mem_write(off+0x44, struct.pack('<f', elapsed))
                                native.uc.mem_write(off+0x60, struct.pack('<f', source))
                                assert lib.bk_clip_set_clock(p, before.slot, elapsed, source, error), error.value
                                state(p)
                                edits += 1
                            if step % 23 == 10:
                                target = slots[(slots.index(initial)+1) % len(slots)]
                                native.select(target, 2)
                                assert lib.bk_clip_request(p, target, error), error.value
                                before = state(p)
                                switches += 1
                            dt = F([0, 0, 1/120, 1/60, .01, .1, .5, 2, 10, 100][step % 10]).value
                            # Alternate actual controlled/ordinary callers late
                            #in each sequence without resetting their shared clock.
                            current_mode = mode if step < 20 or step % 7 else -1
                            try:
                                native.call(ADDRESSES[current_mode], struct.pack('<If', native.obj, dt))
                            except UcError as exc:
                                raise AssertionError(('native execution', label, current_mode,
                                  hex(native.uc.reg_read(UC_X86_REG_EIP)), native.calls)) from exc
                            sample = Sample(99, 77, 88, 66)
                            if current_mode == -1:
                                ok = lib.bk_clip_advance(p, dt, C.byref(sample), error)
                            else:
                                ok = lib.bk_clip_advance_plain(p, dt, current_mode, C.byref(sample), error)
                            assert ok, (label, error.value)
                            current = state(p)
                            old = Timing()
                            assert lib.bk_clip_timing(p, before.slot, C.byref(old))
                            values = tuple(getattr(sample, name) for name, _ in Sample._fields_)
                            expected_calls = [('anim', *values)]
                            if sample.blend:
                                expected_calls += [('morph', *values), ('mata', old.source)]
                            else:
                                expected_calls += [('mata', old.source), ('morph', *values)]
                            assert native.calls == expected_calls, ('submissions', label, native.calls, expected_calls)
                            interval, counter = I(), I()
                            assert lib.bk_clip_frame_clock(p, before.slot, C.byref(interval), C.byref(counter))
                            assert counter.value == 0
                            if current.slot != before.slot and not lib.bk_clip_definition(definition, current.slot).contents.active:
                                empty_chains += 1
                            digest.update(repr(expected_calls).encode())
                            by_mode[current_mode] += 1
                            frames += 1
                        compare_tables(p)
                        previous, sample = state(p), Sample(9, 7, 8, 6)
                        for dt, bad_mode in [(-1, 1), (float('nan'), 0), (float('inf'), 0), (0, 9)]:
                            sample_before = bytes(sample)
                            assert not lib.bk_clip_advance_plain(p, dt, bad_mode, C.byref(sample), error)
                            check = State()
                            assert lib.bk_clip_state(p, C.byref(check)) and bytes(check) == bytes(previous)
                            assert bytes(sample) == sample_before
                            rejections += 1
                    finally:
                        lib.bk_clip_player_destroy(p)
        finally:
            lib.bk_clip_set_destroy(definition)
        records.append({'name': name, 'sha256': hashlib.sha256(raw).hexdigest(),
                        'slots': slots, 'synthetic': synthetic_case})

    label = None
    files = [('bk3_11.pp', f'h0{i}_10.xan') for i in range(1, 6)]
    files += [('bk3_11.pp', 'h03_30.xan'), ('bk3_11.pp', 'h03_31.xan'),
              ('bk3_04.pp', 'cam00_02.xan')]
    if args.limit_assets:
        files = files[:args.limit_assets]
    archives = {pack: Archive(args.data/pack) for pack, _ in files}
    for pack, name in files:
        archive = archives[pack]
        entry = next(e for e in archive.entries if e.name.lower() == name)
        run(archive.read(entry), pack+'/'+name)
        print('Checked plain playback', name, flush=True)
    for index in range(args.synthetic):
        run(synthetic(index), 'synthetic-'+str(index), True)
    report = {'passed': True, 'exe_sha256': hashlib.sha256(exe).hexdigest(),
              'frames': frames, 'by_mode': by_mode, 'clock_edits': edits,
              'requests': switches, 'table_values': table_values,
              'empty_target_chains': empty_chains, 'rejections': rejections,
              'max_error': 0, 'state_sha256': digest.hexdigest(),
              'records': records, 'scope': __doc__, 'gpu_validation': False}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS tertiary playback', frames, edits, switches, table_values,
          empty_chains, rejections, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
