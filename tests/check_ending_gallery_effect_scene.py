"""Real Japanese actors and PCM with48CC18 audio commands from native execution.

Explicit clip/time inputs across30 selected asset configurations and60 loads;
this verifies the reusable gallery effect adapter, not a natural phase8 load.
The reference mixer replays only commands observed from the native function.
Compare state, shared RNG, status, gain, consumed PCM and unchanged actor
clocks/locals/worlds. No GPU, Switch or complete gallery acceptance is implied.
"""
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random

from original_ending_gallery_effect_oracle import (
    ROOT, I, U, F, P, State, Timing, Bindings, AudioCall, Native, fixture)
from ending_selected_binding import bind, Load, Config, CameraState, Presets
from playback_binding import library
from clip_binding import State as ClipState

PACKS = ['bk3_10', 'bk3_13', 'bk3_03', 'bk3_04', 'fambom', 'bk3_02', 'bk3_06']
Submit = C.CFUNCTYPE(I, P, C.POINTER(C.c_int16), C.c_size_t, P)
Poll = C.CFUNCTYPE(I, P, C.POINTER(C.c_uint64), P)


class Sink(C.Structure):
    _fields_ = [('context', P), ('rate', U), ('block_frames', U),
                ('capacity_frames', U), ('submit', Submit), ('poll', Poll)]


class Edit(C.Structure):
    _fields_ = [('slot', U), ('fields', U), ('chain', I), ('next', I), ('source', F)]


class Output:
    def __init__(self):
        self.submitted = self.consumed = self.nonzero = 0
        self.hash = hashlib.sha256()
        def submit(_, pcm, n, _e):
            raw = C.string_at(pcm, n*4)
            self.hash.update(raw)
            self.nonzero += any(raw)
            self.submitted += n
            return 1
        def poll(_, n, _e):
            n[0] = self.consumed
            return 1
        self.submit, self.poll = Submit(submit), Poll(poll)
        self.sink = Sink(None, 48000, 480, 1920, self.submit, self.poll)


