"""Original4d9354/4d9575 and4d679d..4d6e6e resource/curtain transitions.
Nested selectors and45-effect loops execute native instructions unchanged.
Only actual IO/scene/light/scheduler/DirectSound leaves are observed services.
Full4d499b UI and real resource lifecycles are not claimed by this oracle.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame,FIELDS,BYTES
from original_ending_control_oracle import State as Control,STATE_ADDR,FLAG_ADDR
from original_ending_auxiliary_oracle import State as Auxiliary,ADDR
from model_binding import ROOT,library
class State(C.Structure):
 _fields_=[('frame',Frame),('control',Control),('auxiliary',Auxiliary),('previous',C.c_int8),('selected',C.c_int32),('next_mode',C.c_int32),('saved',C.c_uint8*4)]
class Bindings(C.Structure):
 _fields_=[('frame',C.POINTER(Frame)),('control',C.POINTER(Control)),('auxiliary',C.POINTER(Auxiliary)),('previous',C.POINTER(C.c_int8)),('action',C.POINTER(C.c_uint8)),('wanted',C.POINTER(C.c_uint8)),('selected',C.POINTER(C.c_int32)),('next_mode',C.POINTER(C.c_int32)),('saved',C.POINTER(C.c_uint8))]
Load=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_int32,C.c_void_p)
One=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_void_p)
Image=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_uint,C.c_void_p)
Schedule=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_uint8,C.c_void_p)
Query=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
Pause=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
class Ops(C.Structure):
 _fields_=[('context',C.c_void_p),('load',Load),('release',One),('image',Image),('leave',One),('light',One),('schedule',Schedule),('present',Query),('status',Query),('pause',Pause)]
REGIONS=[]
def region(parent,typ,name,address,size):REGIONS.append((getattr(State,parent).offset+getattr(typ,name).offset,address,size))
for name,a in FIELDS:region('frame',Frame,name,a,4)
for name,a in BYTES:region('frame',Frame,name,a,1)
region('frame',Frame,'camera_values',0x721e14,12);region('frame',Frame,'camera_table',0x709fcc,80)
for (name,_),a in zip(Control._fields_[:7],STATE_ADDR):region('control',Control,name,a,4)
for name,a,size in [('targets',0x70c8d8,36),('saved_camera',0x71b41c,64),('variant',0x721b3d,1),('toggles',0x7220f8,8),('pause_selection',0xbeeb7d,1)]:region('control',Control,name,a,size)
for i,a in enumerate(FLAG_ADDR):REGIONS.append((State.control.offset+Control.pause_flags.offset+i,a,1))
for (name,_),a in zip(Auxiliary._fields_,ADDR):region('auxiliary',Auxiliary,name,a,4)
for name,a,size in [('previous',0x721ad4,1),('selected',0x721ed8,4),('next_mode',0x719b20,4),('saved',0x70c8d0,4)]:REGIONS.append((getattr(State,name).offset,a,size))
LOAD=[0x4cf318,0x4d00fa,0x4d1025,0x4d2320,0x4d39e6]
RELEASE=[0x4cfede,0x4d0dca,0x4d20ab,0x4d3753,0x4d43f5]
LEAVE=[0x4e1c8f,0x47dbcc,0x49739a,0x47a033,0x482f91,0x48d7f2]
LIGHT=[0x4a4140,0x4a435a,0x4a4438]
def ptr(obj,offset,typ):return C.cast(C.byref(obj,offset),C.POINTER(typ))
def bindings(s):
 return Bindings(C.pointer(s.frame),C.pointer(s.control),C.pointer(s.auxiliary),ptr(s,State.previous.offset,C.c_int8),ptr(s,State.frame.offset+Frame.transition_action.offset,C.c_uint8),ptr(s,State.frame.offset+Frame.curtain_wanted.offset,C.c_uint8),ptr(s,State.selected.offset,C.c_int32),ptr(s,State.next_mode.offset,C.c_int32),ptr(s,State.saved.offset,C.c_uint8))
class Native(Base):
 buffers,vtable,stub=0x3005000,0x3006000,0x3007000
 def __init__(self,exe):
  super().__init__(exe)
  self.word(self.vtable+0x24,self.stub)
  for i in range(48):self.word(self.buffers+i*32,self.vtable)
  for a in [*LOAD,*RELEASE,*LEAVE,*LIGHT,0x51c47e,0x50de40,0x50e633,0x50e7c1,0x4ad8ec,0x4ad34a,self.stub,0x4d959c,0x4d6b84,0x4d6e6e,0x4d7432]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def string(self,a):return bytes(self.u.mem_read(a,260)).split(b'\0')[0]
 def write(self,s):
  raw=bytes(s)
  for off,a,size in REGIONS:self.u.mem_write(a,raw[off:off+size])
 def read(self):
  s=State.from_buffer_copy(self.initial)
  for off,a,size in REGIONS:C.memmove(C.addressof(s)+off,bytes(self.u.mem_read(a,size)),size)
  return s
 def event(self,event):
  self.trace.append((event,bytes(self.read())))
  if len(self.trace)==self.change_at:
   s=self.read();mutate(s,self.change);self.write(s)
 def hook(self,u,a,size,_):
  if a in [0x4d6e6e,0x4d7432]:self.early=int(a==0x4d7432);u.emu_stop();return
  if a in [0x4d959c,0x4d6b84]:
   slot=2+u.reg_read(UC_X86_REG_ECX)//0x120 if a==0x4d959c else 0
   self.event(('present',slot,self.present[slot]));return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];pop=4;result=0
  def words(n):return struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4))
  if a in LOAD:
   kind=LOAD.index(a);arg=C.c_int32(words(1)[0]).value if kind==2 else -1;self.event(('load',kind,arg))
  elif a in RELEASE:self.event(('release',RELEASE.index(a)))
  elif a in LEAVE:self.event(('leave',LEAVE.index(a)))
  elif a in LIGHT:
   if a==LIGHT[1]:assert self.string(words(1)[0])==b'BK3_L'
   self.event(('light',LIGHT.index(a)))
  elif a==0x51c47e:self.event(('schedule',*words(2)))
  elif a==0x4ad8ec:
   dst,path,name=words(3);assert name==0;u.mem_write(dst,self.string(path)+b'\0')
  elif a==0x50de40:
   obj,path,name,x,y,w,h,en,ex,idle=words(10);g=u.mem_read(0x721b3c,1)[0]
   assert obj==0x73a990 and self.string(path)==b'\\bk3_00.pp' and self.string(name)==f'g0{g+1}_20.bmp'.encode()
   assert (x,y,en,ex,idle)==(0,0,1,1,0)
   assert (w,h)==struct.unpack('<2I',struct.pack('<2f',1280*self.scale,960*self.scale))
   self.image_shapes+=1;self.event(('image',1,g))
  elif a==0x50e633:assert words(2)==(0x73a990,1)
  elif a==0x50e7c1:assert words(1)==(0x73a990,);self.event(('image',0,0))
  elif a==0x4ad34a:
   slot=(words(1)[0]-self.buffers)//32;self.event(('pause',slot));self.flags[slot]=0
  else:
   obj,out=words(2);slot=(obj-self.buffers)//32;result=self.hr[slot];self.word(out,self.flags[slot]);self.event(('status',slot,int(not result and bool(self.flags[slot]&1))));pop=12
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,mode,present,flags,hr,scale,change_at,change):
  self.initial=bytes(s);self.write(s);self.present=present;self.flags=list(flags);self.hr=hr;self.scale=scale;self.change_at=change_at;self.change=change;self.trace=[];self.early=0
  self.u.mem_write(0x5767c8,b'\1');self.u.mem_write(0x721ad0,struct.pack('<f',scale))
  for i in range(48):self.word(0x722334+i*0x120 if i<47 else 0x722214,self.buffers+i*32 if present[i] else 0)
  if mode<2:self.call([0x4d9354,0x4d9575][mode],b'')
  else:
   self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x400);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
   self.u.emu_start(0x4d679d,self.stop,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP) in [0x4d6e6e,0x4d7432]
  return self.trace,bytes(self.read()),self.early

def mutate(s,v):
 s.frame.phase,s.frame.group,s.frame.state_721ee0,s.frame.state_721ee4,s.auxiliary.variant,s.auxiliary.selection,s.previous,s.selected,s.next_mode,s.frame.transition_action=v[:10]
 s.control.variant=v[10];s.control.toggles[:]=v[11:19];s.saved[:]=v[19:23]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);n.image_shapes=0;lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4d9354)
 for name in ['load','release']:getattr(lib,'bk_ending_reload_'+name).argtypes=[C.POINTER(Bindings),C.POINTER(Ops),C.c_void_p]
 lib.bk_ending_reload_transition.argtypes=[C.POINTER(Bindings),C.c_uint8,C.POINTER(Ops),C.POINTER(C.c_int),C.c_void_p]
 lib.bk_ending_final_image.argtypes=[C.c_uint];lib.bk_ending_final_image.restype=C.c_char_p
 actions=[7,6,63,8,74,49,45,47,0,255];calls=0;seen=set();early_count=0;changed=0
 for case in range(14000):
  mode=0 if case<2000 else 1 if case<4000 else 2
  s=State();s.frame.phase=rng.choice([-1,0,1,2,3,4,5,6,7,8,8,9,0x7fffffff]);s.frame.group=case%5;s.frame.state_721ee0=rng.randrange(-9,10);s.frame.state_721ee4=rng.choice([0,1000,1500,999])
  s.frame.transition_action=actions[case%10];s.frame.curtain_wanted=1 if case%7 else rng.choice([0,2,255]);s.frame.finish_blocked=3 if case%11 else rng.choice([0,1,2,4,255])
  s.auxiliary.variant=rng.choice([0,0,1,2,-1]);s.auxiliary.selection=rng.randrange(-2,8);s.previous=rng.choice([8,24,1,-1]);s.selected=-71;s.next_mode=rng.choice([0,0,1,-1,123]);s.control.variant=73
  s.control.toggles[:]=rng.randbytes(8);s.saved[:]=rng.randbytes(4)
  present=[int(rng.randrange(5)!=0) for _ in range(48)];flags=[rng.randrange(4) for _ in range(48)];hr=[int(rng.randrange(11)==0) for _ in range(48)]
  change=[rng.randrange(1,10),rng.randrange(5),rng.randrange(-9,10),rng.choice([0,1000,1500]),rng.randrange(3),rng.randrange(8),rng.choice([8,24,-1]),rng.randrange(8),rng.choice([0,1,-1]),rng.choice(actions),rng.randrange(256),*rng.randbytes(8),*rng.randbytes(4)]
  change_at=rng.randrange(1,100) if case%3==0 else 0
  expected=n.run(s,mode,present,flags,hr,rng.choice([.5,.75,1,1.5]),change_at,change);trace=[];pf=list(flags)
  def event(ev):
   trace.append((ev,bytes(s)))
   if len(trace)==change_at:mutate(s,change)
   return 1
  @Load
  def load(_,kind,arg,err):return event(('load',kind,arg))
  @One
  def release(_,kind,err):return event(('release',kind))
  @Image
  def image(_,create,group,err):return event(('image',create,group))
  @One
  def leave(_,kind,err):return event(('leave',kind))
  @One
  def light(_,kind,err):return event(('light',kind))
  @Schedule
  def schedule(_,target,mode,err):return event(('schedule',target,mode))
  @Query
  def pres(_,slot,out,err):out[0]=present[slot];return event(('present',slot,out[0]))
  @Query
  def status(_,slot,out,err):out[0]=int(not hr[slot] and bool(pf[slot]&1));return event(('status',slot,out[0]))
  @Pause
  def pause(_,slot,err):event(('pause',slot));pf[slot]=0;return 1
  ops=Ops(None,load,release,image,leave,light,schedule,pres,status,pause);b=bindings(s);early=C.c_int(0)
  if mode<2:ok=getattr(lib,'bk_ending_reload_'+['load','release'][mode])(C.byref(b),C.byref(ops),e)
  else:ok=lib.bk_ending_reload_transition(C.byref(b),s.frame.finish_blocked,C.byref(ops),C.byref(early),e)
  assert ok,(case,e.value)
  actual=(trace,bytes(s),early.value)
  if actual!=expected:
   for i,(x,y) in enumerate(zip(trace,expected[0])):
    if x!=y:print('DIFF',case,mode,i,x[0],y[0],[(j,a,b) for j,(a,b) in enumerate(zip(x[1],y[1])) if a!=b]);break
   raise AssertionError((case,[x[0] for x in trace],[x[0] for x in expected[0]],[(j,a,b) for j,(a,b) in enumerate(zip(bytes(s),expected[1])) if a!=b],early.value,expected[2]))
  calls+=len(trace);early_count+=early.value;changed+=change_at>0 and len(trace)>=change_at;seen.update(x[0] for x in trace)
  if case%2000==1999:print('PASS reload cases',case+1,'calls',calls,flush=True)
 for g in range(5):assert lib.bk_ending_final_image(g)==f'g0{g+1}_20.bmp'.encode()
 assert lib.bk_ending_final_image(5) is None and lib.bk_ending_final_image(0xffffffff) is None
 for kind in range(5):assert any(x[:2]==('load',kind) for x in seen) and ('release',kind) in seen
 for kind in range(6):assert ('leave',kind) in seen
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),load_cases=2000,release_cases=2000,curtain_cases=10000,services=calls,mutated_service_states=changed,final_image_constructions=n.image_shapes,early_returns=early_count,exact=True,scope=__doc__)
 (ROOT/'local/original-ending-reload-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
