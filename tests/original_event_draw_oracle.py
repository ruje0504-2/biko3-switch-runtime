"""Full original51c736+4d9733 vs portable ordered dispatch callbacks.
Original selector and event/audio gates execute; scene rendering4d9898,
prepare4e1473, audio duck4e01e4, regular4a4701 and DirectSound GetStatus are
service boundaries. Tests callback ordering, descriptor edits and audio status
changing during special scene draw, not complete event rendering/audio output.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_draw_dispatch_oracle import Input,Plan,DispatchVM
from model_binding import ROOT,library
class Event(C.Structure):_fields_=[('mode',C.c_int32),('present',C.c_uint32)]
Prepare=C.CFUNCTYPE(C.c_int,C.c_void_p)
Scene=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Plan))
Status=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int32),C.POINTER(C.c_uint32))
Duck=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('prepare',Prepare),('scene',Scene),('status',Status),('duck',Duck),('regular',Scene)]
class VM(DispatchVM):
 sound,vt,stub=0x300c000,0x300c100,0x300d000
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4d9898,0x4e01e4,self.stub]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.word(self.sound,self.vt);self.word(self.vt+0x24,self.stub)
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);eax=0;extra=0
  if a==0x4d9733:return
  if a==0x4e1473:self.trace.append(('prepare',))
  elif a==0x4d9898:
   p=self.read(sp+4);self.trace.append(('scene',bytes(u.mem_read(p,216))));self.word(p+self.edit_slot*4,self.edit_value);self.flags^=self.flip
  elif a==0x4e01e4:self.trace.append(('duck',self.read(sp+4)))
  elif a==0x4a4701:self.trace.append(('regular',bytes(u.mem_read(sp+4,216))))
  elif a==self.stub:
   self.trace.append(('status',));self.word(self.read(sp+8),self.flags);eax=self.result;extra=8
  else:raise AssertionError(hex(a))
  u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,eax&0xffffffff)
 def full(self,inp,event,result,flags,flip,edit,value):
  self.word(0x721ec4,event.mode);self.word(0x722454,self.sound if event.present else 0);self.result=result;self.flags=flags;self.flip=flip
  super().run(inp,0,edit,value)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();vm=VM(exe);lib=library();lib.bk_draw_dispatch_run.argtypes=[C.POINTER(Input),C.POINTER(Event),C.POINTER(Ops)];rng=random.Random(0x4d9733);actual=[];state={}
 @Prepare
 def prepare(ctx):actual.append(('prepare',));return 1
 @Scene
 def scene(ctx,p):
  actual.append(('scene',bytes(p.contents)[:216]));C.c_uint32.from_address(C.addressof(p.contents)+state['edit']*4).value=state['value'];state['flags']^=state['flip'];return 1
 @Status
 def status(ctx,result,flags):actual.append(('status',));result[0]=state['result'];flags[0]=state['flags'];return 1
 @Duck
 def duck(ctx,enabled):actual.append(('duck',enabled));return 1
 @Scene
 def regular(ctx,p):actual.append(('regular',bytes(p.contents)[:216]));return 1
 ops=Ops(None,prepare,scene,status,duck,regular);calls={};branches=set()
 for case in range(16000):
  inp=Input(rng.choice([16,16,16,16,8,56,64,72]),rng.randrange(5),rng.choice([0,1,2,7,8,9]))
  for name,_ in Input._fields_[3:]:setattr(inp,name,rng.randrange(1,0xffffffff))
  event=Event(rng.choice([-1,0,1,2,3]),rng.randrange(2));result=rng.choice([0,0,1,-1]);flags=rng.choice([0,1,2,3,0xffffffff]);flip=rng.randrange(2);edit=rng.randrange(52);value=rng.getrandbits(32)
  state.update(result=result,flags=flags,flip=flip,edit=edit,value=value);actual.clear();assert lib.bk_draw_dispatch_run(C.byref(inp),C.byref(event),C.byref(ops))
  vm.full(inp,event,result,flags,flip,edit,value);assert actual==vm.trace,(case,inp.flow,inp.event_state,event.mode,actual,vm.trace)
  if inp.flow==16 and inp.event_state!=7:branches.add((event.mode,event.present,result==0,bool(flags&1)))
  for op in actual:calls[op[0]]=calls.get(op[0],0)+1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=16000,calls=calls,event_branches=len(branches),scope=__doc__)
 (ROOT/'local/original-event-draw-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
