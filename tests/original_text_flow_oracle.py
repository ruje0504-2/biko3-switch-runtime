"""4758f7 +474be3 text refresh/scroll state. Surface/texture creation, upload,
draw and render-state APIs are boundary hooks.4747e0's already-tested glyph
layout is bypassed into its real474be3 tail; time/gates/crop are not hooked.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
class Flow(C.Structure):_fields_=[('delay',C.c_float),('scroll',C.c_float),('target',C.c_float),('enabled',C.c_int32),('started',C.c_int32)]
class Update(C.Structure):_fields_=[('update',C.c_int),('source_y',C.c_int32)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);u=self.u
  u.mem_write(0x6455a0,struct.pack('<I',0x3007000));u.mem_write(0x3007000,struct.pack('<I',0x3007100))
  for offset,addr in [(0x50,0x300e100),(0x54,0x300e110),(0x90,0x300e120),(0x94,0x300e130)]:u.mem_write(0x3007100+offset,struct.pack('<I',addr))
  for addr in [0x300e100,0x300e110,0x300e120,0x300e130,0x475514,0x475e7f,0x475e30,0x476159,0x429f86,0x429fd6,0x475dc7,0x4761af,0x476606,0x4747e0,0x450794]:u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
  u.mem_write(0x6a13b8,struct.pack('<f',1280));u.mem_write(0x6a3c00,struct.pack('<I',0x3009000))
  self.word(0x6a3c08,0x300b000);self.word(0x6a3c0c,0x300c000);self.word(0x6a13e4,0x300d000)
 def word(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
 def hook(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',u.mem_read(sp,40));ret=args[0];a=args[1:];pop=0;value=0
  if addr in [0x300e100,0x300e110]:
   pop=12
   if addr==0x300e110:self.word(a[2],0)
  elif addr in [0x300e120,0x300e130]:
   pop=16
   if addr==0x300e120:self.word(a[3],0)
  elif addr==0x475514:value=0x300b000
  elif addr==0x475e7f:value=0x300c000
  elif addr==0x4747e0:
   self.updates+=1;u.reg_write(UC_X86_REG_EIP,0x474be3);return
  elif addr==0x450794:
   assert a[:5]==(0x300b000,0,0,0x300d000,0),a
   assert a[6:]==(632,536,0),a
   self.rows.append(struct.unpack('<i',struct.pack('<I',a[5]))[0])
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,state,seconds,changed):
  u=self.u
  for addr,value in [(0x6a13d0,state.delay),(0x6a13d4,state.scroll),(0x6a13d8,state.target),(0x733700,seconds)]:u.mem_write(addr,struct.pack('<f',value))
  self.word(0x6a13dc,state.enabled);self.word(0x6a13e0,state.started)
  u.mem_write(0x6a13f8,b'before\0' if changed else b'current\0');u.mem_write(0x3001000,b'current\0')
  self.word(0x6a13b4,192);self.word(0x6a13b0,400)
  u.mem_write(0x3005000,struct.pack('<7i5f2I',192,400,316,268,16,16,1,1,1,1,1,1,0,0x3001000))
  self.updates=0;self.rows=[];self.call(0x4758f7,struct.pack('<I',0x3005000))
  state=Flow(*[struct.unpack('<f',u.mem_read(addr,4))[0] for addr in [0x6a13d0,0x6a13d4,0x6a13d8]],*[struct.unpack('<i',u.mem_read(addr,4))[0] for addr in [0x6a13dc,0x6a13e0]])
  assert bytes(u.mem_read(0x6a13f8,8))==b'current\0'
  assert len(self.rows)==self.updates
  return state,Update(self.updates,self.rows[0] if self.rows else 0)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x474be3)
 lib.bk_text_flow_step.argtypes=[C.POINTER(Flow),C.c_float,C.c_int,C.POINTER(Update)]
 updates=0
 for i in range(16000):
  seconds=C.c_float(rng.choice([0,1/60,0.1,1,10,1e-6])).value
  state=Flow(rng.choice([0,49,49.5,50,50.001,51,rng.uniform(-10,100)]),rng.uniform(-20,200),rng.choice([-1,0,10,100,200]),rng.choice([0,0,1,2,-1]),rng.choice([0,0,1,2,-1]))
  if i%11==0:state.scroll=state.target-seconds*10
  changed=i%2;wanted,update=n.run(state,seconds,changed);actual=Update()
  assert lib.bk_text_flow_step(C.byref(state),seconds,changed,C.byref(actual))
  assert bytes(state)==bytes(wanted),(i,list(bytes(state)),list(bytes(wanted)))
  assert bytes(actual)==bytes(update),(i,actual.update,update.update,actual.source_y,update.source_y)
  updates+=update.update
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=16000,bitmap_updates=updates,scope=__doc__)
 (ROOT/'local/original-text-flow-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
