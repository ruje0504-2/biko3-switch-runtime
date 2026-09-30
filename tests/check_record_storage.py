"""Verify full Gray record codecs, durable file errors, and actual app round trips.
App story entry/selected-stage handoffs are explicit fixtures. Production input
then hits the actual model anchor, drags/releases the prompt and crosses modes
0/1/2/6/4 to create the recorded menu action, terminal21 and unlock.
A fresh process loads the files and plays that lane from the actual gallery UI.
The default clock and logic both advance1/60 second; --mixed-clock retains the
legacy50ms wall clock with1/60 logic. Neither is real-time or Switch validation.
This is not a complete natural story walkthrough.
"""
import argparse, concurrent.futures, hashlib, json, os, subprocess, tempfile, threading
from datetime import datetime, timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--jobs',type=int,default=4);ap.add_argument('--mixed-clock',action='store_true');a=ap.parse_args()
    if a.jobs<1:ap.error('jobs must be positive')
    out=Path(tempfile.mkdtemp(prefix='record-storage-',dir=ROOT/'build/validation'))
    paths=[ROOT/'CMakeLists.txt',ROOT/'test-host.sh',ROOT/'config/dependencies.lock.json']
    paths += [p for folder in ['runtime','tests'] for p in (ROOT/folder).rglob('*') if p.suffix in ['.c','.h','.inc','.py']]
    paths += list((ROOT/'tools').glob('ending_*'))
    hashes={str(p.relative_to(ROOT)):digest(p) for p in sorted(paths) if p.is_file()}
    packs=['bk3_00','bk3_01','bk3_02','bk3_03','bk3_04','bk3_05','bk3_06','bk3_08','bk3_09','bk3_10','bk3_11','bk3_12','bk3_13','bk3_16','bk3_18','fambom']
    datahash={p+'.pp':digest(a.data/(p+'.pp')) for p in packs}
    for p in a.data.iterdir():
        if p.suffix.lower() in ['.fam','.vix','.ftt'] or p.name=='bk3_Gray.b3f':datahash[p.name]=digest(p)
    report=dict(passed=False,scope=__doc__,started_at=datetime.now(timezone.utc).isoformat(),source_sha256=hashes,data_sha256=datahash,exe_sha256=digest(a.exe),commands=[],native=[],applications=[],hardware_validation=False,logic_step_seconds=1/60,wall_step_seconds=.05 if a.mixed_clock else 1/60)
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    report_lock=threading.Lock()
    def checkpoint():
        with report_lock:
            tmp=out/'verification.json.part'
            tmp.write_text(json.dumps(report,indent=2)+'\n')
            tmp.replace(out/'verification.json')
    checkpoint()
    def run(name,cmd,extra=None):
        entry=dict(name=name,argv=list(map(str,cmd)),log=name+'.log');report['commands'].append(entry);checkpoint()
        with (out/entry['log']).open('w') as log:entry['exit_code']=subprocess.run(cmd,cwd=ROOT,env={**env,**(extra or {})},stdout=log,stderr=subprocess.STDOUT).returncode
        checkpoint()
        text=(out/entry['log']).read_text(errors='replace')
        if entry['exit_code'] or any(m in text for m in ['runtime error:','ERROR: AddressSanitizer','Exception ignored on calling ctypes callback']):
            raise RuntimeError(name+':\n'+'\n'.join(text.splitlines()[-12:]))
        return text
    print('Record validation:',out,flush=True)
    try:
        targets=['ending-record-app-probe','test-record-file','test-record-switch-fs','test-unlock-file','test-unlock-switch-fs','test-ending-record']
        for mode,folder,sanitize,kind in [('host','build','OFF','RelWithDebInfo'),('asan','build/asan-static','ON','Debug')]:
            run('configure-'+mode,['cmake','-S','.','-B',folder,'-DBK_BUILD_TESTS=ON','-DBK_WITH_VULKAN=ON','-DBK_SANITIZE='+sanitize,'-DCMAKE_BUILD_TYPE='+kind])
            run('build-'+mode,['cmake','--build',folder,'--target',*targets,'--parallel',str(a.jobs)])
            text=run('unit-'+mode,['ctest','--test-dir',folder,'-R','^(record-file|record-switch-fs|unlock-file|unlock-switch-fs|ending-record)$','--output-on-failure'])
            assert '100% tests passed' in text
        run('build-native-host',['cmake','--build','build','--target','model-test','--parallel',str(a.jobs)])
        run('configure-native-asan',['cmake','-S','.','-B','build/asan','-DBK_BUILD_TESTS=ON','-DBK_WITH_VULKAN=OFF','-DBK_SANITIZE=ON','-DCMAKE_BUILD_TYPE=Debug'])
        run('build-native-asan',['cmake','--build','build/asan','--target','model-test','--parallel',str(a.jobs)])
        site=run('python-site',[str(ROOT/'local/venv/bin/python'),'-c','import sysconfig;print(sysconfig.get_paths()["purelib"])']).strip()
        for mode,folder,python in [('host','build',ROOT/'local/venv/bin/python'),('asan','build/asan',ROOT/'build/asan/ending-oracle-python')]:
            result=out/('native-'+mode+'.json')
            run('native-'+mode,[str(python),str(ROOT/'tests/original_record_storage_oracle.py'),str(a.exe.resolve()),str(a.data.resolve()),'--report',str(result)],dict(BK3_BUILD_DIR=str(ROOT/folder),PYTHONPATH=os.pathsep.join([str(ROOT/'tests'),str(ROOT/'tools'),site])))
            report['native'].append(json.loads(result.read_text()))
        assert report['native'][0]==report['native'][1] and report['native'][0]['passed']
        print('PASS native records ordinary/ASan',flush=True)
        def profile(mode,folder,g,v):
            base=f'{mode}-{g}-{v}';root=out/(base+'-files');summaries=[];input_trace=[]
            for phase in ['produce','replay']:
                text=run(base+'-'+phase,[str(ROOT/folder/'ending-record-app-probe'),str(a.data.resolve()),str(root),phase,str(g),str(v),*(['mixed-clock'] if a.mixed_clock else [])])
                passed=[x for x in text.splitlines() if x.startswith('PASS record-app ')]
                assert len(passed)==1
                if phase=='produce':
                    input_trace=[x for x in text.splitlines() if x.startswith(('record-input ', 'PASS record-input '))]
                    assert len([x for x in input_trace if x.startswith('PASS record-input ')])==1
                summaries.append(passed[0]);print(passed[0]+' ['+mode+']',flush=True)
            return dict(mode=mode,group=g,variant=v,summaries=summaries,input_trace=input_trace,files={name:digest(root/name) for name in ['expected.bkr','save/records.bkr','save/unlocks.bku']},binary_sha256=digest(ROOT/folder/'ending-record-app-probe'))
        # Independent app processes, two at a time; each production write must
        # finish before its new-process replay. No shared output roots.
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(profile,mode,folder,g,v) for g in range(5) for v in range(2) for mode,folder in [('host','build'),('asan','build/asan-static')]]
            errors=[]
            for future in concurrent.futures.as_completed(futures):
                try:
                    report['applications'].append(future.result());checkpoint()
                except Exception as exc:errors.append(str(exc))
            if errors:raise RuntimeError('\n'.join(errors))
        for g in range(5):
            for v in range(2):
                pair=sorted([x for x in report['applications'] if x['group']==g and x['variant']==v],key=lambda x:x['mode'])
                assert len(pair)==2 and pair[0]['summaries']==pair[1]['summaries'] and pair[0]['files']==pair[1]['files'] and pair[0]['input_trace']==pair[1]['input_trace'],(g,v,'pair mismatch')
        run('python-tests',['python3','-m','unittest','discover','-s','tests','-p','test_*.py'])
        assert all(digest(ROOT/p)==h for p,h in hashes.items()),'source changed during validation'
        assert all(digest(a.data/p)==h for p,h in datahash.items()),'data changed during validation'
        report.update(passed=True,application_pairs=10,new_process_replays=20,unit_tests_per_build=5,native_pairs=1)
    except Exception as exc:
        report['error']=str(exc);raise
    finally:
        report['finished_at']=datetime.now(timezone.utc).isoformat()
        report['applications'].sort(key=lambda x:(x['group'],x['variant'],x['mode']))
        checkpoint()
    print('PASS record persistence:',out/'verification.json',flush=True)
if __name__=='__main__':main()
