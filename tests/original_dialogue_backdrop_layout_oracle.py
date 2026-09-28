"""Original dialogue image/curtain construction and draw geometry.

Executes4f0c3c..4f0cb1,4e7033..4e70b0 and full4f19ec with original50de40,
43ed45,443daa,50e633/50e6ba. Only resource/vertex-lock/draw services supplied.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_item_notice_layout_oracle import Native as Base
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class Native(Base):
 addresses=[0x73449c,0x729c10]
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4f0cb1,0x4e70b0,0x50e7c1]:self.u.hook_add(UC_HOOK_CODE,self.extra,begin=a,end=a)
 def extra(self,u,a,size,_):
  if a in [0x4f0cb1,0x4e70b0]:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def initialize(self,width,height,replacement):
  self.width,self.height=width,height;self.created=[];self.draws=[];u=self.u
  # Both scale helpers are original x86; their returned values are also
  # stored internally at708848/84c. Obtain the same rounded global stores.
  for address,source,dest in [(0x4ad790,0x708848,0xbeecf4),(0x4ad7be,0x70884c,0xbeecf8),(0x4ad7ec,0x708848,0x721ad0)]:
   self.call(address,b'');u.mem_write(dest,bytes(u.mem_read(source,4)))
  u.mem_write(0x5767c8,b'\1');u.mem_write(0xbefeec,b'SG10000.bmp\0');u.mem_write(self.stack-0x65c,b'\\bk3_00.pp\0');u.mem_write(self.stack-0x904,b'\\bk3_00.pp\0')
  u.reg_write(UC_X86_REG_EBP,self.stack);self.call(0x4e7033,b'')
  u.reg_write(UC_X86_REG_EBP,self.stack);self.call(0x4f0c3c,b'')
  if replacement:self.call(0x4f19ec,struct.pack('<I',0xbefeec))
  assert self.created==([1,0,0] if replacement else [1,0])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();lib.bk_dialogue_backdrop_extent.argtypes=[C.POINTER(C.c_float),C.c_uint,C.c_int];lib.bk_fade_sprite_step.argtypes=[C.POINTER(Fade),C.c_uint8,C.c_float]
 configs=draws=vertices=0
 for width,height in [(1,1),(320,240),(640,480),(960,720),(1024,768),(1279,719),(1280,720),(16384,8192)]:
  for replacement in [0,1]:
   n.initialize(width,height,replacement)
   for slot in range(2):
    q=(C.c_float*2)();assert lib.bk_dialogue_backdrop_extent(q,width,replacement if slot==0 else 0)
    addr=n.addresses[slot];w,h,x,y=struct.unpack('<4f',n.u.mem_read(addr+0x10c,16));assert (x,y,w,h)==(0,0,*q),(width,slot,replacement,(x,y,w,h),list(q))
    name=bytes(n.u.mem_read(addr,256)).split(b'\0')[0];assert name==(b'SG10000.bmp' if slot==0 else b'ma_02.tga'),name
    # Construction must not reset the external image request latch.
    if slot==0:assert n.u.mem_read(addr+0x167,1)[0]==1
    for case in range(30):
     f=Fade((case%11)/10,2,case%6);dt=C.c_float([0,1/60,.1,.5,1][case%5]).value;wanted=case%2
     points=n.run(slot,f,wanted,dt);assert lib.bk_fade_sprite_step(C.byref(f),wanted,dt)
     coords=[(0,0),(w,0),(w,h),(0,h),(0,0),(w,h)]
     for p,xy in zip(points,coords):
      assert p[:2]==xy and p[4]==int(f.alpha*255)<<24|0xffffff,(width,replacement,slot,case,p,xy,f.alpha)
      vertices+=1
     draws+=1
   configs+=1
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),configurations=configs,draws=draws,vertices=vertices,max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-backdrop-layout-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
