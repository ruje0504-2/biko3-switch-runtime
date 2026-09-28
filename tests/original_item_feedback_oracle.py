"""Full4f5c6a + real51876a +46435e against item pickup with real feedback
adapter/resources. Only DirectSound COM methods are replaced. Message entry
is observed without skipping any instruction. No geometry or text hooks.
"""
import argparse,ctypes as C,hashlib,json,random,struct,sys
from pathlib import Path
from original_prop_route_oracle import Native as Base
from original_item_pickup_oracle import Native as PickupNative,PickupState,Picks,Ops
from original_item_loader_oracle import State
from original_message_oracle import Message
from model_binding import ROOT,library
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Native(PickupNative):
 def __init__(self,exe,text):
  Base.__init__(self,exe);u=self.u;u.mem_map(0x1000,0x1000);u.mem_map(0x4000000,0x10000)
  u.mem_write(0x1234,struct.pack('<I',0x1800))
  for offset,addr in [(0x24,0x300e000),(0x34,0x300e010),(0x30,0x300e020)]:
   u.mem_write(0x1800+offset,struct.pack('<I',addr));u.hook_add(UC_HOOK_CODE,self.com,begin=addr,end=addr)
  u.mem_write(0x4000000,text+b'\0');u.mem_write(0xbe8f48,struct.pack('<I',0x4000000))
  u.hook_add(UC_HOOK_CODE,self.observe,begin=0x51876a,end=0x51876a)
 def observe(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);key,state=struct.unpack('<2I',u.mem_read(sp+4,8));assert state==0xbe8f48
  self.trace.append(('notice',key,self.snapshot()))
 def com(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,obj=struct.unpack('<2I',u.mem_read(sp,8));assert obj==0x1234
  if addr==0x300e000:
   ptr=struct.unpack('<I',u.mem_read(sp+8,4))[0];u.mem_write(ptr,struct.pack('<I',self.status));argc=2;name='status'
  elif addr==0x300e010:
   assert struct.unpack('<I',u.mem_read(sp+8,4))[0]==0;argc=2;name='rewind'
  else:
   assert struct.unpack('<3I',u.mem_read(sp+8,12))==(0,0,0);argc=4;name='play'
  self.trace.append((name,0,self.snapshot()));u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4+argc*4);u.reg_write(UC_X86_REG_EIP,ret)
Submit=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int16),C.c_size_t,C.c_void_p)
Poll=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_uint64),C.c_void_p)
class Sink(C.Structure):_fields_=[('context',C.c_void_p),('rate',C.c_uint32),('block',C.c_uint32),('capacity',C.c_uint32),('submit',Submit),('poll',Poll)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();archive=Archive(a.data/'bk3_05.pp');raw=archive.read(next(e for e in archive.entries if e.name=='i00_00.txt'))
 n=Native(exe,raw);lib=library();error=C.create_string_buffer(256);rng=random.Random(0x46435e);vec=C.c_float*3
 lib.bk_resources_create.argtypes=[C.c_void_p];lib.bk_resources_create.restype=C.c_void_p
 lib.bk_resources_mount.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p];lib.bk_resources_destroy.argtypes=[C.c_void_p]
 lib.bk_audio_create.argtypes=[C.POINTER(Sink),C.c_void_p];lib.bk_audio_create.restype=C.c_void_p
 lib.bk_audio_destroy.argtypes=[C.c_void_p]
 for func in ['bk_audio_poll','bk_audio_fill']:getattr(lib,func).argtypes=[C.c_void_p,C.c_void_p]
 lib.bk_item_feedback_create.argtypes=[C.c_void_p,C.c_void_p,C.c_uint,C.c_int32,C.c_void_p];lib.bk_item_feedback_create.restype=C.c_void_p
 lib.bk_item_feedback_destroy.argtypes=[C.c_void_p];lib.bk_item_feedback_ops.argtypes=[C.c_void_p];lib.bk_item_feedback_ops.restype=Ops
 lib.bk_item_feedback_message.argtypes=[C.c_void_p];lib.bk_item_feedback_message.restype=C.POINTER(Message)
 lib.bk_item_pickup_run.argtypes=[C.POINTER(State),C.c_uint32,C.POINTER(PickupState),C.c_uint32,C.POINTER(C.c_float),C.POINTER(C.c_float),C.POINTER(Ops),C.POINTER(Picks),C.c_void_p]
 submitted=consumed=0
 @Submit
 def submit(ctx,data,frames,err):
  nonlocal submitted
  submitted+=frames;return 1
 @Poll
 def poll(ctx,out,err):out[0]=consumed;return 1
 store=lib.bk_resources_create(error);assert store,error.value
 for pack in [b'bk3_02',b'bk3_05']:assert lib.bk_resources_mount(store,pack,str(a.data/(pack.decode()+'.pp')).encode(),error),error.value
 sink=Sink(None,22050,147,588,submit,poll);audio=lib.bk_audio_create(C.byref(sink),error);assert audio,error.value
 feedback=lib.bk_item_feedback_create(store,audio,2,-777,error);assert feedback,error.value
 ops=lib.bk_item_feedback_ops(feedback);hits=multiple=0;state=PickupState();n.u.mem_write(0xbe9060,bytes(1024));n.u.mem_write(0xbe9470,bytes(4))
 try:
  for frame in range(1200):
   slots=(State*16)();mask=rng.randrange(65536);group=frame%5
   for s in slots:
    s.id=rng.randrange(5);s.hidden=rng.choice([0,0,0,1,2,255]);s.position[:]=[rng.uniform(-25,25),rng.uniform(-1000,1000),rng.uniform(-15,15)]
   current=vec(-20,42,0);previous=vec(20,-7,0)
   if frame%17==0:previous=current
   n.status=rng.choice([0,1,2,3,0xffffffff])
   wanted=n.run(slots,mask,state,group,current,previous);out=Picks()
   assert lib.bk_item_pickup_run(slots,mask,C.byref(state),group,current,previous,C.byref(ops),C.byref(out),error),(frame,error.value)
   actual=([s.hidden for s in slots],list(state.collected),state.notice_visible,state.notice_timer_armed,state.selected_item,state.message_cursor)
   assert actual==wanted,(frame,actual,wanted)
   msg=lib.bk_item_feedback_message(feedback).contents
   assert bytes(msg.bytes)==bytes(n.u.mem_read(0xbe9060,1024)),frame
   assert msg.carriage_returns==struct.unpack('<I',n.u.mem_read(0xbe9470,4))[0],frame
   assert len(n.trace)==4*out.count,(frame,n.trace)
   for j in range(out.count):
    t=n.trace[j*4:j*4+4];assert [x[0] for x in t]==['status','rewind','play','notice']
    assert t[0][2]==t[1][2]==t[2][2]
    assert t[3][1]==group*10000+slots[out.slots[j]].id
   consumed=submitted
   assert lib.bk_audio_poll(audio,error) and lib.bk_audio_fill(audio,error),error.value
   hits+=out.count;multiple+=out.count>1
 finally:lib.bk_item_feedback_destroy(feedback);lib.bk_audio_destroy(audio);lib.bk_resources_destroy(store)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=1200,pickups=hits,multiple_hit_frames=multiple,directsound_calls=hits*3,scope=__doc__)
 (ROOT/'local/original-item-feedback-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
