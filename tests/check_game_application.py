"""Real app replay integration; requires the user's extracted game assets."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('game', type=Path)
    parser.add_argument('--exit', action='store_true', dest='exit_game')
    parser.add_argument('--replay-hz', type=int, default=60, choices=[15,20,30,60])
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='biko3-game-session-') as work:
        work = Path(work)
        output, captures = work/'frame.rgba', work/'captures'
        replay = ROOT/'tests/game_session.replay'
        if args.exit_game:
            replay_text = replay.read_text().replace('1550 0 10\n', '1550 0 10\n1551 0 10\n')
            replay = work/'exit.replay'
            replay.write_text(replay_text)
        if args.replay_hz != 60:
            rows=[]; previous=-1
            for row in replay.read_text().splitlines():
                if not row or row.startswith('#'): continue
                frame,held,pressed=row.split()
                frame=max(previous+1,round(int(frame)*args.replay_hz/60))
                rows.append(f'{frame} {held} {pressed}')
                previous=frame
            replay=work/'paced.replay'
            replay.write_text('\n'.join(rows)+'\n')
        command = [str(args.binary.resolve()), str(args.game.resolve()), str(output),
                   '--frames', str(round(1700*args.replay_hz/60)), '--scene', 'title', '--pause-root', str(captures),
                   '--replay', str(replay)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=180,
                             env={**os.environ,'BK_REPLAY_HZ':str(args.replay_hz), 'BK_DEVELOPMENT_ENTRY':'1'})
        print(run.stdout + run.stderr, end='')
        assert run.returncode == 0, run.returncode
        log = run.stdout + run.stderr
        required = ['Scene switched: game', 'Game paused after HUD capture',
                    'Game resumed with retained session',
                    'Scene switched: title (retained process state)',
                    'Game state: group0 area0 phase1 photos2 menu0',
                    'Active flow: 0x01']
        if args.exit_game:
            required.remove('Scene switched: title (retained process state)')
            required[-1] = 'Active flow: 0x58'
        cursor = 0
        for line in required:
            position = log.find(line, cursor)
            assert position >= cursor, line
            cursor = position + len(line)
        assert output.stat().st_size == 1280*720*4
        assert not (captures/'sy_99.bmp').exists()
        photos = sorted((captures/'album').glob('ri_*.bmp'))
        assert len(photos) == 2, photos
        for photo in photos:
            raw = photo.read_bytes()
            assert raw[:2] == b'BM'
            assert struct.unpack_from('<I', raw, 2)[0] == len(raw)
            assert struct.unpack_from('<ii', raw, 18) == (960, 720)
            assert struct.unpack_from('<HH', raw, 26) == (1, 24)
            assert len(set(raw[54:])) > 100, 'photo has no rendered image range'
        assert not list(captures.rglob('*.part'))
        print('PASS application title/opening/game/photo/pause/resume/pause/' +
              ('exit' if args.exit_game else 'title') +
              f'; 2 BMPs, pause removed; {args.replay_hz}Hz presentation clock')

if __name__ == '__main__':
    main()
