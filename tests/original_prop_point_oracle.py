"""Original4b6fa3 yaw extraction and matrix products, dynamic collision point."""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from original_matrix_oracle import machine
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from model_binding import ROOT,library

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);a=p.parse_args();exe=a.exe.read_bytes();u=machine(exe);lib=library();rng=random.Random(0x4b6fa3)
 fp=C.POINTER(C.c_float);lib.bk_collision_prop_point.argtypes=[fp,fp,fp,fp];stack=0x200e000;outp=0x3000000;stop=0x300f000;worst=0;rejects=0
 for case in range(12000):
  y=rng.uniform(-math.pi,math.pi);s,c=math.sin(y),math.cos(y)
  world=(C.c_float*16)(c,0,-s,0,0,1,0,0,s,0,c,0,3,4,5,1)
  if case%3==0:
   for i in range(12):world[i]=rng.uniform(-1,1)
   world[15]=rng.choice([0,.5,1,2,-1,rng.uniform(-2,2)])
  point=(C.c_float*3)(*[rng.uniform(-100,100) for _ in range(3)]);position=(C.c_float*3)(rng.uniform(-1000,1000),float('nan'),rng.uniform(-1000,1000))
  u.mem_write(stack,struct.pack('<II',stop,outp)+bytes(world)+bytes(point)+bytes(48));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(0x4b6fa3,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
  m=struct.unpack('<16f',u.mem_read(outp,64));expected=[C.c_float(m[12]+position[0]).value,m[13],C.c_float(m[14]+position[2]).value]
  out=(C.c_float*3)();assert lib.bk_collision_prop_point(out,point,world,position)
  for actual,wanted in zip(out,expected):
   d=abs(actual-wanted)/max(1,abs(wanted));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(case,list(out),expected,d)
  if case%100==0:
   held=bytes(out);world[8]=float('inf');assert not lib.bk_collision_prop_point(out,point,world,position);assert bytes(out)==held;rejects+=1
 r=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=12000,max_relative_error=worst,atomic_rejections=rejects,hooks=[],scope=__doc__+' Prop Y deliberately NaN: native ignores it. Includes shear/nonunit homogeneous W and yaw wrap.')
 (ROOT/'local/original-prop-point-oracle.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
if __name__=='__main__':main()
