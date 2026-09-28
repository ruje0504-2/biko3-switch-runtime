"""Check the real default entry and title/load/cancel with isolated saves."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import struct
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('game', type=Path)
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop('BK_DEVELOPMENT_ENTRY', None)
    env.pop('BK_REPLAY_HZ', None)
    with tempfile.TemporaryDirectory(prefix='biko3-original-front-') as work:
        work = Path(work)
        base = [str(args.binary.resolve()), str(args.game.resolve())]
        replay = work / 'load-cancel.replay'
        replay.write_text('140 0 10\n179 0 1\n500 0 8\n520 0 1\n')
        for replayed in (False, True):
            output = work / ('load.rgba' if replayed else 'startup.rgba')
            command = base + [str(output), '--frames', '800' if replayed else '160',
                              '--pause-root', str(work / 'saves')]
            if replayed:
                command += ['--replay', str(replay)]
            run = subprocess.run(command, capture_output=True, text=True,
                                 timeout=180, env=env)
            log = run.stdout + run.stderr
            print(log, end='')
            assert run.returncode == 0, run.returncode
            assert 'FAILED' not in log and 'Active flow: 0x01' in log
            assert output.stat().st_size == 1280 * 720 * 4
            assert 'Original menu scene loaded [flow01 prev00' in log
            if replayed:
                assert 'Checkpoint menu loaded with writable port storage' in log
                assert 'Original menu scene loaded [flow50 prev28' in log
        assert not list((work / 'saves').rglob('checkpoint-*.bks'))
        # Independent encoder: app startup reads its own progress envelope;
        # corruption must be reported without rewriting or erasing it.
        progress = work / 'saves/save/unlocks.bku'
        flags = bytes([1, 2, 128, 255, 3, 4, 5, 6]) * 5
        header = b'BK3UNLK\0' + struct.pack('<4I', 1, 5, 8, 40)
        encoded = header + struct.pack('<II', zlib.crc32(header + flags), 0) + flags
        progress.write_bytes(encoded)
        command = base + [str(work / 'progress.rgba'), '--frames', '16',
                          '--pause-root', str(work / 'saves')]
        run = subprocess.run(command, capture_output=True, text=True, timeout=180, env=env)
        print(run.stdout + run.stderr, end='')
        assert run.returncode == 0 and 'Active flow: 0x01' in run.stderr
        assert progress.read_bytes() == encoded
        corrupt = bytearray(encoded)
        corrupt[35] ^= 1
        progress.write_bytes(corrupt)
        run = subprocess.run(command, capture_output=True, text=True, timeout=180, env=env)
        assert run.returncode != 0 and 'unlock table: checksum mismatch' in run.stderr
        assert progress.read_bytes() == corrupt
    print('PASS default original title real clock160; title/load/cancel800; no checkpoint writes')
    print('PASS application gallery progress load; corrupt progress rejected and preserved')


if __name__ == '__main__':
    main()