def main():
    faulthandler.enable()
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path)
    p.add_argument('data', type=Path)
    p.add_argument('--profiles', type=int, default=30)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    if not 1 <= args.profiles <= 30: p.error('profiles must be1..30')
    exe, lib = args.exe.read_bytes(), library()
    native = Native(exe)
    bind(lib)
    for name, inputs, result in [
        ('bk_actor_pose_select', [P, U, I, P], I),
        ('bk_actor_pose_edit_clips', [P, C.POINTER(Edit), C.c_size_t, P], I),
        ('bk_actor_pose_timing', [P, U, C.POINTER(Timing)], I),
        ('bk_actor_pose_state', [P, C.POINTER(ClipState)], I),
        ('bk_actor_pose_world', [P, C.POINTER(C.c_size_t)], C.POINTER(F)),
        ('bk_actor_pose_local', [P, U], C.POINTER(F)),
        ('bk_audio_create', [C.POINTER(Sink), P], P),
        ('bk_audio_destroy', [P], None),
        ('bk_audio_poll', [P, P], I), ('bk_audio_fill', [P, P], I),
        ('bk_audio_playing', [P, U, C.POINTER(I)], I),
        ('bk_audio_pause', [P, U, P], I),
        ('bk_audio_get_gain', [P, U, C.POINTER(I), C.POINTER(I)], I),
        ('bk_ending_audio_create_entry', [P, P, U, U, U, I, P], P),
        ('bk_ending_audio_destroy', [P], None),
        ('bk_ending_audio_present', [P, U, C.POINTER(I)], I),
        ('bk_ending_audio_call', [P, U, I, I, C.POINTER(AudioCall), C.POINTER(I), P], I),
        ('bk_ending_gallery_effect_apply', [P, P, C.POINTER(U), C.POINTER(Bindings), F, P], I),
    ]:
        fn = getattr(lib, name)
        fn.argtypes, fn.restype = inputs, result
    error = C.create_string_buffer(256)
    store = lib.bk_resources_create(error)
    assert store, error.value
    inputs = {pack: hashlib.sha256((args.data/(pack+'.pp')).read_bytes()).hexdigest() for pack in PACKS}
    records, state_digest = [], hashlib.sha256()
    frames = commands = voices = effects = undefined = retained = rejected = 0
    shared_rng = U(0x48cc18)
    effect_state = State()
    try:
        for pack in PACKS:
            assert lib.bk_resources_mount(store, pack.encode(), str(args.data/(pack+'.pp')).encode(), error), error.value
        for profile in range(args.profiles):
            group, variant, selection = profile//6, profile//3 % 2, profile % 3
            outputs, mixers, audio = [Output(), Output()], [], []
            try:
                for out in outputs:
                    mixer = lib.bk_audio_create(C.byref(out.sink), error)
                    assert mixer, error.value
                    mixers.append(mixer)
                    bank = lib.bk_ending_audio_create_entry(store, mixer, 0, group, variant, -1600, error)
                    assert bank, error.value
                    audio.append(bank)
                for reload in range(2):
                    owner = None
                    try:
                        config, camera, presets = Config(), CameraState(), Presets()
                        assert lib.bk_ending_selected_config(C.byref(config), group, variant, selection)
                        assert lib.bk_menu_camera_dialogue(C.byref(camera))
                        load = Load(group, variant, selection, -1, config.primary, None)
                        clocks = (U*4)(100, 101, 102, 103)
                        before = bytes(effect_state)
                        owner = lib.bk_ending_selected_assets_create(store, C.byref(load), clocks,
                            C.byref(shared_rng), C.byref(camera), C.byref(presets), error)
                        assert owner, error.value
                        assert before == bytes(effect_state)
                        retained += reload != 0
                        pose = lib.bk_ending_selected_assets_pose(owner, 0)
                        model = lib.bk_actor_pose_model(pose).contents
                        def actor_snapshot():
                            clock, count = ClipState(), C.c_size_t()
                            assert lib.bk_actor_pose_state(pose, C.byref(clock))
                            world = lib.bk_actor_pose_world(pose, C.byref(count))
                            local = b''.join(C.string_at(lib.bk_actor_pose_local(pose, n), 64)
                                             for n in range(model.frame_count))
                            timings = (Timing*32)()
                            for n in range(32):
                                assert lib.bk_actor_pose_timing(pose, n, C.byref(timings[n]))
                            return bytes(clock), C.string_at(world, count.value*4), local, bytes(timings)
                        for step in range(32):
                            slot = [6, 7, 10, 11, 17, 18, 4, 6][step % 8]
                            assert lib.bk_actor_pose_select(pose, slot, 1, error), error.value
                            #Explicit boundary inputs, never confused with natural playback.
                            edits = (Edit*6)()
                            for n, target in enumerate([6, 7, 10, 11, 17, 18]):
                                timing = Timing()
                                assert lib.bk_actor_pose_timing(pose, target, C.byref(timing))
                                source = [timing.start, timing.end-1, timing.end, timing.end][step % 4]
                                edits[n] = Edit(target, 4, 0, 0, source)
                            assert lib.bk_actor_pose_edit_clips(pose, edits, 6, error), error.value
                            for mixer, out in zip(mixers, outputs):
                                out.consumed = min(out.submitted, out.consumed+480)
                                assert lib.bk_audio_poll(mixer, error), error.value
                                present = I()
                                assert lib.bk_ending_audio_present(audio[mixers.index(mixer)], 0, C.byref(present))
                                if step % 4 == 0 and present.value:
                                    assert lib.bk_audio_pause(mixer, 0, error), error.value
                            f = fixture(random.Random(profile*100+step), 1)
                            f.frame.group, f.event.value = group, config.event
                            f.auxiliary.variant, f.auxiliary.selection = variant, selection
                            f.state = State.from_buffer_copy(effect_state)
                            f.voice_volume.value, f.effect_volume.value = -700, -1000
                            f.seconds = F([0, 1/60, .1, 6.1][step % 4]).value
                            state = ClipState()
                            assert lib.bk_actor_pose_state(pose, C.byref(state))
                            f.active.value = state.slot
                            for n in range(32):
                                assert lib.bk_actor_pose_timing(pose, n, C.byref(f.timings[n]))
                            for n in range(6):
                                present, playing = I(), I()
                                assert lib.bk_ending_audio_present(audio[0], n, C.byref(present))
                                assert lib.bk_audio_playing(mixers[0], n, C.byref(playing))
                                f.present[n], f.playing[n], f.status_error[n] = present.value, playing.value, 0
                            states, seed = [shared_rng.value], shared_rng.value
                            for n in range(16):
                                seed = (seed*214013+2531011) & 0xffffffff
                                states.append(seed)
                                f.randoms[n] = (seed >> 16) & 0x7fff
                            f.random_index.value = 0
                            want, trace, uninitialized = native.run(f)
                            bindings = Bindings(C.pointer(f.frame), C.pointer(f.auxiliary),
                                C.pointer(f.event), C.pointer(effect_state), C.pointer(f.voice_volume),
                                C.pointer(f.effect_volume))
                            frozen = actor_snapshot()
                            ok = lib.bk_ending_gallery_effect_apply(pose, audio[0], C.byref(shared_rng),
                                C.byref(bindings), f.seconds, error)
                            assert bool(ok) == (not uninitialized), (profile, reload, step, error.value)
                            if uninitialized:
                                assert error.value == b'gallery effect: original cue is uninitialized for active clip'
                                undefined += 1
                            assert bytes(effect_state) == bytes(want.state), (profile, reload, step, 'state')
                            assert shared_rng.value == states[want.random_index.value]
                            assert actor_snapshot() == frozen, (profile, reload, step, 'actor changed')
                            for event, _ in trace:
                                if event[0] != 'audio': continue
                                call, playing = AudioCall(*event[1:]), I()
                                assert lib.bk_ending_audio_call(audio[1], group, variant, selection,
                                    C.byref(call), C.byref(playing), error), error.value
                                commands += 1
                                effects += call.operation == 2
                                voices += call.operation == 4
                            for n in range(48):
                                values = []
                                for mixer, bank in zip(mixers, audio):
                                    present, playing, volume, pan = I(), I(), I(), I()
                                    assert lib.bk_ending_audio_present(bank, n, C.byref(present))
                                    assert lib.bk_audio_playing(mixer, n, C.byref(playing))
                                    valid = lib.bk_audio_get_gain(mixer, n, C.byref(volume), C.byref(pan))
                                    values.append((present.value, playing.value, valid, volume.value, pan.value))
                                assert values[0] == values[1], (profile,reload,step,n,values)
                            for mixer in mixers: assert lib.bk_audio_fill(mixer, error), error.value
                            assert outputs[0].hash.digest() == outputs[1].hash.digest(), (profile,reload,step,'PCM')
                            state_digest.update(bytes(effect_state)+bytes(shared_rng)+b''.join(frozen))
                            frames += 1
                        for primary, bank, rng_pointer in [(None,audio[0],C.byref(shared_rng)),
                                (pose,None,C.byref(shared_rng)), (pose,audio[0],None)]:
                            before = (bytes(effect_state), shared_rng.value, actor_snapshot())
                            assert not lib.bk_ending_gallery_effect_apply(primary, bank, rng_pointer,
                                C.byref(bindings), 1/60, error)
                            assert before == (bytes(effect_state), shared_rng.value, actor_snapshot())
                            rejected += 1
                    finally:
                        if owner: lib.bk_ending_selected_assets_destroy(owner)
                assert outputs[0].nonzero > 0 and outputs[0].hash.digest() == outputs[1].hash.digest()
                records.append(dict(group=group, variant=variant, selection=selection,
                    pcm_frames=outputs[0].submitted, nonzero_blocks=outputs[0].nonzero,
                    pcm_sha256=outputs[0].hash.hexdigest()))
                print('Gallery effect assets', profile+1, '/', args.profiles, flush=True)
            finally:
                for bank in audio: lib.bk_ending_audio_destroy(bank)
                for mixer in mixers: lib.bk_audio_destroy(mixer)
    finally:
        lib.bk_resources_destroy(store)
    assert effects > 0 and voices > 0
    assert all(hashlib.sha256((args.data/(pack+'.pp')).read_bytes()).hexdigest() == digest
               for pack,digest in inputs.items())
    result = dict(passed=True, scope=__doc__, exe_sha256=hashlib.sha256(exe).hexdigest(),
        archive_sha256=inputs, profiles=args.profiles, entries=args.profiles*2, frames=frames,
        audio_commands=commands, effects=effects, voices=voices, retained_states=retained,
        undefined_cue_rejections=undefined, missing_resource_rejections=rejected,
        state_sha256=state_digest.hexdigest(), pcm=records,
        natural_gallery=False, gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('PASS gallery effect scene:', frames, 'frames', commands, 'audio commands',
          state_digest.hexdigest(), flush=True)


if __name__ == '__main__':
    main()
