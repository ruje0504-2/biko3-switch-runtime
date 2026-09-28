"""Original4e82b8 notice resource construction through4e8bcf, including
50de40 and actual sprite dimension/position/alpha setters. Full43ed45 geometry
and443daa color packing execute; file/texture, vertex lock and draw boundaries
are supplied. Checks reload suppression and all25 icon names at five sizes.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_item_notice_oracle import Fade,State,snapshot
from model_binding import ROOT,library
class Sprite(C.Structure):_fields_=[('name',C.c_char_p),('x',C.c_float),('y',C.c_float),('width',C.c_float),('height',C.c_float)]
class Native(Base):
 addresses=[0xbeeb88]+[0x733708+i*0x16c for i in range(5)]+[0xbef608]
 sprites,meshes,vertices=0x3000000,0x3002000,0x3004000
 def __init__(self,exe):
  super().__init__(exe);self.width=640;self.height=480
  for a in [0x4af970,0x4af97a,0x4ad8ec,0x428bb6,0x428caf,0x466805,0x466814,0x43e583,0x443bb8,0x443be5,0x43e96a,0x4c9bf0,0x4e8bcf]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def floats(self,a,*v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def hook(self,u,a,size,_):
  if a==0x4e8bcf:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<5I',u.mem_read(sp,20));value=1
  if a==0x4af970:value=self.width
  elif a==0x4af97a:value=self.height
  elif a==0x4ad8ec:u.mem_write(args[0],bytes(u.mem_read(args[1],260)).split(b'\0')[0]+b'\0')
  elif a==0x43e583:
   i=self.addresses.index(args[0]-0x100);self.created.append(i)
   p=self.sprites+i*0x200;m=self.meshes+i*0x100;u.mem_write(p,bytes(0x200));u.mem_write(m,bytes(0x100));self.word(args[0],p);self.word(p+0x78,m);self.word(m+0x7c,6)
   self.floats(p+0x88,1,1,1);self.floats(p+0xf4,1,1);self.word(p+0x74,1)
   for j,(x,y) in enumerate([(0,0),(1,0),(1,1),(0,1),(0,0),(1,1)]):u.mem_write(self.vertices+i*0x100+j*32,struct.pack('<4fI3f',0,0,0,1,0xffffffff,0,x,y))
  elif a==0x443bb8:self.word(args[2],self.vertices+(args[0]-self.meshes)//0x100*0x100)
  elif a==0x43e96a:
   i=(args[0]-self.sprites)//0x200
   self.draws.append((i,[struct.unpack('<4fI3f',u.mem_read(self.vertices+i*0x100+j*32,32)) for j in range(6)]))
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def initialize(self,group,width,retain):
  self.created=[];self.draws=[];self.width=width;self.height=width*3//4
  self.word(0x7219a8,group);self.floats(0x721ad0,C.c_float(width/1280).value);self.u.mem_write(0x5767c8,b'\1');self.u.mem_write(0xbef778,bytes([retain]));self.call(0x4e82b8,b'')
 def run(self,i,fade,wanted,dt):
  p=self.addresses[i];self.floats(p+0x12c,fade.alpha);self.floats(self.sprites+i*0x200+0x88,-99) # force native packed-color update
  self.floats(p+0x138,fade.speed);self.u.mem_write(p+0x134,bytes([fade.stage]));self.floats(0x733700,dt);self.draws=[]
  self.call(0x50e633,struct.pack('<II',p,wanted));self.call(0x50e6ba,struct.pack('<II',p,0));assert len(self.draws)==1
  return self.draws[0][1]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x50de40)
 lib.bk_item_notice_layout.argtypes=[C.POINTER(Sprite),C.c_uint,C.c_uint];lib.bk_fade_sprite_step.argtypes=[C.POINTER(Fade),C.c_uint8,C.c_float]
 configs=frames=vertices=retained=0
 for width in [320,640,800,960,1279]:
  for group in range(5):
   layout=(Sprite*6)();assert lib.bk_item_notice_layout(layout,group,width);n.initialize(group,width,0);assert n.created==[0,6,1,2,3,4,5]
   for addr,entry in zip(n.addresses,layout):
    name=bytes(n.u.mem_read(addr,256)).split(b'\0')[0];w,h,x,y=struct.unpack('<4f',n.u.mem_read(addr+0x10c,16))
    assert (name,x,y,w,h)==(entry.name,entry.x,entry.y,entry.width,entry.height)
    assert struct.unpack('<f',n.u.mem_read(addr+0x12c,4))[0]==1 and n.u.mem_read(addr+0x134,1)[0]==0
   for case in range(48):
    i=case%6;fade=Fade(rng.random(),rng.choice([0,.1,2,3,80]),rng.randrange(6));dt=C.c_float(rng.choice([0,1/60,.1,.25,1,3])).value;wanted=rng.choice([0,1,2,255]);points=n.run(i,fade,wanted,dt)
    assert lib.bk_fade_sprite_step(C.byref(fade),wanted,dt)
    q=layout[i];x,y,w,h=q.x,q.y,q.width,q.height;f32=lambda v:C.c_float(v).value
    coords=[(x,y),(f32(x+w),y),(f32(x+w),f32(y+h)),(x,f32(y+h)),(x,y),(f32(x+w),f32(y+h))]
    for point,xy in zip(points,coords):
     assert point[:2]==xy,(width,group,case,point[:2],xy)
     assert point[4]==(int(fade.alpha*255)<<24)|0xffffff,(width,group,case,point[4],fade.alpha)
     vertices+=1
    frames+=1
   before=[bytes(n.u.mem_read(p,0x16c)) for p in n.addresses[1:6]]
   n.initialize((group+1)%5,width,1);assert n.created==[0,6]
   assert before==[bytes(n.u.mem_read(p,0x16c)) for p in n.addresses[1:6]];retained+=5;configs+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),configurations=configs,frames=frames,vertices=vertices,retained_icons=retained,max_error=0,scope=__doc__)
 (ROOT/'local/original-item-notice-layout-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
