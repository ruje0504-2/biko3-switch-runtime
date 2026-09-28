"""Pause4f720c: native music/spatial gain with no transport/timer/RNG/animation.
Also compare full4f737b after sharing music-fade implementation.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
from pathlib import Path
from original_background_oracle import State, Timer, Input, Commands, Native, events
from model_binding import ROOT, library

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    n, lib, rng = Native(exe), library(), random.Random(0x4f720c)
    lib.bk_background_pause_step.argtypes = [C.POINTER(State), C.POINTER(Input), C.POINTER(Commands)]
    lib.bk_background_step.argtypes = [C.POINTER(State), C.POINTER(C.c_uint32), C.POINTER(Input), C.POINTER(Commands)]
    counts = {'pause_frames': 0, 'full_frames': 0, 'pause_commands': 0, 'full_commands': 0}
    for case in range(6000):
        s = State(Timer(rng.getrandbits(32), rng.getrandbits(32), rng.randrange(3)), rng.randrange(-10000, 1), rng.choice([-1, 0, 1, 2]), (C.c_int8 * 8)(*(rng.choice([-1, 0, 1, 2]) for _ in range(8))))
        inp = Input()
        inp.now, inp.seconds = rng.getrandbits(32), rng.choice([0, .0001, 1/60, .5, 2, 10])
        inp.group, inp.area = (2, 8) if case % 3 == 0 else (rng.randrange(5), rng.randrange(9))
        inp.background_clip, inp.door_clip = rng.randrange(6), rng.randrange(6)
        inp.music_master, inp.effect_master = rng.randrange(-10000, 1), rng.randrange(-10000, 1)
        inp.player[:], inp.npc[:] = [rng.uniform(-200, 200) for _ in range(3)], [rng.uniform(-200, 200) for _ in range(3)]
        inp.player_yaw = rng.uniform(-720, 720)
        inp.background_present, inp.weather_enabled, inp.weather_present = 1, 1, 1
        inp.ambient_gate = rng.choice([-1, 0, 1, 2])
        seed = rng.getrandbits(32)
        for a in inp.ambient:
            a.present, a.playing, a.loop = rng.randrange(4) != 0, rng.randrange(2), rng.randrange(2)
            a.position[:] = [rng.uniform(-200, 200) for _ in range(3)]
            a.trigger = rng.choice([-1, 0, 0, 1, 37, 99.9, 100, 100.5, 101, 102, 103])
        for kind, address in [('pause', 0x4f720c), ('full', 0x4f737b)]:
            wanted, random_out, choice = n.run(s, seed, inp, address)
            actual, cmd, random_state = State.from_buffer_copy(s), Commands(), C.c_uint32(seed)
            if kind == 'pause':
                assert lib.bk_background_pause_step(C.byref(actual), C.byref(inp), C.byref(cmd))
                assert bytes(actual.ambient_timer) == bytes(s.ambient_timer)
                assert bytes(actual.ambient_latch) == bytes(s.ambient_latch)
                assert random_out == seed
                assert cmd.random_choice == 0
                assert not any(e[0] in ['play', 'stop', 'request', 'advance', 'place'] for e in n.events)
            else:
                assert lib.bk_background_step(C.byref(actual), C.byref(random_state), C.byref(inp), C.byref(cmd))
                assert cmd.random_choice == choice
            assert bytes(actual) == bytes(wanted), (kind, case, bytes(actual).hex(), bytes(wanted).hex())
            assert random_state.value == random_out
            assert events(cmd, n) == n.events, (kind, case, events(cmd, n), n.events)
            counts[kind + '_frames'] += 1
            counts[kind + '_commands'] += len(n.events)
    before, out_before = bytes(actual), bytes(cmd)
    inp.seconds = float('nan')
    assert not lib.bk_background_pause_step(C.byref(actual), C.byref(inp), C.byref(cmd))
    assert bytes(actual) == before and bytes(cmd) == out_before
    report = dict(passed=True, exe_sha256=digest, **counts, scope=__doc__)
    (ROOT / 'local/original-pause-background-oracle.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)

if __name__ == '__main__':
    main()
