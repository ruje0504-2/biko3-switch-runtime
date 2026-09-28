"""Complete original4d7ac4..4d901b control,13 independent hit regions.
Key,sound,warp and actual495d92 transition are required observed services.
Compare snapshots at every call, including mutable auxiliary state. All
camera/target/flag/control arithmetic executes original instructions.
Rectangles and old actor node positions are explicit fixtures, not full UI.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame,Input,FIELDS,BYTES
from original_menu_camera_oracle import State as Camera,I
from original_ending_preset_oracle import Presets
from model_binding import ROOT,library
F=C.c_float;Fp=C.POINTER(F)
class State(C.Structure):
 _fields_=[(n,C.c_int32) for n in ['hover','hover_armed','previous_hover','mode_721ec4','target_choice','previous_phase','state_721eec']]+[('targets',(F*3)*3),('saved_camera',F*16),('variant',C.c_uint8),('toggles',C.c_uint8*8),('pause_selection',C.c_uint8),('pause_flags',C.c_uint8*6)]
class Rect(C.Structure):_fields_=[(n,F) for n in ['x','y','width','height']]
class Bindings(C.Structure):
 _fields_=[('frame',C.POINTER(Frame)),('camera',C.POINTER(Camera)),('presets',C.POINTER(Presets)),('nodes',Fp*4),('rects',C.POINTER(Rect)),('scale',Fp),('volume',C.POINTER(C.c_int32))]
Key=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint,C.POINTER(C.c_uint32),C.c_void_p)
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int32,C.c_void_p)
Aux=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int32,C.POINTER(C.c_int32),C.c_void_p)
Warp=C.CFUNCTYPE(C.c_int,C.c_void_p,F,F,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('key',Key),('sound',Sound),('auxiliary',Aux),('warp',Warp)]
STATE_ADDR=[0x70c8bc,0x709f18,0x709f20,0x721ec4,0x721ecc,0x719b1c,0x721eec]
RECT_ADDR=[0x73527c,0x736aa8,0x737774,0x735f48,0x735998,0x735c70,0x736220,0x7364f8,0x7367d0,0x73749c,0x7371c4,0x738168,0x738440]
FLAG_ADDR=[0x7392cb,0x739437,0x7395a3,0x73970f,0x73987b,0x7399e7]
NODE_ADDR=[0x721ef4,0x719b40,0x721f08,0x721f28]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4b76c2,0x4ad2bf,0x495d92,0x4b768d]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def read_i(self,a):return struct.unpack('<i',self.u.mem_read(a,4))[0]
 def vector(self,a,v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def read_f(self,a,n):return struct.unpack('<'+'f'*n,self.u.mem_read(a,n*4))
 def write(self,s,f,c,p):
  for (name,_),a in zip(State._fields_[:7],STATE_ADDR):self.word(a,getattr(s,name))
  for name,a in FIELDS:self.word(a,getattr(f,name))
  for name,a in BYTES:self.u.mem_write(a,bytes([getattr(f,name)]))
  self.u.mem_write(0x721e14,bytes(f.camera_values));self.u.mem_write(0x709fcc,bytes(f.camera_table))
  self.u.mem_write(0x70c8d8,bytes(s.targets));self.u.mem_write(0x71b41c,bytes(s.saved_camera));self.u.mem_write(0x721b3d,bytes([s.variant]));self.u.mem_write(0x7220f8,bytes(s.toggles));self.u.mem_write(0xbeeb7d,bytes([s.pause_selection]))
  for a,v in zip(FLAG_ADDR,s.pause_flags):self.u.mem_write(a,bytes([v]))
  self.vector(0x71b364,[c.yaw,c.pitch,c.radius,c.height]);self.u.mem_write(0x71b37c,bytes(p));self.u.mem_write(0x71b3dc,bytes(c.matrix))
 def snapshot(self):
  s=State();f=Frame();c=Camera.from_buffer_copy(self.original_camera)
  for (name,_),a in zip(State._fields_[:7],STATE_ADDR):setattr(s,name,self.read_i(a))
  for name,a in FIELDS:setattr(f,name,self.read_i(a))
  for name,a in BYTES:setattr(f,name,self.u.mem_read(a,1)[0])
  for target,field,a,n in [(f,'camera_values',0x721e14,12),(f,'camera_table',0x709fcc,80),(s,'targets',0x70c8d8,36),(s,'saved_camera',0x71b41c,64),(s,'toggles',0x7220f8,8)]:
   C.memmove(C.addressof(target)+getattr(type(target),field).offset,bytes(self.u.mem_read(a,n)),n)
  s.variant=self.u.mem_read(0x721b3d,1)[0];s.pause_selection=self.u.mem_read(0xbeeb7d,1)[0]
  s.pause_flags[:]=[self.u.mem_read(a,1)[0] for a in FLAG_ADDR]
  c.yaw,c.pitch,c.radius,c.height=self.read_f(0x71b364,4);c.matrix[:]=self.read_f(0x71b3dc,16)
  return bytes(s),bytes(f),bytes(c),bytes(self.u.mem_read(0x71b37c,96))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,first,second,third=struct.unpack('<4I',u.mem_read(sp,16));result=0
  if a==0x4b76c2:
   assert third==0;result=self.keys[self.key_index];self.key_index+=1
   event=('key',first,second,result)
  elif a==0x4ad2bf:
   assert second==0;event=('sound',first-100,C.c_int32(third).value)
  elif a==0x4b768d:event=('warp',*struct.unpack('<2f',u.mem_read(sp+4,8)))
  else:event=('auxiliary',C.c_int32(first).value);result=self.aux_result
  self.trace.append((event,self.snapshot()))
  if a==0x495d92 and self.change is not None:
   self.word(0x721ec8,self.change[0]);self.word(0x721e00,self.change[1])
  u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,f,c,p,rects,nodes,inp,scale,volume,keys,aux_result,change):
  self.original_camera=bytes(c);self.write(s,f,c,p);self.keys=keys;self.key_index=0;self.aux_result=aux_result;self.change=change;self.trace=[]
  for a,r in zip(RECT_ADDR,rects):self.vector(a-8,[r.width,r.height,r.x,r.y])
  for i,(a,node) in enumerate(zip(NODE_ADDR,nodes)):
   ptr=0x3001000+i*0x400;self.word(a,ptr);self.vector(ptr+0xf0,node)
  for i in range(8):self.word(0xbeee10+i*0x120,100+i)
  self.vector(0x721ad0,[scale.value]);self.word(0xbe9a10,volume.value)
  self.call(0x4d7ac4,bytes(inp));return self.trace,self.snapshot()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4d7ac4)
 lib.bk_ending_control_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.c_void_p]
 total=aux_calls=warp_calls=0;sounds=[0]*8;hits=[0]*13;worst=0.
 for case in range(24000):
  s=State();f=Frame();c=Camera();p=Presets();c.pose.world[:]=c.matrix[:]=I;c.matrix[12:15]=[11,12,13];c.focus[:]=[77,78,79];c.fov=.7
  c.yaw,c.pitch,c.radius,c.height=[rng.uniform(-360,360) for _ in range(4)]
  for bank in [p.active,p.authored]:
   for row in bank:row[:]=[rng.uniform(-200,200) for _ in range(3)]
  s.variant=rng.choice([0,1,2,255]);s.toggles[:]=[rng.choice([0,1,2,255]) for _ in range(8)]
  s.hover=rng.randrange(48);s.hover_armed=rng.choice([0,1,2,-1]);s.previous_hover=rng.randrange(48)
  s.mode_721ec4=rng.choice([0,1,2,-1,-2,0x7fffffff,-0x80000000]);s.target_choice=rng.randrange(3);s.previous_phase=-7;s.state_721eec=rng.choice([0,4,7]);s.pause_selection=31;s.pause_flags[:]=[rng.randrange(256) for _ in range(6)]
  for row in s.targets:row[:]=[rng.uniform(-200,200) for _ in range(3)]
  s.saved_camera[:]=[rng.uniform(-3,3) for _ in range(16)]
  f.phase=rng.choice([1,2,5,6,7,8,9]);f.camera_mode=rng.choice([0,1,2,3,4,-1]);f.camera_clip=case%3;f.auxiliary_mode=rng.choice([0,1,2,3,-1,-2,0x7fffffff,-0x80000000]);f.camera_cached=99;f.state_721ee4=rng.choice([0,3,7]);f.group=case%5
  f.camera_values[:]=[rng.getrandbits(32) for _ in range(3)]
  nodes=[(F*3)(*[rng.uniform(-1000,1000) for _ in range(3)]) for _ in range(4)]
  rects=(Rect*13)(*[Rect(i*20,10,18,16) for i in range(13)]);selected=case%13
  x,y=selected*20+rng.choice([-1,0,1,18,19]),rng.choice([9,10,15,26,27])
  if case%5==0:
   x,y=10,10
   for i,r in enumerate(rects):r.x=r.y=rng.choice([-10,0,10,11]);r.width=r.height=rng.choice([0,10,20,30])
  if case%29==0:
   # Sum must remain double: x0+float width rounds across int32 boundary.
   x=16777217;y=10;rects[selected]=Rect(16777216,0,1,10)
  for i,r in enumerate(rects):hits[i]+=r.x<=x<=float(r.x)+r.width and r.y<=y<=float(r.y)+r.height
  inp=Input();inp.words[:]=[rng.getrandbits(32) for _ in range(11)];inp.words[9]=x;inp.words[10]=y
  keys=[rng.choice([0,0,0,1,255,256,0x80000000,0xffffffff]) for _ in range(128)]
  aux_result=rng.choice([0,1,256,-1]);change=(rng.choice([0,1,2,3,-1,0x7fffffff]),rng.choice([1,5,6,8,9])) if case%3==0 else None
  scale=F(rng.choice([0,.5,.75,1,1.3333333]));volume=C.c_int32(rng.choice([0,-700,-3000,-10000]));b=Bindings(C.pointer(f),C.pointer(c),C.pointer(p),(Fp*4)(*nodes),rects,C.pointer(scale),C.pointer(volume))
  expected,final=n.run(s,f,c,p,rects,nodes,inp,scale,volume,keys,aux_result,change);trace=[];index=[0]
  def snap():return bytes(s),bytes(f),bytes(c),bytes(p)
  @Key
  def key(_,code,mode,result,err):
   result[0]=keys[index[0]];index[0]+=1;trace.append((('key',code,mode,result[0]),snap()));return 1
  @Sound
  def sound(_,slot,vol,err):trace.append((('sound',slot,vol),snap()));return 1
  @Aux
  def aux(_,proposed,result,err):
   trace.append((('auxiliary',proposed),snap()))
   if change is not None:f.auxiliary_mode,f.phase=change
   result[0]=aux_result;return 1
  @Warp
  def warp(_,px,py,err):trace.append((('warp',px,py),snap()));return 1
  ops=Ops(None,key,sound,aux,warp)
  assert lib.bk_ending_control_step(C.byref(s),C.byref(b),C.byref(inp),C.byref(ops),e),(case,e.value)
  if trace!=expected or snap()!=final:
   for idx,(got,want) in enumerate(zip(trace+[(None,snap())],expected+[(None,final)])):
    if got!=want:
     offsets=[(part,[(j,a,z) for j,(a,z) in enumerate(zip(got[1][part],want[1][part])) if a!=z][:16]) for part in range(4)]
     raise AssertionError((case,idx,got[0],want[0],offsets))
   raise AssertionError((case,len(trace),len(expected)))
  for ev,_ in trace:
   if ev[0]=='sound':sounds[ev[1]]+=1
   aux_calls+=ev[0]=='auxiliary';warp_calls+=ev[0]=='warp'
  total+=len(trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=24000,service_calls=total,sounds=sounds,auxiliary_calls=aux_calls,warp_calls=warp_calls,region_hits=hits,byte_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-control-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
