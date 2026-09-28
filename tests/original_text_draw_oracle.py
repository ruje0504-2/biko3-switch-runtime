"""Full4758f7 pass ordering +475dc7 placement +4761af original unrotated
vertices/color quantization. Platform render-state APIs, texture lifecycle and
vertex-buffer storage/draw are hooks; native geometry/CRT math is executed.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_text_flow_oracle import Native as FlowNative
from model_binding import ROOT,library
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
class Style(C.Structure):_fields_=[(n,C.c_int32) for n in ['x','y','width','height','step_x','step_y','shadow']]+[('opacity',C.c_float),('color',C.c_float*3),('shadow_distance',C.c_float)]
class Pass(C.Structure):_fields_=[('blend',C.c_int),('x',C.c_float),('y',C.c_float),('width',C.c_float),('height',C.c_float),('argb',C.c_uint32)]
class Draw(C.Structure):_fields_=[('count',C.c_uint),('passes',Pass*4)]
class Native(FlowNative):
 def __init__(self,exe):
  super().__init__(exe);u=self.u
  self.word(0x300c048,0x300d000);self.word(0x300d000,0x300d100)
  self.word(0x300d10c,0x300e140);self.word(0x300d110,0x300e150)
  for addr in [0x300e140,0x300e150]:u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
 def hook(self,u,addr,size,data):
  if addr in [0x475dc7,0x4761af]:return
  if addr==0x476606:
   mode=struct.unpack('<I',u.mem_read(0x300c004,4))[0]
   vertices=[struct.unpack('<4f2I2f',u.mem_read(0x300d200+i*32,32)) for i in range(4)]
   self.draws.append((mode,vertices))
  if addr in [0x300e140,0x300e150]:
   sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];pop=4
   if addr==0x300e140:
    vbo,flags,out,unused=struct.unpack('<4I',u.mem_read(sp+4,16));assert (vbo,flags,unused)==(0x300d000,0x801,0);self.word(out,0x300d200);pop=16
   u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret);return
  super().hook(u,addr,size,data)
 def draw(self,style,width):
  u=self.u;self.draws=[];self.word(0x6a13dc,0)
  u.mem_write(0x6a13b8,struct.pack('<f',width));u.mem_write(0x3001000,b'text\0')
  u.mem_write(0x3005000,bytes(style)+struct.pack('<2I',0,0x3001000))
  self.call(0x4758f7,struct.pack('<I',0x3005000));return self.draws

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4761af)
 lib.bk_text_draw.argtypes=[C.POINTER(Style),C.c_uint32,C.POINTER(Draw)]
 calls=vertices=0;worst=0
 for i in range(6000):
  style=Style(rng.randrange(-128,641),rng.randrange(-128,481),rng.randrange(1,641),rng.randrange(1,481),16,16,rng.choice([0,1,-1,2]),rng.choice([0,1,.5,rng.random()]),(C.c_float*3)(*[rng.choice([0,1,.5,rng.random()]) for _ in range(3)]),rng.choice([0,1,-1,2,0.5,3.25]))
  width=rng.choice([320,640,960,1280,1920,3840,1377]);wanted=n.draw(style,width);out=Draw();assert lib.bk_text_draw(C.byref(style),width,C.byref(out))
  assert out.count==len(wanted)
  for p,(mode,verts) in zip(out.passes[:out.count],wanted):
   assert (p.blend,p.argb)==(1 if mode==1 else 0,verts[0][4]),(i,p.argb,hex(verts[0][4]))
   coordinates=[(p.x,p.y),(C.c_float(p.x+p.width).value,p.y),(C.c_float(p.x+p.width).value,C.c_float(p.y+p.height).value),(p.x,C.c_float(p.y+p.height).value)]
   for k,(v,(x,y)) in enumerate(zip(verts,coordinates)):
    err=max(abs(v[0]-x),abs(v[1]-y));worst=max(worst,err);assert err==0,(i,k,v[:2],x,y)
    assert v[2:6]==(0.,1.,p.argb,0)
    assert v[6:]==[(0.,0.),(1.,0.),(1.,1.),(0.,1.)][k]
   vertices+=4;calls+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=6000,draw_calls=calls,vertices=vertices,max_error=worst,scope=__doc__)
 (ROOT/'local/original-text-draw-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
