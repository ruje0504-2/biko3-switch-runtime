"""Compare native 0x408927 SRT transitions; no animation/math hooks."""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from original_animation_oracle import Native,Archive
from animation_binding import library
from model_binding import ROOT,decode
from test_model import fixture,chunk

def check(native,lib,data,rng):
    rc,m,error=decode(lib,data);assert rc==1,error
    err=C.create_string_buffer(256);a=lib.bk_model_animation_create(m,err);assert a,err.value
    try:
        native.bind(data,m.contents);end=lib.bk_model_animation_duration(a)
        out=(C.c_float*(16*m.contents.frame_count))();worst=0;count=0
        pairs=[(0,end),(end,0),(end/3,end*.75),(end+.5,end*2+1.5)]
        pairs += [(rng.uniform(0,3*end),rng.uniform(0,3*end)) for _ in range(16)]
        for first,last in pairs:
            first=C.c_float(first).value;last=C.c_float(last).value
            for weight in [-.25,0,.125,.25,.5,.75,1,1.25]:
                for loop in [0,1]:
                    assert lib.bk_model_animation_blend(a,first,last,weight,loop,out,len(out),err),err.value
                    expected=native.sample(m.contents,first,loop,(last,weight))
                    for i,(v,w) in enumerate(zip(out,expected)):
                        error=abs(v-w)/max(1,abs(w));worst=max(worst,error)
                        assert error<2e-5,(first,last,weight,loop,i,v,w,error)
                    count+=1
        return count,worst
    finally:lib.bk_model_animation_destroy(a);lib.bk_model_destroy(m)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(3401);records=[];synthetic=[]
    for entry in Archive(args.data/'bk3_04.pp').entries:
        if not(entry.name.startswith('cam') and entry.name.endswith('.x')):continue
        # Archive owns a seekable handle; keep reads independent of oracle.
        arc=Archive(args.data/'bk3_04.pp');data=arc.read(entry)
        if data[:4]!=b'OBJM':continue
        count,worst=check(native,lib,data,rng)
        records.append({'entry':entry.name,'checks':count,'max_normalized_error':worst})
        print(entry.name,count,'PASS',worst,flush=True)
    for run in range(50):
        keys=[]
        for i in range(3):
            k=bytearray(220);struct.pack_into('<f',k,0,i*5.5)
            for off in [4,20,36]:struct.pack_into('<I',k,off,1)
            struct.pack_into('<3f',k,8,*[rng.uniform(-200,200) for _ in range(3)])
            struct.pack_into('<3f',k,40,*[rng.uniform(-3,3) for _ in range(3)])
            q=[rng.uniform(-1,1) for _ in range(4)];norm=math.sqrt(sum(v*v for v in q));q=[v/norm for v in q]
            if i and run%4==0:q=[v*(-1 if run%8 else 1) for v in struct.unpack_from('<4f',keys[-1],116)]
            struct.pack_into('<4f',k,116,*q);keys.append(k)
        data=fixture(reverse_frames=True)+chunk(b'ANIM',bytes(64)+struct.pack('<2I',123,1)+struct.pack('<6I',100,0,0,0,0,3)+b''.join(keys))
        count,worst=check(native,lib,data,rng);synthetic.append({'checks':count,'max_normalized_error':worst})
    report={'passed':True,'exe_sha256':hashlib.sha256(exe).hexdigest(),'native_functions':['0x408927','0x4072d9','0x523d9c','0x42d92f','0x409520','0x407d71','0x522d9a'],'scope':'Full SRT transition sampler and hierarchy; no native math hooks; complete linear-position tracks only.','asset_checks':sum(r['checks'] for r in records),'synthetic_checks':sum(r['checks'] for r in synthetic),'max_normalized_error':max(r['max_normalized_error'] for r in records+synthetic),'records':records}
    (ROOT/'local/original-blend-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['asset_checks'],report['synthetic_checks'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
