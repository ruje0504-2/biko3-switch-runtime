"""Original4ae043 matrix interpolation and complete4e0ecb/4bc444 controllers.
FOV device call observed; native trig, matrix and frame code runs unchanged.
Covers two independent clocks, retained +4a4, all FOV gates, three preset
choices and unused sixth argument of4e0ecb. No actor/event lifecycle implied.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_menu_camera_oracle import Native as Base,State,Vec,I,values
from model_binding import ROOT,library
F=C.c_float;Fp=C.POINTER(F);F16=F*16
class Presets(C.Structure):_fields_=[('active',(F*3)*4),('authored',(F*3)*4)]
class Transitions(C.Structure):
 _fields_=[('preset_progress',F),('zoom_progress',F),('zoom_fov',F)]
class Gate(C.Structure):
 _fields_=[('previous_flow',C.c_uint8)]+[(n,C.c_int32) for n in ['phase','state_ed8','state_ee0','state_ee4','state_eec','state_ef0','state_719b20']]
class Native(Base):
 def run(self,s,t,p,choice,offset,gate,flow,kind,dt,extra):
  u=self.u;u.mem_write(self.camera,bytes(0x600));self.events=[];self.fov=s.fov
  self.wi(0x645600,self.context);self.wi(0x645604,self.frame)
  for a in [self.context,self.frame]:
   for off in [0x80,0xc0,0x100]:self.vf(a+off,I)
  self.vf(self.frame+0x80,s.pose.world);self.vf(self.frame+0xc0,s.pose.world)
  self.vf(self.camera+0x420,s.pose.position)
  self.vf(self.camera+0x42c,[s.yaw,s.pitch,s.radius,s.height]);self.vf(self.camera+0x4a4,s.matrix);self.vf(self.camera+0x5c8,s.focus)
  u.mem_write(self.camera+0x444,bytes(p));self.vf(0x733700,[dt])
  for a,v in zip([0x719c64,0x7099a8,0x7099e4],[t.preset_progress,t.zoom_progress,t.zoom_fov]):self.vf(a,[v])
  u.mem_write(0xbeeb84,bytes([flow]));u.mem_write(0x721ad4,bytes([gate.previous_flow]))
  for a,k in zip([0x721e00,0x721ed8,0x721ee0,0x721ee4,0x721eec,0x721ef0,0x719b20],[f[0] for f in Gate._fields_[1:]]):u.mem_write(a,struct.pack('<i',getattr(gate,k)))
  self.call([0x4e0ecb,0x4bc444][kind],struct.pack('<II3fI',self.camera,choice,*offset,extra))
  done=u.reg_read(UC_X86_REG_EAX)&255
  out=State.from_buffer_copy(s);out.pose.world[:]=self.read(self.frame+0xc0,16);out.pose.position[:]=self.read(self.camera+0x420,3)
  out.matrix[:]=self.read(self.camera+0x4a4,16);out.yaw,out.pitch,out.radius,out.height=self.read(self.camera+0x42c,4)
  out.focus[:]=self.read(self.camera+0x5c8,3);out.fov=self.fov
  assert not self.events,self.events
  assert bytes(u.mem_read(self.camera+0x444,96))==bytes(p)
  nt=Transitions(*(self.read(a,1)[0] for a in [0x719c64,0x7099a8,0x7099e4]))
  return out,nt,done

def bind(lib):
 args=[C.POINTER(State),C.POINTER(Transitions),C.POINTER(Presets),C.c_uint,Fp]
 lib.bk_ending_camera_preset.argtypes=args+[C.POINTER(Gate),F,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_ending_camera_preset_zoom.argtypes=args+[C.c_uint8,F,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_ending_camera_config.argtypes=[C.POINTER(Presets),C.c_uint,C.c_uint]
 lib.bk_camera_blend_matrix.argtypes=[Fp,Fp,Fp,F]
 lib.bk_ending_camera_fixed.argtypes=[C.POINTER(State),Fp,C.c_void_p]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x4e0ecb);e=C.create_string_buffer(256)
 worst=0.;completions=[0,0];fov_changes=[0,0]
 def equal(got,want,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(got,want)):
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d)
   assert math.isfinite(d) and d<3e-6,(label,i,g,w,d)
 def matrix(case):
  s=State();s.yaw=rng.uniform(-720,720);s.pitch=rng.choice([-90,90,rng.uniform(-180,180)]);s.radius=rng.uniform(-100,200);s.height=rng.uniform(-40,80)
  assert lib.bk_ending_camera_fixed(C.byref(s),Vec(2,3,4),e)
  # Actual valid rotation plus clamped/nonunit matrix cases.
  if case%5==0:s.matrix[9]=rng.choice([-2,-1,0,1,2])
  return s.matrix
 for case in range(4000):
  target=matrix(case);previous=matrix(case+1);weight=F(rng.choice([-2,-.1,0,.5,1,2,rng.random()])).value
  address=0x3006000;n.call(0x4ae043,struct.pack('<I',address)+bytes(target)+bytes(previous)+struct.pack('<f',weight))
  wanted=n.read(address,16);out=F16(*previous)
  assert lib.bk_camera_blend_matrix(out,target,out,weight)
  equal(out,wanted,('matrix',case))
 for case in range(12000):
  # Keep multiple calls sequential to catch accidentally replacing the source.
  if case%12==0:
   s=State();s.matrix[:]=matrix(case);s.pose.world[:]=matrix(case+1);s.pose.position[:]=[8,9,10];s.yaw=93;s.pitch=31;s.radius=27;s.height=-14;s.focus[:]=[71,72,73];s.fov=.77
   t=Transitions(rng.choice([-1,0,.99,1,2]),rng.choice([-.5,0,.99,1,2]),rng.choice([.2,.4,.40001,1,2]))
  p=Presets();assert lib.bk_ending_camera_config(C.byref(p),case%5,(case//5)%10)
  if case%7==0:p.active[0][case%3]+=45
  choice=case%3;kind=case%2;dt=F(rng.choice([0,.000001,.016,.1,.5,1,2])).value;flow=rng.choice([0x10,0,0x18,0x38,255])
  gate=Gate(rng.choice([0x18,0,8,16,255]),rng.choice([1,8,9]),rng.choice([0,2,3,4,5]),rng.choice([0,4]),rng.choice([0,4]),rng.choice([0,4]),rng.choice([0,4]),rng.choice([0,1]))
  offset=Vec(*(rng.uniform(-200,200) for _ in range(3)))
  wanted,nt,done=n.run(s,t,p,choice,offset,gate,flow,kind,dt,rng.getrandbits(32))
  previous_fov=s.fov;completed=C.c_int(-1)
  fn=lib.bk_ending_camera_preset_zoom if kind else lib.bk_ending_camera_preset
  assert fn(C.byref(s),C.byref(t),C.byref(p),choice,offset,flow if kind else C.byref(gate),dt,C.byref(completed),e),e.value
  equal(values(s),values(wanted),('controller',case));equal([t.preset_progress,t.zoom_progress,t.zoom_fov],[nt.preset_progress,nt.zoom_progress,nt.zoom_fov],('clocks',case))
  assert completed.value==done;completions[kind]+=done;fov_changes[kind]+=s.fov!=previous_fov
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),matrix_blends=4000,preset_frames=6000,zoom_frames=6000,completions=completions,fov_changes=fov_changes,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-ending-preset-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
