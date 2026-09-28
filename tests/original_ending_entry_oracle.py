"""Original4cc7e6..4ccabd dispatch with live loader mutation snapshots.
Resource loaders and48d7f2 are required observing service boundaries, not
pretend implementations. Native record fill/counter reset execute unchanged.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Aux,Frame
from model_binding import ROOT,library
Load=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_int32,C.c_void_p)
Clear=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Prepare=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('load',Load),('clear',Clear),('prepare',Prepare)]
class Bindings(C.Structure):_fields_=[('frame',C.POINTER(Frame)),('control',C.POINTER(Control)),('aux',C.POINTER(Aux)),('option_a',C.POINTER(C.c_uint8)),('option_b',C.POINTER(C.c_uint8)),('selected_group',C.POINTER(C.c_int32)),('gauge',C.POINTER(C.c_float))]
ADDR=[0x721e00,0x721b3d,0x721e04,0x721e20,0x721e24,0x71bcdd,0x71bcde,0x7219a8,0x721b3c,0x721ee0]
SIZES=[4,1,4,4,4,1,1,4,1,4]
LOADERS=[0x4cf318,0x4d00fa,0x4d1025,0x4d2320,0x4d39e6]
class Native:
 stack=0x200e000
 def __init__(self,exe):
  self.u=machine(exe)
  for a in LOADERS+[0x48d7f2,0x534340]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def snap(self):return b''.join(bytes(self.u.mem_read(a,n)) for a,n in zip(ADDR,SIZES))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  if a==0x534340:
   target,value,count=struct.unpack('<3I',u.mem_read(sp+4,12));group=(target-0xb68930)//0x1d4c4;assert count==40000 and value==0xffffffff
   self.trace.append((('clear',group),self.snap()));return
  if a==0x48d7f2:
   self.trace.append((('prepare',),self.snap()));self.word(0x721e04,self.new_variant)
  else:
   loader=LOADERS.index(a);arg=struct.unpack('<i',u.mem_read(sp+4,4))[0] if loader==2 else -1
   self.trace.append((('load',loader,arg),self.snap()))
   if self.mutate:
    self.word(0x721e00,30+loader);self.u.mem_write(0x721b3c,bytes([self.new_group]))
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,snapshot,previous,selection,scale,mutate,new_group,new_variant):
  off=0
  for a,n in zip(ADDR,SIZES):self.u.mem_write(a,snapshot[off:off+n]);off+=n
  self.u.mem_write(0x721ad4,bytes([previous&255]));self.u.mem_write(0x721ad0,struct.pack('<f',scale));self.word(self.stack+0x10,selection)
  for g in range(5):self.u.mem_write(0xb68930+g*0x1d4c4,b'\x5a'*40000);self.word(0xb72570+g*0x1d4c4,99)
  self.mutate=mutate;self.new_group=new_group;self.new_variant=new_variant;self.trace=[]
  self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x1000);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  self.u.emu_start(0x4cc7e6,0x4ccabd,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x4ccabd
  return self.snap(),self.trace,[bytes(self.u.mem_read(0xb68930+g*0x1d4c4,40000))+bytes(self.u.mem_read(0xb72570+g*0x1d4c4,4)) for g in range(5)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4cc7e6);e=C.create_string_buffer(256)
 lib.bk_ending_entry_dispatch.argtypes=[C.POINTER(Bindings),C.c_int8,C.c_uint32,C.c_float,C.POINTER(Ops),C.c_void_p]
 calls=clears=prepares=0;branches=[0]*5
 for case in range(8000):
  f=Frame();f.phase=rng.randrange(100);f.group=case%5;f.state_721ee0=rng.randrange(10)
  c=Control();c.variant=rng.randrange(10);s=Aux();s.variant=case%2;s.progress=rng.random()
  oa=C.c_uint8(rng.randrange(256));ob=C.c_uint8(rng.randrange(256));selected=C.c_int32(-7);gauge=C.c_float(rng.uniform(-1e4,1e4))
  b=Bindings(C.pointer(f),C.pointer(c),C.pointer(s),C.pointer(oa),C.pointer(ob),C.pointer(selected),C.pointer(gauge))
  def snap():return struct.pack('<iBiffBBiBi',f.phase,c.variant,s.variant,s.progress,gauge.value,oa.value,ob.value,selected.value,f.group,f.state_721ee0)
  previous=[8,0x18,0x18,0x18,0,16,56,-1][case%8];selection=rng.choice([0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,0xffffffff]);scale=C.c_float(rng.choice([.5,.75,1,1.5,2,1.333333])).value
  mutate=case%3==0;new_group=(case+2)%5;new_variant=(case//5)%2
  expected=n.run(snap(),previous,selection,scale,mutate,new_group,new_variant);trace=[];records=[b'\x5a'*40000+struct.pack('<I',99) for _ in range(5)]
  @Load
  def load(_,loader,arg,err):
   trace.append((('load',loader,arg),snap()))
   if mutate:f.phase=30+loader;f.group=new_group
   return 1
  @Clear
  def clear(_,group,err):trace.append((('clear',group),snap()));records[group]=b'\xff'*40000+bytes(4);return 1
  @Prepare
  def prepare(_,err):trace.append((('prepare',),snap()));s.variant=new_variant;return 1
  ops=Ops(None,load,clear,prepare)
  assert lib.bk_ending_entry_dispatch(C.byref(b),previous,selection,scale,C.byref(ops),e),(case,e.value)
  assert (snap(),trace,records)==expected,(case,previous,selection,snap(),expected[0],trace,expected[1])
  calls+=len(trace)
  for event,_ in trace:
   if event[0]=='load':branches[event[1]]+=1
   elif event[0]=='clear':clears+=1
   else:prepares+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=8000,calls=calls,loader_calls=branches,record_resets=clears,final_prepares=prepares,byte_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
