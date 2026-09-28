"""Original474a58 glyph metrics and4747e0 pairwise text layout; glyph surface
creation/blits/releases and scrolling are boundary hooks. Native475514 bitmap
expansion is separately run with only allocation/DirectDraw surface hooks.
Source bytes and lookup cover all65536 codes; duplicate source bitmaps share
one expansion case. No offset/layout/bit-expansion instructions are replaced.
"""
import argparse,ctypes as C,hashlib,json,random,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from original_message_oracle import Message
from model_binding import ROOT,library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Glyph(C.Structure):
 _fields_=[('bits',C.c_void_p),('row_bytes',C.c_uint32),('width',C.c_uint32),('height',C.c_uint32),('advance_width',C.c_uint32),('offset_x',C.c_int32),('offset_y',C.c_int32),('advance',C.c_int32)]
class Layout(C.Structure):_fields_=[(n,C.c_int32) for n in ['x','y','width','step_x','step_y']]
class TextGlyph(C.Structure):_fields_=[('code',C.c_uint16),('x',C.c_int32),('y',C.c_int32),('glyph',Glyph)]
class Native(Base):
 def __init__(self,exe,raw):
  super().__init__(exe);u=self.u;u.mem_map(0x4000000,0x600000)
  u.mem_write(0x6a3bfc,raw[:4]);u.mem_write(0x6a3c00,struct.pack('<I',0x4000000));u.mem_write(0x6a3c04,struct.pack('<I',0x4100000))
  u.mem_write(0x4060000,raw[4:0x60004]);u.mem_write(0x4100000,raw[0x60004:])
  # Fake DirectDraw object and a released temporary glyph surface.
  self.device=0x4500000;self.surface=0x4501000;self.pixels=0x4520000
  u.mem_write(0x6455a4,struct.pack('<I',self.device));u.mem_write(self.device,struct.pack('<I',self.device+0x100));u.mem_write(self.surface,struct.pack('<I',self.surface+0x100))
  for table,off,addr in [(self.device,0x18,0x300e100),(self.surface,0x64,0x300e110),(self.surface,0x80,0x300e120),(self.surface,8,0x300e130)]:u.mem_write(table+0x100+off,struct.pack('<I',addr))
  for addr in [0x475514,0x450794,0x450738,0x474be3,0x52043e,0x520538,0x300e100,0x300e110,0x300e120,0x300e130]:u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
  self.expand=False
 def hook(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',u.mem_read(sp,40));ret=args[0];a=args[1:];value=0;pop=0
  if addr==0x475514:
   if self.expand:return
   bits,width,height=a[:3];self.surface_calls.append((bits,width,height));value=self.surface if width and height else 0
  elif addr==0x450794:self.blits.append(a[:9])
  elif addr==0x450738:self.clears+=1
  elif addr==0x52043e:
   assert a[0]<65536;value=0x4510000;u.mem_write(value,bytes(a[0]+16))
  elif addr==0x520538:assert a[0]==0x4510000
  elif addr==0x300e100:
   device,desc,out,outer=a[:4];assert device==self.device and outer==0
   height,width=struct.unpack('<II',u.mem_read(desc+8,8));self.pitch=width*2
   assert width<=4096 and height<=4096
   u.mem_write(self.pixels,bytes(max(self.pitch*height,16)))
   u.mem_write(out,struct.pack('<I',self.surface));pop=16
  elif addr==0x300e110:
   surf,rect,desc,flags,event=a[:5];assert surf==self.surface and flags==0x801
   u.mem_write(desc+0x10,struct.pack('<I',self.pitch));u.mem_write(desc+0x24,struct.pack('<I',self.pixels));pop=20
  elif addr==0x300e120:assert a[:2]==(self.surface,0);pop=8
  elif addr==0x300e130:assert a[0]==self.surface;pop=4
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def glyph(self,code):
  self.blits=[];self.surface_calls=[];self.u.mem_write(0x3001000,bytes([code>>8,code&255]))
  self.call(0x474a58,struct.pack('<4I',0x1234,100,200,0x3001000));return self.u.reg_read(UC_X86_REG_EAX)
 def layout(self,raw,layout):
  self.u.mem_write(0x3001000,raw+b'\0'*8);self.u.mem_write(0x69ebb0,b'changed\0');self.u.mem_write(0x6a13f0,struct.pack('<2i',-123456,-123456))
  x,y,w,sx,sy=[getattr(layout,n) for n in ['x','y','width','step_x','step_y']]
  desc=struct.pack('<6i',x,y,w,256,sx,sy)+bytes(0x34-24)+struct.pack('<I',0x3001000)
  self.u.mem_write(0x3005000,desc);self.blits=[];self.surface_calls=[];self.clears=0
  self.call(0x4747e0,struct.pack('<I',0x3005000));assert self.clears==1
  return [(struct.unpack('<i',struct.pack('<I',a[1]))[0],struct.unpack('<i',struct.pack('<I',a[2]))[0],a[6],a[7]) for a in self.blits]
 def bitmap(self,bits,width,height):
  self.expand=True
  try:
   self.call(0x475514,struct.pack('<3I',bits,width,height));assert self.u.reg_read(UC_X86_REG_EAX)==self.surface
   return b''.join(bytes(self.u.mem_read(self.pixels+y*self.pitch,width*2)) for y in range(height))
  finally:self.expand=False

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();raw=(a.data/'Type_S.FTT').read_bytes();n=Native(exe,raw);lib=library();error=C.create_string_buffer(256);rng=random.Random(0x4747e0)
 lib.bk_font_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_font_decode.restype=C.c_void_p
 lib.bk_font_destroy.argtypes=[C.c_void_p];lib.bk_font_glyph.argtypes=[C.c_void_p,C.c_uint16,C.POINTER(Glyph)]
 lib.bk_text_layout.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(Layout),C.POINTER(TextGlyph),C.c_size_t,C.POINTER(C.c_size_t),C.c_void_p]
 lib.bk_message_lookup.argtypes=[C.c_void_p,C.c_size_t,C.c_int32,C.POINTER(Message),C.c_void_p]
 font=lib.bk_font_decode(raw,len(raw),error);assert font,error.value
 glyphs=bitmaps=pixels=layouts=placed=0;seen=set();resources=[]
 try:
  for code in range(65536):
   g=Glyph();assert lib.bk_font_glyph(font,code,C.byref(g));wanted=n.glyph(code);assert g.advance==wanted,(hex(code),g.advance,wanted)
   if code<0x8000:offset=pitch=width=height=0
   else:offset,pitch,width,height,_=struct.unpack_from('<I4H',raw,4+(code-0x8000)*12)
   assert (g.row_bytes,g.width,g.height,g.advance_width)==(pitch,pitch*8,height,width)
   assert n.surface_calls==[(0x4100000+offset,pitch*8,height)]
   if pitch and height:
    assert n.blits==[(0x1234,100+g.offset_x,200+g.offset_y,n.surface,0,0,g.width,g.height,0)],hex(code)
    b=C.string_at(g.bits,pitch*height);assert b==raw[0x60004+offset:0x60004+offset+pitch*height]
    key=(pitch,height,b)
    if key not in seen:
     seen.add(key);wanted=n.bitmap(0x4100000+offset,pitch*8,height)
     expected=b''.join((b'\xff\xff' if value&(0x80>>bit) else b'\0\0') for value in b for bit in range(8))
     assert wanted==expected,('bitmap',hex(code));bitmaps+=1;pixels+=pitch*8*height
   else:assert not g.bits and not n.blits
   glyphs+=1
   if code%8192==8191:print('glyphs',glyphs,'bitmaps',bitmaps,flush=True)
  def check(text,layout,label):
   nonlocal layouts,placed
   out=(TextGlyph*(len(text)+1))();count=C.c_size_t()
   assert lib.bk_text_layout(font,text,len(text),C.byref(layout),out,len(out),C.byref(count),error),(label,error.value)
   actual=[(g.x,g.y,g.glyph.width,g.glyph.height) for g in out[:count.value] if g.glyph.bits]
   wanted=n.layout(text,layout);assert actual==wanted,(label,actual,wanted)
   layouts+=1;placed+=count.value
  archive=Archive(a.data/'bk3_05.pp')
  for e in archive.entries:
   if e.name!='i00_00.txt':continue
   text=archive.read(e)
   for group in range(5):
    for item in range(5):
     msg=Message();assert lib.bk_message_lookup(text,len(text),group*10000+item,C.byref(msg),error)
     for width in [32,64,632]:check(bytes(msg.bytes[:msg.length]),Layout(0,0,width,32,32),(group,item,width))
  codes=[0x8140,0x8141,0x8142,0x8175,0x8176,0x824f,0x8258,0x8281,0x829a,0x82a1,0x829f,0x82c1,0x8340,0x8387,0xffff]
  for i in range(1800):
   parts=[(rng.choice(codes) if rng.randrange(2) else rng.randrange(0x8000,0x10000)).to_bytes(2,'big') if rng.randrange(5) else rng.choice([b'\r\n',b'\nX',b'\rX']) for _ in range(rng.randrange(1,70))]
   text=b''.join(parts)
   if i%7==0:text=text[:-1]
   if i%11==0:text=text[:len(text)//2]+b'\0'+text[len(text)//2:]
   layout=Layout(rng.randrange(-50,50),rng.randrange(-30,30),rng.choice([0,31,32,33,64,632]),rng.choice([-3,0,16,32,50]),rng.choice([-7,0,32,40]))
   check(text,layout,i)
  for name in ['Type_S.FTT','Type_G.FTT']:
   data=(a.data/name).read_bytes();f=lib.bk_font_decode(data,len(data),error);assert f,error.value;lib.bk_font_destroy(f)
   resources.append(dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
 finally:lib.bk_font_destroy(font)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),glyphs=glyphs,unique_bitmaps=bitmaps,pixels=pixels,layouts=layouts,placed=placed,resources=resources,scope=__doc__)
 (ROOT/'local/original-font-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
