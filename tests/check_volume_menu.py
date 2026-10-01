"""Pair the original volume UI oracle and Japanese Vulkan/PCM probe.
Writes a new validation directory, checks source/archive stability, and never
claims application flow30 integration, full Windows pixels or Switch results.
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
    p.add_argument('data', type=Path)
    p.add_argument('--host-python', type=Path, default=ROOT/'local/venv/bin/python')
    p.add_argument('--asan-python', type=Path, default=ROOT/'build/asan/ending-oracle-python')
    p.add_argument('--jobs', type=int, default=4)
    a = p.parse_args()
    for path in [a.exe, a.host_python, a.asan_python, a.data/'bk3_00.pp', a.data/'bk3_02.pp']:
        if not path.is_file(): p.error('missing input: '+str(path))
    if a.jobs < 1: p.error('jobs must be positive')
    parent = ROOT/'build/validation'; parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='volume-menu-', dir=parent))
    paths = [ROOT/'CMakeLists.txt', ROOT/'test-host.sh', ROOT/'config/dependencies.lock.json',
             ROOT/'tools/volume_menu_probe.c', Path(__file__).resolve()]
    paths += list((ROOT/'runtime/scene').glob('volume_menu*'))
    paths += [ROOT/'tests'/name for name in ['original_volume_menu_oracle.py', 'original_zoom_sprite_oracle.py',
        'original_player_hud_oracle.py', 'original_prop_route_oracle.py', 'original_matrix_oracle.py', 'model_binding.py']]
    paths += [ROOT/name for name in ['runtime/render/vulkan/renderer.c', 'runtime/media/audio.c', 'runtime/media/pcm.c']]
    sources = {str(path.relative_to(ROOT)): digest(path) for path in sorted(set(paths))}
    archives = {pack: digest(a.data/(pack+'.pp')) for pack in ['bk3_00', 'bk3_02']}
    report = dict(passed=False, started_at=datetime.now(timezone.utc).isoformat(),
        source_sha256=sources, archive_sha256=archives, exe_sha256=digest(a.exe),
        commands=[], checks=[], leak_detection=False, real_assets=True,
        application_flow30=False, switch_validation=False, scope=__doc__)
    print('Volume menu validation:', output, flush=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name, command, local_env=env):
        entry = dict(name=name, argv=[str(arg) for arg in command], log=name+'.log')
        report['commands'].append(entry)
        with (output/entry['log']).open('w') as log:
            entry['exit_code'] = subprocess.run(command, cwd=ROOT, env=local_env, stdout=log, stderr=subprocess.STDOUT).returncode
        text = (output/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-16:]))
        return text
    try:
        site = run('python-site', [str(a.host_python.absolute()), '-c', 'import sysconfig;print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir(): raise RuntimeError('invalid Python package directory')
        for label, folder, sanitize, vulkan, kind, targets in [
            ('host', 'build', 'OFF', 'ON', 'RelWithDebInfo', ['model-test', 'volume-menu-probe']),
            ('asan', 'build/asan-static', 'ON', 'ON', 'Debug', ['model-test', 'volume-menu-probe'])]:
            run('configure-'+label, ['cmake','-S','.', '-B',folder, '-DBK_BUILD_TESTS=ON',
                '-DBK_SANITIZE='+sanitize, '-DBK_WITH_VULKAN='+vulkan, '-DCMAKE_BUILD_TYPE='+kind])
            run('build-'+label, ['cmake','--build',folder,'--target',*targets,'--parallel',str(a.jobs)])
        for label, folder, python in [('host','build',a.host_python),('asan','build/asan-static',a.asan_python)]:
            native = output/(label+'-native.json')
            native_env = dict(env, BK3_BUILD_DIR=str(ROOT/folder), PYTHONPATH=os.pathsep.join([str(ROOT/'tests'),str(ROOT/'tools'),site]))
            run(label+'-native', [str(python.absolute()), str(ROOT/'tests/original_volume_menu_oracle.py'),str(a.exe.resolve()),'--output',str(native)], native_env)
            data = json.loads(native.read_text())
            if not data.get('passed') or data['exe_sha256'] != report['exe_sha256']: raise RuntimeError('native result missing pass/digest')
            report['checks'].append(dict(kind='native',mode=label,result=data,library_sha256=digest(ROOT/folder/'libmodel-test.dylib')))
            print(label+' native PASS frames='+str(data['frames']),flush=True)
        for label, folder in [('host','build'),('asan','build/asan-static')]:
            binary=ROOT/folder/'volume-menu-probe'
            text=run(label+'-gpu',[str(binary),str(a.data.resolve())])
            lines=[line for line in text.splitlines() if line.startswith('volume-menu GPU PASS ')]
            if len(lines)!=1:raise RuntimeError('GPU missing terminal pass')
            report['checks'].append(dict(kind='gpu',mode=label,result=lines[0],binary_sha256=digest(binary)))
            print(label+' '+lines[0],flush=True)
        for kind in ['native','gpu']:
            results=[c['result'] for c in report['checks'] if c['kind']==kind]
            if len(results)!=2 or results[0]!=results[1]:raise RuntimeError(kind+' ordinary/ASan disagree')
        if any(digest(ROOT/path)!=value for path,value in sources.items()):raise RuntimeError('source changed during validation')
        if any(digest(a.data/(pack+'.pp'))!=value for pack,value in archives.items()):raise RuntimeError('archive changed during validation')
        report['passed']=True
    except Exception as exc:
        report['error']=str(exc)
        print(str(exc),flush=True)
    finally:
        report['finished_at']=datetime.now(timezone.utc).isoformat()
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    return 0 if report['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
