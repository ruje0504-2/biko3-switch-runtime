"""Native rain construction4fa7fb and complete4fac10 draw dispatch.
RNG, integer limits, sprite phases, placement and43ed45 geometry run original
instructions. File/texture creation, GPU vertex lock/unlock and draw are the
substituted service boundaries. Valid rain effects0/phases1..3 only.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class State(C.Structure):_fields_=[('present',C.c_uint32),('phase',C.c_uint8*16),('x',C.c_float*16),('y',C.c_float*16)]
class Sprite(C.Structure):_fields_=[('slot',C.c_uint32),('x',C.c_float),('y',C.c_float)]
class Draw(C.Structure):_fields_=[('count',C.c_uint32),('sprites',Sprite*16)]
class Native(Base):
 entries=0xbe7888;sprites=0x3000000;vertices=0x3004000
 def __init__(self,exe):
  super().__init__(exe);self.draws=[];self.created=[];self.width=640;self.height=480
  for a in [0x4af970,0x4af97a,0x4ad8ec,0x428bb6,0x428caf,0x466805,0x466814,0x43e583,0x443bb8,0x443be5,0x443daa,0x43e96a]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def floats(self,a,*v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def hook(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<5I',u.mem_read(sp,20));value=1
  if a==0x4af970:value=self.width
  elif a==0x4af97a:value=self.height
  elif a==0x4ad8ec:
   data=bytes(u.mem_read(args[1],260)).split(b'\0')[0]+b'\0';u.mem_write(args[0],data)
  elif a==0x43e583:
   i=(args[0]-self.entries-0x100)//0x16c;assert 0<=i<16;self.created.append(i)
   p=self.sprites+i*0x200;u.mem_write(p,bytes(0x200));self.word(args[0],p);self.word(p+0x78,p)
   self.floats(p+0x88,1,1,1);self.floats(p+0xf4,256,256);self.word(p+0x74,1)
   uv=[(0,0),(1,0),(1,1),(0,1),(0,0),(1,1)]
   for j,(x,y) in enumerate(uv):u.mem_write(self.vertices+i*0x100+j*32,struct.pack('<4fI3f',0,0,0,1,0xffffffff,0,x,y))
  elif a==0x443bb8:
   i=(args[0]-self.sprites)//0x200;self.word(args[2],self.vertices+i*0x100)
  elif a==0x43e96a:
   i=(args[0]-self.sprites)//0x200
   points=[struct.unpack('<2f',u.mem_read(self.vertices+i*0x100+j*32,8)) for j in range(6)]
   alpha=struct.unpack('<f',u.mem_read(args[0]+0x88,4))[0]
   self.draws.append((i,points,alpha))
  #443daa is the GPU vertex-color write boundary; alpha state stays native.
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def initialize(self,g,a,enabled):
  self.created=[];self.u.mem_write(self.entries,bytes(16*0x16c));self.word(0xb54730,enabled);self.word(0x7219a8,g);self.word(0x7219ac,a);self.u.mem_write(0x5767c8,b'\1');self.floats(0x721ad0,.5)
  self.call(0x4fa7fb,b'');out=State()
  for i in range(16):
   p=self.entries+i*0x16c
   if struct.unpack('<I',self.u.mem_read(p+0x100,4))[0]:out.present|=1<<i
   out.phase[i]=self.u.mem_read(p+0x134,1)[0]
   out.x[i],out.y[i]=struct.unpack('<2f',self.u.mem_read(p+0x114,8))
  return out
 def run(self,s,seed,enabled,scale):
  self.draws=[];self.word(0xb54730,enabled);self.floats(0x721ad0,scale);self.word(0x58edd8,seed)
  for i in range(16):
   p=self.entries+i*0x16c;q=self.sprites+i*0x200
   self.word(p+0x100,q if s.present&(1<<i) else 0);self.u.mem_write(p+0x134,bytes([s.phase[i]]));self.floats(p+0x114,s.x[i],s.y[i]);self.floats(q+0x7c,s.x[i],s.y[i]);self.word(q+0x74,1)
  self.call(0x4fac10,b'');out=State.from_buffer_copy(s)
  for i in range(16):
   p=self.entries+i*0x16c;out.phase[i]=self.u.mem_read(p+0x134,1)[0];out.x[i],out.y[i]=struct.unpack('<2f',self.u.mem_read(p+0x114,8))
  return out,struct.unpack('<I',self.u.mem_read(0x58edd8,4))[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();n=Native(exe);rng=random.Random(0x4fac10)
 lib.bk_rain_initialize.argtypes=[C.POINTER(State),C.c_uint,C.c_uint,C.c_int];lib.bk_rain_draw.argtypes=[C.POINTER(State),C.POINTER(C.c_uint32),C.c_int,C.c_float,C.POINTER(Draw)]
 configs=0;draws=0;vertices=0
 for g in range(5):
  for a in range(9):
   for enabled in [0,1]:
    if g==0 and enabled:continue # snow construction belongs to its own oracle
    s=State();assert lib.bk_rain_initialize(C.byref(s),g,a,enabled)
    wanted=n.initialize(g,a,enabled);assert bytes(s)==bytes(wanted),(g,a,enabled,bytes(s),bytes(wanted));assert n.created==list(range(16)) if s.present else not n.created;configs+=1
 n.initialize(4,0,1)
 for case in range(4000):
  s=State();s.present=0xffff if case%4==0 else rng.getrandbits(16)
  for i in range(16):s.phase[i]=rng.randrange(1,4);s.x[i]=rng.uniform(-500,2000);s.y[i]=rng.uniform(-500,2000)
  seed=rng.getrandbits(32);enabled=case%9!=0;scale=C.c_float(rng.choice([.5,.625,.75,.8,1,1.5,2,1/960,.1234567])).value
  wanted,rand=n.run(s,seed,enabled,scale);actual=State.from_buffer_copy(s);r=C.c_uint32(seed);out=Draw();assert lib.bk_rain_draw(C.byref(actual),C.byref(r),enabled,scale,C.byref(out));assert bytes(actual)==bytes(wanted),(case,bytes(actual).hex(),bytes(wanted).hex());assert r.value==rand and out.count==len(n.draws)
  for sprite,got in zip(out.sprites,n.draws):
   i,points,alpha=got;assert i==sprite.slot and alpha==1
   x,y=sprite.x,sprite.y;expected=[(x,y),(x+256,y),(x+256,y+512),(x,y+512),(x,y),(x+256,y+512)]
   assert points==expected,(case,i,points,expected);vertices+=6
  draws+=out.count
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),configurations=configs,frames=4000,sprite_draws=draws,vertices=vertices,max_error=0,scope=__doc__)
 (ROOT/'local/original-rain-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
