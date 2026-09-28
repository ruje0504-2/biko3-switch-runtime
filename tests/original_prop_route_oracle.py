"""Original515145 closed prop routes, including original4b66eb empty query.
No hooks. Tests the unusual old-position facing weight, retained velocity on
snap, independently wrapped indices, crossing event and zero-cycle rejection.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import *
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Motion(C.Structure):
 _fields_=[('position',C.c_float*3),('yaw',C.c_float),('velocity',C.c_float*3),('cursor',C.c_int32),('first',C.c_int32),('last',C.c_int32)]
class Effects(C.Structure):_fields_=[('crossed',C.c_int32),('last_crossed',C.c_int32),('blocked',C.c_int32)]
class Native:
 actor,stack,stop=0x3000000,0x2008000,0x300f000
 def __init__(self,exe):self.u=machine(exe)
 def call(self,address,args):
  u=self.u;u.mem_write(self.stack,struct.pack('<I',self.stop)+args);u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(address,self.stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
 def route(self,raw):self.u.mem_write(0xbf4b60,raw+bytes(1280-len(raw)))
 def run(self,m,distance):
  u=self.u;u.mem_write(self.actor,bytes(0x998));u.mem_write(self.actor+0x29c,bytes(m.position));u.mem_write(self.actor+0x2ac,struct.pack('<f',m.yaw));u.mem_write(self.actor+0x2b4,bytes(m.velocity));u.mem_write(self.actor+0x830,struct.pack('<3i',m.cursor,m.first,m.last));u.mem_write(0x726640,bytes(12));u.mem_write(0xbf4b54,struct.pack('<i',-99))
  self.call(0x515145,struct.pack('<Iif',self.actor,0,distance))
  out=Motion((C.c_float*3)(*struct.unpack('<3f',u.mem_read(self.actor+0x29c,12))),struct.unpack('<f',u.mem_read(self.actor+0x2ac,4))[0],(C.c_float*3)(*struct.unpack('<3f',u.mem_read(self.actor+0x2b4,12))),*struct.unpack('<3i',u.mem_read(self.actor+0x830,12)))
  crossed=u.reg_read(UC_X86_REG_EAX)&255
  event=struct.unpack('<i',u.mem_read(0xbf4b54,4))[0]
  return out,crossed,event

def bind(lib):
 lib.bk_prop_route_step.argtypes=[C.POINTER(Motion),C.c_void_p,C.c_float,C.c_void_p,C.c_int32,C.POINTER(Effects),C.c_void_p]
 lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_route_decode.restype=C.c_void_p
 lib.bk_route_destroy.argtypes=[C.c_void_p]
 lib.bk_segments_intersect_xz.argtypes=[C.POINTER(C.c_int)]+[C.POINTER(C.c_float)]*4

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x515145);err=C.create_string_buffer(256);count=crossings=rejects=0;worst=0
 def run(points,label):
  nonlocal count,crossings,worst,rejects
  raw=b''.join(struct.pack('<4fB3x',*p) for p in points)+bytes(20);r=lib.bk_route_decode(raw,len(raw),err);assert r,err.value;n.route(raw)
  try:
   for case in range(96):
    cursor=case%len(points);p=points[max(0,cursor-1)];position=[p[0]+rng.uniform(-2,2),rng.uniform(-100,100),p[2]+rng.uniform(-2,2)]
    if case%8==0:position=[*points[cursor][:3]]
    first=rng.randrange(len(points));distance=C.c_float([0,.1,1,3,16,48,120,500][case%8]).value
    m=Motion((C.c_float*3)(*position),rng.uniform(-720,720),(C.c_float*3)(0 if case%3 else 2,-11,0 if case%3 else -3),cursor,first,len(points)-1)
    old=Motion.from_buffer_copy(m);out=Effects()
    ok=lib.bk_prop_route_step(C.byref(m),r,distance,None,0,C.byref(out),err)
    if not ok:assert bytes(m)==bytes(old);rejects+=1;continue
    wanted,crossed,last=n.run(old,distance)
    assert (out.crossed,out.blocked)==(crossed,0)
    assert out.last_crossed==(last if crossed else 0)
    assert (m.cursor,m.first,m.last)==(wanted.cursor,wanted.first,wanted.last)
    for a,b in zip([*m.position,m.yaw,*m.velocity],[*wanted.position,wanted.yaw,*wanted.velocity]):
     error=abs(a-b)/max(1,abs(b));worst=max(worst,error);assert math.isfinite(error) and error<3e-6,(label,case,'value',a,b,error,list(old.position),old.cursor,old.first,distance)
    count+=1;crossings+=crossed
  finally:lib.bk_route_destroy(r)
 files=[]
 for path in sorted(args.data.glob('l93_*.ckp')):
  raw=path.read_bytes()[:1280];points=[]
  for off in range(0,len(raw),20):
   p=struct.unpack_from('<4fB3x',raw,off)
   if not any(p):break
   points.append(p)
  if len(points)>=2:run(points,path.name);files.append(path.name)
 for case in range(160):
  points=[]
  for i in range(rng.randrange(2,18)):
   if i and rng.randrange(4)==0:points.append(points[-1]);continue
   points.append((rng.uniform(-400,400),rng.uniform(-20,20),rng.uniform(-400,400),rng.uniform(-360,360),rng.choice([0,0,0,1,2,3,5,255])))
  run(points,('synthetic',case))
 for case in range(12000):
  pts=[(C.c_float*3)(rng.uniform(-1e4,1e4),rng.uniform(-100,100),rng.uniform(-1e4,1e4)) for _ in range(4)]
  if case%5==0:pts[3]=pts[0]
  if case%7==0:pts[2]=pts[0];pts[3]=pts[1]
  argsraw=b''.join(struct.pack('<7f',*pts[i],*pts[i+1],0) for i in [0,2]);n.call(0x4ae8ff,argsraw);wanted=n.u.reg_read(UC_X86_REG_EAX)&255
  hit=C.c_int(-1);assert lib.bk_segments_intersect_xz(C.byref(hit),*pts);assert hit.value==wanted,(case,hit.value,wanted)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),route_steps=count,crossings=crossings,rejections=rejects,intersection_queries=12000,max_relative_error=worst,files=files,hooks=[],scope=__doc__)
 (ROOT/'local/original-prop-route-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
