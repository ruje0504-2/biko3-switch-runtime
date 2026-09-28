"""Full4f5c6a pickup loop and native4ae68d geometry, including ordered
sound/notice service calls and intermediate logical/inventory/UI state.
Only sound46435e and message-resource51876a are hooked. No pickup math hooks.
"""
import argparse,ctypes as C,hashlib,json,random,struct,math
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from original_item_loader_oracle import State
from model_binding import ROOT,library
class PickupState(C.Structure):
 _fields_=[('collected',C.c_uint8*5),('notice_visible',C.c_uint8),('notice_timer_armed',C.c_uint8),('selected_item',C.c_int32),('message_cursor',C.c_int32)]
class Picks(C.Structure):_fields_=[('count',C.c_uint32),('slots',C.c_uint32*16)]
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Notice=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('sound',Sound),('notice',Notice)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for addr in [0x46435e,0x51876a]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
 def snapshot(self):
  return ([self.u.mem_read(0x72685c+i*0x268,1)[0] for i in range(16)],list(self.u.mem_read(0x71bcdc,5)),self.u.mem_read(0xbeecef,1)[0],self.u.mem_read(0xbeeccc,1)[0],struct.unpack('<i',self.u.mem_read(0xbe9460,4))[0],struct.unpack('<i',self.u.mem_read(0xbe8f4c,4))[0])
 def hook(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,a,b=struct.unpack('<3I',u.mem_read(sp,12))
  if addr==0x46435e:assert (a,b)==(0x1234,0);kind,key='sound',0
  else:assert b==0xbe8f48;kind,key='notice',a
  self.trace.append((kind,key,self.snapshot()))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,1)
 def run(self,slots,mask,state,group,current,previous):
  self.u.mem_write(0x726650,bytes(0x268*16))
  for i,s in enumerate(slots):
   p=0x726650+i*0x268
   self.u.mem_write(p,struct.pack('<II',1 if mask&(1<<i) else 0,s.id))
   self.u.mem_write(p+0x20c,bytes([s.hidden]));self.u.mem_write(p+0x250,bytes(s.position));self.u.mem_write(p+0x260,struct.pack('<f',s.yaw))
  self.u.mem_write(0x71b7ac,bytes(current));self.u.mem_write(0x71b7d8,bytes(previous))
  self.u.mem_write(0x71bcdc,bytes(state.collected));self.u.mem_write(0xbeecef,bytes([state.notice_visible]));self.u.mem_write(0xbeeccc,bytes([state.notice_timer_armed]));self.u.mem_write(0xbe9460,struct.pack('<i',state.selected_item));self.u.mem_write(0xbe8f4c,struct.pack('<i',state.message_cursor))
  self.u.mem_write(0x7219a8,struct.pack('<I',group));self.u.mem_write(0x721ac0,struct.pack('<I',0x1234))
  self.trace=[];self.call(0x4f5c6a,b'');return self.snapshot()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4f5c6a);error=C.create_string_buffer(256);vec=C.c_float*3
 lib.bk_item_pickup_run.argtypes=[C.POINTER(State),C.c_uint32,C.POINTER(PickupState),C.c_uint32,C.POINTER(C.c_float),C.POINTER(C.c_float),C.POINTER(Ops),C.POINTER(Picks),C.c_void_p]
 hits=multi=0
 for case in range(12000):
  group=case%5;mask=rng.randrange(65536) if case%7 else 65535
  current=vec(*[rng.uniform(-100,100) for _ in range(3)]);previous=vec(*[rng.uniform(-100,100) for _ in range(3)])
  if case%13==0:previous[0]=current[0];previous[2]=current[2]
  if case%13==1:previous[0]=current[0]
  if case%13==2:previous[2]=current[2]
  slots=(State*16)()
  for i,s in enumerate(slots):
   s.id=rng.randrange(5);s.yaw=rng.uniform(-360,360);s.hidden=rng.choice([0,0,0,0,1,2,128,255])
   s.position[:]=[rng.uniform(-100,100) for _ in range(3)]
   if i%3:
    t=rng.choice([0,1,.5,-.01,1.01]);s.position[0]=current[0]+t*(previous[0]-current[0]);s.position[2]=current[2]+t*(previous[2]-current[2])
    s.position[2]+=rng.choice([0,9.999,10,10.001,-10])
  state=PickupState((C.c_uint8*5)(*[rng.randrange(3) for _ in range(5)]),rng.randrange(3),rng.randrange(3),rng.randrange(-5,6),rng.randrange(-100,101))
  old_hidden=[s.hidden for s in slots];wanted=n.run(slots,mask,state,group,current,previous);trace=[]
  def snapshot():return ([s.hidden for s in slots],list(state.collected),state.notice_visible,state.notice_timer_armed,state.selected_item,state.message_cursor)
  @Sound
  def sound(ctx,error):trace.append(('sound',0,snapshot()));return 1
  @Notice
  def notice(ctx,key,error):trace.append(('notice',key,snapshot()));return 1
  ops=Ops(None,sound,notice);out=Picks()
  assert lib.bk_item_pickup_run(slots,mask,C.byref(state),group,current,previous,C.byref(ops),C.byref(out),error),(case,error.value)
  assert snapshot()==wanted,(case,snapshot(),wanted)
  assert trace==n.trace,(case,trace,n.trace)
  picked=[i for i,s in enumerate(slots) if s.hidden!=old_hidden[i]]
  assert picked==list(out.slots[:out.count]),(case,picked,list(out.slots[:out.count]))
  hits+=out.count;multi+=out.count>1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=12000,pickups=hits,multiple_hit_frames=multi,ordered_service_calls=hits*2,scope=__doc__)
 (ROOT/'local/original-item-pickup-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
