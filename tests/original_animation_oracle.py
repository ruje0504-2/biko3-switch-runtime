"""Compare all supported camera tracks to original 0x406c6b, without game I/O.

The runtime track is prepared from the verified 220-byte ANIM key layout;
prev/next indices replace saved pointer-like values. Original 0x4072d9 prepares
S/R/T and matrices, then 0x406c6b samples. No sampler/math hooks are installed.
"""
import argparse, ctypes as C, hashlib, json, random, struct, sys
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine, multiply
from animation_binding import library
from model_binding import decode,ROOT
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def synthetic(native,lib):
    from test_model import fixture,chunk
    import math
    rng=random.Random(313);worst=0;checks=0
    for run in range(100):
        keys=[]
        for i in range(3):
            k=bytearray(220);struct.pack_into("<f",k,0,i*5.5)
            for off in [4,20,36]:struct.pack_into("<I",k,off,1)
            struct.pack_into("<3f",k,8,*[rng.uniform(-100,100) for _ in range(3)])
            struct.pack_into("<3f",k,40,*[rng.uniform(-2,2) for _ in range(3)])
            q=[rng.uniform(-1,1) for _ in range(4)];norm=math.sqrt(sum(v*v for v in q));q=[v/norm for v in q]
            if i and run%4==0:q=[v*(-1 if run%8 else 1) for v in struct.unpack_from("<4f",keys[-1],116)]
            struct.pack_into("<4f",k,116,*q);keys.append(k)
        data=fixture(reverse_frames=True)+chunk(b"ANIM",bytes(64)+struct.pack("<2I",123,1)+struct.pack("<6I",100,0,0,0,0,3)+b"".join(keys))
        rc,model,error=decode(lib,data);assert rc==1,error
        err=C.create_string_buffer(256);anim=lib.bk_model_animation_create(model,err);assert anim,err.value
        try:
            m=model.contents;native.bind(data,m);out=(C.c_float*32)()
            for t in [0,.001,.5,5.499,5.5,9.25,11,11.25,22.25]:
                t=C.c_float(t).value
                for loop in [0,1]:
                    assert lib.bk_model_animation_sample(anim,t,loop,out,32,err),err.value
                    want=native.sample(m,t,loop)
                    for n,(a,b) in enumerate(zip(out,want)):
                        error=abs(a-b)/max(1,abs(b));worst=max(worst,error)
                        assert error<2e-5,(run,t,loop,n,a,b,error)
                    checks+=1
        finally:lib.bk_model_animation_destroy(anim);lib.bk_model_destroy(model)
    return {"pose_samples":checks,"max_normalized_error":worst,"passed":True}

