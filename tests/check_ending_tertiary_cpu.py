"""Build and compare original third-ending CPU oracles under ASan/UBSan.

Needs the fixed original EXE, the project Python environment, and the existing
prelinked sanitized Python launcher. Results/logs use a new validation folder.
This does not invoke the real third-ending loader, GPU scene or Switch.
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
    paths = [ROOT/'CMakeLists.txt', ROOT/'config/dependencies.lock.json']
    paths += [p for p in (ROOT/'runtime').rglob('*') if p.is_file()]
    paths += list((ROOT/'tests').glob('*.py'))
    paths += [ROOT/'test-host.sh']
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--host-python', type=Path, default=ROOT/'local/venv/bin/python')
    parser.add_argument('--asan-python', type=Path, default=ROOT/'build/asan/ending-oracle-python')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--data', type=Path,
                        help='Explicit Data directory enables the real-XAN playback oracle')
    parser.add_argument('--only', choices=['all', 'control', 'hover', 'inventory', 'presentation', 'action',
                                           'motion', 'playback', 'services'],
                        default='all', help='Run a focused changed component or all CPU callers')
    args = parser.parse_args()
    for path in [args.exe, args.host_python, args.asan_python]:
        if not path.is_file():
            parser.error(f'required input/launcher is missing: {path}')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.only in ['playback', 'services'] and args.data is None:
        parser.error('--data is required for playback/services')
    if args.data is not None and not args.data.is_dir():
        parser.error('--data must be an existing Data directory')
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ending-tertiary-cpu-', dir=parent))
    sources = manifest()
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
                  exe_sha256=digest(args.exe), source_sha256=sources, commands=[], checks=[],
                  leak_detection=False, real_asset_scene=False, switch_validation=False,
                  scope=__doc__)
    print('Third-ending CPU validation:', output, flush=True)
    lock = threading.Lock()

    def execute(name, command, env=None):
        entry = {'name': name, 'argv': [str(s) for s in command],
                 'log': str((output/(name+'.log')).relative_to(ROOT))}
        with lock:
            report['commands'].append(entry)
            print('Running', name, flush=True)
        with (output/(name+'.log')).open('w') as log:
            process = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        entry['exit_code'] = process.returncode
        text = (output/(name+'.log')).read_text(errors='replace')
        if process.returncode or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            with lock:
                print('\n'.join(text.splitlines()[-16:]), flush=True)
            raise RuntimeError(f'{name} failed with exit {process.returncode}')
        return text

    try:
        for label, folder, sanitized, vulkan, kind in [
            ('host', 'build', 'OFF', 'ON', 'RelWithDebInfo'),
            ('asan', 'build/asan', 'ON', 'OFF', 'Debug')]:
            execute('configure-'+label, ['cmake', '-S', '.', '-B', folder,
                    '-DBK_BUILD_TESTS=ON', '-DBK_SANITIZE='+sanitized,
                    '-DBK_WITH_VULKAN='+vulkan, '-DCMAKE_BUILD_TYPE='+kind])
            execute('build-'+label, ['cmake', '--build', folder, '--target', 'model-test',
                                     '--parallel', str(args.jobs)])
        # Preserve the venv executable path: resolving its symlink changes
        # Python's prefix and can silently select system packages instead.
        site = execute('python-site', [str(args.host_python.absolute()), '-c',
                'import sysconfig; print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir():
            raise RuntimeError('host Python returned no usable package directory')

        def check(suffix):
            rows = []
            for mode, folder, python in [('host', 'build', args.host_python),
                                          ('asan', 'build/asan', args.asan_python)]:
                env = dict(os.environ)
                env.update(PYTHONPATH=os.pathsep.join([str(ROOT/'tests'), str(ROOT/'tools'), site]),
                           BK3_BUILD_DIR=str(ROOT/folder), ASAN_OPTIONS='detect_leaks=0',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                name = suffix+'-'+mode
                result = output/(name+'.json')
                script = ({'hover': 'ending_tertiary_hover_compat_oracle.py',
                           'inventory': 'original_ending_inventory_oracle.py'}.get(suffix,
                          'original_ending_tertiary_'+suffix+'_oracle.py'))
                text = execute(name, [str(python.absolute()), str(ROOT/'tests'/script),
                    str(args.exe.resolve()),
                    *([str(args.data.resolve())] if suffix == 'playback' else []),
                    '--output', str(result)], env)
                prefix = 'PASS ending inventory:' if suffix == 'inventory' else 'PASS tertiary '
                summaries = [s for s in text.splitlines() if s.startswith(prefix)]
                if len(summaries) != 1 or not result.is_file():
                    raise RuntimeError(name+' omitted its final coverage report')
                data = json.loads(result.read_text())
                if not data.get('passed') or data.get('exe_sha256') != report['exe_sha256']:
                    raise RuntimeError(name+' did not prove a pass for the requested EXE')
                rows.append({'mode': mode, 'result': data, 'summary': summaries[0],
                             'report': str(result.relative_to(ROOT)),
                             'python_sha256': digest(python.resolve())})
                with lock:
                    print(summaries[0], flush=True)
            if rows[0]['result'] != rows[1]['result'] or rows[0]['summary'] != rows[1]['summary']:
                raise RuntimeError(suffix+' differs between ordinary and sanitized builds')
            return {'name': suffix, 'passed': True, 'runs': rows}

        errors = []
        if args.only == 'all':
            selected = ['control', 'hover', 'inventory', 'presentation', 'action', 'motion']
            if args.data is not None:
                selected.append('playback')
        elif args.only == 'services':
            selected = ['motion', 'playback']
        else:
            selected = [args.only]
        with ThreadPoolExecutor(max_workers=min(3, len(selected))) as pool:
            futures = [pool.submit(check, suffix) for suffix in selected]
            for future in futures:
                try:
                    report['checks'].append(future.result())
                except Exception as exc:
                    errors.append(str(exc))
        if errors:
            raise RuntimeError('; '.join(errors))
        if manifest() != sources:
            raise RuntimeError('source changed during validation')
        report['library_sha256'] = {}
        for folder in ['build', 'build/asan']:
            libraries = list((ROOT/folder).glob('*model-test.dylib'))+list((ROOT/folder).glob('*model-test.so'))
            if len(libraries) != 1:
                raise RuntimeError('cannot identify the tested library in '+folder)
            report['library_sha256'][str(libraries[0].relative_to(ROOT))] = digest(libraries[0])
        report['passed'] = True
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        report['error'] = str(exc)
        print(str(exc), flush=True)
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        path = output/'result.json'
        path.write_text(json.dumps(report, indent=2)+'\n')
        print('Third-ending CPU result:', path, flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
