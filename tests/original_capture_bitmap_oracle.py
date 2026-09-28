"""Original49c4f3+49c7e3+49c3f0 screenshot BMP bytes from D3D24/32 surfaces.
Only allocation, DirectDraw surface I/O and Windows file I/O are intercepted.
Canonical port fixes native bfSize and adds required row padding at odd widths.
"""
import argparse,ctypes as C,hashlib,io,json,random,struct
from pathlib import Path
from PIL import Image as PIL
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class Image(C.Structure):_fields_=[('width',C.c_uint32),('height',C.c_uint32),('rgba',C.c_void_p)]
class Blob(C.Structure):_fields_=[('data',C.c_void_p),('size',C.c_size_t)]
class Time(C.Structure):_fields_=[(x,C.c_uint) for x in ['year','month','day','hour','minute','second']]+[('ticks_ms',C.c_uint32)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);u=self.u;u.mem_map(0x4000000,0x2000000)
  self.surface=0x3008000;self.vtable=0x3009000;self.src=0x4100000
  self.wi(self.surface,self.vtable)
  for off,fn in [(0x58,0x300d100),(0x64,0x300d200),(0x80,0x300d300)]:self.wi(self.vtable+off,fn)
  for ptr,fn in [(0x53f208,0x300e100),(0x53f130,0x300e200),(0x53f20c,0x300e300),(0x53f114,0x300e400),(0x53f110,0x300e500),(0x53f10c,0x300e600)]:self.wi(ptr,fn)
  for a in [0x42d217,0x52043e,0x520538,0x51f2bf,0x300d100,0x300d200,0x300d300,0x300e100,0x300e200,0x300e300,0x300e400,0x300e500,0x300e600]:u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def wi(self,p,v):self.u.mem_write(p,struct.pack('<I',v))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<7I',u.mem_read(sp+4,28));pop=4;result=0
  if a in [0x42d217,0x52043e]:result=self.heap;self.heap+=(args[0]+15)&~15
  elif a==0x300d100:self.wi(args[1]+0x10,self.pitch);pop=12
  elif a==0x300d200:self.wi(args[2]+0x10,self.pitch);self.wi(args[2]+0x24,self.src);pop=24
  elif a==0x300d300:pop=12
  elif a==0x300e100:
   self.path=bytes(u.mem_read(args[0],256)).split(b'\0')[0];result=123;pop=32
  elif a==0x300e200:
   assert args[0]==123;self.written+=bytes(u.mem_read(args[1],args[2]));self.wi(args[3],args[2]);result=1;pop=24
  elif a==0x300e300:assert args[0]==123;pop=8;result=1
  elif a in [0x300e400,0x300e500]:
   t=self.time
   out=(f'{t.year:04d}_{t.month:02d}{t.day:02d}_' if a==0x300e400 else f'{t.hour:02d}{t.minute:02d}_{t.second:02d}').encode()+b'\0'
   u.mem_write(args[4],out);pop=28;result=len(out)
  elif a==0x300e600:result=self.time.ticks_ms
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def call(self,address,args):
  u=self.u;u.mem_write(self.stack,struct.pack('<I',self.stop)+args);u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(address,self.stop,count=100000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
 def run(self,w,h,rgba,bpp,group=None,time=None):
  self.time=time
  self.heap=0x5000000;self.written=b'';self.pitch=w*(bpp//8)+16
  raw=bytearray(self.pitch*h)
  for y in range(h):
   for x in range(w):
    p=(y*w+x)*4;o=y*self.pitch+x*(bpp//8);raw[o:o+3]=rgba[p:p+3][::-1]
    if bpp==32:raw[o+3]=rgba[p+3]
  self.u.mem_write(self.src,bytes(raw));self.call(0x49c4f3,struct.pack('<II',w,h));bmp=self.u.reg_read(UC_X86_REG_EAX)
  for a,v in [(0x705250,bmp),(0x704e28,w),(0x704e2c,h),(0x705244,0),(0x705248,2 if group is None else 1),(0x70524c,self.surface),(0x688294,bpp)]:self.wi(a,v)
  self.wi(0xb53954,group or 0);self.u.mem_write(0x705140,b'output/\0');self.call(0x49c7e3,b'')
  if group is None:assert self.path==b'output/sy_99.bmp'
  return self.written

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x49c7e3);e=C.create_string_buffer(256)
 lib.bk_bitmap_encode.argtypes=[C.POINTER(Image),C.POINTER(Blob),C.c_void_p];lib.bk_blob_free.argtypes=[C.POINTER(Blob)]
 lib.bk_capture_photo_name.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Time)]
 count=pixels=names=0
 for w,h in [(4,1),(8,3),(32,24),(640,480),(1,1),(3,7),(13,11),(321,123)]:
  rgba=rng.randbytes(w*h*4);buf=C.create_string_buffer(rgba);im=Image(w,h,C.cast(buf,C.c_void_p));out=Blob();assert lib.bk_bitmap_encode(C.byref(im),C.byref(out),e),e.value
  try:
   encoded=C.string_at(out.data,out.size);decoded=PIL.open(io.BytesIO(encoded)).convert('RGB').tobytes();assert decoded==b''.join(rgba[i:i+3] for i in range(0,len(rgba),4))
   for bpp in [24,32]:
    native=n.run(w,h,rgba,bpp);canonical=bytearray(native[:54]);stride=(w*3+3)&~3;data=bytearray()
    for y in range(h):data+=native[54+y*w*3:54+(y+1)*w*3]+bytes(stride-w*3)
    struct.pack_into('<I',canonical,2,54+len(data));struct.pack_into('<I',canonical,34,len(data));canonical+=data
    assert encoded==canonical,(w,h,bpp);count+=1;pixels+=w*h
  finally:lib.bk_blob_free(C.byref(out))
 # Invalid/output-owned requests are rejected without replacing live storage.
 for w,h,p in [(0,4,buf),(4,0,buf),(16385,1,buf),(1,16385,buf),(1,1,None)]:
  invalid=Image(w,h,C.cast(p,C.c_void_p) if p else None);out=Blob();assert not lib.bk_bitmap_encode(C.byref(invalid),C.byref(out),e);assert not out.data and not out.size
 for group in range(5):
  for ticks in [0,9,10,99,100,0x7fffffff,0x80000000,0xffffffff]:
   t=Time(2026,9,27,23,58,59,ticks);n.run(4,3,bytes(range(48)),32,group,t);out=C.create_string_buffer(128);assert lib.bk_capture_photo_name(out,group,C.byref(t));assert n.path==b'output/'+out.value,(n.path,out.value);names+=1
 report=dict(photo_names=names,passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),native_encodes=count,pixels=pixels,scope=__doc__)
 (ROOT/'local/original-capture-bitmap-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS capture bitmap',report)
if __name__=='__main__':main()
