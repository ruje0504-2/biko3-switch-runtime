"""Original4c0126 hotkey block and COMPLETE4afeb0/4affb8 config/count writes.
Input query, sound restart helper, root-path resolution and capture allocation
are service boundaries. Movement/interaction after02e8 are outside this audit.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class State(C.Structure):_fields_=[('menu',C.c_uint8),('count',C.c_int32),('photos',C.c_int32*5)]
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Capture=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('sound',Sound),('capture',Capture)]
KEYS=[0x70,0x33453,0x74,0x33454,0x43,0x33455]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4c0131,0x4c02e8,0x4b76c2,0x46435e,0x4ad8ec,0x49c6fe]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  for a,h in [(0xbeee10,100),(0xbef3b0,105),(0xbef5f0,107)]:self.wi(a,h)
  self.wi(0x705298,640);self.wi(0x70529c,480);self.u.mem_write(0x5767c8,b'\x01')
 def wi(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def ri(self,a):return struct.unpack('<i',self.u.mem_read(a,4))[0]
 def text(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0]
 def snapshot(self):return (self.u.mem_read(self.actor+0x7c9,1)[0],self.u.mem_read(0x729780,1)[0],self.ri(0x734050),tuple(struct.unpack('<5i',self.u.mem_read(0x721b14,20))))
 def hook(self,u,a,size,_):
  if a==0x4c0131:u.reg_write(UC_X86_REG_EIP,0x4c01b6);return
  if a==0x4c02e8:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<3I',u.mem_read(sp+4,12));result=0
  if a==0x4b76c2:
   assert args[1:]==(1,0);self.queries.append(args[0]);result=self.edges[KEYS.index(args[0])]|0x776600
  elif a==0x46435e:
   assert args[1]==0;self.trace.append(('sound',args[0]-100,self.snapshot()))
  elif a==0x4ad8ec:u.mem_write(args[0],b'output'+self.text(args[1])+b'\0')
  else:
   raw=bytes(u.mem_read(sp+4,0x424));mode=struct.unpack_from('<i',raw,0x420)[0];assert mode in [1,2];assert self.ri(0x704e20)==1
   assert struct.unpack_from('<2i',raw)==(640,480)
   assert struct.unpack_from('<4i',raw,0x308)==(504,440,128,32)
   assert struct.unpack_from('<i',raw,0x41c)[0]==0
   self.trace.append(('capture',int(mode==1),self.snapshot()));self.configs.append(raw)
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,camera,group,special,edges):
  self.edges=edges;self.trace=[];self.queries=[];self.configs=[]
  u=self.u;u.mem_write(self.actor+0x7c9,bytes([s.menu]));u.mem_write(0x729780,bytes([camera]));self.wi(0x734050,s.count);u.mem_write(0x721b14,bytes(s.photos));self.wi(0x7219a8,group);u.mem_write(0xbef778,bytes([special]));self.wi(0x704e20,0);self.wi(0x705250,0)
  self.call(0x4c0126,struct.pack('<II',0,self.actor));return self.snapshot(),self.trace

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4c01b6);e=C.create_string_buffer(256);steps=captures=0;configs=set();queries=0
 lib.bk_player_hotkeys_step.argtypes=[C.POINTER(State),C.POINTER(C.c_uint8),C.c_uint,C.c_uint8,C.c_uint32,C.POINTER(Ops),C.c_void_p]
 def check(s,camera,g,special,edges):
  nonlocal steps,captures,queries
  wanted,wt=n.run(s,camera,g,special,edges);camera=C.c_uint8(camera);trace=[]
  def snapshot():return (s.menu,camera.value,s.count,tuple(s.photos))
  @Sound
  def sound(_,slot,e):trace.append(('sound',slot,snapshot()));return 1
  @Capture
  def capture(_,photo,e):trace.append(('capture',photo,snapshot()));return 1
  ops=Ops(None,sound,capture);buttons=sum((1<<i) for i in range(3) if edges[2*i] or edges[2*i+1])
  assert lib.bk_player_hotkeys_step(C.byref(s),C.byref(camera),g,special,buttons,C.byref(ops),e),e.value
  assert snapshot()==wanted,(steps,snapshot(),wanted);assert trace==wt,(steps,trace,wt)
  expected=[]
  for i in range(3):
   if i==2 and special==1:continue
   expected.append(KEYS[2*i])
   if not edges[2*i]:expected.append(KEYS[2*i+1])
  assert n.queries==expected
  queries+=len(expected);steps+=1;captures+=sum(x[0]=='capture' for x in trace)
  for c in n.configs:configs.add(hashlib.sha256(c[:8]+c[8:0x408]+c[0x41c:]).hexdigest())
 for mask in range(64):
  for g in range(5):
   for special in [0,1,2,255]:
    for count in [-2147483648,-1,0,98,99,100,101,2147483647]:
     edges=[rng.choice([1,2,128,255]) if mask&(1<<i) else 0 for i in range(6)]
     check(State(rng.randrange(256),count,(C.c_int32*5)(*[rng.randrange(-100,150) for _ in range(5)])),rng.choice([0,1,2,127,128,255]),g,special,edges)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,captures=captures,native_queries=queries,scope=__doc__)
 (ROOT/'local/original-player-hotkeys-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS player hotkeys',report)
if __name__=='__main__':main()
