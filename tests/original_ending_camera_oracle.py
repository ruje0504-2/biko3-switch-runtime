"""Original4bb0a4,4df411,4e1711,4e1d16 with native trig/D3DX/frame/aim arithmetic.
Input and FOV are observed boundaries. 4e1711 animation advance is an explicit
service; track/target values are previously published fixtures, not a complete
animated event scene. No request or pre-publication is allowed in that path.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_menu_camera_oracle import Native as Base,State,Vec,I,values
from model_binding import ROOT,library
class Native(Base):
 def hook(self,u,a,size,_):
  if a!=0x4026fe:return super().hook(u,a,size,_)
  sp=u.reg_read(UC_X86_REG_ESP)
  ret,first=struct.unpack('<2I',u.mem_read(sp,8))
  assert first==self.clip and self.read(sp+8,1)[0]==self.dt
  self.events.append('advance')
  # Simulate changed local data only. Cached world XYZ must remain old.
  self.vf(self.track+0x80+48,[9001,9002,9003])
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_ESP,sp+4)
 def run(self,state,kind,motion,buttons,dt,offset,track,target,flow):
  self.motion=list(motion);self.buttons=buttons;self.dt=dt;self.events=[];self.fov=state.fov
  u=self.u;u.mem_write(self.camera,bytes(0x600));u.mem_write(self.clip,bytes(0x5000))
  self.wi(self.camera,self.clip);self.wi(self.camera+8,self.track);self.wi(0x719448,self.focus);self.wi(0x721ef4,self.focus)
  self.wi(0x645600,self.context);self.wi(0x645604,self.frame)
  for p in [self.frame,self.context]:
   for off in [0x80,0xc0,0x100]:self.vf(p+off,I)
  self.vf(self.frame+0x80,state.pose.world);self.vf(self.frame+0xc0,state.pose.world)
  self.vf(self.camera+0x420,state.pose.position)
  self.vf(self.camera+0x42c,[state.yaw,state.pitch,state.radius,state.height])
  self.vf(self.camera+0x4a4,state.matrix);self.vf(self.camera+0x5c8,state.focus)
  self.vf(self.track+0xf0,track);self.vf(self.focus+0xf0,target)
  self.vf(0x733700,[dt]);u.mem_write(0xbeeb84,bytes([flow]))
  self.call([0x4bb0a4,0x4df411,0x4e1711,0x4e1d16][kind],struct.pack('<I3f',self.camera,*offset))
  out=State.from_buffer_copy(state)
  out.pose.world[:]=self.read(self.frame+0xc0,16);out.pose.position[:]=self.read(self.camera+0x420,3)
  out.yaw,out.pitch,out.radius,out.height=self.read(self.camera+0x42c,4)
  out.matrix[:]=self.read(self.camera+0x4a4,16);out.focus[:]=self.read(self.camera+0x5c8,3);out.fov=self.fov
  assert self.events==(['advance'] if kind==2 else []),self.events
  return out
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4bb0a4);e=C.create_string_buffer(256)
 vp=C.POINTER(C.c_float);sp=C.POINTER(State)
 lib.bk_ending_camera_orbit.argtypes=[sp,vp,C.c_uint,vp,C.c_uint8,C.c_float,C.c_void_p]
 lib.bk_ending_camera_fixed.argtypes=[sp,vp,C.c_void_p]
 lib.bk_ending_camera_track_pose.argtypes=[sp,vp,vp,C.c_float,C.c_void_p]
 lib.bk_ending_camera_manual.argtypes=[sp,vp,C.c_uint,vp,vp,C.c_float,C.c_void_p]
 worst=0.;counts=[0,0,0,0];rejects=0
 for case in range(20000):
  s=State();s.pose.world[:]=I;s.matrix[:]=I
  s.pose.world[12:15]=[rng.uniform(-300,300) for _ in range(3)]
  s.pose.position[:]=[rng.uniform(-300,300) for _ in range(3)]
  s.yaw=rng.choice([-720,-360,0,360,720,rng.uniform(-1000,1000)])
  s.pitch=rng.uniform(-180,180);s.radius=rng.uniform(-50,250);s.height=rng.uniform(-50,80)
  s.focus[:]=[71,72,73];s.fov=1
  kind=case%4;dt=C.c_float(rng.choice([0,.00001,1/60,.1,.5,1,2])).value
  motion=(C.c_float*2)(rng.uniform(-200,200),rng.uniform(-200,200));buttons=(case//4)%4
  offset=Vec(*(rng.uniform(-1000,1000) for _ in range(3)))
  track=Vec(*(rng.uniform(-200,200) for _ in range(3)));target=Vec(*(rng.uniform(-100,100) for _ in range(3)))
  flow=rng.choice([0x10,0x38,8,0,255])
  wanted=n.run(s,kind,motion,buttons,dt,offset,track,target,flow)
  if kind==0:ok=lib.bk_ending_camera_orbit(C.byref(s),motion,buttons,offset,flow,dt,e)
  elif kind==1:ok=lib.bk_ending_camera_fixed(C.byref(s),offset,e)
  elif kind==2:ok=lib.bk_ending_camera_track_pose(C.byref(s),track,target,dt,e)
  else:ok=lib.bk_ending_camera_manual(C.byref(s),motion,buttons,offset,target,dt,e)
  assert ok,(case,e.value)
  for i,(got,want) in enumerate(zip(values(s),values(wanted))):
   error=abs(got-want)/max(1,abs(want));worst=max(worst,error)
   assert math.isfinite(error) and error<3e-6,(case,kind,i,got,want,error)
  counts[kind]+=1
  if case%100==0:
   before=bytes(s)
   assert not lib.bk_ending_camera_fixed(C.byref(s),Vec(float('nan'),0,0),e)
   assert bytes(s)==before;rejects+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),orbit_steps=counts[0],fixed_steps=counts[1],track_steps=counts[2],manual_steps=counts[3],atomic_rejections=rejects,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-ending-camera-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS ending camera',report,flush=True)
if __name__=='__main__':main()
