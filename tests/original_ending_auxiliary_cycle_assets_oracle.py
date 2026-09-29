"""4965b9 with actual five bk3_08 actors, original scheduler/SRT/publication.

Explicit495d92 mode1 then3 establishes the automatic clip ring. After that,
the original automatic controller, configured requests, RNG and animation
scheduler run without replacement. Voice is observed and the optional eye
texture target is absent; actual PCM/eyes use the separate resource probe.
This is a component fixture, not natural ending entry or its parent stage.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from original_actor_clip_edits_oracle import Native, Timing
from original_actor_phase_oracle import bind
from original_ending_auxiliary_oracle import (
    Native as Aux, State as AuxState, Frame, Ops, Active, Write, Request,
    Audio, Eyes,
)
from original_ending_aux_clips_oracle import Native as AuxGlobals
from original_ending_auxiliary_tick_oracle import Prediction, Cycle, TickOps, Predict, Random
from original_clip_edits_oracle import Edit
from playback_binding import library
from model_binding import ROOT, decode
from clip_binding import State
from bk3_assets import Archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    args = parser.parse_args()
    exe, lib = args.exe.read_bytes(), library()
    bind(lib)
    archive, error = Archive(args.data / 'bk3_08.pp'), C.create_string_buffer(256)
    for name, types in [
        ('bk_actor_pose_edit_clips', [C.c_void_p, C.POINTER(Edit), C.c_size_t, C.c_void_p]),
        ('bk_actor_pose_request_mode', [C.c_void_p, C.c_uint, C.c_int, C.c_void_p]),
        ('bk_actor_pose_advance', [C.c_void_p, C.c_int, C.c_float, C.c_void_p]),
        ('bk_actor_pose_timing', [C.c_void_p, C.c_uint, C.POINTER(Timing)]),
        ('bk_actor_pose_prediction', [C.c_void_p, C.c_uint, C.POINTER(Prediction)]),
        ('bk_ending_auxiliary_change', [C.POINTER(AuxState), C.POINTER(Frame),
            C.c_int32, C.c_int32, C.c_int32, C.POINTER(Ops), C.POINTER(C.c_int32), C.c_void_p]),
        ('bk_ending_auxiliary_tick', [C.POINTER(AuxState), C.POINTER(Frame),
            C.POINTER(Cycle), C.c_float, C.POINTER(C.c_int32), C.POINTER(TickOps), C.c_void_p]),
        ('bk_random_next', [C.POINTER(C.c_uint32)]),
    ]:
        getattr(lib, name).argtypes = types
    lib.bk_ending_auxiliary_cycle_initial.restype = Cycle
    worst, matrices, frames, changes, calls, clocks = 0.0, 0, 0, 0, 0, 0
    transitions = {13: 0, 14: 0}
    cycle, seed = lib.bk_ending_auxiliary_cycle_initial(), C.c_uint32(0x4965b9)
    voice = C.c_int32(-800)

    def equal(got, want, label):
        nonlocal worst
        for index, (g, w) in enumerate(zip(got, want)):
            # Loaded static zero-duration slots may retain the same -Inf
            # rate. Equality is valid; subtracting matching infinities is not.
            if g == w:
                continue
            difference = abs(g - w) / max(1, abs(w))
            worst = max(worst, difference)
            assert math.isfinite(difference) and difference < 3e-5, (label, index, g, w)

    for group in range(1, 6):
        name = f'h{group:02}_00'
        raw = archive.read(next(x for x in archive.entries if x.name == name + '.x'))
        xan = archive.read(next(x for x in archive.entries if x.name == name + '.xan'))
        result, model, message = decode(lib, raw)
        assert result == 1, message
        clips = lib.bk_clip_set_decode(xan, len(xan), error)
        assert clips, error.value
        actor = None
        try:
            vm = Native(exe)
            vm.bind(raw, model.contents)
            vm.bind_clip(xan, vm.root, 4, False)
            origin = (C.c_float * 3)(0, 0, 0)
            vm.place_actor(origin, 0)
            observer = Aux.__new__(Aux)
            observer.u, observer.actor = vm.uc, vm.clip
            for address in (0x4946b4, 0x49490a, 0x4a07a9, 0x4ad2bf):
                vm.uc.hook_add(UC_HOOK_CODE, observer.hook, begin=address, end=address)
            actor = lib.bk_actor_pose_create(model, clips, vm.root, None, origin, 0, 4, 0, error)
            assert actor, error.value
            vm.publish()
            lib.bk_actor_pose_publish(actor)
            state, frame = AuxState(1, 0, 0, 10, 0, 0, 0, .7, 0, 0), Frame()
            frame.group, frame.phase, frame.auxiliary_mode = group - 1, 6, 3
            trace = []

            @Active
            def active(_, out, err):
                clock = State()
                ok = lib.bk_actor_pose_state(actor, C.byref(clock))
                out[0] = clock.slot
                return ok

            @Write
            def write(_, index, kind, value, err):
                timing = Timing()
                assert lib.bk_actor_pose_timing(actor, index, C.byref(timing))
                edit = Edit(index, (1, 2, 4)[kind], value if kind == 0 else 0,
                            value if kind == 1 else 0, timing.start if kind == 2 else 0)
                return lib.bk_actor_pose_edit_clips(actor, C.byref(edit), 1, err)

            @Request
            def request(_, index, err):
                return lib.bk_actor_pose_request_mode(actor, index, 1, err)

            @Audio
            def audio(_, call, out, err):
                c = call.contents
                trace.append(('audio', c.operation, c.slot, c.cue, c.bank, c.flags, c.volume))
                out[0] = 0
                return 1

            @Eyes
            def eyes(_, index, err):
                trace.append(('eyes', index))
                return 1

            @Predict
            def prediction(_, index, out, err):
                return lib.bk_actor_pose_prediction(actor, index, out)

            @Random
            def random_value(_, out, err):
                out[0] = lib.bk_random_next(C.byref(seed))
                return 1

            ops = Ops(None, active, write, request, audio, eyes)
            tick_ops = TickOps(ops, prediction, random_value)

            def globals():
                AuxGlobals.globals(observer, state, frame)
                observer.word(0x721b28, vm.clip)

            def check(label):
                nonlocal matrices, clocks
                fingerprint = hashlib.sha256()
                for node in range(model.contents.frame_count):
                    for function, offset in (('bk_actor_pose_local', 0x80), ('bk_actor_pose_frame', 0xc0)):
                        values = getattr(lib, function)(actor, node)[:16]
                        equal(values, vm.floats(vm.frames + node * 0x400 + offset, 16),
                              (name, label, node, function))
                        fingerprint.update(struct.pack('<16f', *values))
                        matrices += 1
                clock = State()
                assert lib.bk_actor_pose_state(actor, C.byref(clock))
                equal([getattr(clock, k) for k, _ in State._fields_], vm.state(), (name, label, 'clock'))
                for index in range(128):
                    snapshot = Prediction()
                    assert lib.bk_actor_pose_prediction(actor, index, C.byref(snapshot))
                    base = vm.clip + 0x190 + index * 156
                    expected = [struct.unpack('<i', vm.uc.mem_read(base + 0x50, 4))[0]] + [
                        struct.unpack('<f', vm.uc.mem_read(base + off, 4))[0] for off in (0x58, 0x60, 0x5c)]
                    equal([snapshot.duration, snapshot.end, snapshot.source, snapshot.rate], expected,
                          (name, label, 'prediction', index))
                    clocks += 1
                return fingerprint.digest()

            check('initial')
            for step in range(360):
                # Original mode changes form the ring; no hand-written clip
                # links, active flags or fabricated completion inputs.
                if step < 2:
                    globals()
                    expected = vm.call(0x495d92, struct.pack('<i', 1 if step == 0 else 3))
                    snapshot = observer.read()[:2]
                    result = C.c_int32()
                    assert lib.bk_ending_auxiliary_change(C.byref(state), C.byref(frame),
                            1 if step == 0 else 3, -800, -500, C.byref(ops), C.byref(result), error), error.value
                    assert result.value == expected and (bytes(state), bytes(frame)) == snapshot
                    changes += 1
                dt = C.c_float(.016 if step < 2 else (0, .016, .1, .5, 1)[step % 5]).value
                assert lib.bk_actor_pose_advance(actor, -1, dt, error), error.value
                vm.call(0x4026fe, struct.pack('<If', vm.clip, dt))
                held = check((step, 'before automatic'))
                if step % 60 == 0:
                    cycle.scale = .01 if step % 120 else .02
                globals()
                observer.word(0x58edd8, seed.value)
                observer.word(0x55469c, struct.unpack('<I', struct.pack('<f', cycle.scale))[0])
                vm.uc.mem_write(0x55696d, bytes([cycle.countdown & 255]))
                observer.word(0x733700, struct.unpack('<I', struct.pack('<f', dt))[0])
                vm.call(0x4965b9, b'')
                expected_state = observer.read()[:2]
                expected_trace = [event for event, _ in observer.trace]
                expected_seed = struct.unpack('<I', vm.uc.mem_read(0x58edd8, 4))[0]
                expected_countdown = struct.unpack('<b', vm.uc.mem_read(0x55696d, 1))[0]
                trace.clear()
                assert lib.bk_ending_auxiliary_tick(C.byref(state), C.byref(frame),
                        C.byref(cycle), dt, C.byref(voice), C.byref(tick_ops), error), error.value
                assert (bytes(state), bytes(frame)) == expected_state, (name, step, 'aliases')
                assert (seed.value, cycle.countdown) == (expected_seed, expected_countdown), (name, step, 'retained')
                assert trace == expected_trace, (name, step, trace, expected_trace)
                assert held == check((step, 'after automatic')), (name, step, 'unexpected pose mutation')
                for event in trace:
                    if event[:2] == ('audio', 3):
                        transitions[13 if event[3] == 9 else 14] += 1
                calls += len(trace)
                vm.publish()
                lib.bk_actor_pose_publish(actor)
                check((step, 'published'))
                frames += 1
            print(name, 'PASS', flush=True)
        finally:
            lib.bk_actor_pose_destroy(actor)
            lib.bk_clip_set_destroy(clips)
            lib.bk_model_destroy(model)
    assert all(transitions.values())
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(), actors=5,
            frames=frames, setup_mode_changes=changes, service_calls=calls,
            transitions=transitions, matrices=matrices, slot_clock_snapshots=clocks,
            max_relative_error=worst, scope=__doc__)
    (ROOT / 'local/original-ending-auxiliary-cycle-assets-oracle.json').write_text(
            json.dumps(report, indent=2) + '\n')
    print('PASS', json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