class Native:
    # Largest accepted key block: 1999*220 + 0x10000 < 512 KiB.
    # Keep >448-track costumes below the pose fixture at 0x20000000.
    stride=0x80000
    def __init__(self,exe):
        self.uc=machine(exe);self.capacity=0x800000;self.uc.mem_map(0x4000000,self.capacity)
        self.stack=0x2008000;self.stop=0x300f000
    def bind(self,data,model):
        c=next((model.chunks[i] for i in range(model.chunk_count) if model.chunks[i].tag==b'ANIM'),None)
        if c is None:self.tracks=[];return
        a=data[c.offset:c.offset+c.size];count=struct.unpack_from('<I',a,68)[0];pos=72;self.tracks=[]
        if count*self.stride>self.capacity:
            self.uc.mem_unmap(0x4000000,self.capacity);self.capacity=count*self.stride
            self.uc.mem_map(0x4000000,self.capacity)
        for index in range(count):
            ident,_,_,_,_,n=struct.unpack_from('<6I',a,pos);pos+=24
            assert 1<n<2000
            raw=bytearray(a[pos:pos+n*220]);pos+=n*220
            for j in range(n):struct.pack_into('<ii',raw,j*220+212,j-1,j+1 if j+1<n else -1)
            obj=0x4000000+index*self.stride;frame=obj+0x2000;keys=obj+0x10000
            self.uc.mem_write(obj,b'\0'*0x3000);self.uc.mem_write(keys,bytes(raw))
            for off,v in [(0x74,frame),(0x7c,n),(0x80,n),(0x84,keys),(0x88,n-1),(0x8c,1)]:self.uc.mem_write(obj+off,struct.pack('<I',v))
            self.uc.mem_write(obj+0xec,b'\xff'*401*4)
            target=next(i for i in range(model.frame_count) if model.frames[i].id==ident)
            times=[struct.unpack_from('<f',raw,j*220)[0] for j in range(n)]
            self.tracks.append((obj,frame,target,times))
        assert pos==len(a)
    def sample(self,model,t,loop,blend=None):
        local=[list(model.frames[i].local) for i in range(model.frame_count)]
        for obj,frame,target,_ in self.tracks:
            self.uc.mem_write(obj+0x8c,struct.pack('<I',loop))
            args=struct.pack('<IIf',self.stop,obj,t)
            if blend is not None:args+=struct.pack('<2f',*blend)
            self.uc.mem_write(self.stack,args)
            self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x406c6b if blend is None else 0x408927,self.stop,count=20000000)
            if self.uc.reg_read(UC_X86_REG_EIP)!=self.stop:raise ValueError('original sampler did not return')
            local[target]=struct.unpack('<16f',self.uc.mem_read(frame+0x80,64))
        return self.compose_world(model,local)
    def compose_world(self,model,local):
        world={};pending=set(range(model.frame_count))
        while pending:
            ready=[i for i in pending if model.frames[i].parent_index==0xffffffff or model.frames[i].parent_index in world]
            assert ready
            for i in ready:
                parent=model.frames[i].parent_index
                world[i]=local[i] if parent==0xffffffff else multiply(self.uc,local[i],world[parent])
                pending.remove(i)
        return [v for i in range(model.frame_count) for v in world[i]]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path)
    args=p.parse_args();exe=args.exe.read_bytes();native=Native(exe);lib=library();archive=Archive(args.data/'bk3_04.pp')
    records=[];rng=random.Random(303)
    for entry in archive.entries:
        if not(entry.name.startswith('cam') and entry.name.endswith('.x')):continue
        data=archive.read(entry)
        if data[:4]!=b'OBJM':continue
        result,model,error=decode(lib,data);assert result==1,error
        err=C.create_string_buffer(256);anim=lib.bk_model_animation_create(model,err)
        assert anim,(entry.name,err.value)
        try:
            m=model.contents;native.bind(data,m);end=lib.bk_model_animation_duration(anim)
            # Every stored key plus every key interval midpoint, shuffled seeks,
            # non-integer overrun and exact loop boundaries in both loop modes.
            times=sorted({t for _,_,_,ts in native.tracks for t in ts} | {float((a+b)/2) for _,_,_,ts in native.tracks for a,b in zip(ts,ts[1:])})
            times += [end+x for x in [.001,.5,1,1.5,end,end+.25,2*end+1.75]]
            times += [rng.uniform(0,end*3) for _ in range(64)]
            rng.shuffle(times);worst=0;checks=0
            output=(C.c_float*(m.frame_count*16))()
            for loop in [0,1]:
                for t in times:
                    t=C.c_float(t).value
                    assert lib.bk_model_animation_sample(anim,t,loop,output,len(output),err),err.value
                    expected=native.sample(m,t,loop)
                    for index,(actual,want) in enumerate(zip(output,expected)):
                        error=abs(actual-want)/max(1,abs(want));worst=max(worst,error)
                        if error>2e-5:raise AssertionError((entry.name,t,loop,index,actual,want,error))
                    checks+=1
            records.append({'entry':entry.name,'sha256':hashlib.sha256(data).hexdigest(),'tracks':len(native.tracks),'keys':sum(len(t[3]) for t in native.tracks),'duration_ticks':end,'pose_samples':checks,'max_normalized_error':worst})
            print(entry.name,checks,'PASS',worst,flush=True)
        finally:lib.bk_model_animation_destroy(anim);lib.bk_model_destroy(model)
    extra=synthetic(native,lib)
    report={'synthetic':extra,'passed':True,'exe_sha256':hashlib.sha256(exe).hexdigest(),'native_sampler':'0x406c6b','native_preparation':'0x4072d9','native_world_composition':'0x522d9a','scope':'220-byte complete SRT keys; original math unhooked; game dispatch/actor anchoring not exercised','models':len(records),'tracks':sum(r['tracks'] for r in records),'keys':sum(r['keys'] for r in records),'pose_samples':sum(r['pose_samples'] for r in records),'records':records}
    (ROOT/'local/original-animation-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print('TOTAL',report['models'],report['tracks'],report['keys'],report['pose_samples'],flush=True)
if __name__=='__main__':main()
