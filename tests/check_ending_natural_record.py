"""Ordinary ending input -> save -> fresh-process gallery replay, host/ASan.

The incoming story handoff is explicit. Ordinary phases1/2/7/5 use real input,
not a forced selected-stage action. Variant1 keeps its isolated stage fixture
as one regression case; its natural phases3/4 are NOT covered by this suite.
Both simulated clocks advance1/60 second; this is not hardware/performance or
a complete natural story walkthrough. Existing codec/oracle evidence is not
rerun or relabeled as new coverage here.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('jobs must be positive')
    data = args.data.resolve()
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='record-natural-', dir=parent))
    paths = [ROOT/'CMakeLists.txt', ROOT/'test-host.sh', ROOT/'config/dependencies.lock.json']
    paths += [p for folder in ['runtime', 'tests'] for p in (ROOT/folder).rglob('*')
              if p.suffix in ['.c', '.h', '.inc', '.py']]
    paths += [p for p in (ROOT/'tools').glob('ending_*') if p.is_file()]
    sources = {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}
    packs = ['bk3_00', 'bk3_01', 'bk3_02', 'bk3_03', 'bk3_04', 'bk3_05',
             'bk3_06', 'bk3_08', 'bk3_09', 'bk3_10', 'bk3_11', 'bk3_12',
             'bk3_13', 'bk3_16', 'bk3_18', 'fambom']
    assets = {name+'.pp': digest(data/(name+'.pp')) for name in packs}
    assets.update({p.name: digest(p) for p in data.iterdir()
                   if p.suffix.lower() in ['.fam', '.vix', '.ftt'] or p.name == 'bk3_Gray.b3f'})
    report = dict(passed=False, scope=__doc__, source_sha256=sources,
                  data_sha256=assets, started_at=datetime.now(timezone.utc).isoformat(),
                  commands=[], applications=[], logic_step_seconds=1/60,
                  wall_step_seconds=1/60, hardware_validated=False,
                  natural_story_entry_validated=False, natural_variant1_validated=False)
    lock = threading.Lock()

    def checkpoint():
        with lock:
            pending = out/'verification.json.part'
            pending.write_text(json.dumps(report, indent=2)+'\n')
            pending.replace(out/'verification.json')

    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}

    def run(name, cmd):
        entry = dict(name=name, argv=list(map(str, cmd)), log=name+'.log')
        report['commands'].append(entry)
        checkpoint()
        with (out/entry['log']).open('w') as log:
            entry['exit_code'] = subprocess.run(cmd, cwd=ROOT, env=env, stdout=log,
                                               stderr=subprocess.STDOUT).returncode
        checkpoint()
        text = (out/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or any(s in text for s in
                ['runtime error:', 'ERROR: AddressSanitizer', 'Exception ignored on calling ctypes callback']):
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-15:]))
        return text

    print('Natural record validation:', out, flush=True)
    checkpoint()
    try:
        for mode, folder, sanitize, kind in [('host', 'build', 'OFF', 'RelWithDebInfo'),
                                             ('asan', 'build/asan-static', 'ON', 'Debug')]:
            run('configure-'+mode, ['cmake', '-S', '.', '-B', folder,
                '-DBK_BUILD_TESTS=ON', '-DBK_WITH_VULKAN=ON', '-DBK_SANITIZE='+sanitize,
                '-DCMAKE_BUILD_TYPE='+kind])
            run('build-'+mode, ['cmake', '--build', folder, '--target',
                'ending-record-app-probe', '--parallel', str(args.jobs)])

        def profile(mode, folder, group, variant, natural):
            prefix = ('natural' if natural else 'isolated')+f'-{mode}-{group}-{variant}'
            files = out/(prefix+'-files')
            binary = ROOT/folder/'ending-record-app-probe'
            summaries, trace, replay_trace = [], [], []
            binary_hash = digest(binary)
            for phase in ['produce', 'replay']:
                operation = ('natural-' if natural else '')+phase
                output = run(prefix+'-'+phase, [str(binary), str(data), str(files),
                                               operation, str(group), str(variant)])
                passed = [line for line in output.splitlines() if line.startswith('PASS record-app ')]
                assert len(passed) == 1, prefix
                summaries.append(passed[0])
                if phase == 'produce':
                    trace = [line for line in output.splitlines() if line.startswith(
                        ('record-early ', 'record-input ', 'PASS record-early ', 'PASS record-input '))]
                    assert sum(line.startswith('PASS record-early ') for line in trace) == int(natural)
                    assert sum(line.startswith('PASS record-input ') for line in trace) == 1
                else:
                    replay_trace = [line for line in output.splitlines()
                                    if line.startswith(('record-replay ', 'PASS record-replay '))]
                    assert sum(line.startswith('PASS record-replay ') for line in replay_trace) == 1
                print(passed[0]+' ['+mode+']', flush=True)
            assert digest(binary) == binary_hash, prefix+' binary changed'
            return dict(mode=mode, group=group, variant=variant, natural=natural,
                        summaries=summaries, input_trace=trace, replay_trace=replay_trace,
                        binary_sha256=binary_hash,
                        files={name: digest(files/name) for name in
                               ['expected.bkr', 'save/records.bkr', 'save/unlocks.bku']})

        cases = [(g, 0, True) for g in range(5)] + [(0, 1, False)]
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(profile, mode, folder, *case) for case in cases
                       for mode, folder in [('host', 'build'), ('asan', 'build/asan-static')]]
            errors = []
            for future in as_completed(futures):
                try:
                    report['applications'].append(future.result())
                    checkpoint()
                except Exception as exc:
                    errors.append(str(exc))
            if errors:
                raise RuntimeError('\n'.join(errors))
        for group, variant, natural in cases:
            pair = [p for p in report['applications'] if p['group'] == group and p['variant'] == variant]
            assert len(pair) == 2 and {p['mode'] for p in pair} == {'host', 'asan'}
            for key in ['summaries', 'input_trace', 'replay_trace', 'files']:
                assert pair[0][key] == pair[1][key], (group, variant, key)
        assert all(digest(ROOT/path) == sha for path, sha in sources.items()), 'source changed'
        assert all(digest(data/path) == sha for path, sha in assets.items()), 'data changed'
        report.update(passed=True, natural_pairs=5, isolated_regression_pairs=1,
                      new_process_replays=12)
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        report['applications'].sort(key=lambda p: (p['group'], p['variant'], p['mode']))
        checkpoint()
    print('PASS natural records:', out/'verification.json', flush=True)


if __name__ == '__main__':
    main()
