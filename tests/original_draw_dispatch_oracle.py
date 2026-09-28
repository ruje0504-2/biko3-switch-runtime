"""Original51c736 root/mode selection and conditional event draw handoff.
4e1473/4d9733/4a4701 are observed service boundaries; synthetic event callbacks
may edit the descriptor and handle the draw. Does not implement those events.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_prop_route_oracle import Native
from model_binding import ROOT,library
class Input(C.Structure):
 _fields_=[('flow',C.c_uint32),('group',C.c_int32),('event_state',C.c_int32)]+[(n,C.c_uint32) for n in ['root_733e28','root_bef77c','root_bef780','root_721b28','root_721b2c','root_721b34']]
class Plan(C.Structure):
 _fields_=[('objects',C.c_uint32*52),('mode',C.c_int32),('shadow',C.c_int32),('event',C.c_uint32)]
class DispatchVM(Native):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4e1473,0x4d9733,0x4a4701]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);self.trace.append(a)
  if a==0x4d9733:
   p=self.read(sp+4);self.before=bytes(u.mem_read(p,216));self.word(p+self.edit_slot*4,self.edit_value)
  if a==0x4a4701:self.after=bytes(u.mem_read(sp+4,216))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,self.handled if a==0x4d9733 else 0)
 def run(self,inp,handled,edit_slot,edit_value):
  self.trace=[];self.before=self.after=None;self.handled=handled;self.edit_slot=edit_slot;self.edit_value=edit_value
  self.u.mem_write(0xbeeb84,bytes([inp.flow]));self.word(0x7219a8,inp.group);self.word(0x721e00,inp.event_state)
  for i,(address,(_,typ)) in enumerate(zip([0x733e28,0xbef77c,0xbef780,0x721b28,0x721b2c,0x721b34],Input._fields_[3:])):
   value=getattr(inp,Input._fields_[3+i][0]);obj=0x3000000+i*0x400
   self.word(address,obj if value else 0);self.word(obj+0x160,obj+0x200);self.word(obj+0x214,value)
  self.call(0x51c736,b'')

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();vm=DispatchVM(exe);lib=library();lib.bk_draw_dispatch_select.argtypes=[C.POINTER(Input),C.POINTER(Plan)];rng=random.Random(0x51c736);events=handled_count=rejects=0;flows=set()
 for case in range(18000):
  flow=case%256 if case<256 else rng.choice([2,4,8,16,16,56,64,72,rng.randrange(256)])
  inp=Input(flow,rng.choice([-1,0,1,4,5,0x7fffffff]),rng.choice([-1,0,1,2,7,8,9]))
  for name,_ in Input._fields_[3:]:setattr(inp,name,0 if rng.randrange(4)==0 else rng.randrange(1,0xffffffff))
  p=Plan();C.memset(C.byref(p),0x5a,C.sizeof(p));before=bytes(p);ok=lib.bk_draw_dispatch_select(C.byref(inp),C.byref(p))
  if not ok:
   assert flow==56 and (not inp.root_bef77c or not inp.root_bef780) and bytes(p)==before;rejects+=1;continue
  handled=rng.choice([0,1,0xffffffff]);edit=rng.randrange(54);value=rng.getrandbits(32);vm.run(inp,handled,edit,value)
  expected=bytes(p)[:216];assert (vm.before if p.event else vm.after)==expected,(case,flow,p.mode,vm.trace)
  if p.event:
   events+=1;assert vm.trace[:2]==[0x4e1473,0x4d9733]
   if handled:assert vm.after is None;handled_count+=1
   else:
    edited=bytearray(expected);struct.pack_into('<I',edited,edit*4,value);assert vm.after==bytes(edited)
  else:assert vm.trace==[0x4a4701]
  flows.add(flow)
 assert len(flows)==256
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=18000,executed=18000-rejects,rejected_missing_gallery_roots=rejects,flow_values=len(flows),event_handoffs=events,event_draws_handled=handled_count,scope=__doc__)
 (ROOT/'local/original-draw-dispatch-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
