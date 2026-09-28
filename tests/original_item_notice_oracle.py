"""51a190 phase1 notice segment: original timer, six sprite transitions and
font gates execute. Other HUD/rain, font service and sprite draw are boundaries.
Includes rapid selection changes, timer wrap and expiry during transitions.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_item_pickup_oracle import PickupState as Pickup
from original_text_flow_oracle import Flow
from model_binding import ROOT,library
class Fade(C.Structure):_fields_=[('alpha',C.c_float),('speed',C.c_float),('stage',C.c_uint8)]
class State(C.Structure):_fields_=[('duration',C.c_uint32),('deadline',C.c_uint32),('panel',Fade),('icons',Fade*5)]
class Frame(C.Structure):_fields_=[('bind_message',C.c_int),('draw_text',C.c_int)]
class Native(Base):
 addresses=[0xbeeb88]+[0x733708+i*0x16c for i in range(5)]
 def __init__(self,exe):
  super().__init__(exe)
  self.u.mem_write(0x53f10c,struct.pack('<I',0x300e000))
  for addr in [0x4fac10,0x4cbca4,0x4cb902,0x4aa9f4,0x4aaa17,0x300e000,0x51a3bc]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
 def hook(self,u,addr,size,_):
  if addr==0x51a3bc:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  if addr==0x4aa9f4:
   assert struct.unpack('<I',u.mem_read(sp+4,4))[0]==0xbe9060
   self.bound+=1
  if addr==0x4aaa17:self.draws+=1
  u.reg_write(UC_X86_REG_EAX,self.now if addr==0x300e000 else 0)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,state,pickup,flow,dt,now):
  u=self.u;self.now=now;self.bound=self.draws=0
  for addr,fade in zip(self.addresses,[state.panel,*state.icons]):
   data=bytearray(0x16c);struct.pack_into('<4f',data,0x10c,1010,208,138,727)
   struct.pack_into('<f',data,0x12c,fade.alpha);data[0x134]=fade.stage
   struct.pack_into('<f',data,0x138,fade.speed);data[0x148:0x14b]=bytes([1,1,0]);u.mem_write(addr,bytes(data))
  u.mem_write(0xbeecc4,struct.pack('<IIB',state.duration,state.deadline,pickup.notice_timer_armed))
  u.mem_write(0xbeecef,bytes([pickup.notice_visible]));u.mem_write(0xbe9460,struct.pack('<i',pickup.selected_item))
  u.mem_write(0x71ba88,b'\1');u.mem_write(0x733700,struct.pack('<f',dt))
  u.mem_write(0x6a13d0,bytes(flow))
  self.call(0x51a190,b'')
  wanted=State.from_buffer_copy(state)
  for addr,fade in zip(self.addresses,[wanted.panel,*wanted.icons]):
   fade.alpha=struct.unpack('<f',u.mem_read(addr+0x12c,4))[0];fade.stage=u.mem_read(addr+0x134,1)[0]
  wanted.deadline=struct.unpack('<I',u.mem_read(0xbeecc8,4))[0]
  p=Pickup.from_buffer_copy(pickup);p.notice_visible=u.mem_read(0xbeecef,1)[0];p.notice_timer_armed=u.mem_read(0xbeeccc,1)[0]
  return wanted,p,Flow.from_buffer_copy(u.mem_read(0x6a13d0,C.sizeof(Flow))),Frame(self.bound,self.draws)
def snapshot(s):return (s.duration,s.deadline,[(x.alpha,x.speed,x.stage) for x in [s.panel,*s.icons]])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x51a249)
 lib.bk_item_notice_step.argtypes=[C.POINTER(State),C.POINTER(Pickup),C.POINTER(Flow),C.c_float,C.c_uint32,C.POINTER(Frame)]
 lib.bk_item_notice_initialize.argtypes=[C.POINTER(State),C.c_int]
 total=bound=draws=expiries=0;stages=set()
 def check(s,p,f,dt,now):
  nonlocal total,bound,draws,expiries
  wanted,wp,wf,wout=native.run(s,p,f,dt,now);out=Frame();before=p.notice_visible
  assert lib.bk_item_notice_step(C.byref(s),C.byref(p),C.byref(f),dt,now,C.byref(out))
  assert snapshot(s)==snapshot(wanted),(total,snapshot(s),snapshot(wanted))
  assert bytes(p)==bytes(wp),(total,bytes(p),bytes(wp))
  assert bytes(f)==bytes(wf),(total,bytes(f),bytes(wf))
  assert bytes(out)==bytes(wout),(total,bytes(out),bytes(wout))
  total+=1;bound+=out.bind_message;draws+=out.draw_text;expiries+=before==1 and p.notice_visible==0
  stages.update(x.stage for x in [s.panel,*s.icons])
 for case in range(3600):
  s=State(rng.choice([0,1,5000,0xffffffff]),rng.getrandbits(32))
  for x in [s.panel,*s.icons]:x.alpha=rng.random();x.speed=rng.choice([0,.1,2,7,100]);x.stage=rng.randrange(6)
  p=Pickup();p.notice_visible=rng.choice([0,1,1,2,255]);p.notice_timer_armed=rng.choice([0,1,2,255]);p.selected_item=rng.randrange(-1,7)
  f=Flow(21,0,0,rng.randrange(-1,3),rng.randrange(-1,3))
  check(s,p,f,C.c_float(rng.choice([0,1/60,.1,1,10])).value,rng.choice([0,5000,0x7fffffff,0x80000000,0xfffffffe,rng.getrandbits(32)]))
 for sequence in range(32):
  s=State();assert lib.bk_item_notice_initialize(C.byref(s),0);p=Pickup();f=Flow()
  now=[0,0x7ffff500,0xfffff500,100000][sequence%4]
  for tick in range(480):
   if tick in [2,20,45,46,47,70,390]:p.notice_visible=1;p.notice_timer_armed=0;p.selected_item=rng.randrange(5)
   dt=C.c_float([1/60,.025,.05,.1][sequence%4]).value;now=(now+int(dt*1000))&0xffffffff
   check(s,p,f,dt,now)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=total,message_bindings=bound,text_draws=draws,expirations=expiries,observed_stages=sorted(stages),scope=__doc__)
 (ROOT/'local/original-item-notice-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
