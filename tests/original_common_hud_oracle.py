"""51a190 tail: actual fade/timer/outcome dispatch,4c2609,4f75dd,51c47e.
Only time, DirectSound Play, current-scene release and pause load are boundaries.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class Timer(C.Structure):_fields_=[('duration',C.c_uint32),('deadline',C.c_uint32),('armed',C.c_uint8)]
class State(C.Structure):_fields_=[('curtain',Fade),('wait',Timer),('gate',C.c_uint8),('action',C.c_uint8),('blocked',C.c_uint8)]
class Flow(C.Structure):_fields_=[('current',C.c_uint8),('previous',C.c_uint8),('target',C.c_uint8),('mode',C.c_uint8)]
class Bindings(C.Structure):_fields_=[('outcome',C.POINTER(C.c_uint8)),('menu',C.POINTER(C.c_uint8)),('response',C.POINTER(C.c_uint8)),('pause',C.POINTER(C.c_uint8)),('group',C.POINTER(C.c_uint32)),('area',C.POINTER(C.c_uint32)),('flow',C.POINTER(C.c_uint8)),('special',C.c_uint8)]
Op=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Release=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_void_p)
Schedule=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_uint8,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('outcome',Op),('response',Op),('schedule',Schedule),('pause',Op)]
class FlowOps(C.Structure):_fields_=[('context',C.c_void_p),('release',Release)]
class Frame(C.Structure):_fields_=[('alpha',C.c_float)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  self.u.mem_write(0x53f10c,struct.pack('<I',0x300e000))
  for ptr,handle,vtable,fn in [(0xb537d8,0x3006000,0x3006100,0x300e100),(0xb53558,0x3006200,0x3006300,0x300e200)]:
   self.u.mem_write(ptr,struct.pack('<I',handle));self.u.mem_write(handle,struct.pack('<I',vtable));self.u.mem_write(vtable+0x30,struct.pack('<I',fn))
  for a in [0x51a196,0x51a3c8,0x300e000,0x300e100,0x300e200,0x4e77bf,0x4e7671]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def hook(self,u,addr,size,_):
  if addr==0x51a196:u.reg_write(UC_X86_REG_EIP,0x51a3bc);return
  if addr==0x51a3c8:self.alpha=struct.unpack('<f',u.mem_read(0xbeeb44,4))[0];return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0;pop=4
  if addr==0x300e000:result=self.now
  elif addr in [0x300e100,0x300e200]:
   assert args[1:]==(0,0,0);assert args[0]==(0x3006000 if addr==0x300e100 else 0x3006200);self.trace.append(('outcome' if addr==0x300e100 else 'response',));pop=20
  elif addr==0x4e77bf:self.trace.append(('release',args[0]&255))
  else:assert args[0]==4;self.trace.append(('pause',))
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,values,flow,dt,now,special):
  u=self.u;self.trace=[];self.now=now;raw=bytearray(0x16c);struct.pack_into('<f',raw,0x12c,s.curtain.alpha);raw[0x134]=s.curtain.stage;struct.pack_into('<f',raw,0x138,s.curtain.speed);raw[0x148:0x14b]=bytes([1,1,0]);u.mem_write(0xbeea18,bytes(raw));u.mem_write(0xbeeb54,bytes(s.wait));u.mem_write(0xbeeb7c,bytes([s.gate,0,s.action,s.blocked]))
  for addr,value in zip([0x71bcd8,0x71bcd9,0x71bcda,0x725d38],[x.value for x in values[:4]]):u.mem_write(addr,bytes([value]))
  for addr,value in zip([0x7219a8,0x7219ac],[x.value for x in values[4:]]):u.mem_write(addr,struct.pack('<I',value))
  for addr,name in [(0xbeeb84,'current'),(0x721ad4,'previous'),(0xbfbbb9,'target'),(0xbfbb9c,'mode')]:u.mem_write(addr,bytes([getattr(flow,name)]))
  u.mem_write(0x733700,struct.pack('<f',dt));u.mem_write(0xbef778,bytes([special]));self.call(0x51a190,b'')
  result=State.from_buffer_copy(s);result.curtain.alpha=struct.unpack('<f',u.mem_read(0xbeeb44,4))[0];result.curtain.stage=u.mem_read(0xbeeb4c,1)[0];result.wait=Timer.from_buffer_copy(u.mem_read(0xbeeb54,C.sizeof(Timer)));result.gate=u.mem_read(0xbeeb7c,1)[0];result.action=u.mem_read(0xbeeb7e,1)[0];result.blocked=u.mem_read(0xbeeb7f,1)[0]
  v=tuple([u.mem_read(x,1)[0] for x in [0x71bcd8,0x71bcd9,0x71bcda,0x725d38]]+[struct.unpack('<I',u.mem_read(x,4))[0] for x in [0x7219a8,0x7219ac]])
  f=Flow(*(u.mem_read(x,1)[0] for x in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]));return result,v,f,self.alpha,self.trace

def snapshot(s):return (s.curtain.alpha,s.curtain.speed,s.curtain.stage,s.wait.duration,s.wait.deadline,s.wait.armed,s.gate,s.action,s.blocked)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x51a3bc);error=C.create_string_buffer(256);total=releases=pauses=0;destinations=set()
 lib.bk_common_hud_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Ops),C.c_float,C.c_uint32,C.POINTER(Frame),C.c_void_p]
 lib.bk_flow_transition_schedule.argtypes=[C.POINTER(Flow),C.POINTER(FlowOps),C.c_uint8,C.c_uint8,C.c_void_p]
 lib.bk_common_hud_initialize.argtypes=[C.POINTER(State)];lib.bk_common_hud_entry_reset.argtypes=[C.POINTER(State)]
 def check(s,values,flow,dt,now,special):
  nonlocal total,releases,pauses
  wanted,wv,wf,wa,wt=n.run(s,values,flow,dt,now,special);trace=[]
  def simple(name):
   def cb(_,e):trace.append((name,));return 1
   return Op(cb)
  @Release
  def release(_,f,e):trace.append(('release',f));return 1
  fops=FlowOps(None,release)
  @Schedule
  def schedule(_,target,mode,e):
   destinations.add((target,mode));return lib.bk_flow_transition_schedule(C.byref(flow),C.byref(fops),target,mode,e)
  ops=Ops(None,simple('outcome'),simple('response'),schedule,simple('pause'))
  b=Bindings(*(C.pointer(x) for x in values),C.cast(C.byref(flow,Flow.current.offset),C.POINTER(C.c_uint8)),special);frame=Frame()
  assert lib.bk_common_hud_step(C.byref(s),C.byref(b),C.byref(ops),dt,now,C.byref(frame),error),error.value
  assert snapshot(s)==snapshot(wanted),(total,snapshot(s),snapshot(wanted));assert tuple(x.value for x in values)==wv,(total,'bindings');assert bytes(flow)==bytes(wf),(total,bytes(flow),bytes(wf));assert frame.alpha==wa;assert trace==wt,(total,trace,wt)
  total+=1;releases+=sum(x[0]=='release' for x in trace);pauses+=sum(x[0]=='pause' for x in trace)
 for case in range(12000):
  s=State(Fade(rng.choice([0,.25,.75,1]),rng.choice([0,.1,2,10]),rng.randrange(6)),Timer(rng.choice([0,1,1000,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,255])),rng.choice([0,1,2,255]),rng.choice([0,1,2,3,4,5,6,255]),rng.choice([0,1,2,255]))
  values=[C.c_uint8(rng.choice([0,0,0,1,4,255])),C.c_uint8(rng.choice([0,1,2,255])),C.c_uint8(rng.choice([0,1,2,3,4,255])),C.c_uint8(rng.randrange(256)),C.c_uint32(rng.randrange(5)),C.c_uint32(rng.randrange(9))];f=Flow(*[rng.randrange(256) for _ in range(4)])
  check(s,values,f,C.c_float(rng.choice([0,1/60,.25,1,10])).value,rng.choice([0,1000,0x7fffffff,0x80000000,0xfffffffe,rng.getrandbits(32)]),rng.choice([0,1,2,255]))
 for event in range(6):
  s=State();lib.bk_common_hud_initialize(C.byref(s));lib.bk_common_hud_entry_reset(C.byref(s));values=[C.c_uint8(0) for _ in range(4)]+[C.c_uint32(3),C.c_uint32(2)];f=Flow(2,8,0,0)
  for tick in range(240):
   if tick==100:
    if event==0:values[0].value=1
    elif event==1:values[1].value=1
    else:values[2].value=event-1
   check(s,values,f,C.c_float(1/60).value,1000+tick*17,event%2)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=total,releases=releases,pauses=pauses,destinations=sorted(destinations),scope=__doc__)
 (ROOT/'local/original-common-hud-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS common HUD',total,'steps',releases,'releases',pauses,'pauses',sorted(destinations))
if __name__=='__main__':main()
