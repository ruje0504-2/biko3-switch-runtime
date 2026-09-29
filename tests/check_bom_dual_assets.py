"""Build and pair original BOM append/three-actor binding checks.

Real models and VIX are used;4d2320's enclosing loader, GPU and actual Switch
play are not covered. ASan/UBSan are enabled, with leak detection disabled.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
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


def manifest():
    paths = [ROOT / 'CMakeLists.txt', ROOT / 'config/dependencies.lock.json']
    paths += [path for path in (ROOT / 'runtime').rglob('*') if path.is_file()]
    paths += list((ROOT / 'tests').glob('*.py'))
    return {str(path.relative_to(ROOT)): digest(path) for path in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--cases', type=int, default=2)
    parser.add_argument('--steps', type=int, default=60)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--with-ending-assets', action='store_true',
                        help='also compare all five4d2320 CPU asset profiles and retained backgrounds')
    parser.add_argument('--suites',
                        help='comma-separated targeted suites; defaults to the normal selection')
    parser.add_argument('--effects-steps', type=int, default=48,
                        help='mixed ordinary/controlled/fixed calls per actual actor (at least24)')
    args = parser.parse_args()
    available = ['append', 'dual_assets', 'configured_request', 'tertiary_assets',
                 'tertiary_material', 'tertiary_mixed_effects']
    suites = available if args.with_ending_assets else available[:2]
    if args.suites:
        suites = args.suites.split(',')
        if len(set(suites)) != len(suites) or any(s not in available for s in suites):
            parser.error('suites must be distinct names from: ' + ', '.join(available))
    host_python = ROOT / 'local/venv/bin/python'
    asan_python = ROOT / 'build/asan/ending-oracle-python'
    if args.cases < 1 or args.steps < 1 or args.jobs < 1:
        parser.error('cases, steps and jobs must be positive')
    if args.effects_steps < 24:
        parser.error('effects-steps must be at least24')
    for path in [args.exe, args.data / 'bk3_11.pp', args.data / 'fambom.pp', host_python, asan_python]:
        if not path.is_file():
            parser.error('required input/launcher missing: ' + str(path))
    parent = ROOT / 'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='bom-dual-assets-', dir=parent))
    sources = manifest()
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
                  exe_sha256=digest(args.exe), source_sha256=sources, commands=[], checks=[],
                  leak_detection=False, full_ending_loader=False, switch_validation=False,
                  scope=__doc__)
    lock = threading.Lock()
    print('BOM dual-assets validation:', output, flush=True)

    def execute(name, command, env=None):
        log_path = output / (name + '.log')
        entry = dict(name=name, argv=[str(value) for value in command],
                     log=str(log_path.relative_to(ROOT)))
        with lock:
            report['commands'].append(entry)
            print('Running', name, flush=True)
        with log_path.open('w') as log:
            process = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        entry['exit_code'] = process.returncode
        text = log_path.read_text(errors='replace')
        if process.returncode or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            with lock:
                print('\n'.join(text.splitlines()[-18:]), flush=True)
            raise RuntimeError(f'{name} failed with exit {process.returncode}')
        return text

    try:
        for label, folder, sanitized, vulkan, kind in [
            ('host', 'build', 'OFF', 'ON', 'RelWithDebInfo'),
            ('asan', 'build/asan', 'ON', 'OFF', 'Debug')]:
            execute('configure-' + label, ['cmake', '-S', '.', '-B', folder,
                '-DBK_BUILD_TESTS=ON', '-DBK_SANITIZE=' + sanitized,
                '-DBK_WITH_VULKAN=' + vulkan, '-DCMAKE_BUILD_TYPE=' + kind])
            execute('build-' + label, ['cmake', '--build', folder, '--target', 'model-test',
                                      '--parallel', str(args.jobs)])
        site = execute('python-site', [str(host_python), '-c',
            'import sysconfig; print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir():
            raise RuntimeError('host Python returned no package directory')

        def run(suite, mode):
            folder, python = ('build', host_python) if mode == 'host' else ('build/asan', asan_python)
            env = dict(os.environ)
            env.update(PYTHONPATH=os.pathsep.join([str(ROOT / 'tests'), str(ROOT / 'tools'), site]),
                       BK3_BUILD_DIR=str(ROOT / folder), ASAN_OPTIONS='detect_leaks=0',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            name = suite + '-' + mode
            result_path = output / (name + '.json')
            script = ('original_ending_tertiary_assets_oracle.py' if suite in ['tertiary_assets', 'tertiary_mixed_effects']
                      else 'original_ending_tertiary_material_oracle.py' if suite == 'tertiary_material'
                      else 'original_configured_request_oracle.py' if suite == 'configured_request'
                      else 'original_bom_' + suite + '_oracle.py')
            command = [str(python), str(ROOT / 'tests' / script),
                       str(args.exe.resolve()), str(args.data.resolve()), '--output', str(result_path)]
            if suite == 'dual_assets':
                command += ['--cases', str(args.cases), '--steps', str(args.steps)]
            elif suite == 'tertiary_mixed_effects':
                command += ['--effects-steps', str(args.effects_steps), '--reloads', '0']
            text = execute(name, command, env)
            data = json.loads(result_path.read_text())
            if not data.get('passed') or data.get('exe_sha256') != report['exe_sha256']:
                raise RuntimeError(name + ' omitted a pass for the requested EXE')
            if suite == 'tertiary_mixed_effects':
                records = data.get('records', [])
                if len(records) != 10 or {(r['group'], r['variant']) for r in records} != {
                        (g, v) for g in range(5) for v in range(2)}:
                    raise RuntimeError(name + ' omitted an actual group/variant')
                for record in records:
                    effects = record.get('mixed_effects', {})
                    modes = effects.get('by_mode', {})
                    expected_actors = 3 if record['group'] == 2 else 1
                    if (effects.get('frames') != args.effects_steps * expected_actors
                            or not effects.get('native_sampling')
                            or not effects.get('held_plain_after_blend')
                            or any(not modes.get(str(mode)) for mode in [-1, 0, 1, 2])
                            or (record['group'] == 2 and
                                (not modes.get('-2') or not effects.get('fixed_to_seconds')
                                 or effects.get('terminal_owner_failures') != 1))):
                        raise RuntimeError(name + ' omitted required shared-cache/failure coverage')
            summaries = [line for line in text.splitlines() if line.startswith(('PASS BOM append ', 'PASS dual BOM ', 'PASS tertiary assets TOTAL ', 'PASS tertiary materials ', 'PASS configured request '))
                         and not line.startswith('PASS dual BOM case ')]
            if len(summaries) != 1:
                raise RuntimeError(name + ' omitted its final coverage summary')
            with lock:
                print(name + ': ' + summaries[0], flush=True)
            return dict(suite=suite, mode=mode, result=data, summary=summaries[0],
                        report=str(result_path.relative_to(ROOT)), python_sha256=digest(python.resolve()))

        rows, errors = [], []
        with ThreadPoolExecutor(max_workers=4) as pool:
            futures = [pool.submit(run, suite, mode) for suite in suites for mode in ['host', 'asan']]
            for future in futures:
                try:
                    rows.append(future.result())
                except Exception as exc:
                    errors.append(str(exc))
        for suite in suites:
            pair = [row for row in rows if row['suite'] == suite]
            if len(pair) != 2 or pair[0]['result'] != pair[1]['result'] or pair[0]['summary'] != pair[1]['summary']:
                errors.append(suite + ' ordinary/sanitized results did not match')
            else:
                report['checks'].append(dict(name=suite, passed=True, runs=pair))
        if errors:
            raise RuntimeError('; '.join(errors))
        if manifest() != sources:
            raise RuntimeError('source changed during validation')
        report['library_sha256'] = {}
        for folder in ['build', 'build/asan']:
            candidates = list((ROOT / folder).glob('*model-test.dylib')) + list((ROOT / folder).glob('*model-test.so'))
            if len(candidates) != 1:
                raise RuntimeError('cannot identify tested library in ' + folder)
            report['library_sha256'][str(candidates[0].relative_to(ROOT))] = digest(candidates[0])
        report['passed'] = True
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        report['error'] = str(exc)
        print(str(exc), flush=True)
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        path = output / 'result.json'
        path.write_text(json.dumps(report, indent=2) + '\n')
        print('BOM dual-assets result:', path, flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
