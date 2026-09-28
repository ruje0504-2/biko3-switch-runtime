"""Complete51c4bf flow50 UI policy with original fade/pulse sprite programs.
Only edge input, sound restart and resource loader are intercepted. Loader
fixtures mutate live retained globals to verify before/after ownership order.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_item_notice_oracle import Fade
from original_common_hud_oracle import State as Common,Flow,Timer
from model_binding import ROOT,library
class Pulse(C.Structure):_fields_=[('fade',Fade),('direction',C.c_uint8)]
class State(C.Structure):_fields_=[('background',Fade),('special_background',Fade),('prompt_base',Fade),('prompt',Pulse),('awaiting',C.c_uint8)]
class Draw(C.Structure):_fields_=[('asset',C.c_int),('alpha',C.c_float)]
class Frame(C.Structure):_fields_=[('count',C.c_uint),('draws',Draw*4)]
Load=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_void_p)
Click=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('load',Load),('click',Click)]
ADDR=[0xbe9898,0xb53568,0xbeea18,0xb53960,0xb53acc]
def fade(s):return (s.alpha,s.speed,s.stage)
def snapshot(s,c,f):return (fade(s.background),fade(s.special_background),fade(s.prompt_base),fade(s.prompt.fade),s.prompt.direction,s.awaiting,fade(c.curtain),c.blocked,bytes(f))
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.wi(0xbeef30,123)
  for a in [0x4b76c2,0x46435e,0x4e7671,0x50e7a5]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def wi(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def install(self,a,f,pulse=None):
  raw=bytearray(0x16c);struct.pack_into('<f',raw,0x12c,f.alpha);raw[0x134]=f.stage;struct.pack_into('<f',raw,0x138,f.speed);raw[0x148:0x14b]=bytes([1,1,11 if pulse is not None else 0]);raw[0x160]=pulse or 0;self.u.mem_write(a,bytes(raw))
 def readfade(self,a):return Fade(struct.unpack('<f',self.u.mem_read(a+0x12c,4))[0],struct.unpack('<f',self.u.mem_read(a+0x138,4))[0],self.u.mem_read(a+0x134,1)[0])
 def read(self):
  s=State(self.readfade(ADDR[0]),self.readfade(ADDR[1]),self.readfade(ADDR[3]),Pulse(self.readfade(ADDR[4]),self.u.mem_read(ADDR[4]+0x160,1)[0]),self.u.mem_read(0xbfbba8,1)[0]);c=Common();c.curtain=self.readfade(ADDR[2]);c.blocked=self.u.mem_read(0xbeeb7f,1)[0];f=Flow(*(self.u.mem_read(a,1)[0] for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]));return s,c,f
 def mutate(self):
  if self.mutation:
   blocked,stage,alpha,target=self.mutation;self.u.mem_write(0xbeeb7f,bytes([blocked]));self.u.mem_write(0xbeeb4c,bytes([stage]));self.u.mem_write(0xbeeb44,struct.pack('<f',alpha));self.u.mem_write(0xbfbbb9,bytes([target]));self.u.mem_write(0xbfbba8,b'\xff')
 def hook(self,u,a,size,_):
  if a==0x50e7a5:
   frame=u.reg_read(UC_X86_REG_EBP);p=struct.unpack('<I',u.mem_read(frame+8,4))[0];self.draws.append((ADDR.index(p),self.readfade(p).alpha));return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<3I',u.mem_read(sp+4,12));result=0
  if a==0x4b76c2:
   assert args[0] in [0,0x5a,0x33450] and args[1:]==(1,0);result=int(args[0]==self.key)
  elif a==0x46435e:assert args[:2]==(123,0);self.trace.append(('click',snapshot(*self.read())))
  else:self.trace.append(('load',args[0]&255,snapshot(*self.read())));self.mutate()
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,c,f,special,key,seconds,mutation):
  self.draws=[];self.trace=[];self.key=key;self.mutation=mutation
  for a,v in zip(ADDR,[s.background,s.special_background,c.curtain,s.prompt_base,s.prompt.fade]):self.install(a,v,s.prompt.direction if a==ADDR[4] else None)
  for a,v in [(0xbeeb84,f.current),(0x721ad4,f.previous),(0xbfbbb9,f.target),(0xbfbb9c,f.mode),(0xbef778,special),(0xbeeb7f,c.blocked),(0xbfbba8,s.awaiting)]:self.u.mem_write(a,bytes([v]))
  self.u.mem_write(0x733700,struct.pack('<f',seconds));self.call(0x51c4bf,b'');return snapshot(*self.read()),self.draws,self.trace

class Geometry(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4e6df7,0x4e72fb,0x50de40]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def hook(self,u,a,size,_):
  if a==0x4e6df7:
   u.mem_write(u.reg_read(UC_X86_REG_EBP)-0x904,b'assets\\bk3_00.pp\0');u.reg_write(UC_X86_REG_EIP,0x4e6fb3);return
  if a==0x4e72fb:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',u.mem_read(sp+4,40))
  if args[0] in ADDR:
   name=bytes(u.mem_read(args[2],48)).split(b'\0')[0];geom=struct.unpack('<4f',u.mem_read(sp+16,16));self.rows.append((ADDR.index(args[0]),name,geom))
  u.mem_write(args[0]+0x134,b'\0');ret=struct.unpack('<I',u.mem_read(sp,4))[0];u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,w,h,special):
  self.rows=[]
  for a,v in [(0x721ad0,w/1280),(0xbeecf4,w/1024),(0xbeecf8,h/768)]:self.u.mem_write(a,struct.pack('<f',v))
  self.u.mem_write(0xbef778,bytes([special]));self.call(0x4e6dee,b'');return self.rows

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x51c4bf);error=C.create_string_buffer(256);steps=loads=clicks=draws=0
 lib.bk_flow_loading_step.argtypes=[C.POINTER(State),C.POINTER(Common),C.POINTER(Flow),C.c_uint8,C.c_int,C.c_float,C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
 def check(s,c,f,special,key,dt,mutation):
  nonlocal steps,loads,clicks,draws
  want,wd,wt=n.run(s,c,f,special,key,dt,mutation);trace=[]
  @Load
  def load(_,target,e):
   trace.append(('load',target,snapshot(s,c,f)))
   if mutation:
    c.blocked,c.curtain.stage,c.curtain.alpha,f.target=mutation;s.awaiting=255
   return 1
  @Click
  def click(_,e):trace.append(('click',snapshot(s,c,f)));return 1
  ops=Ops(None,load,click);frame=Frame()
  assert lib.bk_flow_loading_step(C.byref(s),C.byref(c),C.byref(f),special,key>=0,dt,C.byref(ops),C.byref(frame),error),error.value
  assert snapshot(s,c,f)==want,(steps,snapshot(s,c,f),want);assert trace==wt,(steps,trace,wt)
  assert [(d.asset,d.alpha) for d in frame.draws[:frame.count]]==wd,(steps,[(d.asset,d.alpha) for d in frame.draws[:frame.count]],wd)
  steps+=1;loads+=sum(t[0]=='load' for t in trace);clicks+=sum(t[0]=='click' for t in trace);draws+=len(wd)
 def rf():return Fade(rng.choice([0,.1,.5,.99,1]),rng.choice([0,.1,2,10]),rng.randrange(6))
 for case in range(16000):
  s=State(rf(),rf(),rf(),Pulse(rf(),rng.choice([0,1,2,255])),rng.choice([0,1,2,255]));c=Common();c.curtain=rf();c.blocked=rng.choice([0,1,2,255]);f=Flow(0x50,rng.choice([2,4,8,0x38,0x48,0x50,255]),rng.randrange(256),case if case<256 else rng.choice([0,1,1,1,1,2,3,255]));special=rng.choice([0,1,1,2,255]);key=rng.choice([-1,0,0x5a,0x33450]);dt=C.c_float(rng.choice([0,1/60,.25,1,10])).value
  mutation=(rng.choice([0,1,2]),rng.randrange(6),rng.choice([0,.5,1]),rng.randrange(256)) if case%3==0 else None
  check(s,c,f,special,key,dt,mutation)
 # Long natural progressions: opaque -> load -> opaque -> target, including
 # special explicit confirmation (do not synthesize completion by writingstage).
 for special in [0,1,2]:
  for mode in range(4):
   for previous in [2,0x38]:
    s=State(Fade(1,2,1),Fade(1,2,1),Fade(1,2,1),Pulse(Fade(1,2,1),0),0);c=Common();c.curtain=Fade(1,2,3);f=Flow(0x50,previous,2,mode)
    for tick in range(300):
     check(s,c,f,special,0x33450 if s.awaiting==1 else -1,C.c_float(1/60).value,None)
     if f.current!=0x50:break
    assert f.current==2,(special,mode,previous)
 geometry=Geometry(exe);layouts=0
 lib.bk_flow_loading_layout.argtypes=[C.POINTER(C.c_float),C.c_int,C.c_uint,C.c_uint]
 lib.bk_flow_loading_image.argtypes=[C.c_int];lib.bk_flow_loading_image.restype=C.c_char_p
 for w,h in [(640,480),(1280,720),(320,240),(503,367),(1001,733),(1920,1080),(16383,16381)]:
  for special in [0,1,2,255]:
   rows=geometry.run(w,h,special);assert len(rows)==(5 if special==1 else 4)
   for asset,name,geom in rows:
    out=(C.c_float*4)();assert lib.bk_flow_loading_layout(out,asset,w,h)
    assert tuple(out)==geom,(w,h,asset,tuple(out),geom);assert lib.bk_flow_loading_image(asset)==name;layouts+=1
 report=dict(layouts=layouts,passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,loads=loads,clicks=clicks,draws=draws,scope=__doc__)
 (ROOT/'local/original-flow-loading-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS flow loading',report)
if __name__=='__main__':main()
