"""Actor XAN request/reset/authored scalar playback vs unhooked x86 scheduler."""
import argparse, hashlib, json, random, struct, sys
from pathlib import Path
from original_clip_oracle import Native, run
from clip_binding import library
from model_binding import ROOT
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();arc=Archive(args.data/'bk3_01.pp')
    records=[];rng=random.Random(350)
    for actor in range(6):
        name=f'h{actor:02}_80.xan';entry=next(e for e in arc.entries if e.name==name);data=arc.read(entry)
        slots=[i for i in range(128) if struct.unpack_from('<2f',data,512+0x190+i*156+0x54)!=(0,0)]
        checks=0;worst=0
        for authored in [False,True]:
            # Repeated requests must preserve progress, including after a chain.
            n,e=run(native,lib,data,slots,[1/120]*2200+[0,.1,1,10,100]*4,
                    instant=2,authored=authored,repeat=True)
            checks+=n;worst=max(worst,e)
            for mode in [0,1,2]:
                n,e=run(native,lib,data,slots,[rng.choice([0,1/120,.01,.1,1,10]) for _ in range(90)],
                        instant=mode,authored=authored,
                        switches={5:(slots[-1],2),6:(slots[-1],2),25:(slots[0],0),50:(slots[-1],1)})
                checks+=n;worst=max(worst,e)
        # Directly advance the stored phase without any selection call.
        n,e=run(native,lib,data,[0],[0,1/120,.1,1,10,100]*8,instant=None,authored=True)
        checks+=n;worst=max(worst,e)
        records.append(dict(entry=name,sha256=hashlib.sha256(data).hexdigest(),slots=slots,
                            checks=checks,max_normalized_error=worst))
        print(name,checks,'PASS',worst,flush=True)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),
                native_functions=['0x401b0a','0x401d24','0x401f71','0x4025b9','0x4026fe'],
                scope='Fresh and authored scalar playback; actual scheduler and request unhooked. Only SRT submission hooked. No actor gameplay or GPU execution.',
                records=records,checks=sum(r['checks'] for r in records),
                max_normalized_error=max(r['max_normalized_error'] for r in records))
    (ROOT/'local/original-actor-clip-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print('TOTAL',report['checks'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
