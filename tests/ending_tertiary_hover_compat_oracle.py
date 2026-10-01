"""Inactive-target hover compatibility, separate from strict476720 equivalence.

Defined frames execute the original unmodified CPU controller. For the two
undefined disabled-target branches and the opening target still pickable in
clip4, the comparison skips477305..4773A9
in the x86 controller. This deliberately suppresses an uninitialized hover
cue, preserving the actual target and subsequent key/selection processing.
It is not evidence of equivalence to the original undefined behavior.
"""
import argparse
import ctypes as C
import hashlib
import itertools
import json
import random
from pathlib import Path
from types import SimpleNamespace

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP
from original_ending_tertiary_control_oracle import (
    Bindings, F, Input, Native, Ops, P, State, compare, fixture, library, portable)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    assert hashlib.sha256(exe).hexdigest() == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    lib = library()
    signature = [C.POINTER(State), C.POINTER(Bindings), C.POINTER(Input),
                 F, C.POINTER(Ops), P]
    lib.bk_ending_tertiary_control_step.argtypes = signature
    lib.bk_ending_tertiary_control_play.argtypes = signature
    playable = SimpleNamespace(bk_ending_tertiary_control_step=lib.bk_ending_tertiary_control_play)
    native = Native(exe)
    rng = random.Random(0x477305)
    digest = hashlib.sha256()
    for case in range(1200):
        f = fixture(rng, case)
        want, expected = native.run(f)
        ok, got, trace, error = portable(playable, f)
        assert ok, (case, error)
        compare(want, got, expected, trace, ('defined', case))
        digest.update(bytes(got.state) + bytes(got.records))

    patched = Native(exe)
    skips = 0
    def skip_undefined(u, _address, _size, _context):
        nonlocal skips
        f = patched.f
        assert f.pick_result[1] == f.actions[5] and f.unavailable[0] or \
               f.pick_result[1] == f.actions[10] and f.unavailable[1] or \
               f.pick_result[1] == f.initial_targets[f.frame.group]
        skips += 1
        u.reg_write(UC_X86_REG_EIP, 0x4773aa)
    patched.u.hook_add(UC_HOOK_CODE, skip_undefined, begin=0x477305, end=0x477305)
    cases = failures = confirmations = 0
    for group, blocked, flag, keys, choice, latch in itertools.product(
            range(5), range(2), [1, 2, -1], [0, 1 << 4, 1 << 6, 1 << 7], [-1, 0, 1], [0, 1]):
        f = fixture(rng, 1)
        f.frame.group = group
        f.active[0] = 4
        f.open.value = 0
        f.frame.camera_mode = 0
        f.present[1] = False
        f.auxiliary.pending = 0
        f.pending_effect.value = -1
        f.pick_result = (1, f.actions[5 if blocked == 0 else 10])
        f.unavailable[blocked] = flag
        f.keys, f.chosen = keys, choice
        f.latches[1] = latch
        f.previous.value = 8
        ok, _, _, error = portable(lib, f)
        assert not ok and 'uninitialized local' in error
        want, expected = patched.run(f)
        ok, got, trace, error = portable(playable, f)
        assert ok, (cases, error)
        compare(want, got, expected, trace, ('disabled', cases))
        assert got.frame.camera_cached == f.pick_result[1]
        assert bytes(got.records) == bytes(f.records)
        assert got.face_mode.value == f.face_mode.value and bytes(got.latches) == bytes(f.latches)
        assert bytes(got.state) == bytes(f.state)
        events = [e[0] for e, _ in trace]
        assert not set(events) & {'expression', 'voice', 'request', 'begin'}
        assert ('choose' in events) == bool(keys)
        confirmations += bool(keys)
        digest.update(bytes(got.state) + bytes(got.records))
        if cases % 19 == 0:
            at = 1 + cases % len(trace)
            want, expected = patched.run(f, at)
            ok, got, prefix, _ = portable(playable, f, at)
            assert not ok
            compare(want, got, expected, prefix, ('prefix', cases))
            failures += 1
        cases += 1
    opening_cases = 0
    for group, keys, choice, latch in itertools.product(
            range(5), [0, 1 << 4, 1 << 6, 1 << 7], [-1, 0, 1], [0, 1]):
        f = fixture(rng, 1)
        f.frame.group = group
        f.active[0] = 4
        f.open.value = 0
        f.frame.camera_mode = 0
        f.present[1] = False
        f.auxiliary.pending = 0
        f.pending_effect.value = -1
        f.initial_targets[:] = [17, 32, 17, 17, 32]
        f.pick_result = (1, f.initial_targets[group])
        f.keys, f.chosen = keys, choice
        f.latches[1] = latch
        f.previous.value = 8
        ok, _, _, error = portable(lib, f)
        assert not ok and 'uninitialized local' in error
        want, expected = patched.run(f)
        ok, got, trace, error = portable(playable, f)
        assert ok, (opening_cases, error)
        compare(want, got, expected, trace, ('opening', opening_cases))
        assert got.frame.camera_cached == f.pick_result[1]
        assert bytes(got.records) == bytes(f.records)
        assert got.face_mode.value == f.face_mode.value and bytes(got.latches) == bytes(f.latches)
        assert bytes(got.state) == bytes(f.state)
        events = [event[0] for event, _ in trace]
        assert not set(events) & {'expression', 'voice', 'request', 'begin'}
        assert ('choose' in events) == bool(keys)
        confirmations += bool(keys)
        digest.update(bytes(got.state) + bytes(got.records))
        opening_cases += 1
    # Keep unrelated undefined targets explicit; compatibility is not a blanket
    # success fallback for invalid control input.
    f.pick_result = (1, 38)
    ok, _, _, error = portable(playable, f)
    assert not ok and 'uninitialized local' in error
    report = dict(passed=True, scope=__doc__, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  defined_frames=1200, disabled_frames=cases, opening_target_frames=opening_cases,
                  confirmations=confirmations,
                  failure_prefixes=failures, patched_native_skips=skips,
                  unrelated_rejections=1, state_sha256=digest.hexdigest())
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS tertiary hover compatibility:', json.dumps(report, sort_keys=True))


if __name__ == '__main__':
    main()
