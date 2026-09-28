"""Original dialogue panel/prompt construction, fade geometry and font rectangle."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBP
from original_item_notice_layout_oracle import Native as Base,Sprite
from original_opening_prompt_oracle import Pulse
from original_item_notice_oracle import Fade
from original_dialogue_ui_oracle import Ui
from model_binding import ROOT,library
class Style(C.Structure):_fields_=[('x',C.c_int32),('y',C.c_int32),('width',C.c_int32),('height',C.c_int32),('step_x',C.c_int32),('step_y',C.c_int32),('shadow',C.c_int32),('opacity',C.c_float),('color',C.c_float*3),('distance',C.c_float)]
class Native(Base):
 addresses=[0x734058,0x7341c4]
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4ef803,0x4f0b1e,0x4aa32e]:self.u.hook_add(UC_HOOK_CODE,self.extra,begin=a,end=a)
 def extra(self,u,a,size,_):
  if a in [0x4ef803,0x4f0b1e]:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<9I',u.mem_read(sp,36));self.font=tuple(C.c_int32(x).value for x in args[1:]);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def initialize(self,width,direction):
  self.width=width;self.height=width*3//4;self.created=[];self.draws=[];u=self.u
  u.mem_write(0x721ad0,struct.pack('<f',C.c_float(width/1280).value));u.mem_write(self.stack-0x65c,b'bk3_00.pp\0');u.mem_write(self.stack-0xa7c,b'Type_S.FTT\0');u.mem_write(0x734324,bytes([direction]));u.reg_write(UC_X86_REG_EBP,self.stack);self.call(0x4ef72a,b'')
  u.reg_write(UC_X86_REG_EBP,self.stack);self.call(0x4f0a8e,b'')
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();lib.bk_dialogue_ui_layout.argtypes=[C.POINTER(Sprite),C.c_uint];lib.bk_dialogue_ui_text_style.argtypes=[C.POINTER(Style)];lib.bk_dialogue_ui_initialize.argtypes=[C.POINTER(Ui)]
 draws=vertices=0
 for width in [1,320,640,960,1024,1279,1280,16384]:
  for direction in [0,1,2,255]:
   n.initialize(width,direction);assert n.created==[0,1]
   s=Ui();s.prompt.direction=direction;lib.bk_dialogue_ui_initialize(C.byref(s));assert s.prompt.direction==n.u.mem_read(0x734324,1)[0]
   style=Style();assert lib.bk_dialogue_ui_text_style(C.byref(style));assert (style.x,style.y,style.width,style.height,style.step_x,style.step_y,-1)==n.font
   q=(Sprite*2)();assert lib.bk_dialogue_ui_layout(q,width)
   for i in range(2):
    p=n.addresses[i];v=q[i];name=bytes(n.u.mem_read(p,256)).split(b'\0')[0];w,h,x,y=struct.unpack('<4f',n.u.mem_read(p+0x10c,16));assert(name,x,y,w,h)==(v.name,v.x,v.y,v.width,v.height)
    f=s.panel if i==0 else s.prompt.fade
    assert (f.alpha,f.speed,f.stage)==(struct.unpack('<f',n.u.mem_read(p+0x12c,4))[0],struct.unpack('<f',n.u.mem_read(p+0x138,4))[0],n.u.mem_read(p+0x134,1)[0])
    # Layout draw checks hold alpha explicitly; full idle11 transition was
    # already tested by the complete4f0e44 oracle and pulse regression.
    for c in range(10):
     n.u.mem_write(p+0x14a,b'\0');f=Fade(c/9,2,c%6);points=n.run(i,f,c%2,C.c_float(.1).value)
     coords=[(x,y),(C.c_float(x+w).value,y),(C.c_float(x+w).value,C.c_float(y+h).value),(x,C.c_float(y+h).value),(x,y),(C.c_float(x+w).value,C.c_float(y+h).value)]
     for point,xy in zip(points,coords):
      assert point[:2]==xy
      vertices+=1
     draws+=1
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),configurations=32,draws=draws,vertices=vertices,max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-ui-layout-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
