"""Original4b61e5 with real collision meshes and synthetic edge sequences.
Original4aed7a/4ae8ff/CRTcos/sqrt execute. Singular original slope math is
reported separately; port skips that edge and records a diagnostic.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_collision_oracle import Mesh,Vector,bind,Archive
from model_binding import ROOT,library,decode
class State(C.Structure):_fields_=[('distance',C.c_float),('point',Vector),('singular',C.c_uint32)]
class Native(Base):
 mesh,actor,camera,vertices,indices,normals=0x3100000,0x3102000,0x3103000,0x3110000,0x3610000,0x3810000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x3100000,0x900000)
 def bind(self,m):
  u=self.u
  u.mem_write(self.mesh,bytes(0x11c));u.mem_write(self.mesh+0x104,struct.pack('<5I',m.vertex_count,m.index_count,self.vertices,self.indices,self.normals))
  assert m.vertex_count*60<0x500000 and m.index_count*4<0x200000
  raw=bytearray(m.vertex_count*60)
  for i in range(m.vertex_count):raw[i*60:i*60+12]=bytes(m.vertices[i])
  if raw:u.mem_write(self.vertices,bytes(raw))
  if m.index_count:
   u.mem_write(self.indices,C.string_at(m.indices,m.index_count*4));u.mem_write(self.normals,C.string_at(m.normals,m.index_count*4))
 def run(self,state,actor,camera):
  u=self.u;u.mem_write(self.actor,bytes(0x900));u.mem_write(self.actor+0x298,struct.pack('<f',18));u.mem_write(self.actor+0x29c,bytes(actor))
  u.mem_write(self.camera,bytes(0x600));u.mem_write(self.camera+0x420,bytes(camera));u.mem_write(self.camera+0x43c,struct.pack('<f',state.distance));u.mem_write(self.camera+0x524,bytes(state.point))
  u.mem_write(self.stack,struct.pack('<4I',self.stop,self.mesh,self.actor,self.camera));u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(0x4b61e5,self.stop,count=50000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
  out=State.from_buffer_copy(state);out.distance=struct.unpack('<f',u.mem_read(self.camera+0x43c,4))[0];out.point[:]=struct.unpack('<3f',u.mem_read(self.camera+0x524,12))
  return out,u.reg_read(UC_X86_REG_EAX)&255

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x4b61e5);err=C.create_string_buffer(256)
 lib.bk_follow_obstacle_mesh.argtypes=[C.POINTER(State),C.POINTER(Mesh),Vector,Vector,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_game_follow_obstacles_enabled.argtypes=[C.c_int32,C.c_int32]
 cases=hits=singular=defined=0;worst=0
 def run(mesh,actor,camera,distance,label):
  nonlocal cases,hits,singular,defined,worst
  s=State(distance,Vector(13,19,23),0);wanted,wh=n.run(s,actor,camera);hit=C.c_int()
  assert lib.bk_follow_obstacle_mesh(C.byref(s),C.byref(mesh),actor,camera,C.byref(hit),err),(label,err.value)
  cases+=1;hits+=hit.value
  if s.singular or not all(math.isfinite(v) for v in [wanted.distance,*wanted.point]):singular+=1;return
  assert hit.value==wh,(label,'hit',hit.value,wh,list(actor),list(camera))
  for a,b in zip([s.distance,*s.point],[wanted.distance,*wanted.point]):
   delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta);assert delta<3e-6,(label,a,b,delta)
  defined+=1
 for case in range(2400):
  vertices=(Vector*9)();indices=(C.c_uint32*9)(*range(9));normals=(Vector*3)()
  for i in range(9):vertices[i][:]=[rng.uniform(-100,100),rng.uniform(-1e5,1e5),rng.uniform(-100,100)]
  for i in range(3):normals[i][1]=rng.choice([0,.5,.500171,-1,1])
  actor=Vector(rng.uniform(-90,90),rng.uniform(-5,5),rng.uniform(-90,90));camera=Vector(rng.uniform(-90,90),rng.uniform(-5,5),rng.uniform(-90,90))
  if case%8==0:
   vertices[0][:]=[-20,1000,0];vertices[1][:]=[20,1000,0];vertices[2][:]=[20,-1000,10]
   actor[:]=[0,7,-20];camera[:]=[0,17,20];normals[0][1]=0
  mesh=Mesh(b'fixture',9,9,vertices,indices,normals);n.bind(mesh);run(mesh,actor,camera,rng.choice([0,20,200]),('synthetic',case))
 arc=Archive(args.data/'bk3_03.pp');entries={e.name.lower():e for e in arc.entries};scenes=[];real_meshes=0
 for file in sorted(args.data.glob('*.atr')):
  ok,m,msg=decode(lib,arc.read(entries[file.with_suffix('.x').name]));assert ok,msg
  world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),err)
  raw=file.read_bytes();collision=lib.bk_collision_create(m,world,len(world),file.with_suffix('.x').name.upper().encode(),raw,len(raw),err);assert collision,err.value
  try:
   count=lib.bk_collision_count(collision)
   for mi in range(count):
    mesh=lib.bk_collision_mesh(collision,mi).contents;n.bind(mesh);real_meshes+=1
    for sample in range(4):
     at=rng.randrange(mesh.vertex_count);v=mesh.vertices[at]
     actor=Vector(v[0]+rng.uniform(-100,100),v[1]+20,v[2]+rng.uniform(-100,100));camera=Vector(v[0]+rng.uniform(-100,100),v[1]+30,v[2]+rng.uniform(-100,100))
     run(mesh,actor,camera,200,(file.name,mi,sample))
   scenes.append(file.name);print(file.name,'PASS',count,flush=True)
  finally:lib.bk_collision_destroy(collision);lib.bk_model_destroy(m)
 gatechecks=0
 for g in range(-1,6):
  for a in range(-1,10):
   n.u.mem_write(0x7219a8,struct.pack('<2i',g,a));n.call(0x4bef54,b'');skip=n.u.reg_read(UC_X86_REG_EAX);n.call(0x4bed86,struct.pack('<2i',g,a));enabled=(not skip) and bool(n.u.reg_read(UC_X86_REG_EAX)&255)
   assert lib.bk_game_follow_obstacles_enabled(g,a)==enabled;gatechecks+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),mesh_queries=cases,defined_queries=defined,nonfinite_original_queries=singular,port_hits=hits,real_meshes=real_meshes,scenes=scenes,gate_queries=gatechecks,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-follow-obstacle-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
