"""Run full42aa47 dispatch and42b1bd/42c3fc sorting over synthetic global queues.
GPU submission and deformation services are hooked; native camera inversion,
world*view, length/bias, exchange loops and flush branches execute unchanged.
Raw uint32 texture keys exercise the original algorithm, not a claim that
portable resource IDs reproduce Windows texture-pointer allocation order.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_prop_route_oracle import Native
from model_binding import ROOT, library

class Key(C.Structure):
 _fields_=[('sorted',C.c_uint32),('texture',C.c_uint32),('priority',C.c_uint32),('distance',C.c_float)]
F16=C.c_float*16
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class QueueVM(Native):
 ordinary,transparent,frames,meshes,camera,device,vtable,stub=0x4000000,0x4010000,0x4020000,0x4040000,0x4060000,0x4061000,0x4062000,0x4063000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x70000);self.trace=[];self.result=[]
  for a in [0x43056d,0x42af66,0x42afbb,0x42ade8,0x42b934,0x42b1bd,0x42c3fc,0x42c56a,self.stub]:
   self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.write(self.device,self.vtable);self.write(self.vtable+0x50,self.stub);self.write(0x6455a0,self.device)
 def write(self,a,v):self.u.mem_write(a,struct.pack('<I',v))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def order(self,base,count):return [(self.read(base+i*0x11c+0x100)-self.frames)//0x400 for i in range(count)]
 def hook(self,u,a,size,_):
  self.trace.append(a)
  if a==0x42c56a:
   self.distances={identity:struct.unpack('<f',u.mem_read(self.transparent+i*0x11c+0x110,4))[0] for i,identity in enumerate(self.order(self.transparent,self.tc))};return
  if a in [0x42b1bd,0x42c3fc]:return
  if a==0x42b934:self.result=self.order(self.ordinary,self.oc)+self.order(self.transparent,self.tc)
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);u.reg_write(UC_X86_REG_ESP,sp+4+(12 if a==self.stub else 0));u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
 def run(self,items,camera,enabled):
  self.trace=[];self.result=[];self.distances={};self.oc=self.tc=0
  self.u.mem_write(self.camera+0xc0,struct.pack('<16f',*camera));self.write(0x645604,self.camera)
  for i,(key,world,bias) in enumerate(items):
   frame=self.frames+i*0x400;mesh=self.meshes+i*0x200
   self.u.mem_write(frame,bytes(0x400));self.write(frame+0x70,1);self.u.mem_write(frame+0xc0,bytes(world))
   self.u.mem_write(mesh,bytes(0x200));self.write(mesh+4,0x3ea);self.write(mesh+0x98,key.texture);self.write(mesh+0xf4,key.priority);self.write(mesh+0xf8,bias)
   if key.sorted:entry=self.transparent+self.tc*0x11c;self.tc+=1
   else:entry=self.ordinary+self.oc*0x11c;self.oc+=1
   self.u.mem_write(entry,bytes(0x11c));self.u.mem_write(entry,bytes(world));self.write(entry+0x100,frame);self.write(entry+0x104,mesh)
  self.write(0x6429ac,self.oc);self.write(0x645620,self.ordinary);self.write(0x6429a8,self.tc);self.write(0x645624,self.transparent)
  self.call(0x42aa47,struct.pack('<I',enabled))
  return self.result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();vm=QueueVM(exe);lib=library()
 lib.bk_draw_distance.argtypes=[C.POINTER(C.c_float),C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_uint32]
 lib.bk_draw_order.argtypes=[C.POINTER(Key),C.c_uint32,C.POINTER(C.c_uint32),C.POINTER(C.c_float)]
 lib.bk_matrix_inverse.argtypes=[C.POINTER(C.c_float),C.POINTER(C.c_float)]
 lib.bk_draw_order_cache_create.argtypes=[C.c_uint32];lib.bk_draw_order_cache_create.restype=C.c_void_p
 lib.bk_draw_order_cache_destroy.argtypes=[C.c_void_p]
 lib.bk_draw_order_cached.argtypes=[C.c_void_p,*lib.bk_draw_order.argtypes]
 cache=lib.bk_draw_order_cache_create(33);assert cache
 rng=random.Random(0x42aa47);entries=distances=0;worst=0.;branches=set()
 for case in range(2400):
  n=case%33;items=[];camera=F16(*I);a=rng.uniform(-3,3);camera[0]=camera[10]=math.cos(a);camera[2]=-math.sin(a);camera[8]=math.sin(a)
  for j in [12,13,14]:camera[j]=rng.uniform(-1000,1000)
  view=F16();assert lib.bk_matrix_inverse(view,camera)
  for i in range(n):
   world=F16(*I)
   for j in [12,13,14]:world[j]=rng.choice([0,10,-10]) if case%4==0 else rng.uniform(-1e4,1e4)
   world[15]=rng.choice([1,.75,2,-1]);bias=rng.choice([0,0,1,99,0xffffffff])
   k=Key(case%2 if case%5==0 else rng.randrange(2),rng.choice([0,1,0xffffff,0x1000000,0x1000001,0xffffffff,rng.getrandbits(32)]),rng.randrange(5),0)
   d=C.c_float();assert lib.bk_draw_distance(C.byref(d),world,view,bias);k.distance=d.value;items.append((k,world,bias))
  keys=(Key*n)(*[x[0] for x in items]);order=(C.c_uint32*n)();scratch=(C.c_float*n)();assert lib.bk_draw_order(keys,n,order,scratch)
  cached=(C.c_uint32*n)()
  for repeat in range(3):
   assert lib.bk_draw_order_cached(cache,keys,n,cached,scratch)
   assert list(cached)==list(order),(case,repeat,list(cached),list(order))
  enabled=0 if case%19==0 else 1
  actual=vm.run(items,camera,enabled);branches.add((bool(vm.oc),enabled))
  if enabled:assert list(order)==actual,(case,list(order),actual)
  sorted_calls=vm.trace.count(0x42b1bd),vm.trace.count(0x42c3fc)
  assert sorted_calls==((1,1) if vm.oc and enabled else (0,0)),(case,sorted_calls)
  if vm.oc and enabled:
   for identity,native in vm.distances.items():
    expected=keys[identity].distance
    difference=abs(native-expected)/max(1,abs(native));worst=max(worst,difference)
    assert difference<2e-7,(case,identity,native,expected,difference);distances+=1
  entries+=n
 lib.bk_draw_order_cache_destroy(cache)
 report=dict(passed=True,cached_calls=7200,exe_sha256=hashlib.sha256(exe).hexdigest(),flushes=2400,entries=entries,native_distance_checks=distances,max_relative_distance_error=worst,branches=sorted(branches),scope=__doc__,hooks=['43056d shader setup','42af66/42afbb/42ade8 deformation services','42b934 transparent draw boundary','IDirect3DDevice::SetRenderState; ordinary frames hidden after sorting'])
 (ROOT/'local/original-draw-order-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
