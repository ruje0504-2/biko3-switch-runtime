"""Whole4b26b8 append /4b2f41 cleanup with ten actual prop collision models.

Allocation and named-frame lookup are service boundaries. Original selection,
mesh copying, yaw transform, normal calculation and cleanup execute x86.
Published world and prop positions are explicit fixtures, not prop simulation.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_collision_oracle import Native as StaticNative,bind
from model_binding import ROOT,Model,library,decode
from bk3_assets import Archive
class Prop(C.Structure):
 _fields_=[('active',C.c_int32),('kind',C.c_int32),('model',C.POINTER(Model)),('world',C.POINTER(C.c_float)),('world_floats',C.c_size_t),('model_name',C.c_char_p),('position',C.c_float*3)]
class Native(StaticNative):
 def __init__(self,exe):
  super().__init__(exe);self.u.hook_add(UC_HOOK_CODE,self.free,begin=0x46da06,end=0x46da06)
 def free(self,u,a,size,data):
  sp=u.reg_read(UC_X86_REG_ESP);ret,p=struct.unpack('<II',u.mem_read(sp,8));self.freed.append(p);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def boundary(self,u,a,size,data):
  if a!=0x425904:return super().boundary(u,a,size,data)
  sp=u.reg_read(UC_X86_REG_ESP);ret,root,name,out=struct.unpack('<4I',u.mem_read(sp,16));frame=self.lookups[root,self.string(name)];u.mem_write(out,struct.pack('<I',frame));u.reg_write(UC_X86_REG_EAX,frame);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def prepare(self,props):
  self.cursor,self.allocations,self.lookups,self.freed=0x4500000,{}, {}, []
  u=self.u;u.mem_write(self.scene,bytes(12));u.mem_write(0x729d80,bytes(16*0x998))
  for i,p in enumerate(props):
   base=0x729d80+i*0x998
   if not p.active:continue
   clip=self.alloc(0x180);meta=self.alloc(0x20);root=self.alloc(0x280)
   u.mem_write(base,struct.pack('<I',clip));u.mem_write(base+0xc,struct.pack('<i',p.kind));u.mem_write(base+0x29c,bytes(p.position));u.mem_write(clip+0x160,struct.pack('<I',meta));u.mem_write(meta+0x14,struct.pack('<I',root))
   if p.kind not in [0,1,10,13,14,15,16,17,18,19]:continue
   model=p.model.contents
   wanted=self.string(0x557a74+p.kind*256)
   fi=next(j for j in range(model.frame_count) if bytes(model.frames[j].name)==wanted);f=model.frames[fi];frame=self.alloc(0x280);self.lookups[root,wanted]=frame
   u.mem_write(frame+0x80,bytes((C.c_float*16)(*p.world[fi*16:fi*16+16])));u.mem_write(frame+0xc0,bytes((C.c_float*16)(*p.world[fi*16:fi*16+16])))
   mesh=model.meshes[f.mesh_index];children=[]
   for k in range(mesh.submesh_count):
    s=model.submeshes[mesh.first_submesh+k];child=self.alloc(0x200);v=self.alloc(s.vertex_count*60);idx=self.alloc(s.index_count*2)
    name=bytes(mesh.name)+b'@'+p.model_name if mesh.submesh_count==1 else self.child_name(bytes(mesh.name),k,p.model_name)
    u.mem_write(child+4,struct.pack('<I',0x3ea));u.mem_write(child+8,name+b'\0');u.mem_write(v,C.string_at(s.vertices,s.vertex_count*60));u.mem_write(idx,C.string_at(s.indices,s.index_count*2))
    for off,value in [(0x7c,v),(0x84,s.vertex_count),(0x8c,idx),(0x90,s.index_count)]:u.mem_write(child+off,struct.pack('<I',value))
    children.append(child)
   if len(children)==1:obj=children[0]
   else:
    obj=self.alloc(0x100);refs=self.alloc(len(children)*4);u.mem_write(refs,struct.pack('<'+'I'*len(children),*children));u.mem_write(obj+4,struct.pack('<I',0x3f5));u.mem_write(obj+0x70,struct.pack('<II',len(children),refs))
   u.mem_write(frame+0x244,struct.pack('<I',obj))
 def append(self):
  self.call(0x4b26b8,struct.pack('<I',self.scene),limit=400000000)
  self.fixed_count,self.count,self.meshes=struct.unpack('<III',self.u.mem_read(self.scene,12))
 def clear(self):
  self.call(0x4b2f41,struct.pack('<I',self.scene),limit=1000000)
  assert struct.unpack('<I',self.u.mem_read(self.scene+4,4))[0]==0
  assert len(self.freed)==self.count*3

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x4b26b8);error=C.create_string_buffer(256)
 lib.bk_collision_begin_props.argtypes=[C.c_void_p,C.POINTER(Prop),C.c_size_t,C.c_void_p];lib.bk_collision_end_props.argtypes=[C.c_void_p];lib.bk_collision_kind.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_int8)]
 arc=Archive(args.data/'bk3_07.pp');files={e.name:e for e in arc.entries};names=['h93_00.x','train_4ryou.x','h98_00.x','h93_10.x','h93_11.x','h93_12.x','h93_13.x','h93_14.x','h98_20.x','h98_30.x'];kinds=[0,1,10,13,14,15,16,17,18,19]
 assets=[];records=[];meshes=vertices=triangles=0;worst=0
 try:
  for name in names:
   raw=arc.read(files[name]);ok,m,msg=decode(lib,raw);assert ok,msg;world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),error);assets.append((m,world));records.append(dict(name=name,sha256=hashlib.sha256(raw).hexdigest()))
  empty=bytes(4293124);m,world=assets[0];c=lib.bk_collision_create(m,world,len(world),b'BASE.X',empty,len(empty),error);assert c,error.value
  try:
   for case in range(120):
    props=(Prop*16)();keeps=[]
    for i in range(16):
     j=(case+i)%10;m,base=assets[j];world=(C.c_float*len(base))(*base);keeps.append(world)
     # Held pose with explicit yaw/translation/nonunit-W on the named frame.
     yaw=rng.uniform(-math.pi,math.pi);sn,cs=math.sin(yaw),math.cos(yaw)
     for fi in range(m.contents.frame_count):
      for off,value in [(0,cs),(2,-sn),(8,sn),(10,cs),(12,99),(13,77),(14,55),(15,[1,1,.5,2][case%4])]:world[fi*16+off]=value
     props[i]=Prop(0 if (case+i)%7==0 else 1,kinds[j],m,world,len(world),names[j].upper().encode(),(C.c_float*3)(rng.uniform(-500,500),rng.uniform(-30,30),rng.uniform(-500,500)))
     if (case+i)%11==0:props[i].kind=11 # active but not collision-enabled
    n.prepare(props);n.append();assert lib.bk_collision_begin_props(c,props,16,error),error.value
    assert lib.bk_collision_count(c)==n.count
    for i in range(n.count):
     actual=lib.bk_collision_mesh(c,i).contents;want=n.mesh_data(i);assert actual.name==want['name'];assert list(actual.indices[:actual.index_count])==list(want['indices'])
     pairs=list(zip([x for v in actual.vertices[:actual.vertex_count] for x in v],[x for v in want['vertices'] for x in v]))+list(zip([x for v in actual.normals[:actual.index_count//3] for x in v],want['normals']))
     for a,b in pairs:
      d=abs(a-b)/max(1,abs(b));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(case,i,a,b,d)
     kind=C.c_int8();assert lib.bk_collision_kind(c,i,C.byref(kind));assert kind.value==C.c_int8(n.u.mem_read(n.meshes+i*0x11c+0x118,1)[0]).value
     meshes+=1;vertices+=actual.vertex_count;triangles+=actual.index_count//3
    n.clear();lib.bk_collision_end_props(c);assert lib.bk_collision_count(c)==0
   print('PASS 120 dynamic batches and cleanup',flush=True)
  finally:lib.bk_collision_destroy(c)
 finally:
  for m,w in assets:lib.bk_model_destroy(m)
 r=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),batches=120,prop_slots=1920,meshes=meshes,vertices=vertices,triangles=triangles,max_relative_error=worst,assets=records,scope=__doc__)
 (ROOT/'local/original-dynamic-collision-oracle.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
if __name__=='__main__':main()
