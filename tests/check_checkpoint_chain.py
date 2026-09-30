"""Five characters, eight consecutive checkpoint saves and next openings.

Area completion is an explicit response fixture, as is each initial character
checkpoint. Actual menus, files, area handover, full next opening and30 tracking
frames run in the application. Also rechecks repeated saves and five loads
under the Switch no-replace-rename contract. Host/ASan pairs are not hardware
or a natural full-story walkthrough.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
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
    if args.jobs < 1:
        parser.error('jobs must be positive')
    data = args.data.resolve()
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='checkpoint-chain-', dir=parent))
    sources = [p for p in (ROOT/'runtime').rglob('*') if p.suffix in ['.c', '.h', '.inc']]
    sources += [ROOT/p for p in ['CMakeLists.txt', 'config/dependencies.lock.json',
                'tools/save_flow_probe.c', 'tools/no_replace_rename.c',
                'tests/check_checkpoint_chain.py']]
    packs = ['bk3_00', 'bk3_01', 'bk3_02', 'bk3_03', 'bk3_04', 'bk3_05',
             'bk3_06', 'bk3_07', 'bk3_15', 'bk3_16', 'bk3_20']
    assets = [data/(p+'.pp') for p in packs]
    assets += [p for p in data.iterdir() if p.is_file() and p.suffix.lower() != '.pp']
    report = dict(passed=False, scope=__doc__, hardware_validated=False,
                  natural_story_validated=False, started_at=datetime.now(timezone.utc).isoformat(),
                  source_sha256={str(p.relative_to(ROOT)): digest(p) for p in sorted(sources)},
                  data_sha256={p.name: digest(p) for p in sorted(assets)}, commands=[], runs=[])
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1', 'BK_NATIVE_GAME_CLOCK': '1'}
    for key in ['BK_SAVE_CHAIN_GROUP', 'BK_REPEAT_SAVE']:
        env.pop(key, None)

    def checkpoint():
        pending = out/'result.json.part'
        pending.write_text(json.dumps(report, indent=2)+'\n')
        pending.replace(out/'result.json')

    print('Checkpoint chain validation:', out, flush=True)
    checkpoint()
    try:
        for mode, folder, sanitize, kind in [('host', 'build', 'OFF', 'RelWithDebInfo'),
                                             ('asan', 'build/asan-static', 'ON', 'Debug')]:
            config_log = out/('configure-'+mode+'.log')
            config = ['cmake', '-S', '.', '-B', folder, '-DBK_BUILD_TESTS=ON',
                      '-DBK_WITH_VULKAN=ON', '-DBK_SANITIZE='+sanitize,
                      '-DCMAKE_BUILD_TYPE='+kind]
            with config_log.open('w') as stream:
                code = subprocess.run(config, cwd=ROOT, env=env, stdout=stream,
                                      stderr=subprocess.STDOUT).returncode
            report['commands'].append(dict(argv=config, exit_code=code, log=config_log.name))
            checkpoint()
            assert code == 0, config_log
            log = out/('build-'+mode+'.log')
            cmd = ['cmake', '--build', folder, '--target', 'save-flow-probe',
                   'save-flow-switch-fs-probe', '--parallel', str(args.jobs)]
            with log.open('w') as stream:
                code = subprocess.run(cmd, cwd=ROOT, env=env, stdout=stream,
                                      stderr=subprocess.STDOUT).returncode
            report['commands'].append(dict(argv=cmd, exit_code=code, log=log.name))
            checkpoint()
            assert code == 0, log

        def run(mode, folder, group):
            kind = 'chain' if group is not None else 'rename'
            name = f'{kind}-{mode}' + (f'-{group}' if group is not None else '')
            binary = ROOT/folder/('save-flow-probe' if group is not None else 'save-flow-switch-fs-probe')
            binary_hash = digest(binary)
            cmd = [str(binary), str(data), str(out/(name+'-files')), str(out/(name+'.rgba'))]
            extra = {'BK_SAVE_CHAIN_GROUP': str(group)} if group is not None else {'BK_REPEAT_SAVE': '1'}
            with (out/(name+'.log')).open('w') as stream:
                code = subprocess.run(cmd, cwd=ROOT, env={**env, **extra}, stdout=stream,
                                      stderr=subprocess.STDOUT).returncode
            text = (out/(name+'.log')).read_text(errors='replace')
            summaries = [line for line in text.splitlines() if line.startswith('PASS ')]
            traces = [line for line in text.splitlines() if line.startswith(('CHECKPOINT_', 'checkpoint entry'))]
            assert code == 0 and not any(s in text for s in ['ERROR: AddressSanitizer', 'runtime error:']), name+'\n'+text[-2000:]
            assert summaries.count('PASS GPU allocations returned to renderer baseline') == 1, name
            if group is not None:
                assert len(traces) == 16, (name, traces)
                for area in range(1, 9):
                    assert re.fullmatch(f'CHECKPOINT_STORED group{group} area{area} last_crossed[0-9]+', traces[2*(area-1)])
                    assert re.fullmatch(f'CHECKPOINT_CHAIN group{group} area{area} saved opening\\+tracking complete frames[0-9]+', traces[2*(area-1)+1])
                assert sum(bool(re.fullmatch(f'PASS checkpoint chain group{group} saves8 openings8 frames[0-9]+', s)) for s in summaries) == 1
            else:
                assert len(traces) == 5 and 'PASS three repeated menu saves to the same checkpoint' in summaries
                assert sum(s.startswith('PASS save-flow ') for s in summaries) == 1
            assert digest(binary) == binary_hash, name+' binary changed'
            print(name, summaries[-1], flush=True)
            return dict(mode=mode, group=group, kind=kind, exit_code=code, log=name+'.log',
                        argv=cmd, env=extra, summaries=summaries, trace=traces,
                        readback_sha256=digest(out/(name+'.rgba')) if group is None else None,
                        binary_sha256=binary_hash)

        errors = []
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(run, mode, folder, group) for group in [0, 1, 2, 3, 4, None]
                       for mode, folder in [('host', 'build'), ('asan', 'build/asan-static')]]
            for future in as_completed(futures):
                try:
                    report['runs'].append(future.result())
                except Exception as exc:
                    errors.append(str(exc))
                checkpoint()
        assert not errors, '\n'.join(errors)
        for group in [0, 1, 2, 3, 4, None]:
            pair = [r for r in report['runs'] if r['group'] == group]
            assert len(pair) == 2 and {r['mode'] for r in pair} == {'host', 'asan'}
            for key in ['summaries', 'trace', 'readback_sha256']:
                assert pair[0][key] == pair[1][key], (group, key)
        assert all(digest(ROOT/p) == sha for p, sha in report['source_sha256'].items()), 'source changed'
        assert all(digest(data/p) == sha for p, sha in report['data_sha256'].items()), 'data changed'
        report['passed'] = True
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        checkpoint()
    print('PASS checkpoint chain six host/ASan pairs:', out, flush=True)


if __name__ == '__main__':
    main()
