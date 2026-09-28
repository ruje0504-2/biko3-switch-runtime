"""Audit an explicitly OFFLINE native-runtime capture against Python WAV/math.

Consumes actual command timestamps, not the C mixer or its cursor helpers.
Checks every stereo output sample and every played-cursor/envelope observation.
"""
import argparse
import array
import bisect
import ctypes as C
import hashlib
import io
import json
import math
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from bk3_assets import Archive


def f32(value):
    return C.c_float(value).value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('trace', type=Path)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--report', type=Path, default=ROOT/'local/audio-queue-audit.json')
    args = parser.parse_args()
    archives, clips, voices, cursors = {}, {}, [[], []], []
    target = smoothed = 0.0
    pending_count = inactive_count = wrap_count = 0

    def load(pack, name):
        key = (pack, name)
        if key not in clips:
            if pack not in archives:
                archives[pack] = Archive(args.data / (pack + '.pp'))
            archive = archives[pack]
            entry = next(e for e in archive.entries if e.name.lower() == name.lower())
            with wave.open(io.BytesIO(archive.read(entry)), 'rb') as wav:
                assert wav.getsampwidth() == 2 and wav.getcomptype() == 'NONE'
                samples = array.array('h', wav.readframes(wav.getnframes()))
                if sys.byteorder != 'little':
                    samples.byteswap()
                clips[key] = (wav.getframerate(), wav.getnchannels(), wav.getnframes(), samples)
        return clips[key]

    def state_at(history, frame):
        at = bisect.bisect_right([e['at'] for e in history], frame) - 1
        return history[at] if at >= 0 else None

    for line in args.trace.read_text().splitlines():
        row = json.loads(line)
        if row['type'] in ('play', 'gain', 'clear'):
            history = voices[row['voice']]
            if row['type'] == 'play':
                row['clip'] = load(row['pack'], row['name'])
                row['origin'] = row['at']
            elif row['type'] == 'gain':
                previous = history[-1]
                row = {**previous, 'at': row['at'], 'volume': row['volume'], 'pan': row['pan']}
            else:
                row['clip'] = None
            history.append(row)
        elif row['type'] == 'cursor':
            history = voices[1]
            state = state_at(history, row['consumed'])
            playing = buffered = source = 0
            pending = any(e['at'] > row['consumed'] for e in history)
            level = 0.0
            if state and state['clip']:
                rate, channels, count, samples = state['clip']
                source = (row['consumed'] - state['origin']) * rate // 48000
                playing = state['loop'] or source < count
                if state['loop']:
                    wrap_count += source >= count
                    source %= count
                else:
                    source = min(source, count)
                buffered = row['consumed'] < row['submitted']
                if playing and buffered:
                    start = source * channels
                    magnitude = sum(abs(samples[(start + i) % len(samples)]) for i in range(220)) // 110
                    target = f32(magnitude / 512)
                    if target < smoothed:
                        smoothed = max(target, f32(smoothed - 10.0 * f32(.01)))
                    elif target > smoothed:
                        smoothed = min(target, f32(smoothed + 10.0 * f32(.01)))
                    smoothed = min(smoothed, 9.0)
                    level = smoothed
            assert (row['source_frame'], row['playing'], row['buffered'], row['pending']) == (
                source, int(playing), int(buffered), int(pending)), row
            assert tuple(f32(row[k]) for k in ('target', 'smoothed', 'level')) == (
                target, smoothed, level), (row, target, smoothed, level)
            pending_count += pending
            inactive_count += not (playing and buffered)
            cursors.append(row)
        else:
            assert row['type'] == 'end' and row['offline']
            end = row
    with wave.open(str(args.capture), 'rb') as wav:
        assert wav.getparams()[:4] == (2, 2, 48000, end['frames'])
        raw = wav.readframes(wav.getnframes())
        actual = array.array('h', raw)
        if sys.byteorder != 'little':
            actual.byteswap()
    indices = [-1, -1]
    maximum_error = 0
    for frame in range(end['frames']):
        mixed = [0.0, 0.0]
        for voice, history in enumerate(voices):
            index = indices[voice]
            while index + 1 < len(history) and history[index + 1]['at'] <= frame:
                index += 1
            indices[voice] = index
            if index < 0 or not history[index]['clip']:
                continue
            state = history[index]
            rate, channels, count, samples = state['clip']
            at, fraction = divmod((frame - state['origin']) * rate, 48000)
            if not state['loop'] and at >= count:
                continue
            at %= count
            next_ = (at + 1) % count if state['loop'] else min(at + 1, count - 1)
            def gain(value):
                return 0.0 if value <= -10000 else math.pow(10.0, value / 2000)
            for c in range(2):
                channel = c if channels == 2 else 0
                a, b = samples[at * channels + channel], samples[next_ * channels + channel]
                pan = -state['pan'] if c == 0 and state['pan'] > 0 else state['pan'] if c == 1 and state['pan'] < 0 else 0
                contribution = f32((a + (b - a) * (fraction / 48000)) * (gain(state['volume']) * gain(pan)))
                mixed[c] = f32(mixed[c] + contribution)
        for c, value in enumerate(mixed):
            quantized = math.floor(value + .5) if value >= 0 else math.ceil(value - .5)
            expected = min(32767, max(-32768, quantized))
            delta = abs(actual[frame * 2 + c] - expected)
            maximum_error = max(maximum_error, delta)
            assert not delta, (frame, c, actual[frame * 2 + c], expected)
    report = dict(passed=True, offline=True, device_verified=False,
                  output_frames=end['frames'], output_samples=len(actual),
                  clips=len(clips), commands=sum(map(len, voices)),
                  cursor_checks=len(cursors), pending_cursor_checks=pending_count,
                  inactive_cursor_checks=inactive_count, loop_cursor_checks=wrap_count,
                  queue_drains=end['queue_drains'], max_pcm_error=maximum_error,
                  wav_sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest(),
                  trace_sha256=hashlib.sha256(args.trace.read_bytes()).hexdigest())
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
