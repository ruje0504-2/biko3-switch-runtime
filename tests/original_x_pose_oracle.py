"""Text X camera pose import vs original447667 text/key parser and sampler.
Geometry/material bodies intentionally remain opaque in the pose-only API.
Native heap, registry and sprintf are service boundaries; decimal conversion,
key insertion, W sign conversion, sparse completion and SRT are unmodified.
Frame matrix decimals additionally pass original44b671 and D3DX composition.
"""
import argparse, ctypes as C, hashlib, json, math, re, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_matrix_oracle import multiply
from animation_binding import library, PoseSample
from model_binding import ROOT, Model
from bk3_assets import Archive
I = [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class Native(Base):
    text, heap, frame, output = 0x4000000,0x6000000,0x3001000,0x3003000
    def __init__(self, exe):
        super().__init__(exe); self.u.mem_map(self.text,0x400000);self.u.mem_map(self.heap,0x1000000)
        self.cursor=self.heap;self.alloc={};self.target='';self.track=0
        for a in [0x42d217,0x42d249,0x42d270,0x42a0a5,0x42a0cd,0x429c6b,0x53491b]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v))
    def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
    def string(self,a):
        b=bytearray()
        while self.u.mem_read(a+len(b),1)!=b'\0':
            b+=self.u.mem_read(a+len(b),1);assert len(b)<1024
        return bytes(b)
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret,x,y,z=struct.unpack('<4I',u.mem_read(sp,16));result=0
        if a in [0x42d217,0x42d249]:
            n=x if a==0x42d217 else y;result=self.cursor;self.cursor+=(n+15)&~15
            assert self.cursor<self.heap+0x1000000
            u.mem_write(result,bytes(n));self.alloc[result]=n
            if a==0x42d249:u.mem_write(result,bytes(u.mem_read(x,min(n,self.alloc[x]))))
        elif a==0x429c6b:
            assert self.string(x).decode()==self.target,(self.string(x),self.target)
            assert y==0x3ec;result=self.frame
        elif a==0x53491b:
            assert self.string(y)==b'%s'
            s=self.string(z);u.mem_write(x,s+b'\0');result=len(s)
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def call(self,a,args):
        u=self.u;u.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(a,self.stop,count=300000000)
        assert u.reg_read(UC_X86_REG_EIP)==self.stop,(hex(a),hex(u.reg_read(UC_X86_REG_EIP)))
    def parse(self,text):
        self.cursor=self.heap;self.alloc={};self.u.mem_write(self.text,text+b'\0'*512)
        self.target=re.search(rb'Animation\s*\{\s*\{\s*(\w+)',text).group(1).decode()
        self.word(0x686870,0);self.u.mem_write(self.frame,bytes(0x400))
        for off in [0x80,0xc0,0x100]:self.u.mem_write(self.frame+off,struct.pack('<16f',*I))
        self.call(0x447667,struct.pack('<3I',self.text,0,self.output))
        self.track=self.read(self.output);count=self.read(self.track+0x7c)
        self.call(0x4072d9,struct.pack('<I',self.track))
        return bytes(self.u.mem_read(self.read(self.track+0x84),count*220))
    def sample(self,t,loop,blend=None):
        self.word(self.track+0x8c,loop)
        args=struct.pack('<If',self.track,t)
        if blend is not None:args+=struct.pack('<2f',*blend)
        self.call(0x406c6b if blend is None else 0x408927,args)
        return list(struct.unpack('<16f',self.u.mem_read(self.frame+0x80,64)))
    def matrix(self,raw):
        self.u.mem_write(self.text,raw+b'\0'*512);p=self.text;out=[]
        for i in range(16):
            self.call(0x44b671,struct.pack('<2I',p,self.output));p=self.u.reg_read(UC_X86_REG_EAX)
            out.append(struct.unpack('<f',self.u.mem_read(self.output,4))[0])
        return out

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256)
    lib.bk_model_x_pose_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(C.POINTER(Model)),C.c_void_p]
    arc=Archive(a.data/'bk3_04.pp');records=[];worst=0.;matrices=0;keys=0
    def equal(got,want,label):
        nonlocal worst
        for i,(g,w) in enumerate(zip(got,want)):
            d=abs(g-w)/max(1,abs(w));worst=max(worst,d)
            assert math.isfinite(d) and d<3e-6,(label,i,g,w,d)
    for name in ['cam00_03.x','cam00_60.x','cam01_50.x']:
        raw=arc.read(next(e for e in arc.entries if e.name==name));m=C.POINTER(Model)();anim=None
        assert lib.bk_model_x_pose_decode(raw,len(raw),C.byref(m),e)==1,(name,e.value)
        try:
            model=m.contents;assert model.frame_count==10 and model.mesh_count==0
            assert C.string_at(model.source,len(raw))==raw
            found=list(re.finditer(rb'Frame\s+(\w+)\s*\{\s*FrameTransformMatrix\s*\{([^}]+)',raw))
            assert len(found)==9
            for i,r in enumerate(found,1):
                assert model.frames[i].name==r.group(1)
                equal(model.frames[i].local,n.matrix(r.group(2)),(name,'base',i));matrices+=1
            anim=lib.bk_model_animation_create(m,e);assert anim,e.value
            native=n.parse(raw[raw.index(b' Animation {'):])
            c=model.chunks[0];generated=C.string_at(C.addressof(model.source.contents)+c.offset,c.size)
            count=struct.unpack_from('<I',generated,92)[0];assert len(native)==count*220
            # Native4072d9 completes sparse values; real three files have all channels.
            for i in range(count):
                k=generated[96+i*220:96+(i+1)*220];v=native[i*220:(i+1)*220]
                for off,length in [(0,20),(20,4),(36,16),(116,16)]:
                    assert k[off:off+length]==v[off:off+length],(name,i,off)
                keys+=1
            node=next(i for i in range(model.frame_count) if model.frames[i].name.decode()==n.target)
            out=(C.c_float*(model.frame_count*16))();duration=lib.bk_model_animation_duration(anim)
            for step in range(180):
                t=C.c_float([0,.001,.5,1,18.7,duration,duration+.1,duration*2.1][step%8]+(step//8)*.13).value
                other=C.c_float(duration*(step%17)/16).value;weight=C.c_float((step%11)/10).value
                request=PoseSample(t,other,weight,step%3==0,step%2)
                assert lib.bk_model_animation_pose(anim,C.byref(request),None,out,len(out),e),e.value
                pose=n.sample(t,request.loop,(other,weight) if request.blend else None)
                world={}
                for i in range(model.frame_count):
                    f=model.frames[i];local=pose if i==node else list(f.local)
                    world[i]=local if f.parent_index==0xffffffff else list(multiply(n.u,local,world[f.parent_index]))
                    equal(out[i*16:(i+1)*16],world[i],(name,step,i));matrices+=1
            records.append(dict(name=name,sha256=hashlib.sha256(raw).hexdigest(),frames=model.frame_count,keys=count,samples=180))
            print(name,'PASS',flush=True)
        finally:
            lib.bk_model_animation_destroy(anim);lib.bk_model_destroy(m)
    r=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,keys=keys,matrices=matrices,max_relative_error=worst,scope=__doc__)
    (ROOT/'local/original-x-pose-oracle.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
if __name__=='__main__':main()
