"""Seven actual BOM auxiliaries: native40168c/4021a1 ANIM+MORP and publication.
MATA and other effect slots are absent in this fixture: this does not claim
complete model dispatch or the full ending loader. Native SRT, MORP, clocks,
group/track cache/mask and hidden guards are executed, not substituted.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from original_actor_clip_edits_oracle import Native as ActorNative
from original_actor_phase_oracle import bind as bind_actor
from original_morph_pose_oracle import bind as bind_morph
from playback_binding import library
from clip_binding import State,Sample
from model_binding import ROOT,Model,Vertex,decode
from bk3_assets import Archive
class Visibility(C.Structure):_fields_=[('frame',C.c_uint32),('hidden',C.c_uint32)]
def bind(lib):
 bind_actor(lib);bind_morph(lib)
 lib.bk_actor_pose_create_loaded.argtypes=[C.POINTER(Model),C.c_void_p,C.c_uint32,C.POINTER(C.c_float),C.c_float,C.c_void_p];lib.bk_actor_pose_create_loaded.restype=C.c_void_p
 lib.bk_actor_pose_request_active.argtypes=[C.c_void_p,C.c_uint,C.c_void_p]
 lib.bk_actor_pose_advance_frame.argtypes=[C.c_void_p,C.POINTER(Sample),C.POINTER(C.c_int),C.c_void_p]
 lib.bk_actor_pose_visibility.argtypes=[C.c_void_p,C.POINTER(Visibility),C.c_size_t,C.c_void_p]
 lib.bk_morph_group_create.argtypes=[C.POINTER(Model),C.c_void_p];lib.bk_morph_group_create.restype=C.c_void_p
 lib.bk_morph_group_destroy.argtypes=[C.c_void_p]
 lib.bk_morph_group_sample.argtypes=[C.c_void_p,C.c_float,C.POINTER(C.c_uint32),C.c_size_t,C.c_void_p]
 lib.bk_morph_group_mesh.argtypes=[C.c_void_p,C.c_uint32];lib.bk_morph_group_mesh.restype=C.c_void_p
 lib.bk_morph_group_time.argtypes=[C.c_void_p];lib.bk_morph_group_time.restype=C.c_float
 lib.bk_morph_group_tracks.argtypes=[C.c_void_p];lib.bk_morph_group_tracks.restype=C.c_uint32
class Native(ActorNative):
 def __init__(self,exe):
  super().__init__(exe);self.uc.mem_map(0x30000000,0x2000000);self.cursor=0x30000000;self.meshes={};self.mesh_ids={};self.group_morph=0
  for a in [0x445a8e,0x445ac1]:self.uc.hook_add(UC_HOOK_CODE,self.lock,begin=a,end=a)
 def alloc(self,data):
  address=self.cursor;self.cursor+=(len(data)+15)&~15;self.cursor+=16;assert self.cursor<0x32000000
  if data:self.uc.mem_write(address,data)
  return address
 def lock(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,mesh=struct.unpack('<2I',u.mem_read(sp,8));assert mesh in self.meshes
  if a==0x445a8e:
   cp,vp=struct.unpack('<2I',u.mem_read(sp+8,8));count,vertices=self.meshes[mesh];self.word(cp,count);self.word(vp,vertices)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def morph(self,lib,model,morph):
  tracks=[]
  for i in range(lib.bk_model_morph_count(morph)):
   info=lib.bk_model_morph_track(morph,i).contents
   if info.submesh not in self.mesh_ids:
    m=model.submeshes[info.submesh];mesh=self.alloc(bytes(0x120));vertices=self.alloc(C.string_at(m.vertices,m.vertex_count*60));self.meshes[mesh]=(m.vertex_count,vertices);self.mesh_ids[info.submesh]=mesh;self.word(mesh+0x84,m.vertex_count)
   # Include the native +0x9c 401-entry time-bucket cache; zero entries fall back to first key.
   mesh=self.mesh_ids[info.submesh];track=self.alloc(bytes(0x800));keys=self.alloc(bytes(info.key_count*0x60));tracks.append(track)
   self.word(track+0x70,mesh);self.word(track+0x7c,info.key_count);self.word(track+0x80,keys);self.word(track+0x84,keys+(info.key_count-1)*0x60);self.word(track+0x88,1)
   for j in range(info.key_count):
    key=lib.bk_model_morph_key(morph,i,j).contents;vertex_data=self.alloc(C.string_at(key.vertices,key.vertex_count*60));ptr=keys+j*0x60
    self.uc.mem_write(ptr,struct.pack('<fIII',key.time,key.interpolation,key.vertex_count,vertex_data));self.word(ptr+0x18,keys+(j+1)*0x60 if j+1<info.key_count else 0);self.uc.mem_write(ptr+0x1c,bytes(key.matrix))
   self.vector(track+0x78,[key.time])
  self.group_morph=self.alloc(bytes(0x100));array=self.alloc(struct.pack('<'+'I'*len(tracks),*tracks));self.mask=self.alloc(bytes(len(tracks)*4));self.count=len(tracks)
  self.word(self.group_morph+0x78,len(tracks));self.word(self.group_morph+0x80,array);self.word(self.group_morph+0x88,self.mask);self.word(self.model+0x168,self.group_morph)
 def set_mask(self,mask):
  self.word(self.group_morph+0x84,mask is not None)
  if mask is not None:self.uc.mem_write(self.mask,bytes(mask))

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--materials',action='store_true');a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256);matrices=vertices=frames=hidden=0;worst=0.;records=[];material_checks=0
 if a.materials:
  from original_material_animation_oracle import Native as MaterialNative,bind as bind_material
  bind_material(lib)
 def equal(got,want,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(got,want)):
   if g==w:continue
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(label,i,g,w,d)
 for pack,name in [(8,f'h{i:02}_01') for i in range(1,6)]+[(11,'h03_30'),(11,'h03_31')]:
  arc=Archive(a.data/f'bk3_{pack:02}.pp');raw=arc.read(next(x for x in arc.entries if x.name==name+'.x'));xan=arc.read(next(x for x in arc.entries if x.name==name+'.xan'));ok,model,msg=decode(lib,raw);assert ok,msg
  clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value;pose=group=morph=material_animation=material_pose=None;material_vm=None
  try:
   vm=Native(exe);vm.bind(raw,model.contents);seed=struct.unpack_from('<i',xan,512+0x140)[0];requested=struct.unpack_from('<i',xan,512+0x148)[0];assert seed==requested
   vm.bind_clip(xan,vm.root,seed,False);origin=(C.c_float*3)(0,0,0);vm.place_actor(origin,0)
   pose=lib.bk_actor_pose_create_loaded(model,clips,vm.root,origin,0,error);assert pose,error.value
   morph=lib.bk_model_morph_create(model,error);assert morph,error.value;group=lib.bk_morph_group_create(model,error);assert group,error.value;vm.morph(lib,model.contents,morph)
   if a.materials:
    chunks=[model.contents.chunks[i] for i in range(model.contents.chunk_count) if model.contents.chunks[i].tag==b'MATA']
    if chunks:
     c=chunks[0];material_vm=MaterialNative(exe,u=vm.uc);material_vm.load(raw[c.offset:c.offset+c.size],model.contents);vm.word(vm.model+0x158,material_vm.group)
     material_animation=lib.bk_material_animation_create(model,error);material_pose=lib.bk_material_pose_create(model,error);assert material_animation and material_pose,error.value
   vm.publish();lib.bk_actor_pose_publish(pose)
   def check(label):
    nonlocal matrices,vertices,material_checks
    state=State();assert lib.bk_actor_pose_state(pose,C.byref(state));equal([getattr(state,k) for k,_ in State._fields_],vm.state(),(name,label,'clock'))
    for i in range(model.contents.frame_count):
     for getter,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0)]:
      equal(getattr(lib,getter)(pose,i)[:16],vm.floats(vm.frames+i*0x400+off,16),(name,label,i,getter));matrices+=1
    if material_vm:
     from model_binding import Material
     assert lib.bk_material_animation_time(material_animation)==struct.unpack('<f',vm.uc.mem_read(material_vm.group+0x74,4))[0]
     for i,p in enumerate(material_vm.materials):
      mat=lib.bk_material_pose_material(material_pose,i).contents;got=struct.unpack('<17f',C.string_at(C.addressof(mat)+Material.diffuse.offset,68));want=struct.unpack('<17f',vm.uc.mem_read(p+0x70,68))
      for j in range(17):
       if material_vm.undefined[p] and j in [11,15]:assert got[j]==0
       else:equal([got[j]],[want[j]],(name,label,'material',i,j));material_checks+=1
    assert lib.bk_morph_group_time(group)==vm.floats(vm.group_morph+0x74,1)[0]
    for index,p in vm.mesh_ids.items():
     count,ptr=vm.meshes[p];mesh=lib.bk_morph_group_mesh(group,index);assert mesh
     got=C.string_at(lib.bk_morph_mesh_vertices(mesh),count*60);want=bytes(vm.uc.mem_read(ptr,count*60))
     if got!=want:
      for i in range(count):
       equal(struct.unpack_from('<9f',got,i*60),struct.unpack_from('<9f',want,i*60),(name,label,index,i));assert got[i*60+36:(i+1)*60]==want[i*60+36:(i+1)*60]
     vertices+=count
   check('loaded')
   active=[i for i in range(128) if lib.bk_clip_definition(clips,i).contents.active]
   for step in range(120):
    slot=0 if step==0 else active[(step//15)%len(active)]
    if step%15==0:
     vm.call(0x40168c,struct.pack('<II',vm.clip,slot));assert lib.bk_actor_pose_request_active(pose,slot,error),error.value
    hide=step%11==5;vis=Visibility(vm.root,7 if hide else 0);vm.call(0x423a99,struct.pack('<II',vm.frames+vm.root*0x400,vis.hidden));assert lib.bk_actor_pose_visibility(pose,C.byref(vis),1,error)
    mask=None if step%4==0 else (C.c_uint32*vm.count)(*[0 if (step+i)%3==0 else (0xffffffff if i%2 else 1) for i in range(vm.count)])
    vm.set_mask(mask);sample=Sample(13,71,82,93);submitted=C.c_int(-1)
    vm.call(0x4021a1,struct.pack('<I',vm.clip));assert lib.bk_actor_pose_advance_frame(pose,C.byref(sample),C.byref(submitted),error),error.value
    assert submitted.value==int(not hide)
    if not hide:
     if material_vm:assert lib.bk_material_animation_sample(material_animation,sample.from_tick,material_pose,error),error.value
     assert sample.blend==0 and lib.bk_morph_group_sample(group,sample.from_tick,mask,vm.count if mask is not None else 0,error),error.value
    else:assert bytes(sample)==bytes(Sample(13,71,82,93));hidden+=1
    check((step,'held'))
    if step%5==0:vm.publish();lib.bk_actor_pose_publish(pose);check((step,'published'))
    frames+=1
   # Same-time mask changes must not replay earlier skipped tracks.
   time=lib.bk_morph_group_time(group);vm.set_mask(None);vm.call(0x433a7d,struct.pack('<If',vm.group_morph,time));assert lib.bk_morph_group_sample(group,time,None,0,error);check('repeated-time')
   records.append(dict(pack=pack,name=name,tracks=vm.count,frames=120,sha256=hashlib.sha256(raw).hexdigest()));print('PASS fixed actor',records[-1],flush=True)
  finally:
   if a.materials:lib.bk_material_pose_destroy(material_pose);lib.bk_material_animation_destroy(material_animation)
   lib.bk_morph_group_destroy(group);lib.bk_model_morph_destroy(morph);lib.bk_actor_pose_destroy(pose);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,hidden_frames=hidden,matrices=matrices,vertices=vertices,max_normalized_error=worst,assets=records,material_checks=material_checks,scope=__doc__ if not a.materials else 'Actual native4021a1 ANIM/MATA/MORP dispatch on7 auxiliaries, hidden and publication; other effect slots absent; interpolated material specular/emissive W portable0 policy.')
 (ROOT/('local/original-auxiliary-animation-oracle.json' if a.materials else 'local/original-fixed-actor-oracle.json')).write_text(json.dumps(result,indent=2)+'\n');print('PASS',frames,matrices,vertices,worst,flush=True)
if __name__=='__main__':main()
