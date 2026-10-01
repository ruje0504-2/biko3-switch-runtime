"""Group0 item1 pickup -> area2/3/4 save/load -> special story, host/ASan.

The incoming area1 loader and initial random seed are fixtures. Later
transitions use production movement, NPC visibility, collision, menus and disk
saves. This is not a full story walkthrough, a visual comparison or a Switch
hardware test.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    parent = ROOT / 'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='natural-item-', dir=parent))
    report = dict(passed=False, scope=__doc__, commands=[], runs=[])
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}

    def run(name, argv):
        log = out / (name + '.log')
        with log.open('w') as stream:
            result = subprocess.run(list(map(str, argv)), cwd=ROOT, env=env,
                                    stdout=stream, stderr=subprocess.STDOUT)
        report['commands'].append(dict(name=name, argv=list(map(str, argv)),
                                       exit_code=result.returncode, log=log.name))
        text = log.read_text(errors='replace')
        if result.returncode or any(s in text for s in
                ['ERROR: AddressSanitizer', 'runtime error:']):
            raise RuntimeError(name + '\n' + '\n'.join(text.splitlines()[-15:]))
        return text

    print('Natural item validation:', out, flush=True)
    try:
        for mode, folder, sanitize, build_type in [
                ('host', 'build', 'OFF', 'RelWithDebInfo'),
                ('asan', 'build/asan-static', 'ON', 'Debug')]:
            run('configure-' + mode, ['cmake', '-S', '.', '-B', folder,
                '-DBK_BUILD_TESTS=ON', '-DBK_WITH_VULKAN=ON',
                '-DBK_SANITIZE=' + sanitize, '-DCMAKE_BUILD_TYPE=' + build_type])
            run('build-' + mode, ['cmake', '--build', folder, '--target',
                'natural-item-save-probe', '--parallel', args.jobs])
            binary = ROOT / folder / 'natural-item-save-probe'
            binary_sha256 = digest(binary)
            files = out / (mode + '-files')
            checkpoint = files / 'save/checkpoint-0.bks'
            for operation, area in [('produce', 2), ('reload', 2),
                                    ('continue', 3), ('reload', 3),
                                    ('continue', 4), ('reload', 4), ('story', 4)]:
                before = digest(checkpoint) if checkpoint.exists() else None
                text = run(f'{mode}-{operation}-{area}',
                           [binary, args.data.resolve(), files, operation])
                expected = f'PASS natural-item {operation} group0 area{area} inventory01000'
                match = re.search('^' + re.escape(expected) + r' frames(\d+)$',
                                  text, re.MULTILINE)
                if not match or 'PASS GPU allocations returned to renderer baseline' not in text:
                    raise RuntimeError('missing route/teardown assertion: ' + expected)
                after = digest(checkpoint)
                if operation in ('reload', 'story') and before != after:
                    raise RuntimeError('read-only reload changed the saved file')
                report['runs'].append(dict(mode=mode, operation=operation,
                    area=area, inventory=[0, 1, 0, 0, 0], frames=int(match[1]),
                    binary_sha256=binary_sha256, checkpoint_sha256=after,
                    summary=match[0]))
                print(match[0] + ' [' + mode + ']', flush=True)
        report['passed'] = True
    finally:
        (out / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS natural item route:', out / 'verification.json', flush=True)


if __name__ == '__main__':
    main()
