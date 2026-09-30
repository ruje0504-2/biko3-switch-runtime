"""Actual XAN scheduler comparison for gallery chaining and reverse playback.

Fixed60Hz and explicit mixed millisecond intervals are separate fixtures.
The native selected clip can chain with source below end at fixed60Hz;
this test preserves that limitation, not a successful full-replay claim.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from original_clip_oracle import Native, Archive
from original_ending_tertiary_playback_oracle import bind, Timing
from original_clip_edits_oracle import Edit
from clip_binding import library, State, Sample


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('data', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native, lib = Native(exe), library()
    bind(lib)
    lib.bk_clip_edit.argtypes = [C.c_void_p, C.POINTER(Edit), C.c_size_t, C.c_void_p]
    lib.bk_clip_loop_mode.argtypes = [C.c_void_p, C.c_uint, C.POINTER(C.c_int32)]
    lib.bk_clip_completed_chain.argtypes = [C.c_void_p, C.c_uint, C.POINTER(C.c_int)]
    error = C.create_string_buffer(256)
    frames = reverse_frames = table_checks = 0
    state_hash = hashlib.sha256()
    results = []

    def compare(p, label):
        got = State()
        assert lib.bk_clip_state(p, C.byref(got))
        want = State(*native.state())
        assert bytes(got) == bytes(want), (label, native.state())
        state_hash.update(bytes(got))

    def advance(p, seconds, reverse, label):
        nonlocal frames, reverse_frames
        dt, sample = C.c_float(seconds).value, Sample()
        native.call(0x402e18 if reverse else 0x4026fe,
                    struct.pack('<If', native.obj, dt))
        ok = lib.bk_clip_advance_plain(p, dt, 0, C.byref(sample), error) if reverse \
            else lib.bk_clip_advance(p, dt, C.byref(sample), error)
        assert ok, (label, error.value)
        assert native.calls == [tuple(getattr(sample, k) for k, _ in Sample._fields_)], label
        compare(p, label)
        frames += 1
        reverse_frames += int(dt < 0)

    #Real secondary loops: compare the predicted next boundary against
    #the native scheduler's observed loop counter, including suppression.
    archive = Archive(args.data/'bk3_09.pp')
    for group in range(1, 6):
        name = f'h0{group}_02.xan'
        raw = archive.read(next(e for e in archive.entries if e.name == name))
        definition = lib.bk_clip_set_decode(raw, len(raw), error)
        assert definition, error.value
        try:
            for mode, intervals in [('fixed60', [1/60]),
                                     ('mixed-ms', [.020, .033, .014, .038])]:
                native.bind(raw, True)
                p = lib.bk_clip_player_create_loaded(definition, error)
                assert p, error.value
                try:
                    native.select(12, 1)
                    assert lib.bk_clip_select(p, 12, 1, error), error.value
                    hits = crossings = 0
                    maximum = 0.
                    d = lib.bk_clip_definition(definition, 12).contents
                    for frame in range(4096):
                        before = State()
                        assert lib.bk_clip_state(p, C.byref(before))
                        full_dt = C.c_float(intervals[frame % len(intervals)]).value
                        dt = C.c_float(full_dt*.5).value
                        delta = 0 if not before.blend_done and before.blend_elapsed < 1e-6 \
                            else C.c_float(dt*60).value
                        crossing = delta > 0 and (C.c_float(before.elapsed+delta).value >= d.duration or
                            C.c_float(before.source+delta*before.rate).value > d.end)
                        hits += before.source >= d.end-full_dt*C.c_float(.3).value*60*2
                        maximum = max(maximum, before.source)
                        advance(p, dt, False, (name, mode, frame))
                        after = State(*native.state())
                        assert crossing == (after.loops > before.loops), (name, mode, frame)
                        crossings += int(crossing)
                    assert crossings > 0
                    if group >= 4 and mode == 'fixed60': assert hits == 0
                    results.append(dict(asset='bk3_09/'+name, mode=mode,
                                        native_window_hits=hits, loop_crossings=crossings,
                                        max_source=maximum))
                finally:
                    lib.bk_clip_player_destroy(p)
        finally:
            lib.bk_clip_set_destroy(definition)

    for pack, names in [
        ('bk3_10', [f'h0{g}_{s:02}.xan' for g in range(1, 6) for s in (3, 4, 5)]),
        ('bk3_11', [f'h0{g}_10.xan' for g in range(1, 6)] + ['h03_30.xan', 'h03_31.xan']),
    ]:
        archive = Archive(args.data/(pack+'.pp'))
        for name in names:
            raw = archive.read(next(e for e in archive.entries if e.name == name))
            definition = lib.bk_clip_set_decode(raw, len(raw), error)
            assert definition, error.value
            try:
                if pack == 'bk3_10':
                    for mode, intervals in [('fixed60', [1/120]),
                                             ('mixed-ms', [.010, .0165, .007, .019])]:
                        native.bind(raw, True)
                        p = lib.bk_clip_player_create_loaded(definition, error)
                        assert p, error.value
                        try:
                            for slot in (5, 6, 7, 10, 11, 13, 14, 15, 17, 18):
                                edit = Edit(slot, 1, 1, 0, 0)
                                assert lib.bk_clip_edit(p, C.byref(edit), 1, error), error.value
                                native.word(native.obj+0x190+slot*156+0x70, 1)
                            native.select(6, 1)
                            assert lib.bk_clip_select(p, 6, 1, error), error.value
                            hits, maximum = 0, 0.
                            for frame in range(4096):
                                advance(p, intervals[frame % len(intervals)], False,
                                        (name, mode, frame))
                                for slot in (6, 7, 10, 11):
                                    timing, loop, completed = Timing(), C.c_int32(), C.c_int()
                                    assert lib.bk_clip_timing(p, slot, C.byref(timing))
                                    assert lib.bk_clip_loop_mode(p, slot, C.byref(loop))
                                    assert lib.bk_clip_completed_chain(p, slot, C.byref(completed))
                                    ptr = native.obj+0x190+slot*156
                                    want = Timing(*struct.unpack('<2f', native.uc.mem_read(ptr+0x54, 8)),
                                                  struct.unpack('<f', native.uc.mem_read(ptr+0x60, 4))[0])
                                    assert bytes(timing) == bytes(want)
                                    assert loop.value == struct.unpack('<i', native.uc.mem_read(ptr, 4))[0]
                                    elapsed = struct.unpack('<f', native.uc.mem_read(ptr+0x44, 4))[0]
                                    duration = struct.unpack('<i', native.uc.mem_read(ptr+0x50, 4))[0]
                                    rate = struct.unpack('<f', native.uc.mem_read(ptr+0x5c, 4))[0]
                                    chain = struct.unpack('<i', native.uc.mem_read(ptr+0x70, 4))[0]
                                    expected = native.state()[0] != slot and chain and not loop.value and \
                                        duration > 0 and rate > 0 and want.source > want.start and elapsed >= duration
                                    assert completed.value == bool(expected)
                                    table_checks += 4
                                    if slot == 7:
                                        maximum = max(maximum, timing.source)
                                        hits += int(timing.source >= timing.end)
                            if name == 'h01_03.xan':
                                assert (hits == 0) if mode == 'fixed60' else (hits > 0)
                            results.append(dict(asset=pack+'/'+name, mode=mode,
                                                end_hits=hits, max_source=maximum))
                        finally:
                            lib.bk_clip_player_destroy(p)
                else:
                    for slot in range(128):
                        d = lib.bk_clip_definition(definition, slot).contents
                        if not d.active or d.start < 1 or d.end <= d.start or d.duration <= 0:
                            continue
                        native.bind(raw)
                        p = lib.bk_clip_player_create(definition, error)
                        assert p, error.value
                        try:
                            native.select(slot, 1)
                            assert lib.bk_clip_select(p, slot, 1, error)
                            source = C.c_float((d.start+d.end)/2).value
                            elapsed = C.c_float(d.duration/2).value
                            native.uc.mem_write(native.obj+0x190+slot*156+0x44, struct.pack('<f', elapsed))
                            native.uc.mem_write(native.obj+0x190+slot*156+0x60, struct.pack('<f', source))
                            assert lib.bk_clip_set_clock(p, slot, elapsed, source, error)
                            for frame in range(64):
                                advance(p, (-1 if frame % 2 == 0 else 1)/120,
                                        True, (name, slot, frame))
                        finally:
                            lib.bk_clip_player_destroy(p)
            finally:
                lib.bk_clip_set_destroy(definition)
    report = dict(passed=True, exe_sha256=digest, frames=frames,
                  reverse_frames=reverse_frames, table_checks=table_checks,
                  results=results, state_sha256=state_hash.hexdigest(),
                  scope=__doc__, fixed60_selected_end_stall_preserved=True,
                  fixed60_secondary_window_stall_preserved=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS gallery timing:', frames, reverse_frames, table_checks,
          state_hash.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
