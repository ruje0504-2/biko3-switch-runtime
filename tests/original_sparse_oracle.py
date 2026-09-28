"""Sparse/empty channels, non-unit quaternions and shuffled sample seeks."""
import ctypes as C, hashlib, json, math, random, struct, sys
from pathlib import Path
from original_animation_oracle import Native
from animation_binding import library
from model_binding import ROOT,decode
from test_model import fixture,chunk

def main():
    exe=Path(sys.argv[1]).read_bytes();native=Native(exe);lib=library();rng=random.Random(351)
    checks=0;worst=0
    for trial in range(160):
        count=2+trial%10;times=[0]
        for _ in range(count-1):times.append(times[-1]+rng.choice([.25,1,2.25,7]))
        if times[-1]<1:times[-1]=1
        keys=[]
        for i,time in enumerate(times):
            k=bytearray(220);struct.pack_into('<f',k,0,time)
            for channel,off in enumerate([4,20,36]):
                # All missing, missing seed, long gaps, single authored key,
                # held tail, and independently sparse channels.
                flag=(i==0 or rng.random()<.25) if trial%5 else (i==count//2)
                if trial%7==channel:flag=False
                if trial%11==0:flag=(i==0)
                struct.pack_into('<I',k,off,int(flag))
            struct.pack_into('<3f',k,8,*[rng.uniform(-50,50) for _ in range(3)])
            struct.pack_into('<3f',k,40,*[rng.uniform(-2,2) for _ in range(3)])
            q=[rng.uniform(-1,1) for _ in range(4)];n=math.sqrt(sum(v*v for v in q))
            scale=rng.choice([0,.5,.99,1,1.02,1.4]);q=[v/n*scale for v in q]
            if i and trial%4==0:q=[-v for v in struct.unpack_from('<4f',keys[-1],116)]
            struct.pack_into('<4f',k,116,*q);keys.append(k)
        data=fixture(reverse_frames=True)+chunk(b'ANIM',bytes(64)+struct.pack('<2I',123,1)+struct.pack('<6I',100,0,0,0,0,count)+b''.join(keys))
        rc,model,error=decode(lib,data);assert rc==1,error
        err=C.create_string_buffer(256);a=lib.bk_model_animation_create(model,err);assert a,(trial,err.value)
        try:
            m=model.contents;native.bind(data,m);out=(C.c_float*(m.frame_count*16))()
            samples=times+[(x+y)/2 for x,y in zip(times,times[1:])]+[times[-1]+.5,times[-1]+1.5,times[-1]*2+.25]
            rng.shuffle(samples)
            for loop in [0,1]:
                for t in samples:
                    assert lib.bk_model_animation_sample(a,t,loop,out,len(out),err),err.value
                    expected=native.sample(m,t,loop)
                    for j,(v,w) in enumerate(zip(out,expected)):
                        e=abs(v-w)/max(1,abs(w));worst=max(worst,e)
                        assert math.isfinite(w) and e<=3e-5,(trial,t,loop,j,v,w,e)
                    checks+=1
        finally:lib.bk_model_animation_destroy(a);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),
                trials=160,pose_samples=checks,max_normalized_error=worst,
                scope='Unhooked 0x4072d9/0x406c6b, synthetic independent missing channels and non-unit quaternions')
    (ROOT/'local/original-sparse-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',checks,worst)
if __name__=='__main__':main()
