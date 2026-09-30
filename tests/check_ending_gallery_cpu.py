"""Pair48302B/483AF0/4843AA/4855C9/48758C/488674/48C8C2/48BCBB/48CC18 and optional effect/audio adapters.

Needs the fixed EXE and the existing sanitized Python launcher. Outputs go to
a fresh validation directory. --data adds Japanese actor/PCM checks. None of
these checks constitutes a complete phase8 scene or Switch validation.
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
    p.add_argument('--data', type=Path)
    p.add_argument('--suites', default='control,normal,secondary,selected,tertiary,auxiliary,presentation,effect')
    args = p.parse_args()
    suites = args.suites.split(',')
    if not suites or len(set(suites)) != len(suites) or any(s not in ['control', 'normal', 'secondary', 'selected', 'tertiary', 'auxiliary', 'presentation', 'effect', 'timing'] for s in suites):
        p.error('--suites must be distinct members of control,normal,secondary,selected,tertiary,auxiliary,presentation,effect,timing')
    if 'timing' in suites and not args.data:
        p.error('timing requires --data')
    if args.data:
        if not {'effect', 'timing'}.intersection(suites): p.error('--data requires effect or timing')
        if 'effect' in suites: suites.append('effect-scene')
    for path in [args.exe, args.host_python, args.asan_python]:
        if not path.is_file(): p.error(f'missing required input/launcher: {path}')
    if args.jobs < 1: p.error('--jobs must be positive')
    archives = {}
    if args.data:
        for pack in ['bk3_09', 'bk3_10', 'bk3_11', 'bk3_13', 'bk3_03', 'bk3_04', 'fambom', 'bk3_02', 'bk3_06']:
            path = args.data/(pack+'.pp')
            if not path.is_file(): p.error('missing archive: '+str(path))
            archives[pack] = digest(path)
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ending-gallery-cpu-', dir=parent))
    paths = [ROOT/'CMakeLists.txt', ROOT/'config/dependencies.lock.json', ROOT/'test-host.sh']
    paths += [path for path in (ROOT/'runtime').rglob('*') if path.is_file()]
    paths += list((ROOT/'tests').glob('*.py'))
    sources = {str(path.relative_to(ROOT)): digest(path) for path in sorted(paths)}
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
                  source_sha256=sources, exe_sha256=digest(args.exe), commands=[], checks=[],
                  leak_detection=False, real_asset_effect='effect-scene' in suites,
                  real_asset_timing='timing' in suites, switch_validation=False,
                  archive_sha256=archives, suites=suites,
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
            for suite in suites:
                result = output/(mode+'-'+suite+'.json')
                script = ('check_ending_gallery_effect_scene.py' if suite == 'effect-scene'
                          else 'original_ending_gallery_'+suite+'_oracle.py')
                command = [str(python.absolute()), str(ROOT/'tests'/script), str(args.exe.resolve())]
                if suite in ['effect-scene', 'timing']: command.append(str(args.data.resolve()))
                text = run('oracle-'+mode+'-'+suite, command+['--output', str(result)], env)
                data = json.loads(result.read_text())
                prefix = 'PASS gallery '+suite.replace('-', ' ')+':'
                summaries = [line for line in text.splitlines() if line.startswith(prefix)]
                if len(summaries) != 1 or not data.get('passed') or data.get('exe_sha256') != report['exe_sha256']:
                    raise RuntimeError(mode+' '+suite+' omitted a matching terminal pass')
                report['checks'].append(dict(mode=mode, suite=suite, result=data, summary=summaries[0],
                    library_sha256=digest(ROOT/folder/'libmodel-test.dylib'),
                    python_sha256=digest(python.resolve())))
                print(summaries[0], flush=True)
        for suite in suites:
            results = [check['result'] for check in report['checks'] if check['suite'] == suite]
            if len(results) != 2 or results[0] != results[1]:
                raise RuntimeError(suite+' ordinary and sanitized outcomes differ')
        if args.data and any(digest(args.data/(pack+'.pp')) != value for pack,value in archives.items()):
            raise RuntimeError('archive changed during validation')
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
