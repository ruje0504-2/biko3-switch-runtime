"""4018c8 versus401b0a on actual loaded XAN descriptors, including empties.

The native request functions and state writes execute unmodified. Only
model/root pointer setup is a fixture. No animation, rendering or gameplay
equivalence is claimed. Configured requests must retain empty-slot state,
while ten-tick requests may select it. Active/requested and hidden cases
remain distinct. Tests every slot of selected actual camera/background and
third-ending primary/auxiliary files, with ordinary/sanitized parity.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path

from original_clip_oracle import Native
from clip_binding import library, State
from model_binding import ROOT
from bk3_assets import Archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/original-configured-request-oracle.json')
    args = parser.parse_args()
    exe, lib = args.exe.read_bytes(), library()
    lib.bk_clip_player_create_loaded.argtypes = [C.c_void_p, C.c_void_p]
    lib.bk_clip_player_create_loaded.restype = C.c_void_p
    vm, error = Native(exe), C.create_string_buffer(256)
    records, checks, empty_configured, empty_ten_tick, worst = [], 0, 0, 0, 0.0
    digest = hashlib.sha256()
    names = {
        'bk3_11': [f'h{i:02}_10.xan' for i in range(1, 6)] + ['h03_30.xan', 'h03_31.xan'],
        'bk3_03': [f'm{i:02}_{v}.xan' for i in range(1, 6) for v in [90, 91]],
        'bk3_04': ['cam00_00.xan', 'cam00_03.xan'],
    }
    for pack, files in names.items():
        archive = Archive(args.data / (pack + '.pp'))
        for name in files:
            raw = archive.read(next(e for e in archive.entries if e.name == name))
            clips = lib.bk_clip_set_decode(raw, len(raw), error)
            assert clips, (name, error.value)
            try:
                active = [i for i in range(128) if lib.bk_clip_definition(clips, i).contents.active]
                for target in range(128):
                    for mode in [0, 1]:
                        for selected in [False, True]:
                            player = lib.bk_clip_player_create_loaded(clips, error)
                            assert player, (name, error.value)
                            try:
                                vm.bind(raw, authored=True)
                                vm.word(vm.root + 0x70, int(selected))
                                if selected and active:
                                    vm.select(active[-1], 1)
                                    assert lib.bk_clip_select(player, active[-1], 1, error)
                                before = State()
                                assert lib.bk_clip_state(player, C.byref(before))
                                # Repeat covers both differing and already
                                # requested targets, including empty no-ops.
                                for repeat in range(2):
                                    vm.select(target, 3 if mode else 2)
                                    assert lib.bk_clip_request_mode(player, target, mode, error), (name, target, mode, error.value)
                                    state = State()
                                    assert lib.bk_clip_state(player, C.byref(state))
                                    expected = vm.state()
                                    for i, (field, _) in enumerate(State._fields_):
                                        got, want = getattr(state, field), expected[i]
                                        delta = abs(got - want) / max(1, abs(want))
                                        worst = max(worst, delta)
                                        assert math.isfinite(delta) and delta <= 1e-6, (name, target, mode, selected, repeat, field, got, want)
                                    if not lib.bk_clip_definition(clips, target).contents.active:
                                        if mode:
                                            assert bytes(state) == bytes(before)
                                            empty_configured += 1
                                        else:
                                            assert state.slot == state.requested == target
                                            empty_ten_tick += 1
                                    digest.update(bytes(state))
                                    checks += 1
                            finally:
                                lib.bk_clip_player_destroy(player)
                records.append(dict(pack=pack, name=name, sha256=hashlib.sha256(raw).hexdigest(), active_slots=active))
            finally:
                lib.bk_clip_set_destroy(clips)
    result = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), checks=checks,
                  empty_configured=empty_configured, empty_ten_tick=empty_ten_tick,
                  max_normalized_error=worst, state_sha256=digest.hexdigest(), records=records, scope=__doc__)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS configured request', checks, empty_configured, empty_ten_tick, worst, digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
