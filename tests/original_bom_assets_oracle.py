"""Actual4a4dc3 binder, node lookup/math, global publication, registry mapping,
40168c/4021a1 ANIM/MATA/MORP and hiding. Filesystem/heap/refcounts and matrix
stack lifetime are boundaries; math, ordering and all animation run natively.
Explicit7 actor pairs use BOM metadata only as test fixtures, not dispatch.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_fixed_actor_oracle import Native as FixedNative,bind as bind_fixed,Visibility
from original_material_animation_oracle import Native as MaterialNative,bind as bind_material
from original_bom_oracle import Config,NAMES,OFFSETS
from model_binding import ROOT,Model,Material,decode
from playback_binding import library
from clip_binding import State
from bk3_assets import Archive
NONE=0xffffffff
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
F16=C.c_float*16
class Actor(C.Structure):_fields_=[('pose',C.c_void_p),('name',C.c_char_p),('forest_actor',C.c_uint32),('root',C.c_uint32)]
class Binding(C.Structure):_fields_=[(x,C.c_uint32) for x in ['parent','reference','child','primary_aux','secondary_aux','source','target','selection_count','missing_selection']]
class Mesh(C.Structure):_fields_=[(x,C.c_uint32) for x in ['actor','submesh','frame']]
def bind(lib):
 bind_fixed(lib);bind_material(lib)
 lib.bk_bom_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Config),C.c_void_p]
 lib.bk_actor_pose_root_local.argtypes=[C.c_void_p,C.POINTER(C.c_float),C.c_void_p]
 lib.bk_actor_pose_parent_world.argtypes=[C.c_void_p,C.c_uint32];lib.bk_actor_pose_parent_world.restype=C.POINTER(C.c_float)
 lib.bk_actor_pose_hidden.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
 for name,args,result in [('create',[C.POINTER(C.c_void_p),C.c_uint32,C.c_void_p],C.c_void_p),('destroy',[C.c_void_p],None),('node',[C.c_void_p,C.c_uint32,C.c_uint32],C.c_uint32),('attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p],C.c_int),('refresh',[C.c_void_p,C.c_void_p],C.c_int)]:
  f=getattr(lib,'bk_actor_forest_'+name);f.argtypes=args;f.restype=result
 for name,args,result in [('create',[C.c_void_p,C.c_char_p,C.POINTER(Config),C.POINTER(Actor),C.c_void_p,C.c_void_p],C.c_void_p),('destroy',[C.c_void_p],None),('binding',[C.c_void_p,C.c_uint32],C.POINTER(Binding)),('mesh',[C.c_void_p,C.c_uint32],C.POINTER(Mesh)),('mesh_count',[C.c_void_p],C.c_uint32),('mapping',[C.c_void_p,C.c_uint32,C.POINTER(C.c_int32),C.POINTER(C.POINTER(C.c_uint32)),C.POINTER(C.c_size_t)],C.c_int),('materials',[C.c_void_p],C.c_void_p),('morph',[C.c_void_p],C.c_void_p),('advance_frame',[C.c_void_p,C.c_void_p],C.c_int)]:
  f=getattr(lib,'bk_bom_assets_'+name);f.argtypes=args;f.restype=result
 lib.bk_resources_create.argtypes=[C.c_void_p];lib.bk_resources_create.restype=C.c_void_p
 lib.bk_resources_mount.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p]
 lib.bk_resources_destroy.argtypes=[C.c_void_p]
class Native(FixedNative):
 def __init__(self,exe,files):
  super().__init__(exe);self.files=files;self.uc.mem_map(0x36000000,0x200000);self.stub=0x361f0000;self.vt=self.stub+0x100
  self.uc.mem_write(self.vt,bytes(self.uc.mem_read(0x53fb10,0x48)));self.word(self.vt+8,self.stub+16)
  self.word(0x53f0f8,self.stub);self.events=[]
  for p in [self.stub,self.stub+16,0x52440d,0x46d9b4,0x42a0a5,0x42a0cd,0x4a736b,0x4a7276,0x4a71de,0x42892a,0x428a23,0x4655aa,0x4653d2]:self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=p,end=p)
  for p in [0x422c49,0x4230bd,0x423be2,0x4aaa40,0x4aab5e,0x40168c,0x4021a1,0x423a99]:self.uc.hook_add(UC_HOOK_CODE,self.observe,begin=p,end=p)
 def read(self,p):return struct.unpack('<I',self.uc.mem_read(p,4))[0]
 def string(self,p):
  b=bytearray()
  while self.uc.mem_read(p,1)!=b'\0':b+=self.uc.mem_read(p,1);p+=1
  return bytes(b)
 def observe(self,u,a,size,_):self.events.append(a)
 def boundary(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);args=[self.read(sp+4+i*4) for i in range(4)];result=0;extra=0
  if a==self.stub:result=int(self.string(args[0])!=self.string(args[1]));extra=8
  elif a==self.stub+16:extra=4
  elif a==0x52440d:
   u.mem_write(self.matrix_stack,struct.pack('<5I',self.vt,1024,self.matrices,0,1));self.vector(self.matrices,I);self.word(args[1],self.matrix_stack);extra=8
  elif a==0x46d9b4:result=self.alloc(bytes(args[0]))
  elif a==0x4a7276:result=args[0]
  elif a==0x4a71de:
   b=self.files.get(self.string(args[0]).lower())
   if b is not None:self.word(args[1],self.alloc(b));result=len(b)
  elif a==0x4655aa:result=len(self.files.get(self.string(args[1]).lower(),b''))
  elif a==0x4653d2:
   b=self.files.get(self.string(args[1]).lower())
   if b is not None and args[3]:assert args[3]==len(b);u.mem_write(args[2],b);result=1
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret)
 def setup(self,lib,models,poses,names,config,packed):
  self.primary=0x36000000;self.pclip=0x36180000;self.pmodel=self.pclip+0x6000;self.config=self.pclip+0x7000;self.output=self.config+0x5000
  self.frame_bases=[self.primary,self.frames];self.roots=[]
  for actor,m in enumerate(models):
   children={i:[] for i in range(m.frame_count)};root=next(i for i,f in enumerate(m.frames[:m.frame_count]) if f.parent_index==NONE);self.roots.append(root)
   for i in range(m.frame_count):
    f=m.frames[i];p=self.frame_bases[actor]+i*0x400
    if actor==0:self.uc.mem_write(p,bytes(0x400));self.uc.mem_write(p+8,f.name+b'\0')
    for getter,off in [('local',0x80),('frame',0xc0),('parent_world',0x100)]:self.uc.mem_write(p+off,C.string_at(getattr(lib,'bk_actor_pose_'+getter)(poses[actor],i),64))
    if f.parent_index!=NONE:children[f.parent_index].append(i)
   if actor==0:
    links=0x36100000
    for parent,items in children.items():
     p=self.primary+parent*0x400;self.word(p+0x238,len(items));self.word(p+0x230,links+items[0]*16 if items else 0)
     for j,i in enumerate(items):self.word(links+i*16,self.primary+i*0x400);self.word(links+i*16+8,links+items[j+1]*16 if j+1<len(items) else 0);self.word(self.primary+i*0x400+0x22c,p)
  self.word(self.pclip+0x160,self.pmodel);self.word(self.pmodel+0x14,self.primary+self.roots[0]*0x400)
  roots=[self.frame_bases[i]+self.roots[i]*0x400 for i in range(2)];links=0x36170000
  self.word(self.context+0x238,2);self.word(self.context+0x230,links)
  for i,p in enumerate(roots):self.word(links+i*16,p);self.word(links+i*16+8,links+16 if i==0 else 0);self.word(p+0x22c,self.context)
  self.registry={};table=self.alloc(bytes(sum(m.submesh_count for m in models)*0x88));entry=0
  for actor,m in enumerate(models):
   for s in range(m.submesh_count):
    sm=m.submeshes[s];frame=next(i for i in range(m.frame_count) if m.frames[i].mesh_index==sm.mesh_index)
    if actor==1 and s in self.mesh_ids:p=self.mesh_ids[s]
    else:
     p=self.alloc(bytes(0x120));vertices=self.alloc(C.string_at(sm.vertices,sm.vertex_count*60));self.meshes[p]=(sm.vertex_count,vertices)
    self.word(p+0x100,self.frame_bases[actor]+frame*0x400);self.registry[actor,s]=p
    name=C.create_string_buffer(256);assert lib.bk_model_submesh_name(C.pointer(m),s,names[actor],name,C.create_string_buffer(256));self.uc.mem_write(table+entry*0x88,name.value+b'\0');self.word(table+entry*0x88+0x80,p);self.word(table+entry*0x88+0x84,0x3ea);entry+=1
  self.word(0x645614,table);self.word(0x645618,entry)
  self.word(self.config,config.count)
  if packed:self.uc.mem_write(self.config+4,b'fixture.pp\0')
  for i,b in enumerate(config.bindings[:config.count]):
   for field,off in zip(NAMES,OFFSETS):self.uc.mem_write(self.config+off+i*260,getattr(b,field)+b'\0')
  self.word(self.config+0x4208,config.mode);self.events=[]
 def call(self,address,args):
  self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args);self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
  self.uc.emu_start(address,self.stop,count=2000000000);assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop,(hex(address),hex(self.uc.reg_read(UC_X86_REG_EIP)))
 def initialize(self):self.call(0x4a4dc3,struct.pack('<5I',self.config,self.output,self.pclip,self.clip,0))

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--limit',type=int,default=7);ap.add_argument('--motion',action='store_true');ap.add_argument('--seconds',action='store_true');a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind(lib);err=C.create_string_buffer(256)
 if a.seconds:
  lib.bk_bom_assets_advance.argtypes=[C.c_void_p,C.c_float,C.c_void_p]
  lib.bk_actor_pose_request.argtypes=[C.c_void_p,C.c_uint,C.c_void_p]
 if a.motion:
  import bom_motion_binding
  bom_motion_binding.bind(lib)
 lib.bk_model_submesh_name.argtypes=[C.POINTER(Model),C.c_uint32,C.c_char_p,C.c_void_p,C.c_void_p]
 store=lib.bk_resources_create(err);assert store;arcs={i:Archive(a.data/f'bk3_{i:02}.pp') for i in [8,11]}
 for i in arcs:assert lib.bk_resources_mount(store,f'bk3_{i:02}'.encode(),str(a.data/f'bk3_{i:02}.pp').encode(),err)
 bom=Archive(a.data/'fambom.pp');records=[];matrices=vertices=maps=frames=material_checks=0;worst=0.
 def equal(x,y,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(x,y)):
   if g==w:continue
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(label,i,g,w,d)
 for entry in [e for e in bom.entries if e.name.endswith('.bom')][:a.limit]:
  raw=bom.read(entry);config=Config();assert lib.bk_bom_decode(raw,len(raw),C.byref(config),err)
  pack=11 if entry.name in ['jouhansin.bom','kahansin.bom'] else 8;arc=arcs[pack];files={e.name.lower().encode():arc.read(e) for e in arc.entries if e.name.lower().endswith('.vix')};models=[];clips=[];poses=[];roots=[];names=[];rawmodels=[];xans=[];forest=assets=morph=None
  try:
   for which,name in enumerate([config.primary,config.secondary]):
    xan=arc.read(next(e for e in arc.entries if e.name.encode()==name));xans.append(xan);name=xan[:256].split(b'\0')[0];names.append(name);rawmodel=arc.read(next(e for e in arc.entries if e.name.encode()==name));rawmodels.append(rawmodel);ok,m,msg=decode(lib,rawmodel);assert ok,msg;models.append(m)
    clip=lib.bk_clip_set_decode(xan,len(xan),err);assert clip,err.value;clips.append(clip);root=next(i for i,f in enumerate(m.contents.frames[:m.contents.frame_count]) if f.parent_index==NONE);roots.append(root)
    pose=lib.bk_actor_pose_create_loaded(m,clip,root,(C.c_float*3)(0,0,0),0,err);assert pose,err.value;poses.append(pose);local=F16(*m.contents.frames[root].local);local[12]+=2.25*which;local[13]-=1.5*which;assert lib.bk_actor_pose_root_local(pose,local,err)
   forest=lib.bk_actor_forest_create((C.c_void_p*2)(*poses),2,err);assert forest,err.value
   for i in range(2):assert lib.bk_actor_forest_attach(forest,0,lib.bk_actor_forest_node(forest,i,roots[i]),err)
   vm=Native(exe,files);vm.bind(rawmodels[1],models[1].contents);seed=struct.unpack_from('<i',xans[1],512+0x140)[0];vm.bind_clip(xans[1],vm.root,seed,False)
   morph=lib.bk_model_morph_create(models[1],err);assert morph;vm.morph(lib,models[1].contents,morph);matvm=MaterialNative(exe,u=vm.uc)
   chunks=[c for c in models[1].contents.chunks[:models[1].contents.chunk_count] if c.tag==b'MATA']
   if chunks:
    c=chunks[0];matvm.load(rawmodels[1][c.offset:c.offset+c.size],models[1].contents);vm.word(vm.model+0x158,matvm.group)
   vm.setup(lib,[m.contents for m in models],poses,names,config,packed=len(records)%2==0)
   vm.initialize();expected_events=[x for _ in range(config.count) for x in [0x422c49,0x4230bd]]+[0x423be2,0x4aaa40,0x4aab5e,0x40168c,0x4021a1]
   assert vm.events[:len(expected_events)]==expected_events,(entry.name,[hex(x) for x in vm.events]);assert vm.events[len(expected_events)]==0x423a99
   inputs=(Actor*2)(*[Actor(poses[i],names[i],i,roots[i]) for i in range(2)])
   assets=lib.bk_bom_assets_create(store,f'bk3_{pack:02}'.encode(),C.byref(config),inputs,forest,err);assert assets,(entry.name,err.value)
   for i in range(config.count):
    b=lib.bk_bom_assets_binding(assets,i).contents
    for field,offset,actor in [('parent',0x70594c,0),('reference',0x70598c,0),('child',0x70592c,1),('primary_aux',0x70596c,0),('secondary_aux',0x7059ac,1)]:
     want=vm.read(offset+i*4);got=getattr(b,field);assert want==(0 if got==NONE else vm.frame_bases[actor]+got*0x400),(entry.name,i,field)
    for field,offset in [('source',0x7080e4),('target',0x708104)]:
     idx=getattr(b,field);ref=lib.bk_bom_assets_mesh(assets,idx).contents if idx!=NONE else None;assert vm.read(offset+i*4)==(vm.registry[ref.actor,ref.submesh] if ref else 0)
    assert b.selection_count==vm.read(0x708164+i*4)
    assert b.missing_selection==int(bool(config.bindings[i].selection) and config.bindings[i].selection.lower() not in files)
    group=C.c_int32();sources=C.POINTER(C.c_uint32)();count=C.c_size_t();assert lib.bk_bom_assets_mapping(assets,i,C.byref(group),C.byref(sources),C.byref(count));assert group.value==vm.read(0x708184+i*4)
    want=vm.read(0x708124+i*4);assert list(sources[:count.value])==list(struct.unpack('<'+'I'*count.value,vm.uc.mem_read(want,count.value*4))) if count.value else True;maps+=count.value
   def check(where):
    nonlocal matrices,vertices,material_checks
    for actor,m in enumerate(models):
     for f in range(m.contents.frame_count):
      for getter,off in [('local',0x80),('frame',0xc0),('parent_world',0x100)]:equal(getattr(lib,'bk_actor_pose_'+getter)(poses[actor],f)[:16],vm.floats(vm.frame_bases[actor]+f*0x400+off,16),(entry.name,where,actor,f,getter));matrices+=1
    state=State();assert lib.bk_actor_pose_state(poses[1],C.byref(state));equal([getattr(state,k) for k,_ in State._fields_],vm.state(),(entry.name,where,'clock'))
    g=lib.bk_bom_assets_morph(assets);assert lib.bk_morph_group_time(g)==vm.floats(vm.group_morph+0x74,1)[0]
    for s,p in vm.mesh_ids.items():
     count,ptr=vm.meshes[p];mesh=lib.bk_morph_group_mesh(g,s);got=C.string_at(lib.bk_morph_mesh_vertices(mesh),count*60);want=bytes(vm.uc.mem_read(ptr,count*60))
     if got!=want:
      for v in range(count):equal(struct.unpack_from('<9f',got,v*60),struct.unpack_from('<9f',want,v*60),(entry.name,where,s,v));assert got[v*60+36:(v+1)*60]==want[v*60+36:(v+1)*60]
     vertices+=count
    if chunks:
     pose=lib.bk_bom_assets_materials(assets)
     for i,p in enumerate(matvm.materials):
      m=lib.bk_material_pose_material(pose,i).contents;got=struct.unpack('<17f',C.string_at(C.addressof(m)+Material.diffuse.offset,68));want=vm.floats(p+0x70,17)
      for j in range(17):
       if matvm.undefined[p] and j in [11,15]:assert got[j]==0
       else:equal([got[j]],[want[j]],(entry.name,where,'material',i,j));material_checks+=1
    for f in range(models[1].contents.frame_count):
     hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(poses[1],f,C.byref(hidden));assert hidden.value==vm.read(vm.frames+f*0x400+0x70)
   check('initialized')
   motion=bom_motion_binding.Check(lib,vm,assets,config,err,equal) if a.motion else None
   active=[i for i in range(128) if lib.bk_clip_definition(clips[1],i).contents.active]
   for step in range(120 if a.seconds else 60):
    if step%10==0:
     vis=Visibility(roots[1],1 if step%20==0 else 0);assert lib.bk_actor_pose_visibility(poses[1],C.byref(vis),1,err);vm.call(0x423a99,struct.pack('<II',vm.frames+vm.root*0x400,vis.hidden))
    if a.seconds:
     if step%15==0:
      slot=active[(step//15)%len(active)];vm.call(0x401b0a,struct.pack('<II',vm.clip,slot));assert lib.bk_actor_pose_request(poses[1],slot,err)
     dt=C.c_float([0,.016,.05,.1,.25][step%5]).value
     vm.call(0x4026fe,struct.pack('<If',vm.clip,dt));assert lib.bk_bom_assets_advance(assets,dt,err),err.value
    else:
     vm.call(0x4021a1,struct.pack('<I',vm.clip));assert lib.bk_bom_assets_advance_frame(assets,err)
    check(step)
    if motion:
     before=[]
     for pose in poses:
      state=State();assert lib.bk_actor_pose_state(pose,C.byref(state));before.append(bytes(state))
     motion.step(step);check(('motion',step))
     for i,pose in enumerate(poses):
      state=State();assert lib.bk_actor_pose_state(pose,C.byref(state));assert bytes(state)==before[i]
    if step%7==0:vm.call(0x423be2,b'');assert lib.bk_actor_forest_refresh(forest,err);check(('refresh',step))
    frames+=1
   records.append(dict(name=entry.name,bindings=config.count,missing=sum(lib.bk_bom_assets_binding(assets,i).contents.missing_selection for i in range(config.count)),frames=120 if a.seconds else 60))
   if motion:records[-1].update(motion_calls=motion.calls,completed=motion.completed,followed=motion.followed)
   print('PASS BOM owner',records[-1],flush=True)
  finally:
   lib.bk_bom_assets_destroy(assets);lib.bk_actor_forest_destroy(forest);lib.bk_model_morph_destroy(morph)
   for p in poses:lib.bk_actor_pose_destroy(p)
   for c in clips:lib.bk_clip_set_destroy(c)
   for m in models:lib.bk_model_destroy(m)
 lib.bk_resources_destroy(store)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,frames=frames,matrices=matrices,vertices=vertices,mapped_vertices=maps,material_checks=material_checks,max_normalized_error=worst,scope=__doc__)
 if a.motion:result.update(motion=True,scope=__doc__+bom_motion_binding.__doc__)
 if a.seconds:result.update(seconds=True,scope='Actual4026fe clock/ANIM/MORP-blend/MATA/MORP sampling and hidden/publication on7 BOM owners. Includes original401b0a requests; no production ending dispatcher.')
 (ROOT/('local/original-bom-seconds-oracle.json' if a.seconds else 'local/original-bom-motion-assets-oracle.json' if a.motion else 'local/original-bom-assets-oracle.json')).write_text(json.dumps(result,indent=2)+'\n');print('PASS',frames,matrices,vertices,maps,material_checks,worst,flush=True)
if __name__=='__main__':main()
