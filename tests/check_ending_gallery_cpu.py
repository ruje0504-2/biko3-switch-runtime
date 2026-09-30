"""Run the48BCBB original-instruction oracle on ordinary and ASan/UBSan builds.

Needs the fixed EXE and the existing sanitized Python launcher. Outputs go to
a fresh validation directory. Does not load game assets or run a Switch scene.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path)
    p.add_argument('--host-python', type=Path, default=ROOT/'local/venv/bin/python')
    p.add_argument('--asan-python', type=Path, default=ROOT/'build/asan/ending-oracle-python')
    p.add_argument('--jobs', type=int, default=8)
    args = p.parse_args()
    for path in [args.exe, args.host_python, args.asan_python]:
        if not path.is_file(): p.error(f'missing required input/launcher: {path}')
    if args.jobs < 1: p.error('--jobs must be positive')
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ending-gallery-cpu-', dir=parent))
    paths = [ROOT/'CMakeLists.txt', ROOT/'config/dependencies.lock.json', ROOT/'test-host.sh']
    paths += [path for path in (ROOT/'runtime').rglob('*') if path.is_file()]
    paths += list((ROOT/'tests').glob('*.py'))
    sources = {str(path.relative_to(ROOT)): digest(path) for path in sorted(paths)}
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
                  source_sha256=sources, exe_sha256=digest(args.exe), commands=[], checks=[],
                  leak_detection=False, real_asset_scene=False, switch_validation=False,
                  scope=__doc__)
    print('Gallery CPU validation:', output, flush=True)

    def run(name, command, env=None):
        entry = dict(name=name, argv=[str(arg) for arg in command], log=name+'.log')
        report['commands'].append(entry)
        with (output/entry['log']).open('w') as log:
            entry['exit_code'] = subprocess.run(command, cwd=ROOT, env=env,
                stdout=log, stderr=subprocess.STDOUT).returncode
        text = (output/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-16:]))
        return text

    try:
        site = run('python-site', [str(args.host_python.absolute()), '-c',
                   'import sysconfig; print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir(): raise RuntimeError('invalid Python package directory')
        for mode, folder, python, sanitize, vulkan, kind in [
                ('host', 'build', args.host_python, 'OFF', 'ON', 'RelWithDebInfo'),
                ('asan', 'build/asan', args.asan_python, 'ON', 'OFF', 'Debug')]:
            run('configure-'+mode, ['cmake', '-S', '.', '-B', folder,
                '-DBK_BUILD_TESTS=ON', '-DBK_SANITIZE='+sanitize,
                '-DBK_WITH_VULKAN='+vulkan, '-DCMAKE_BUILD_TYPE='+kind])
            run('build-'+mode, ['cmake', '--build', folder, '--target', 'model-test',
                               '--parallel', str(args.jobs)])
            env = dict(os.environ)
            env.update(PYTHONPATH=os.pathsep.join([str(ROOT/'tests'), str(ROOT/'tools'), site]),
                       BK3_BUILD_DIR=str(ROOT/folder), ASAN_OPTIONS='detect_leaks=0',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            result = output/(mode+'.json')
            text = run('oracle-'+mode, [str(python.absolute()),
                str(ROOT/'tests/original_ending_gallery_presentation_oracle.py'),
                str(args.exe.resolve()), '--output', str(result)], env)
            data = json.loads(result.read_text())
            summaries = [line for line in text.splitlines() if line.startswith('PASS gallery presentation:')]
            if len(summaries) != 1 or not data.get('passed') or data.get('exe_sha256') != report['exe_sha256']:
                raise RuntimeError(mode+' omitted a matching terminal pass')
            report['checks'].append(dict(mode=mode, result=data, summary=summaries[0],
                library_sha256=digest(ROOT/folder/'libmodel-test.dylib'),
                python_sha256=digest(python.resolve())))
            print(summaries[0], flush=True)
        if report['checks'][0]['result'] != report['checks'][1]['result']:
            raise RuntimeError('ordinary and sanitized outcomes differ')
        if any(not (ROOT/path).is_file() or digest(ROOT/path) != value for path, value in sources.items()):
            raise RuntimeError('source changed during validation')
        report['passed'] = True
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        (output/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS gallery CPU pair:', output/'verification.json', flush=True)


if __name__ == '__main__':
    main()
