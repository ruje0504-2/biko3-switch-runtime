"""Original4758f7/4747e0/474a58/474be3 combined text caches and scroll vs
CPU canvas. DirectDraw surfaces/blits and upload are software boundary hooks;
geometry/lookup/layout/caching/time run as original instructions. Initial
surface bytes are defined black in both fixtures. No out-of-bounds crops.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_text_flow_oracle import Native as FlowNative,Flow
from original_text_draw_oracle import Style,Draw
from original_text_image_oracle import Image
from original_message_oracle import Message
from model_binding import ROOT,library
from bk3_assets import Archive
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
class Native(FlowNative):
 def __init__(self,exe,font,width,height):
  super().__init__(exe);u=self.u;u.mem_map(0x4000000,0x600000)
  self.word(0x6a3bfc,struct.unpack_from('<I',font)[0]);self.word(0x6a3c00,0x4000000);self.word(0x6a3c04,0x4100000)
  u.mem_write(0x4060000,font[4:0x60004]);u.mem_write(0x4100000,font[0x60004:])
  self.full=bytearray(1280*960);self.viewport=bytearray(width*height);self.width=width;self.height=height
  self.word(0x6a13e4,0x4500000);self.word(0x4501000,0x4501100);self.word(0x4501108,0x300e140)
  u.hook_add(UC_HOOK_CODE,self.hook,begin=0x300e140,end=0x300e140)
  u.hook_add(UC_HOOK_CODE,self.hook,begin=0x450738,end=0x450738)
  self.word(0x6a13f0,-1);self.word(0x6a13f4,-1);self.word(0x6a13b0,-1);self.word(0x6a13b4,-1)
  u.mem_write(0x69ebb0,b'\0');u.mem_write(0x6a13f8,b'\0');self.word(0x6a3c0c,0)
 def hook(self,u,addr,size,data):
  if addr==0x4747e0:return
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',u.mem_read(sp,40));ret=args[0];a=args[1:];pop=0;value=0
  if addr==0x475514:
   bits,w,h=a[:3]
   if not bits:value=0x300b000
   elif w and h:
    b=bytes(u.mem_read(bits,w*h//8));self.glyph=(w,h,bytes(255 if v&(0x80>>i) else 0 for v in b for i in range(8)));value=0x4501000
  elif addr==0x450738:
   assert a[0]==0x4500000;self.full[:]=bytes(len(self.full));self.rasters+=1
  elif addr==0x450794:
   dest,x,y,src,sx,sy,w,h,flags=a
   x,y,sx,sy=struct.unpack('<4i',struct.pack('<4I',x,y,sx,sy));assert flags==0
   if src==0x4501000:
    gw,gh,b=self.glyph;assert (dest,sx,sy,w,h)==(0x4500000,0,0,gw,gh)
    for yy in range(max(0,y),min(960,y+h)):
     for xx in range(max(0,x),min(1280,x+w)):self.full[yy*1280+xx]=b[(yy-y)*w+xx-x]
   else:
    assert (dest,src,x,y,sx,w,h)==(0x300b000,0x4500000,0,0,0,self.width,self.height)
    assert 0<=sy<=960-h
    for yy in range(h):self.viewport[yy*w:(yy+1)*w]=self.full[(yy+sy)*1280:(yy+sy)*1280+w]
    self.crops+=1
  elif addr==0x300e140:pop=4
  else:
   if addr in [0x475e7f,0x476159]:self.uploads+=1
   return super().hook(u,addr,size,data)
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,style,text,state,seconds):
  u=self.u;self.rasters=self.crops=self.uploads=0
  u.mem_write(0x3001000,text+b'\0'*4);u.mem_write(0x3005000,bytes(style)+struct.pack('<2I',0,0x3001000))
  for addr,value in [(0x6a13d0,state.delay),(0x6a13d4,state.scroll),(0x6a13d8,state.target),(0x733700,seconds)]:u.mem_write(addr,struct.pack('<f',value))
  self.word(0x6a13dc,state.enabled);self.word(0x6a13e0,state.started);self.call(0x4758f7,struct.pack('<I',0x3005000))
  return Flow(*[struct.unpack('<f',u.mem_read(addr,4))[0] for addr in [0x6a13d0,0x6a13d4,0x6a13d8]],*[struct.unpack('<i',u.mem_read(addr,4))[0] for addr in [0x6a13dc,0x6a13e0]])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();raw=(a.data/'Type_S.FTT').read_bytes();n=Native(exe,raw,256,192);lib=library();err=C.create_string_buffer(256)
 lib.bk_font_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_font_decode.restype=C.c_void_p;lib.bk_font_destroy.argtypes=[C.c_void_p]
 lib.bk_text_canvas_create.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p];lib.bk_text_canvas_create.restype=C.c_void_p
 lib.bk_text_canvas_destroy.argtypes=[C.c_void_p];lib.bk_text_canvas_image.argtypes=[C.c_void_p];lib.bk_text_canvas_image.restype=C.POINTER(Image)
 lib.bk_text_canvas_prepare.argtypes=[C.c_void_p,C.POINTER(Style),C.c_void_p,C.c_size_t,C.c_float,C.c_uint32,C.POINTER(Flow),C.POINTER(Draw),C.POINTER(C.c_int),C.c_void_p]
 lib.bk_message_lookup.argtypes=[C.c_void_p,C.c_size_t,C.c_int32,C.POINTER(Message),C.c_void_p]
 font=lib.bk_font_decode(raw,len(raw),err);assert font,err.value;canvas=lib.bk_text_canvas_create(font,128,96,err);assert canvas,err.value
 archive=Archive(a.data/'bk3_05.pp');rawtext=archive.read(next(e for e in archive.entries if e.name=='i00_00.txt'));texts=[b'']
 for g in range(5):
  for i in range(5):
   m=Message();assert lib.bk_message_lookup(rawtext,len(rawtext),g*10000+i,C.byref(m),err);texts.append(bytes(m.bytes[:m.length]))
 flow=Flow(0,0,0,1,0);style=Style(192,400,128,96,16,16,1,1,(C.c_float*3)(1,1,1),1)
 rasters=crops=uploads=0;held=0
 try:
  for frame in range(780):
   text=texts[(frame//3)%len(texts)]
   if frame%13==0:flow.enabled=1;flow.started=0;flow.scroll=0;flow.target=(frame%7)*8
   if frame%13==4:flow.enabled=0
   if frame%13==5:flow.enabled=1;flow.started=0
   if frame%13==8:flow.enabled=1;flow.started=1;flow.delay=51
   if frame%5==0:style.x=180+frame%20
   style.step_x=16 if frame%4 else 4;style.step_y=16 if frame%9 else 0
   seconds=C.c_float([0,1/60,.1,1][frame%4]).value
   wanted=n.run(style,text,flow,seconds);draw=Draw();upload=C.c_int()
   assert lib.bk_text_canvas_prepare(canvas,C.byref(style),text,len(text),seconds,1280,C.byref(flow),C.byref(draw),C.byref(upload),err),(frame,err.value)
   assert bytes(flow)==bytes(wanted),(frame,bytes(flow),bytes(wanted))
   image=lib.bk_text_canvas_image(canvas).contents;rgba=C.string_at(image.rgba,image.width*image.height*4)
   assert rgba[::4]==n.viewport,(frame,'pixels')
   assert rgba[1::4]==n.viewport and rgba[2::4]==n.viewport and rgba[3::4]==b'\xff'*len(n.viewport)
   assert bool(upload.value)==bool(n.uploads),(frame,upload.value,n.uploads)
   rasters+=n.rasters;crops+=n.crops;uploads+=bool(n.uploads);held+=not n.crops
 finally:lib.bk_text_canvas_destroy(canvas);lib.bk_font_destroy(font)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=780,rasters=rasters,crops=crops,upload_frames=uploads,held_viewports=held,pixels=780*256*192,scope=__doc__)
 (ROOT/'local/original-text-canvas-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
