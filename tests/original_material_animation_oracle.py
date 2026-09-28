"""Native MATA loader/key normalization, group sampler, setter and release.
Only heap allocation/free, exported ID lookup and registry detach are boundaries.
Native interpolation leaves specular/emissive W uninitialized: compare all
other fields, audit those two as explicit portable-zero policy when interpolated.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,Model,Material,Frame,Chunk,library,decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Values(C.Structure):_fields_=[('diffuse',C.c_float*4),('ambient',C.c_float*4),('specular',C.c_float*4),('emissive',C.c_float*4),('power',C.c_float)]
class Edit(C.Structure):_fields_=[('index',C.c_uint32),('id',C.c_uint32),('values',Values)]
class Track(C.Structure):_fields_=[('material_index',C.c_uint32),('material_id',C.c_uint32),('loop',C.c_uint32),('key_count',C.c_uint32)]
class Key(C.Structure):_fields_=[('time',C.c_float),('values',Values)]
def bind(lib):
 for n,args,r in [
  ('bk_material_animation_create',[C.POINTER(Model),C.c_void_p],C.c_void_p),('bk_material_animation_destroy',[C.c_void_p],None),
  ('bk_material_animation_tracks',[C.c_void_p],C.c_uint32),('bk_material_animation_track',[C.c_void_p,C.c_uint32],C.POINTER(Track)),
  ('bk_material_animation_key',[C.c_void_p,C.c_uint32,C.c_uint32],C.POINTER(Key)),
  ('bk_material_animation_sample',[C.c_void_p,C.c_float,C.c_void_p,C.c_void_p],C.c_int),
  ('bk_material_animation_restore',[C.c_void_p,C.c_void_p,C.c_void_p],C.c_int),('bk_material_animation_time',[C.c_void_p],C.c_float),
  ('bk_material_pose_create',[C.POINTER(Model),C.c_void_p],C.c_void_p),('bk_material_pose_destroy',[C.c_void_p],None),
  ('bk_material_pose_material',[C.c_void_p,C.c_uint32],C.POINTER(Material)),('bk_material_pose_values',[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p],C.c_int)]:
  f=getattr(lib,n);f.argtypes=args;f.restype=r
class Native:
 stack=0x2008000;stop=0x300f000;data=0x4000000
 def __init__(self,exe,u=None):
  self.u=machine(exe) if u is None else u;self.data=0x34000000 if u is not None else 0x4000000;self.heap=self.data+0x1000000;self.u.mem_map(self.data,0x2000000);self.cursor=self.heap;self.ids={};self.undefined={};self.events=[];self.freed=[]
  for p in [0x42d217,0x42d270,0x415ed8,0x429da0]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=p,end=p)
  self.u.hook_add(UC_HOOK_CODE,self.setter,begin=0x43041a,end=0x43041a)
 def read(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
 def word(self,p,v):self.u.mem_write(p,struct.pack('<I',v))
 def alloc(self,n):
  p=self.cursor;self.cursor+=(n+15)&~15;self.cursor+=16;assert self.cursor<self.heap+0x1000000;self.u.mem_write(p,bytes(n));return p
 def hook(self,u,a,n,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',u.mem_read(sp,8));result=0
  if a==0x42d217:result=self.alloc(arg)
  elif a==0x42d270:self.freed.append(arg)
  elif a==0x415ed8:result=self.ids.get(arg,0)
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def setter(self,u,a,n,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,mat,source=struct.unpack('<3I',u.mem_read(sp,12))
  self.undefined[mat]=ret in [0x42f921,0x42fb71]
  self.events.append((mat,ret,bytes(u.mem_read(source,68))))
 def call(self,address,args=b'',pattern=0x3e123456):
  self.u.mem_write(self.stack-4096,struct.pack('<I',pattern)*1024)
  self.u.mem_write(self.stack,struct.pack('<I',self.stop)+args);self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  self.u.emu_start(address,self.stop,count=10000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
 def load(self,payload,model):
  self.cursor=self.heap;self.ids={};self.materials=[];self.undefined={};self.events=[];self.freed=[]
  for i in range(model.material_count):
   m=model.materials[i];p=self.alloc(0xd0);source=self.alloc(68);self.u.mem_write(source,C.string_at(C.addressof(m)+Material.diffuse.offset,68))
   self.call(0x43041a,struct.pack('<II',p,source));self.word(p+4,0x3f6);self.word(p+0x6c,1);self.ids[m.id]=p;self.materials.append(p)
  self.u.mem_write(self.data,payload);self.word(self.stack-0x164,self.data);self.word(self.stack-0x148,0x3000000)
  self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x2000);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  self.u.emu_start(0x419c86,0x419e03,count=10000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x419e03
  assert self.read(self.stack-0x164)==self.data+len(payload)
  self.group=self.read(0x3000000);self.tracks=[]
  for i in range(self.read(self.group+0x78)):
   track=self.read(self.read(self.group+0x7c)+i*4);keys=[];p=self.read(track+0x7c)
   for j in range(self.read(track+0x78)):
    keys.append(bytes(self.u.mem_read(p,72)));p=self.read(p+0x4c)
   assert p==0
   self.tracks.append(dict(pointer=track,material=self.materials.index(self.read(track+0x70)),loop=self.read(track+0x84),keys=keys))
 def sample(self,time,pattern):self.events=[];self.call(0x4300e0,struct.pack('<If',self.group,time),pattern)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();vm=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);rng=random.Random(0x42f5f8)
 records=[];samples=keys=tracks=compared=policy=rejects=0;worst=0.;policy_values=set()
 def run(model,payload,label,steps):
  nonlocal samples,keys,tracks,compared,policy,rejects,worst
  vm.load(payload,model.contents);animation=lib.bk_material_animation_create(model,error);assert animation,(label,error.value);pose=lib.bk_material_pose_create(model,error);assert pose,(label,error.value)
  try:
   assert lib.bk_material_animation_tracks(animation)==len(vm.tracks)
   times={0.,.125,1.,1.25}
   for i,t in enumerate(vm.tracks):
    got=lib.bk_material_animation_track(animation,i).contents
    assert (got.material_index,got.loop,got.key_count)==(t['material'],t['loop'],len(t['keys']))
    for j,want in enumerate(t['keys']):
     key=lib.bk_material_animation_key(animation,i,j).contents;assert bytes(key)==want,(label,i,j,'key');keys+=1
     times.update([key.time,max(0,key.time-.001),key.time+.001,key.time+.999,key.time+1,key.time*2+.25])
    tracks+=1
   def check(where):
    nonlocal compared,policy,worst
    assert lib.bk_material_animation_time(animation)==struct.unpack('<f',vm.u.mem_read(vm.group+0x74,4))[0]
    for i,p in enumerate(vm.materials):
     got=lib.bk_material_pose_material(pose,i).contents;raw=C.string_at(C.addressof(got)+Material.diffuse.offset,68);want=bytes(vm.u.mem_read(p+0x70,68))
     g,w=struct.unpack('<17f',raw),struct.unpack('<17f',want)
     for j in range(17):
      if vm.undefined[p] and j in [11,15]:
       assert g[j]==0;policy_values.add(want[j*4:j*4+4].hex());policy+=1;continue
      d=abs(g[j]-w[j])/max(1,abs(w[j]));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,where,i,j,g[j],w[j],d)
      compared+=1
     original=model.contents.materials[i];assert got.id==original.id and got.name==original.name and bytes(C.c_float(got.unknown))==bytes(C.c_float(original.unknown))
   check('load');times=sorted(times)
   for step in range(steps):
    time=C.c_float(times[step%len(times)] if step%3 else rng.uniform(0,max(times)+30)).value
    if step%9==4:time=lib.bk_material_animation_time(animation)
    vm.sample(time,[0x3e123456,0x40012345,0xbe654321][step%3]);assert lib.bk_material_animation_sample(animation,time,pose,error),(label,error.value);check(step);samples+=1
    if step%13==0:
     # External material setter and same-time group skip share one pose.
     i=step%model.contents.material_count;m=model.contents.materials[i];vals=[rng.uniform(-.5,1.5) for _ in range(17)];edit=Edit(i,m.id,Values.from_buffer_copy(struct.pack('<17f',*vals)))
     source=vm.alloc(68);vm.u.mem_write(source,bytes(edit.values));vm.call(0x43041a,struct.pack('<II',vm.materials[i],source));assert lib.bk_material_pose_values(pose,C.byref(edit),1,error)
     vm.sample(time,0x3f123456);assert lib.bk_material_animation_sample(animation,time,pose,error);check('same after setter')
     saved=[bytes(lib.bk_material_pose_material(pose,j).contents) for j in range(model.contents.material_count)];old=lib.bk_material_animation_time(animation)
     assert not lib.bk_material_animation_sample(animation,float('nan'),pose,error)
     assert saved==[bytes(lib.bk_material_pose_material(pose,j).contents) for j in range(model.contents.material_count)] and old==lib.bk_material_animation_time(animation);rejects+=1
   vm.call(0x42ffbb,struct.pack('<I',vm.group));assert lib.bk_material_animation_restore(animation,pose,error);check('release restore')
  finally:lib.bk_material_pose_destroy(pose);lib.bk_material_animation_destroy(animation)
 # Actual MATA-bearing models, including backgrounds and facial assets.
 for path in sorted(a.data.glob('*.pp')):
  arc=Archive(path)
  for entry in arc.entries:
   if not entry.name.lower().endswith('.x'):continue
   raw=arc.read(entry)
   if not raw.startswith(b'OBJM'):continue
   pos=12;payload=None
   while pos<len(raw):
    size=struct.unpack_from('<I',raw,pos+4)[0]
    if raw[pos:pos+4]==b'MATA':payload=raw[pos+8:pos+8+size]
    pos+=8+size
   if payload is None:continue
   ok,m,msg=decode(lib,raw);assert ok,msg
   try:run(m,payload,(path.name,entry.name),120)
   finally:lib.bk_model_destroy(m)
   records.append(dict(pack=path.name,name=entry.name,sha256=hashlib.sha256(raw).hexdigest()));print('PASS',path.name,entry.name,flush=True)
 # Deliberately unsorted/duplicate float-rounded times, nonboolean loops,
 # zero/single-key tracks and repeated material targets; not asset assumptions.
 for case in range(60):
  mats=(Material*2)();frame=Frame();frame.parent_index=frame.mesh_index=0xffffffff
  for i in range(2):mats[i].id=i+1;mats[i].diffuse[:]=[.2,.4,.7,1.00001];mats[i].power=3
  payload=bytearray(72);struct.pack_into('<I',payload,68,3)
  for t in range(3):
   ts=[[],[0],[20,0,5,5,1],[10,30,1],[16777217,16777216,0]][(case+t)%5]
   payload+=struct.pack('<4I',t%2+1,0xffffffff,len(ts),[0,1,7,0xffffffff][(case+t)%4])
   for time in ts:
    vals=[rng.uniform(-2,3) for _ in range(17)];payload+=struct.pack('<i17f',time,*vals)
  data=(C.c_ubyte*len(payload)).from_buffer_copy(payload);chunk=Chunk(b'MATA',0,len(payload));model=Model();model.source=data;model.source_size=len(payload);model.chunks=C.pointer(chunk);model.chunk_count=1;model.materials=mats;model.material_count=2;model.frames=C.pointer(frame);model.frame_count=1
  run(C.pointer(model),bytes(payload),('synthetic',case),100)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),models=len(records),synthetic=60,samples=samples,tracks=tracks,keys=keys,defined_field_comparisons=compared,undefined_w_policy_checks=policy,observed_undefined_w_patterns=sorted(policy_values),atomic_rejections=rejects,max_normalized_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-material-animation-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(records),samples,keys,compared,policy,worst,flush=True)
if __name__=='__main__':main()
