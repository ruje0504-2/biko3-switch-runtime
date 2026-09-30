"""Pair special cameras or complete special-world CPU resource adapters.

Camera suite covers duplicate SRT and existing ending cameras. World suite
covers original51b647 with real body/face/eyes/cameras, recursive draw-disable
flags, and synthetic Vulkan snapshot regression. No production flow48 UI,
PCM/video playback or Switch hardware acceptance is claimed.
"""
import argparse
from datetime import datetime, timezone
import hashlib, json, os
from pathlib import Path
import subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path);parser.add_argument('data',type=Path)
    parser.add_argument('--host-python',type=Path,default=ROOT/'local/venv/bin/python')
    parser.add_argument('--asan-python',type=Path,default=ROOT/'build/asan/ending-oracle-python')
    parser.add_argument('--jobs',type=int,default=8)
    parser.add_argument('--suite',choices=['camera','world'],default='camera')
    args=parser.parse_args()
    for path in [args.exe,args.host_python,args.asan_python,args.data/'bk3_04.pp',args.data/'bk3_14.pp']:
        if not path.is_file():parser.error('missing input: '+str(path))
    if args.jobs<1:parser.error('--jobs must be positive')
    parent=ROOT/'build/validation';parent.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='special-'+args.suite+'-',dir=parent))
    paths=[ROOT/'CMakeLists.txt',ROOT/'test-host.sh',ROOT/'config/dependencies.lock.json']
    paths += [p for p in (ROOT/'runtime').rglob('*') if p.is_file()]
    paths += list((ROOT/'tests').glob('*.py'))+list((ROOT/'tests').glob('*.c'))
    paths += [ROOT/'tools/actor_view_probe.c']
    sources={str(p.relative_to(ROOT)):digest(p) for p in sorted(paths)}
    asset_paths=[args.data/'bk3_04.pp',args.data/'bk3_14.pp']
    if args.suite=='world':asset_paths += [args.data/'bk3_01.pp']+[args.data/f'h{i:02}_55.fam' for i in range(1,6)]
    assets={str(p.resolve()):digest(p) for p in asset_paths}
    report=dict(passed=False,started_at=datetime.now(timezone.utc).isoformat(),scope=__doc__,
        exe_sha256=digest(args.exe),source_sha256=sources,asset_sha256=assets,commands=[],checks=[],
        suite=args.suite,leak_detection=False,production_flow48=False,switch_validation=False)
    print('Special '+args.suite+' validation:',output,flush=True)
    def run(name,command,env=None):
        entry=dict(name=name,argv=[str(x) for x in command],log=name+'.log');report['commands'].append(entry)
        with (output/entry['log']).open('w') as log:
            entry['exit_code']=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT).returncode
        text=(output/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or 'runtime error:' in text or 'ERROR: AddressSanitizer' in text:
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-15:]))
        return text
    try:
        site=run('python-site',[str(args.host_python.absolute()),'-c','import sysconfig; print(sysconfig.get_paths()["purelib"])']).strip()
        if not Path(site).is_dir():raise RuntimeError('invalid Python package directory')
        for mode,folder,python,sanitize,vulkan,kind in [
            ('host','build',args.host_python,'OFF','ON','RelWithDebInfo'),
            ('asan','build/asan',args.asan_python,'ON','OFF','Debug')]:
            run('configure-'+mode,['cmake','-S','.','-B',folder,'-DBK_BUILD_TESTS=ON',
                '-DBK_SANITIZE='+sanitize,'-DBK_WITH_VULKAN='+vulkan,'-DCMAKE_BUILD_TYPE='+kind])
            targets=['test-animation','test-menu-camera','test-ending-camera','test-node-reference'] if args.suite=='camera' else ['test-actor-forest','test-frame-tree','test-face','test-eye-pose','test-menu-camera','test-lighting-pass']
            ctests='camera-animation|menu-camera|ending-camera|node-reference' if args.suite=='camera' else 'actor-forest|frame-tree|face-controller|eye-pose|menu-camera|lighting-pass'
            run('build-'+mode,['cmake','--build',folder,'--target','model-test',*targets,'--parallel',str(args.jobs)])
            env=dict(os.environ)
            env.update(PYTHONPATH=os.pathsep.join([str(ROOT/'tests'),str(ROOT/'tools'),site]),
                BK3_BUILD_DIR=str(ROOT/folder),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            run('ctest-'+mode,['ctest','--test-dir',folder,'--output-on-failure','-R',
                '^('+ctests+')$'],env)
            results={}
            suites=[
                ('special','original_special_camera_oracle.py','PASS special camera:'),
                ('duplicates','original_animation_duplicates_oracle.py','PASS animation duplicates:'),
                ('ending-regression','original_ending_camera_assets_oracle.py','PASS {')] if args.suite=='camera' else [
                ('world','original_special_world_oracle.py','PASS special world:'),
                ('forest','original_actor_forest_oracle.py','"passed": true')]
            for name,script,marker in suites:
                result=output/(mode+'-'+name+'.json')
                text=run(mode+'-'+name,[str(python.absolute()),str(ROOT/'tests'/script),
                    str(args.exe.resolve()),str(args.data.resolve()),'--output',str(result)],env)
                data=json.loads(result.read_text())
                if not data.get('passed') or data.get('exe_sha256')!=report['exe_sha256'] or text.count(marker)!=1:
                    raise RuntimeError(mode+' '+name+' omitted matching terminal pass')
                results[name]=data
                print('PASS',mode,name,flush=True)
            if args.suite=='world':
                gpu_folder='build' if mode=='host' else 'build/asan-static'
                if mode=='asan':
                    run('configure-asan-vulkan',['cmake','-S','.','-B',gpu_folder,'-DBK_BUILD_TESTS=ON',
                        '-DBK_SANITIZE=ON','-DBK_WITH_VULKAN=ON','-DCMAKE_BUILD_TYPE=Debug'])
                run('build-'+mode+'-vulkan',['cmake','--build',gpu_folder,'--target','actor-view-probe','--parallel',str(args.jobs)])
                text=run(mode+'-gpu',[str(ROOT/gpu_folder/'actor-view-probe'),str(output/(mode+'-gpu-fixture'))],env)
                summaries=[line for line in text.splitlines() if line.startswith('PASS actor view GPU:')]
                if len(summaries)!=1:raise RuntimeError(mode+' GPU omitted terminal pass')
                results['gpu']=summaries[0]
                print('PASS',mode,'GPU',flush=True)
            report['checks'].append(dict(mode=mode,results=results,
                library_sha256=digest(ROOT/folder/'libmodel-test.dylib'),python_sha256=digest(python.resolve())))
        if report['checks'][0]['results']!=report['checks'][1]['results']:
            raise RuntimeError('ordinary and sanitized outcomes differ')
        if any(not (ROOT/p).is_file() or digest(ROOT/p)!=v for p,v in sources.items()):
            raise RuntimeError('source changed during validation')
        if any(digest(Path(p))!=v for p,v in assets.items()) or digest(args.exe)!=report['exe_sha256']:
            raise RuntimeError('input changed during validation')
        report['passed']=True
    except Exception as exc:
        report['error']=str(exc);raise
    finally:
        report['finished_at']=datetime.now(timezone.utc).isoformat()
        (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS special '+args.suite+' pair:',output/'verification.json',flush=True)

if __name__=='__main__':main()
