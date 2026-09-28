"""Original4b66eb obstacle queries and515145 movement on actual dynamic meshes.
Native mesh construction substitutes allocation and named-frame lookup only;
queries and complete route motion execute original x86 without hooks.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import *
from original_dynamic_collision_oracle import Native,Prop
from original_collision_oracle import bind as bind_collision
from original_prop_route_oracle import Motion,Effects,bind as bind_route
from model_binding import ROOT,library,decode
from bk3_assets import Archive
V=C.c_float*3
class VM(Native):
 def query(self,kind,a,b):
  actor=0x300a000;self.u.mem_write(actor,bytes(0x998));self.u.mem_write(actor+0xc,struct.pack('<i',kind));self.u.mem_write(actor+0x29c,bytes(a));self.call(0x4b66eb,struct.pack('<II3f',self.scene,actor,*b),limit=10000000);return self.u.reg_read(UC_X86_REG_EAX)&255
 def motion(self,m,distance,kind):
  actor=0x300a000;u=self.u;u.mem_write(actor,bytes(0x998));u.mem_write(actor+0xc,struct.pack('<i',kind));u.mem_write(actor+0x29c,bytes(m.position));u.mem_write(actor+0x2ac,struct.pack('<f',m.yaw));u.mem_write(actor+0x2b4,bytes(m.velocity));u.mem_write(actor+0x830,struct.pack('<3i',m.cursor,m.first,m.last));u.mem_write(0xbf4b54,struct.pack('<i',-1));u.mem_write(0x726640,bytes(u.mem_read(self.scene,12)))
  self.call(0x515145,struct.pack('<Iif',actor,0,distance),limit=10000000)
  return Motion(V(*struct.unpack('<3f',u.mem_read(actor+0x29c,12))),struct.unpack('<f',u.mem_read(actor+0x2ac,4))[0],V(*struct.unpack('<3f',u.mem_read(actor+0x2b4,12))),*struct.unpack('<3i',u.mem_read(actor+0x830,12))),u.reg_read(UC_X86_REG_EAX)&255,struct.unpack('<i',u.mem_read(0xbf4b54,4))[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=VM(exe);lib=library();bind_collision(lib);bind_route(lib);err=C.create_string_buffer(256);rng=random.Random(0x4b66eb)
 lib.bk_collision_begin_props.argtypes=[C.c_void_p,C.POINTER(Prop),C.c_size_t,C.c_void_p];lib.bk_collision_prop_blocked.argtypes=[C.c_void_p,C.c_int32,C.POINTER(C.c_float),C.POINTER(C.c_float),C.POINTER(C.c_int),C.c_void_p]
 arc=Archive(args.data/'bk3_07.pp');names=['h93_00.x','train_4ryou.x','h98_00.x','h93_10.x','h93_11.x','h93_12.x','h93_13.x','h93_14.x','h98_20.x','h98_30.x'];kinds=[0,1,10,13,14,15,16,17,18,19];queries=hits=steps=blocked=0;worst=0
 for name,kind in zip(names,kinds):
  raw=arc.read(next(e for e in arc.entries if e.name==name));ok,m,msg=decode(lib,raw);assert ok,msg;world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),err);c=lib.bk_collision_create(m,world,len(world),b'EMPTY.X',bytes(4293124),4293124,err);assert c,err.value
  props=(Prop*1)(Prop(1,kind,m,world,len(world),name.upper().encode(),V(0,0,0)));n.prepare(props);n.append();assert lib.bk_collision_begin_props(c,props,1,err),err.value
  try:
   for i in range(600):
    mesh=lib.bk_collision_mesh(c,i%lib.bk_collision_count(c)).contents;v=mesh.vertices[i%mesh.vertex_count];a=V(v[0]+rng.uniform(-30,30),rng.uniform(-500,500),v[2]+rng.uniform(-30,30));b=V(v[0]+rng.uniform(-30,30),rng.uniform(-500,500),v[2]+rng.uniform(-30,30));querykind=kind if i%5==0 else rng.choice(kinds)
    hit=C.c_int(-1);assert lib.bk_collision_prop_blocked(c,querykind,a,b,C.byref(hit),err),err.value;wanted=n.query(querykind,a,b);assert hit.value==wanted,(name,i,querykind,hit.value,wanted);queries+=1;hits+=hit.value
    if i%3:continue
    points=[(*a,0,0),(*b,0,0),(b[0]+5,b[1],b[2]+7,90,3)];rawroute=b''.join(struct.pack('<4fB3x',*p) for p in points)+bytes(20);n.u.mem_write(0xbf4b60,rawroute+bytes(1280-len(rawroute)));r=lib.bk_route_decode(rawroute,len(rawroute),err);assert r,err.value
    try:
     old=Motion(a,rng.uniform(-180,180),V(0,0,0),1,0,2);motion=Motion.from_buffer_copy(old);distance=C.c_float([.5,3,8,30,100][i%5]).value;out=Effects();assert lib.bk_prop_route_step(C.byref(motion),r,distance,c,querykind,C.byref(out),err),err.value;want,event,last=n.motion(old,distance,querykind)
     assert motion.cursor==want.cursor and out.crossed==event and (not event or out.last_crossed==last)
     for x,y in zip([*motion.position,motion.yaw,*motion.velocity],[*want.position,want.yaw,*want.velocity]):
      diff=abs(x-y)/max(1,abs(y));worst=max(worst,diff);assert math.isfinite(diff) and diff<3e-6,(name,i,x,y,diff)
     steps+=1;blocked+=out.blocked
    finally:lib.bk_route_destroy(r)
  finally:lib.bk_collision_destroy(c);lib.bk_model_destroy(m)
  print(name,'PASS',flush=True)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),queries=queries,hits=hits,motion_steps=steps,blocked_motion_steps=blocked,max_relative_error=worst,models=names,scope=__doc__)
 (ROOT/'local/original-prop-collision-query-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
