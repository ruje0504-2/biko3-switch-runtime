"""Original4e252b: manual action selection, ordered requests and clip links.

Original parent and descriptor writes execute; model requests/root visibility
are observed services. Shared group, selected key, mode and clip can change at
these callbacks. Compares all128 descriptor links and all retained scalars.
This is a controller oracle, not a real-resource/production validation.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
from pathlib import Path
from original_ending_normal_action_oracle import (
    Fixture as ActionFixture, Native as ActionNative, fixture as action_fixture,
    SHARED as ACTION_SHARED, I, U, P, Actor)
from original_ending_normal_control_oracle import (
    State as ParentState, Bindings as ParentBindings, Ops as ParentOps)
from model_binding import ROOT, library

Write = C.CFUNCTYPE(I, P, I, I, I, P)


class Ops(C.Structure):
    _fields_ = [('normal', ParentOps), ('write', Write)]


SHARED = ACTION_SHARED + ['present', 'playing', 'manual_state', 'manual_clip', 'links']


class Fixture(ActionFixture):
    def clone(self):
        out = super().clone()
        for name in ['manual_state', 'manual_clip', 'links']:
            value = getattr(self, name)
            setattr(out, name, type(value).from_buffer_copy(value))
        return out

    def snapshot(self):
        return super().snapshot() + tuple(bytes(getattr(self, n)) for n in
                                          ['manual_state', 'manual_clip', 'links'])

    def effect(self, event, number):
        super().effect(event, number)
        if number == self.mutate_at:
            self.manual_state.manual_choice = (self.manual_state.manual_choice + 1) % 8
            self.manual_state.manual_action = (self.manual_state.manual_action + 1) % 4
            self.manual_clip.value += 2


class Native(ActionNative):
    def spans(self, s):
        yield from super().spans(s)
        yield 0x575634, C.addressof(s.manual_state) + ParentState.manual_choice.offset, 1
        yield 0x719b60, C.addressof(s.manual_state) + ParentState.manual_action.offset, 1
        yield 0x709db4, C.addressof(s.manual_clip), 4
        for slot in range(128):
            yield self.primary + slot * 0x9c + 0x200, C.addressof(s.links[slot]), 8

    def run(self, fixture):
        self.current = fixture.clone()
        self.write(self.current)
        self.trace, self.last_loaded, self.undefined_voice = [], 0, None
        self.call(0x4e252b, b'')
        return self.trace, self.read().snapshot()


def portable(lib, fixture, fail_at=0):
    s, trace, error = fixture.clone(), [], C.create_string_buffer(256)

    def commit(event):
        trace.append((event, s.snapshot()))
        if len(trace) == fail_at:
            return 0
        s.effect(event, len(trace))
        return 1

    @Actor
    def root(_c, actor, flag, _e):
        return commit(('root', actor, flag))

    @Actor
    def request(_c, actor, clip, _e):
        return commit(('request', actor, clip))

    @Write
    def write(_c, clip, field, value, _e):
        assert 0 <= clip < 128 and field in (0, 1)
        s.links[clip][field] = value
        return 1

    b = ParentBindings(frame=C.pointer(s.frame), auxiliary=C.pointer(s.aux))
    ops = Ops(ParentOps(request=request, root_flag=root), write)
    ok = lib.bk_ending_normal_manual_step(C.byref(s.manual_clip),
        C.byref(s.manual_state), C.byref(b), C.byref(ops), error)
    return ok, trace, s.snapshot(), error.value


def fixture(rng, case):
    base = action_fixture(rng, case)
    s = Fixture()
    s.__dict__.update(base.__dict__)
    s.manual_state = ParentState()
    s.manual_state.manual_choice = [-128, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 127][case % 12]
    s.manual_state.manual_action = [-128, -1, 0, 1, 2, 3, 4, 127][case // 12 % 8]
    s.manual_clip = I(rng.randrange(3, 97))
    s.links = ((I * 2) * 128)(*[(I * 2)(rng.randrange(2), rng.randrange(128))
                               for _ in range(128)])
    s.actor_exists = 1
    s.mutate_at = rng.randrange(1, 5) if case % 4 == 0 else 0
    return s


def compare(label, a, b):
    for n, (av, bv) in enumerate(zip(a, b)):
        assert av == bv, (label, SHARED[n], av, bv)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--cases', type=int, default=7680)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    n, lib = Native(exe), library()
    lib.bk_ending_normal_manual_step.argtypes = [C.POINTER(I),
        C.POINTER(ParentState), C.POINTER(ParentBindings), C.POINTER(Ops), P]
    rng = random.Random(0x4e252b)
    events, modes, mutated, failed = {}, {}, 0, 0
    for case in range(args.cases):
        s = fixture(rng, case)
        expected, final = n.run(s)
        ok, actual, got, error = portable(lib, s)
        assert ok, (case, error)
        assert len(actual) == len(expected), (case, 'call count', actual, expected)
        for step, (a, b) in enumerate(zip(actual, expected)):
            assert a[0] == b[0], (case, step, a[0], b[0])
            compare((case, step, a[0]), a[1], b[1])
            events[a[0][0]] = events.get(a[0][0], 0) + 1
        compare((case, 'final'), got, final)
        mode = str(s.manual_state.manual_action)
        modes[mode] = modes.get(mode, 0) + 1
        mutated += bool(0 < s.mutate_at <= len(expected))
        if case % 7 == 0:
            stop = 1 + case % len(expected)
            ok, prefix, got, _ = portable(lib, s, stop)
            assert not ok and prefix == expected[:stop]
            compare((case, 'failed prefix'), got, expected[stop - 1][1])
            failed += 1
    report = dict(passed=True, exe_sha256=digest, frames=args.cases, modes=modes,
                  service_calls=events, callback_mutations=mutated,
                  failed_prefixes=failed, descriptor_links=128 * args.cases,
                  scope=__doc__)
    (ROOT / 'local/original-ending-normal-manual-oracle.json').write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
