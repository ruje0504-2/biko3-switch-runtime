"""CPU white-alpha glyph masks vs original4747e0/474a58 placement and source
bitmap bytes. Plain full-rectangle overwrite follows the original BltFast
call flags. Clipping to canvas bounds is an explicit portable policy, not a
claim about DirectDraw's response to an out-of-bounds blit. No scrolling/UI.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_font_oracle import Native,Layout
from original_message_oracle import Message
from model_binding import ROOT,library
from bk3_assets import Archive
class Image(C.Structure):_fields_=[('width',C.c_uint32),('height',C.c_uint32),('rgba',C.c_void_p)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();raw=(a.data/'Type_S.FTT').read_bytes();n=Native(exe,raw);lib=library();error=C.create_string_buffer(256);rng=random.Random(0x475514)
 lib.bk_font_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_font_decode.restype=C.c_void_p;lib.bk_font_destroy.argtypes=[C.c_void_p]
 lib.bk_text_image.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(Layout),C.c_uint32,C.c_uint32,C.POINTER(Image),C.c_void_p]
 lib.bk_image_free.argtypes=[C.POINTER(Image)]
 lib.bk_message_lookup.argtypes=[C.c_void_p,C.c_size_t,C.c_int32,C.POINTER(Message),C.c_void_p]
 font=lib.bk_font_decode(raw,len(raw),error);assert font,error.value
 images=pixels=0
 def check(text,layout,width,height):
  nonlocal images,pixels
  image=Image();assert lib.bk_text_image(font,text,len(text),C.byref(layout),width,height,C.byref(image),error),error.value
  try:
   n.layout(text,layout);calls=[c for c in n.surface_calls if c[1] and c[2]];assert len(calls)==len(n.blits)
   expected=bytearray(b'\xff\xff\xff\0'*(width*height))
   for (bits,w,h),blit in zip(calls,n.blits):
    x,y=struct.unpack('<2i',struct.pack('<2I',*blit[1:3]));assert blit[4:]==(0,0,w,h,0)
    pitch=w//8;offset=bits-0x4100000+0x60004
    for yy in range(max(0,y),min(height,y+h)):
     for xx in range(max(0,x),min(width,x+w)):
      value=raw[offset+(yy-y)*pitch+(xx-x)//8]
      expected[(yy*width+xx)*4+3]=255 if value&(0x80>>((xx-x)%8)) else 0
   actual=C.string_at(image.rgba,width*height*4);assert actual==expected,(images,next((i for i,(x,y) in enumerate(zip(actual,expected)) if x!=y),None))
   images+=1;pixels+=width*height
  finally:lib.bk_image_free(C.byref(image))
 try:
  archive=Archive(a.data/'bk3_05.pp');text=archive.read(next(e for e in archive.entries if e.name=='i00_00.txt'))
  for group in range(5):
   for item in range(5):
    msg=Message();assert lib.bk_message_lookup(text,len(text),group*10000+item,C.byref(msg),error)
    check(bytes(msg.bytes[:msg.length]),Layout(0,0,632,32,32),632,96)
  for i in range(320):
   text=b''.join(rng.choice([b'\x81\x75',b'\x81\x76',b'\r\n',b'\nX',(rng.randrange(0x8000,0x10000)).to_bytes(2,'big')]) for _ in range(rng.randrange(1,60)))
   check(text,Layout(rng.randrange(-20,20),rng.randrange(-20,20),128,rng.choice([-5,0,1,16,32]),rng.choice([-8,0,16,32])),160,96)
 finally:lib.bk_font_destroy(font)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),images=images,pixels=pixels,scope=__doc__)
 (ROOT/'local/original-text-image-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
