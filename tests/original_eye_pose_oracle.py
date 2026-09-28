"""Replay complete original 4a0823 with real cached eye frames and synthetic poses."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_eye_assets_oracle import bind as asset_bind, Config, Archive
from model_binding import ROOT, library, decode

class Eye(C.Structure):
    _fields_=[('local',C.c_float*16),('world',C.c_float*16),('parent',C.c_float*16)]
def identity(x=0,y=0,z=0):return [1,0,0,0,0,1,0,0,0,0,1,0,x,y,z,1]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
class Native:
    stack,stop,state,target=0x200e000,0x300f000,0x3000000,0x3002000
    def __init__(self,exe):self.u=machine(exe)
    def seed(self,eyes):
        for i,e in enumerate(eyes):
            p=0x3001000+i*0x400
            self.u.mem_write(p+0x80,bytes(e))
            self.u.mem_write(self.state+0x18+i*4,struct.pack('<I',p))
    def call(self,target,mode,variant,pitch,yaw):
        u=self.u;u.mem_write(self.target+0xc0,struct.pack('<16f',*target))
        u.mem_write(self.state+0x24,struct.pack('<2I',variant,mode))
        u.mem_write(self.stack,struct.pack('<5I',self.stop,self.state,bits(pitch),bits(yaw),self.target))
        u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(0x4a0823,self.stop,count=3000000)
        assert u.reg_read(UC_X86_REG_EIP)==self.stop
        return (Eye*2)(*[Eye.from_buffer_copy(bytes(u.mem_read(0x3001080+i*0x400,C.sizeof(Eye)))) for i in range(2)])

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();lib=library();asset_bind(lib);n=Native(exe);error=C.create_string_buffer(256)
    lib.bk_eye_pose_aim.argtypes=[C.POINTER(Eye),C.POINTER(C.c_float),C.c_uint32,C.c_uint32,C.c_float,C.c_float,C.c_void_p];lib.bk_eye_pose_aim.restype=C.c_int
    lib.bk_matrix_multiply.argtypes=[C.POINTER(C.c_float)]*3
    lib.bk_matrix_quaternion.argtypes=[C.POINTER(C.c_float)]*2
    def multiply(a,b):
        out=(C.c_float*16)();lib.bk_matrix_multiply(out,(C.c_float*16)(*a),(C.c_float*16)(*b));return list(out)
    def rotated(rng):
        angle=rng.uniform(-2,2);q=[math.sin(angle/2)*v for v in [0,.6,.8]]+[math.cos(angle/2)]
        out=(C.c_float*16)();lib.bk_matrix_quaternion(out,(C.c_float*4)(*q));out[12:15]=[rng.uniform(-40,40) for _ in range(3)]
        for row in range(3):
            scale=rng.uniform(.8,2)
            for col in range(3):out[row*4+col]*=scale
        return list(out)
    rng=random.Random(0x4a0823);count=values=rejects=0;worst=0;records=[]
    def check(eyes,target,mode,variant,pitch,yaw,label):
        nonlocal count,values,worst
        expected=n.call(target,mode,variant,pitch,yaw)
        assert lib.bk_eye_pose_aim(eyes,(C.c_float*16)(*target),mode,variant,pitch,yaw,error),(label,error.value)
        for i in range(2):
            assert bytes(eyes[i].parent)==bytes(expected[i].parent)
            for field in ['local','world']:
                for j,(x,y) in enumerate(zip(getattr(eyes[i],field),getattr(expected[i],field))):
                    d=abs(x-y)/max(1,abs(y));worst=max(worst,d)
                    assert math.isfinite(d) and d<3e-5,(label,count,i,field,j,x,y,d)
                    values+=1
        count+=1
    for case in range(600):
        eyes=(Eye*2)()
        for i in range(2):
            p=rotated(rng);l=rotated(rng)
            if case%4==0:p[15]=.85
            eyes[i].local[:]=l;eyes[i].parent[:]=p
            # Published eye world is independent of the current submitted local.
            eyes[i].world[:]=multiply(rotated(rng),p)
        n.seed(eyes)
        for step in range(4):
            t=identity(*[rng.uniform(-100,100) for _ in range(3)])
            if case%19==0:t=list(eyes[0].world) # zero direction for one eye
            check(eyes,t,[0,1,7,0xffffffff][case%4],step%2,[0,.1,.2,1][step],[.1,0,.6,1][step],('synthetic',case,step))
    store=lib.bk_resources_create(error);assert store
    assert lib.bk_resources_mount(store,b'bk3_01',str(a.data/'bk3_01.pp').encode(),error)
    ar=Archive(a.data/'bk3_01.pp');entries={e.name.encode():e for e in ar.entries}
    try:
        for path in sorted(a.data.glob('*.fam')):
            raw=path.read_bytes();c=Config();assert lib.bk_face_config_decode(raw,len(raw),C.byref(c),error)
            if c.actor_clip not in entries:continue
            name=ar.read(entries[c.actor_clip])[:256].split(b'\0')[0];data=ar.read(entries[name]);ok,m,msg=decode(lib,data);assert ok,msg
            resource=lib.bk_eye_assets_create(store,b'bk3_01',m,name,C.byref(c),error);assert resource,error.value
            try:
                binding=lib.bk_eye_assets_binding(resource).contents
                if 0xffffffff in binding.frames:continue
                world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),error)
                eyes=(Eye*2)()
                for i,index in enumerate(binding.frames):
                    f=m.contents.frames[index];eyes[i].local[:]=list(f.local);eyes[i].world[:]=world[index*16:index*16+16]
                    eyes[i].parent[:]=world[f.parent_index*16:f.parent_index*16+16] if f.parent_index!=0xffffffff else identity()
                n.seed(eyes)
                center=list(eyes[0].world)[12:15]
                for step in range(60):
                    t=identity(center[0]+100*math.sin(step*.2),center[1]+25*math.cos(step*.17),center[2]+60)
                    check(eyes,t,binding.mode,(step//15)%2,.25 if step%9 else 0,.4 if step%11 else 0,(path.name,step))
                records.append(dict(config=path.name,model=name.decode(),model_sha256=hashlib.sha256(data).hexdigest(),frames=list(binding.frames),steps=60))
                print(path.name,'PASS',flush=True)
            finally:lib.bk_eye_assets_destroy(resource);lib.bk_model_destroy(m)
    finally:lib.bk_resources_destroy(store)
    # Failed second-eye input and bad parameters must leave BOTH eyes intact.
    for case in range(7):
        e=(Eye*2)()
        for eye in e:eye.local[:]=eye.world[:]=eye.parent[:]=identity()
        t=identity(1,2,3);mode=0;variant=0;pitch=.2;yaw=.4
        if case==0:e[1].parent[0]=0
        elif case==1:e[1].world[0]=float('nan')
        elif case==2:variant=2
        elif case==3:pitch=-1
        elif case==4:yaw=float('inf')
        elif case==5:t[8]=float('nan')
        else:e[1].local[15]=float('inf')
        before=bytes(e);assert not lib.bk_eye_pose_aim(e,(C.c_float*16)(*t),mode,variant,pitch,yaw,error)
        assert bytes(e)==before;rejects+=1
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=count,matrix_values=values,actual_profiles=len(records),atomic_rejections=rejects,max_normalized_error=worst,records=records,native_functions=['0x4a0823','0x422ea4','0x4a0bc7','0x4a74c0','0x42403c','0x4241e3'],scope='Complete native gaze function, no substituted math. Synthetic independent local/world/parent caches plus 60 consecutive poses per actual primary eye pair. Actor playback integration and native call-site dispatch not covered.')
    (ROOT/'local/original-eye-pose-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',count,'gaze samples, max error',worst)
if __name__=='__main__':main()
