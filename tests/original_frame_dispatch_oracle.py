"""Execute original51a682 and4ec78d dispatch; stage bodies are observing service
boundaries. Compares branch latching, phase->NPC mode timing and ordered calls.
This checks dispatch only, not native full-game equivalence of component math.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
MAP={0x4b26b8:0,0x4f737b:1,0x4c1313:2,0x4c009b:3,0x4b8a89:4,0x4bfea9:5,0x4fc8f9:6,0x4fc36d:7,0x5121ce:8,0x512102:9,0x4ef095:10,0x4f4306:11,0x4f3c80:12,0x4f3d34:13,0x4f5c6a:14,0x4b2f41:16}
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [*MAP,0x4bdc12,0x4bdfb0,0x4be290,0x4ec7eb]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def hook(self,u,a,size,_):
  if a in MAP or a==0x4ec7eb:
   event=MAP.get(a,15);self.trace.append((event,u.mem_read(0x71ba88,1)[0],u.mem_read(0x729360,1)[0]))
   if event==self.mutate:u.mem_write(0x71ba88,bytes([self.newphase]))
  if a==0x4ec7eb:return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
 def run(self,phase,mode,mutate,newphase,stage,transition):
  self.trace=[];self.mutate=mutate;self.newphase=newphase
  u=self.u;u.mem_write(0x71ba88,bytes([phase]));u.mem_write(0x729360,bytes([mode]));u.mem_write(0xbef784,struct.pack('<i',stage));u.mem_write(0x729780,bytes([transition]));self.call(0x51a682,b'')
  return self.trace,u.mem_read(0x71ba88,1)[0],u.mem_read(0x729360,1)[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x51a682)
 CB=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_void_p)
 lib.bk_game_frame_dispatch.argtypes=[C.POINTER(C.c_uint8),C.POINTER(C.c_int8),CB,C.c_void_p,C.c_void_p]
 error=C.create_string_buffer(256);total=0
 for case in range(2400):
  phase=C.c_uint8(case if case<256 else rng.choice([0,1,2,3,255]));mode=C.c_int8(rng.randrange(-128,128));mutate=rng.randrange(-1,17);newphase=rng.randrange(256)
  wanted=n.run(phase.value,mode.value&255,mutate,newphase,rng.choice([-1,0,1,2,9]),rng.choice([0,1,2,255]));trace=[]
  @CB
  def consume(ctx,event,err):
   trace.append((event,phase.value,mode.value&255))
   if event==mutate:phase.value=newphase
   return 1
  assert lib.bk_game_frame_dispatch(C.byref(phase),C.byref(mode),consume,None,error)
  assert (trace,phase.value,mode.value&255)==wanted,(case,trace,wanted)
  total+=len(trace)
 # Portable failure contract: fail fast, cleanup once after successful begin,
 # retain first error even if cleanup also fails. No original crash equivalence.
 failures=0
 for branch in [0,1,2,3]:
  for failure in range(17):
   phase=C.c_uint8(branch);mode=C.c_int8(-7);trace=[]
   @CB
   def broken(ctx,event,err):
    trace.append(event)
    if event==failure or event==16:
     C.memmove(err,(b'primary\0' if event==failure else b'cleanup\0'),8);return 0
    return 1
   assert not lib.bk_game_frame_dispatch(C.byref(phase),C.byref(mode),broken,None,error)
   if failure in trace and failure!=16:
    assert trace[-1]==(0 if failure==0 else 16) and error.value==b'primary'
   else:assert trace[-1]==16
   assert trace.count(16)==(failure!=0);failures+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=2400,events=total,failure_cases=failures,scope=__doc__)
 (ROOT/'local/original-frame-dispatch-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
