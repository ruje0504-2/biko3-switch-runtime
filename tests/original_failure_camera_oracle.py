"""Five original failure-camera functions, including unhooked sin/cos, aim,
world setters and signed convergence tests. Only FOV device setter is hooked.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_aim_oracle import Pose,I
from model_binding import ROOT,library
Vec=C.c_float*3
class Input(C.Structure):
 _fields_=[('group',C.c_uint32),('player',Vec),('player_height',C.c_float),('player_yaw',C.c_float),('npc',Vec),('npc_height',C.c_float),('npc_yaw',C.c_float),('prop',Vec),('prop_yaw',C.c_float),('seconds',C.c_float)]
class Effects(C.Structure):_fields_=[('arrived',C.c_int),('write_fov',C.c_int),('fov',C.c_float)]
FUNCTIONS=[0x4bc919,0x4bcab3,0x4bcca9,0x4bd04f,0x4bd3ed]
class Native(Base):
 frame,context,camera=0x3009000,0x300a000,0x71af38
 def __init__(self,exe):
  super().__init__(exe);self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x42cf0e,end=0x42cf0e)
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,value=struct.unpack('<If',u.mem_read(sp,8));self.fov.append(value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def vector(self,a,v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def run(self,pose,kind,inp,index):
  u=self.u;self.fov=[];u.mem_write(self.camera,bytes(0x600))
  for a in [self.frame,self.context]:
   u.mem_write(a,bytes(0x200))
   for off in [0x80,0xc0,0x100]:self.vector(a+off,I)
  self.vector(self.frame+0x80,pose.world);self.vector(self.frame+0xc0,pose.world);self.vector(self.camera+0x420,pose.position)
  u.mem_write(0x645600,struct.pack('<II',self.context,self.frame));u.mem_write(0x7219a8,struct.pack('<I',inp.group));self.vector(0x733700,[inp.seconds])
  self.vector(0x71b7ac,inp.player);self.vector(0x71b7a8,[inp.player_height]);self.vector(0x71b7bc,[inp.player_yaw]);self.vector(0x729084,inp.npc);self.vector(0x729080,[inp.npc_height]);self.vector(0x729094,[inp.npc_yaw]);u.mem_write(0x728de4,struct.pack('<I',index));self.vector(0x72a01c+index*0x998,inp.prop);self.vector(0x72a02c+index*0x998,[inp.prop_yaw])
  self.call(FUNCTIONS[kind],struct.pack('<I',self.camera))
  out=Pose();out.world[:]=struct.unpack('<16f',u.mem_read(self.frame+0xc0,64));out.position[:]=struct.unpack('<3f',u.mem_read(self.camera+0x420,12))
  return out,u.reg_read(UC_X86_REG_EAX)&255,self.fov

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4bc919);e=C.create_string_buffer(256);worst=0;total=arrivals=0
 lib.bk_failure_camera_step.argtypes=[C.POINTER(Pose),C.c_int,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
 for kind in range(5):
  for case in range(2400):
   vec=lambda:Vec(*(rng.uniform(-2000,2000) for _ in range(3)))
   pose=Pose((C.c_float*16)(*I),vec());pose.world[12:15]=list(vec())
   inp=Input(case%5,vec(),rng.uniform(10,25),rng.uniform(-720,720),vec(),rng.uniform(10,25),rng.uniform(-720,720),vec(),rng.uniform(-720,720),rng.choice([0,1/60,.1,1,10]))
   if case%16==0:pose.position[:]=[10000]*3
   wanted,ready,fov=n.run(pose,kind,inp,case%16);effects=Effects()
   assert lib.bk_failure_camera_step(C.byref(pose),kind,C.byref(inp),C.byref(effects),e),e.value
   assert effects.arrived==ready,(kind,case,'convergence',effects.arrived,ready)
   assert fov==([C.c_float(.2).value] if effects.write_fov else [])
   for x,y in zip([*pose.world,*pose.position],[*wanted.world,*wanted.position]):
    error=abs(x-y)/max(1,abs(y));worst=max(worst,error);assert math.isfinite(error) and error<3e-6,(kind,case,x,y,error)
   if case%16==0:assert effects.arrived==1
   total+=1;arrivals+=effects.arrived
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=total,arrivals=arrivals,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-failure-camera-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS failure camera',total,'steps',arrivals,'arrivals','max-relative-error',worst)
if __name__=='__main__':main()
