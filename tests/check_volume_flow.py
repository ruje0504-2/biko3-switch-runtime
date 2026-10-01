"""Pair real title/pause volume flows, fresh-process reload and failed writes.
Also checks MAX defaults, reset, previews and saved MAX reload.
Uses an initial RNG seed fixture, then real application input. Host Vulkan
and ASan checks do not establish Switch hardware or complete story acceptance.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
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
    data = args.data.resolve()
    packs = ['bk3_00', 'bk3_01', 'bk3_02', 'bk3_03', 'bk3_04', 'bk3_05',
             'bk3_06', 'bk3_07', 'bk3_15', 'bk3_16', 'bk3_20', 'bk3_18']
    if args.jobs < 1 or any(not (data/(p+'.pp')).is_file() for p in packs):
        parser.error('positive jobs and all twelve production archives required')
    parent = ROOT/'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='volume-flow-', dir=parent))
    sources = [p for p in (ROOT/'runtime').rglob('*') if p.suffix in ('.c', '.h')]
    sources += [ROOT/p for p in ['CMakeLists.txt', 'config/dependencies.lock.json',
                'tests/test_volume_file.c', 'tools/no_replace_rename.c',
                'tools/volume_flow_probe.c', 'tests/check_volume_flow.py']]
    source_hash = {str(p.relative_to(ROOT)): digest(p) for p in sorted(sources)}
    assets = [data/(p+'.pp') for p in packs]
    if (data/'volsetting.cfg').exists(): assets.append(data/'volsetting.cfg')
    asset_hash = {p.name: digest(p) for p in assets}
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
        scope=__doc__, source_sha256=source_hash, asset_sha256=asset_hash,
        commands=[], checks=[], leak_detection=False, switch_validation=False)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name, command):
        entry = dict(name=name, argv=[str(a) for a in command], log=name+'.log')
        report['commands'].append(entry)
        with (out/entry['log']).open('w') as log:
            entry['exit_code'] = subprocess.run(command, cwd=ROOT, env=env,
                stdout=log, stderr=subprocess.STDOUT).returncode
        text = (out/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-20:]))
        return text
    print('Volume flow validation:', out, flush=True)
    try:
        for label, build, sanitize, kind in [
            ('host', ROOT/'build', 'OFF', 'RelWithDebInfo'),
            ('asan', ROOT/'build/asan-static', 'ON', 'Debug')]:
            run(label+'-configure', ['cmake', '-S', ROOT, '-B', build,
                '-DBK_BUILD_TESTS=ON', '-DBK_WITH_VULKAN=ON',
                '-DBK_SANITIZE='+sanitize, '-DCMAKE_BUILD_TYPE='+kind])
            run(label+'-build', ['cmake', '--build', build, '--target',
                'volume-flow-probe', 'test-volume-file', 'test-volume-switch-fs',
                '--parallel', str(args.jobs)])
            run(label+'-storage', ['ctest', '--test-dir', build,
                '-R', '^volume-(file|switch-fs)$', '--output-on-failure'])
            binary = build/'volume-flow-probe'
            saved = out/(label+'-saved'); saved.mkdir()
            rejected = out/(label+'-rejected'); rejected.mkdir()
            defaults = out/(label+'-defaults'); defaults.mkdir()
            saved_hash = {}
            for mode, port in [('produce', saved), ('reload', saved), ('reject', rejected),
                               ('defaults', defaults), ('reload-max', defaults)]:
                text = run(label+'-'+mode, [binary, data, port, mode])
                lines = [l for l in text.splitlines() if l.startswith('volume-flow PASS ')]
                if len(lines) != 1: raise RuntimeError('missing terminal pass: '+label+'/'+mode)
                record = dict(mode=label, kind=mode, result=lines[0], binary_sha256=digest(binary))
                settings = port/'save/volume.cfg'
                if mode != 'reject':
                    wanted = (0, 0, 0) if mode in ('defaults', 'reload-max') else (-1500, -6000, -3000)
                    if settings.read_bytes() != struct.pack('<3i', *wanted):
                        raise RuntimeError('unexpected persisted settings')
                    if mode in ('produce', 'defaults'): saved_hash[port] = digest(settings)
                    elif saved_hash[port] != digest(settings): raise RuntimeError('reload changed settings')
                    record['settings_sha256'] = digest(settings)
                elif settings.exists(): raise RuntimeError('failed store published a file')
                if list(port.rglob('*.part')): raise RuntimeError('temporary file remains')
                report['checks'].append(record)
                print(label+' '+lines[0], flush=True)
        for kind in ['produce', 'reload', 'reject', 'defaults', 'reload-max']:
            results = [c['result'] for c in report['checks'] if c['kind'] == kind]
            if len(results) != 2 or results[0] != results[1]:
                raise RuntimeError(kind+' ordinary/ASan output or PCM mismatch')
        if any(digest(ROOT/p) != h for p, h in source_hash.items()):
            raise RuntimeError('source changed during validation')
        if any(digest(data/p) != h for p, h in asset_hash.items()):
            raise RuntimeError('input assets changed during validation')
        report['passed'] = True
    except Exception as exc:
        report['error'] = str(exc)
        print(str(exc), flush=True)
    finally:
        report['finished_at'] = datetime.now(timezone.utc).isoformat()
        (out/'result.json').write_text(json.dumps(report, indent=2)+'\n')
    return 0 if report['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
