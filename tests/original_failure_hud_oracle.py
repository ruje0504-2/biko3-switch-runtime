"""Original51AFCB failure HUD, all fade/request/reset and51C47E instructions.
Boundaries: decoded confirm keys, sound, message/font binding, weather/text draw
and actual scene release. No decision, sprite transition or flow hook.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_common_hud_oracle import State as Common, Flow, FlowOps, Release
from original_item_notice_oracle import Fade
from original_text_flow_oracle import Flow as Text
from model_binding import ROOT, library
class State(C.Structure):
 _fields_=[('advances',C.c_uint32),('visible',C.c_uint8)]
class Bindings(C.Structure):
 _fields_=[('common',C.POINTER(Common)),('panel',C.POINTER(Fade)),('prompt',C.POINTER(Fade)),('text',C.POINTER(Text)),('group',C.POINTER(C.c_uint32))]+[(x,C.POINTER(C.c_uint8)) for x in ['outcome','overlay','visible']]+[(x,C.POINTER(C.c_int8)) for x in ['script','mode','stimulus']]+[('behavior',C.POINTER(C.c_int32)),('duration',C.POINTER(C.c_uint32))]
Op=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Message=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.c_void_p)
Schedule=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_uint8,C.c_void_p)
class Ops(C.Structure):
 _fields_=[('context',C.c_void_p),('click',Op),('message',Message),('release',Release),('schedule',Schedule)]
class Frame(C.Structure):
 _fields_=[('panel',C.c_float),('prompt',C.c_float),('curtain',C.c_float),('text',C.c_int)]
ADDR=[0xbeeb88,0xbef608,0xbeea18]
VALUES=[(0x7219a8,'I'),(0x71bcd8,'B'),(0x725d38,'B'),(0x729100,'B'),(0x71bd08,'b'),(0x71bcdb,'b'),(0x729101,'b'),(0x729638,'i'),(0x72963c,'I')]
def fs(f):return f.alpha,f.speed,f.stage
def snapshot(s,common,panel,prompt,text,values,flow):
 return s.advances,s.visible,fs(common.curtain),common.blocked,fs(panel),fs(prompt),bytes(text),tuple(v.value for v in values),bytes(flow)
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  self.u.mem_write(0xbef290,struct.pack('<I',0x1234))
  for a in [0x4b76c2,0x46435e,0x51876a,0x4aa9f4,0x4fac10,0x4aaa17,0x4e77bf,0x51b120,0x51b157,0x51b19a]:
   self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def read_float(self,a):return struct.unpack('<f',self.u.mem_read(a,4))[0]
 def hook(self,u,a,size,_):
  if a in [0x51b120,0x51b157,0x51b19a]:
   i=[0x51b120,0x51b157,0x51b19a].index(a);self.alphas[i]=self.read_float(ADDR[i]+0x12c);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<3I',u.mem_read(sp+4,12));result=0
  if a==0x4b76c2:
   assert args[0] in [0,0x5a,0x33450,0x33451,0x33452] and args[1:]==(1,0)
   result=int(self.advance)
  elif a==0x46435e:
   assert args[:2]==(0x1234,0);self.trace.append(('click',))
  elif a==0x51876a:
   assert args[1]==0xb54760;self.trace.append(('message',args[0]))
  elif a==0x4aa9f4:
   assert args[0]==0xb54878;self.trace.append(('bind',))
  elif a==0x4aaa17:self.draw_text=1
  elif a==0x4e77bf:self.trace.append(('release',args[0]&255))
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,common,panel,prompt,text,values,flow,advance,seconds):
  u=self.u;self.trace=[];self.alphas=[None]*3;self.advance=advance;self.draw_text=0
  for a,f in zip(ADDR,[panel,prompt,common.curtain]):
   raw=bytearray(0x16c);struct.pack_into('<f',raw,0x12c,f.alpha);struct.pack_into('<f',raw,0x138,f.speed);raw[0x134]=f.stage;raw[0x148:0x14b]=bytes([1,1,0]);u.mem_write(a,bytes(raw))
  u.mem_write(0xbeeb7f,bytes([common.blocked]));u.mem_write(0xbfbbb4,struct.pack('<I',s.advances));u.mem_write(0xbf9b98,bytes([s.visible]));u.mem_write(0x6a13d0,bytes(text));u.mem_write(0x733700,struct.pack('<f',seconds))
  for (a,fmt),v in zip(VALUES,values):u.mem_write(a,struct.pack('<'+fmt,v.value))
  for a,v in zip([0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c],bytes(flow)):u.mem_write(a,bytes([v]))
  self.call(0x51afcb,b'')
  out=State(struct.unpack('<I',u.mem_read(0xbfbbb4,4))[0],u.mem_read(0xbf9b98,1)[0]);c=Common.from_buffer_copy(common)
  sprites=[Fade(self.read_float(a+0x12c),self.read_float(a+0x138),u.mem_read(a+0x134,1)[0]) for a in ADDR]
  c.curtain=sprites[2];c.blocked=u.mem_read(0xbeeb7f,1)[0]
  vs=[type(v)(struct.unpack('<'+fmt,u.mem_read(a,struct.calcsize(fmt)))[0]) for (a,fmt),v in zip(VALUES,values)]
  ft=Flow(*(u.mem_read(a,1)[0] for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]))
  t=Text.from_buffer_copy(u.mem_read(0x6a13d0,C.sizeof(Text)))
  return snapshot(out,c,sprites[0],sprites[1],t,vs,ft),tuple(self.alphas)+(self.draw_text,),self.trace

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x51afcb);error=C.create_string_buffer(256);count=messages=releases=0
 lib.bk_failure_hud_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Ops),C.c_int,C.c_float,C.POINTER(Frame),C.c_void_p]
 lib.bk_flow_transition_schedule.argtypes=[C.POINTER(Flow),C.POINTER(FlowOps),C.c_uint8,C.c_uint8,C.c_void_p]
 for case in range(12000):
  def fade():return Fade(rng.choice([0,.1,.75,1]),rng.choice([0,.1,2,10]),rng.randrange(6))
  s=State(rng.choice([0,0,1,2,0x7fffffff,0xffffffff]),rng.choice([0,1,2,255]));c=Common();c.curtain=fade();c.blocked=rng.choice([0,1,2,255]);panel,prompt=fade(),fade();t=Text(12,15,40,rng.randrange(-1,3),rng.randrange(-1,3));values=[C.c_uint32(case%5)]+[C.c_uint8(rng.choice([0,1,2,3,4,5,6,6,255]))]+[C.c_uint8(rng.randrange(256)) for _ in range(2)]+[C.c_int8(rng.randrange(-128,128)) for _ in range(3)]+[C.c_int32(rng.randrange(-2,20)),C.c_uint32(rng.getrandbits(32))];flow=Flow(0x40,2,0,rng.randrange(256));advance=rng.randrange(2);seconds=C.c_float(rng.choice([0,1/60,.1,1,10])).value
  expected,draws,events=n.run(s,c,panel,prompt,t,values,flow,advance,seconds);trace=[]
  @Op
  def click(_,e):trace.append(('click',));return 1
  @Message
  def message(_,label,e):trace.extend([('message',label),('bind',)]);return 1
  @Release
  def release(_,target,e):trace.append(('release',target));return 1
  release_ops=FlowOps(None,release)
  @Schedule
  def schedule(_,target,mode,e):return lib.bk_flow_transition_schedule(C.byref(flow),C.byref(release_ops),target,mode,e)
  ops=Ops(None,click,message,release,schedule);bindings=Bindings(C.pointer(c),C.pointer(panel),C.pointer(prompt),C.pointer(t),*(C.pointer(v) for v in values));frame=Frame()
  assert lib.bk_failure_hud_step(C.byref(s),C.byref(bindings),C.byref(ops),advance,seconds,C.byref(frame),error),error.value
  actual=snapshot(s,c,panel,prompt,t,values,flow)
  assert actual==expected,(case,actual,expected)
  assert (frame.panel,frame.prompt,frame.curtain,frame.text)==draws,(case,'draw',draws,frame.text)
  assert trace==events,(case,trace,events)
  count+=1;messages+=sum(x[0]=='message' for x in trace);releases+=sum(x[0]=='release' for x in trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=count,message_bindings=messages,release_calls=releases,scope=__doc__)
 (ROOT/'local/original-failure-hud-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS failure HUD',count,'frames',messages,'messages',releases,'releases')
if __name__=='__main__':main()
