"""Original4cf318 packaged resource/pose load, including actual4a4dc3,
40150c, configured clip requests,4b79e0, name lookup, cached targets and
fixed camera. Resource decoding is the boundary; native global attachment,
refresh, math, auxiliary ANIM/MATA/MORP execute unchanged. Face warmup state/RNG additionally runs in its original controller VM.
Media/device work are observing boundaries (their output is excluded), not claims
of a complete ending scene. Native caller resource names are asserted.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP
from original_bom_assets_oracle import Native as BomNative,bind as bom_bind,Config,MaterialNative,I,NONE,F16
from original_ending_camera_assets_oracle import bindings as camera_bind,State,Presets,values
from model_binding import ROOT,Model,Material
from original_ending_face_init import EndingController
from original_face_controller_oracle import State as FaceState
from playback_binding import library
from bk3_assets import Archive
class NormalConfig(C.Structure):
 _fields_=[(n,C.c_char*32) for n in ['primary','auxiliary','face','bom']]+[('position',C.c_float*3)]+[(n,C.c_int32) for n in ['yaw','camera_yaw','expression_a','expression_b']]+[('actions',C.c_int32*80),('camera_table',(C.c_uint32*4)*5)]
def bind(lib):
 lib.bk_ending_normal_assets_load_background.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p]
 lib.bk_ending_normal_background.argtypes=[C.c_uint,C.c_uint];lib.bk_ending_normal_background.restype=C.c_char_p
 bom_bind(lib);camera_bind(lib)
 lib.bk_face_init.argtypes=[C.POINTER(FaceState),C.c_uint,C.c_uint,C.c_void_p]
 lib.bk_ending_normal_assets_face_state.argtypes=[C.c_void_p];lib.bk_ending_normal_assets_face_state.restype=C.POINTER(FaceState)
 for name,args,rest in [('create',[C.c_void_p,C.c_uint,C.c_uint,C.POINTER(C.c_uint32),C.POINTER(C.c_uint32),C.POINTER(State),C.POINTER(Presets),C.c_void_p],C.c_void_p),('destroy',[C.c_void_p],None),('pose',[C.c_void_p,C.c_uint],C.c_void_p),('config',[C.c_void_p],C.POINTER(NormalConfig)),('node',[C.c_void_p,C.c_uint],C.c_uint32),('oyu',[C.c_void_p],C.c_uint32),('target',[C.c_void_p,C.c_uint],C.POINTER(C.c_float)),('bom',[C.c_void_p],C.c_void_p)]:
  f=getattr(lib,'bk_ending_normal_assets_'+name);f.argtypes=args;f.restype=rest
class Native(BomNative):
 def __init__(self,exe,files):
  super().__init__(exe,files);self.uc.mem_map(0x37000000,0x400000);self.load_names=[];self.paths=[];self.face_calls=[]
  for off,iat in enumerate([0x53f214,0x53f218,0x53f21c,0x53f2f0]):
   p=0x361f1000+off*16;self.word(iat,p);self.uc.hook_add(UC_HOOK_CODE,self.io,begin=p,end=p)
  for a in [0x4ad8ec,0x4ad97e,0x401074,0x428a70,0x428bb6,0x428caf,0x428b69,0x4a7109,0x4f2dae,0x4a07a9,0x4a5870,0x51fa7b,0x429c6b,0x4b1916,0x521d78,0x521d2c,0x42d295]:self.uc.hook_add(UC_HOOK_CODE,self.io,begin=a,end=a)
 def service(self,u,a,size,user):
  if a==0x42cf0e:self.fov=struct.unpack('<f',u.mem_read(u.reg_read(UC_X86_REG_ESP)+4,4))[0]
  super().service(u,a,size,user)
 def io(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);args=[self.read(sp+4+4*i) for i in range(5)];result=0;extra=0
  if a==0x361f1000:
   u.mem_write(args[0],self.string(args[1])+b'\0');result=args[0];extra=8
  elif a==0x361f1010:result=len(self.string(args[0]));extra=4
  elif a==0x361f1020:u.mem_write(args[0],self.string(args[0])+self.string(args[1])+b'\0');result=args[0];extra=8
  elif a==0x361f1030:
   fmt=self.string(args[1]);out=fmt%args[2] if b'%' in fmt else fmt;u.mem_write(args[0],out+b'\0');result=len(out)
  elif a==0x4ad8ec:
   out=(self.string(args[1]) if args[1] else b'')+(self.string(args[2]) if args[2] else b'');u.mem_write(args[0],out+b'\0');result=args[0];self.paths.append(out)
  elif a==0x4ad97e:
   folder,_,name=self.string(args[2]).rpartition(b'\\');u.mem_write(args[0],folder+b'\\\0');u.mem_write(args[1],name+b'\0')
  elif a==0x4a7109:result=self.context
  elif a==0x4f2dae:
   self.face_calls.append((self.string(args[3]),self.string(args[4])))
   assert self.string(args[4]).lstrip(b'\\')==self.bom_name.replace(b'.bom',b'.fam')
  elif a==0x4a07a9:assert args[1]==1
  elif a==0x4a5870:
   assert self.string(args[1]).lstrip(b"\\")==self.bom_name,(self.string(args[1]),self.bom_name)
   # Preserve caller's archive path while supplying decoded original BOM fields.
   archive=bytes(u.mem_read(args[0]+4,260));u.mem_write(args[0],self.config_bytes);u.mem_write(args[0]+4,archive);result=1
  elif a==0x401074:
   name=self.string(args[0]).lstrip(b'\\');assert name in self.byname,name
   clip,root=self.byname[name];self.load_names.append(name)
   # Resource-load boundary returns the actual decoded tree. Execute native
   # append/global refresh and40150c orientation before returning the clip.
   p=0x361f2000+len(self.load_names)*0x100;code=bytearray()
   def push(v):code.extend(b'\x68'+struct.pack('<I',v))
   def call(dest):code.extend(b'\xe8'+struct.pack('<i',dest-(p+len(code)+5)))
   push(root);push(self.context);call(0x422015);code.extend(b'\x83\xc4\x08')
   for v in [0,0,0,clip]:push(v)
   call(0x40150c);code.extend(b'\x83\xc4\x10\xb8'+struct.pack('<I',clip)+b'\xc3')
   u.mem_write(p,bytes(code));u.reg_write(UC_X86_REG_EIP,p);return
  elif a==0x51fa7b:result=0x373f0000
  elif a==0x429c6b:
   if args[1]!=0x3eb:return
   assert self.string(args[0]).lower()==b'd_moza.bmp',self.string(args[0]);result=0x373f1000
  elif a==0x521d78:result=0x373f2000
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret)
 def extra_actor(self,m,xan,index):
  base=0x37000000+(index-2)*0x100000;clip=base+0x80000;model=clip+0x6000;links=base+0x90000
  self.uc.mem_write(clip,xan[512:]);self.word(clip+0x18c,0);self.word(clip+0x160,model)
  root=next(i for i,f in enumerate(m.frames[:m.frame_count]) if f.parent_index==NONE);self.word(model+0x14,base+root*0x400)
  children={i:[] for i in range(m.frame_count)}
  for i in range(m.frame_count):
   f=m.frames[i];p=base+i*0x400;self.uc.mem_write(p+8,f.name+b'\0')
   for off,v in [(0x80,f.local),(0xc0,I),(0x100,I)]:self.vector(p+off,v)
   if f.parent_index!=NONE:children[f.parent_index].append(i);self.word(p+0x22c,base+f.parent_index*0x400)
  for parent,items in children.items():
   p=base+parent*0x400;self.word(p+0x238,len(items));self.word(p+0x230,links+items[0]*16 if items else 0)
   for j,i in enumerate(items):self.word(links+i*16,base+i*0x400);self.word(links+i*16+8,links+items[j+1]*16 if j+1<len(items) else 0)
  self.bases.append(base);self.clips.append(clip);self.root_ids.append(root);return clip,base+root*0x400
 def prepare(self,lib,models,poses,xans,names,bom,group,variant,state):
  self.setup(lib,models[:2],poses[:2],names[:2],bom,True)
  self.uc.mem_write(self.pclip,xans[0][512:]);self.word(self.pclip+0x18c,0);self.word(self.pclip+0x160,self.pmodel)
  self.bases=self.frame_bases.copy();self.clips=[self.pclip,self.clip];self.root_ids=self.roots.copy()
  self.byname={f'h{group+1:02}_00.xan'.encode():(self.pclip,self.bases[0]+self.roots[0]*0x400),f'h{group+1:02}_01.xan'.encode():(self.clip,self.bases[1]+self.roots[1]*0x400)}
  for i in range(2,len(models)):self.byname[[b'cam00_00.xan',b'cam00_03.xan',b'm02_92.xan'][i-2]]=self.extra_actor(models[i],xans[i],i)
  self.config_bytes=bytes(self.uc.mem_read(self.config,0x420c));self.bom_name=f'h{group+1:02}_00.bom'.encode()
  # Every loader starts detached; the rendered camera is the first child.
  for i in range(len(models)):self.word(self.bases[i]+self.root_ids[i]*0x400+0x22c,0)
  link=0x373e0000;self.word(self.context+0x238,1);self.word(self.context+0x230,link);self.word(self.context+0x234,link);self.word(link,self.rendered);self.word(link+8,0);self.word(self.rendered+0x22c,self.context)
  for off in [0x80,0xc0]:self.vector(self.rendered+off,state.pose.world)
  self.word(0x645600,self.context);self.word(0x645604,self.rendered)
  self.uc.mem_write(0x721b3c,bytes([group,0]));self.word(0x7219a8,group);self.word(0x721e04,variant);self.uc.mem_write(0x5767c8,b'\1');self.uc.mem_write(0x70c8fc,f'h{group+1:02}_00.xan\0'.encode())
  self.vector(0x71af38+0x420,state.pose.position);self.vector(0x71af38+0x4a4,state.matrix);self.vector(0x71af38+0x5c8,state.focus);self.fov=state.fov
  self.word(0x719b20,1);self.uc.mem_write(0x709c70,b'\x35'*56)
 def camera(self):
  s=State();s.pose.world[:]=self.floats(self.rendered+0xc0,16);s.pose.position[:]=self.floats(0x71af38+0x420,3)
  s.yaw,s.pitch,s.radius,s.height=self.floats(0x71af38+0x42c,4);s.matrix[:]=self.floats(0x71af38+0x4a4,16);s.focus[:]=self.floats(0x71af38+0x5c8,3);s.fov=self.fov;return s
 def clip_state(self,clip):
  old=self.clip;self.clip=clip
  try:return self.state()
  finally:self.clip=old

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--limit',type=int,default=10);ap.add_argument('--background',action='store_true');a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind(lib);err=C.create_string_buffer(256)
 store=lib.bk_resources_create(err);arcs={}
 for pack in ['bk3_08','bk3_04','bk3_03','fambom']:
  assert lib.bk_resources_mount(store,pack.encode(),str(a.data/(pack+'.pp')).encode(),err),err.value;arcs[pack]=Archive(a.data/(pack+'.pp'))
 def read(pack,name):return arcs[pack].read(next(e for e in arcs[pack].entries if e.name==name))
 files={e.name.lower().encode():arcs['bk3_08'].read(e) for e in arcs['bk3_08'].entries if e.name.endswith('.vix')}
 worst=0.;matrices=loads=vertices=material_fields=maps=background_matrices=0
 def equal(got,want,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(got,want)):
   if g==w:continue
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(label,i,g,w,d)
 try:
  for case in range(a.limit):
   group=case//2%5;variant=case%2;state=State();state.pose.world[:]=state.matrix[:]=I;state.pose.world[12]=21;state.matrix[12]=-17;state.focus[:]=[7,8,9];state.fov=.4
   initial=State.from_buffer_copy(state);presets=Presets();rng=C.c_uint32(123+case);owner=lib.bk_ending_normal_assets_create(store,group,variant,(C.c_uint32*4)(100,110,120,130),C.byref(rng),C.byref(state),C.byref(presets),err);assert owner,err.value
   baseline=[];base_clips=[];morph=None
   try:
    poses=[lib.bk_ending_normal_assets_pose(owner,i) for i in range(5 if group==1 else 4)];models=[lib.bk_actor_pose_model(p).contents for p in poses]
    xan_names=[f'h{group+1:02}_00.xan',f'h{group+1:02}_01.xan','cam00_00.xan','cam00_03.xan']+(['m02_92.xan'] if group==1 else []);packs=['bk3_08','bk3_08','bk3_04','bk3_04']+(['bk3_03'] if group==1 else [])
    xans=[read(p,n) for p,n in zip(packs,xan_names)];names=[x[:256].split(b'\0')[0] for x in xans]
    for m,xan in zip(models[:2],xans[:2]):
     clip=lib.bk_clip_set_decode(xan,len(xan),err);base_clips.append(clip);root=next(i for i,f in enumerate(m.frames[:m.frame_count]) if f.parent_index==NONE)
     pose=lib.bk_actor_pose_create_loaded(C.byref(m),clip,root,(C.c_float*3)(0,0,0),0,err);assert pose;baseline.append(pose)
    raw_bom=read('fambom',f'h{group+1:02}_00.bom');bom=Config();assert lib.bk_bom_decode(raw_bom,len(raw_bom),C.byref(bom),err)
    vm=Native(exe,files);raw=C.string_at(models[1].source,models[1].source_size);vm.bind(raw,models[1]);seed=struct.unpack_from('<i',xans[1],512+0x140)[0];vm.bind_clip(xans[1],vm.root,seed,False)
    morph=lib.bk_model_morph_create(C.byref(models[1]),err);assert morph;vm.morph(lib,models[1],morph);mat=MaterialNative(exe,u=vm.uc)
    chunk=next(c for c in models[1].chunks[:models[1].chunk_count] if c.tag==b'MATA');mat.load(raw[chunk.offset:chunk.offset+chunk.size],models[1]);vm.word(vm.model+0x158,mat.group)
    vm.prepare(lib,models,baseline,xans,names,bom,group,variant,initial)
    try:vm.call(0x4cf318,b'')
    except Exception:
     print('native fault',hex(vm.uc.reg_read(UC_X86_REG_EIP)),vm.load_names,vm.face_calls, 'clips',[hex(vm.read(p)) for p in [0x721b28,0x721b2c]],'child', vm.string(0x710da0+0x928),'expected',bom.bindings[0].child,'rootname',vm.string(vm.frames+8),'found',[hex(vm.read(p)) for p in [0x70594c,0x70598c,0x70592c]],flush=True);raise
    assert vm.load_names==[s.encode() for s in xan_names],vm.load_names
    assert len(vm.face_calls)==1
    if group==2:
     idx=lib.bk_ending_normal_assets_oyu(owner);assert vm.read(0x719b44)==(0 if idx==NONE else vm.bases[0]+idx*0x400)
    face_state=lib.bk_ending_normal_assets_face_state(owner).contents
    initial_face=FaceState();assert lib.bk_face_init(C.byref(initial_face),face_state.eye_count,face_state.mouth_count,err)
    face_vm=EndingController(exe);face_vm.initialize(initial_face,123+case,[100,110,120,130])
    assert bytes(face_state)==bytes(face_vm.state())
    assert rng.value==struct.unpack('<I',face_vm.u.mem_read(0x58edd8,4))[0]
    cfg=lib.bk_ending_normal_assets_config(owner).contents
    assert bytes(cfg.actions)==bytes(vm.uc.mem_read(0x709db8,320));assert bytes(cfg.camera_table)==bytes(vm.uc.mem_read(0x709fcc,80))
    assert cfg.expression_a==vm.read(0x721df4) and cfg.expression_b==vm.read(0x721df0)
    for actor,m in enumerate(models):
     for f in range(m.frame_count):
      for getter,off in [('local',0x80),('frame',0xc0),('parent_world',0x100)]:equal(getattr(lib,'bk_actor_pose_'+getter)(poses[actor],f)[:16],vm.floats(vm.bases[actor]+f*0x400+off,16),(group,variant,actor,f,getter));matrices+=1
     from clip_binding import State as ClipState
     st=ClipState();assert lib.bk_actor_pose_state(poses[actor],C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.clip_state(vm.clips[actor]),(group,variant,actor,'clock'))
    for i in range(39):
     idx=lib.bk_ending_normal_assets_node(owner,i);assert vm.read(0x721ef4+i*4)==(0 if idx==NONE else vm.bases[0]+idx*0x400),(group,i,idx)
    for i in range(3):equal(lib.bk_ending_normal_assets_target(owner,i)[:3],vm.floats(0x70c8d8+i*12,3),(group,'target',i))
    equal(values(state),values(vm.camera()),(group,variant,'camera'));assert bytes(presets)==bytes(vm.uc.mem_read(0x71af38+0x444,96))
    bom_owner=lib.bk_ending_normal_assets_bom(owner)
    for i in range(bom.count):
     group_index=C.c_int32();sources=C.POINTER(C.c_uint32)();count=C.c_size_t()
     assert lib.bk_bom_assets_mapping(bom_owner,i,C.byref(group_index),C.byref(sources),C.byref(count))
     assert group_index.value==vm.read(0x708184+i*4)
     assert list(sources[:count.value])==list(struct.unpack('<'+'I'*count.value,vm.uc.mem_read(vm.read(0x708124+i*4),count.value*4))) if count.value else True
     maps+=count.value
    materials=lib.bk_bom_assets_materials(bom_owner)
    for i,p in enumerate(mat.materials):
     material=lib.bk_material_pose_material(materials,i).contents
     got=struct.unpack('<17f',C.string_at(C.addressof(material)+Material.diffuse.offset,68));want=vm.floats(p+0x70,17)
     for field in range(17):
      if mat.undefined[p] and field in [11,15]:assert got[field]==0
      else:equal([got[field]],[want[field]],(group,variant,'material',i,field));material_fields+=1
    aux=lib.bk_bom_assets_morph(bom_owner)
    for s,p in vm.mesh_ids.items():
     count,ptr=vm.meshes[p];mesh=lib.bk_morph_group_mesh(aux,s);got=C.string_at(lib.bk_morph_mesh_vertices(mesh),count*60);want=bytes(vm.uc.mem_read(ptr,count*60));assert got==want,(group,s,'morph');vertices+=count
    assert bytes(vm.uc.mem_read(0x709c70,56))==b'\x35'*56
    if a.background:
     expected=vm.string(0x55f4bc+(group*2+variant)*260).lstrip(b"\\")
     assert lib.bk_ending_normal_background(group,variant)==expected
     held_targets=[bytes(C.string_at(lib.bk_ending_normal_assets_target(owner,i),12)) for i in range(3)]
     assert lib.bk_ending_normal_assets_load_background(owner,store,err),err.value
     if group!=1:
      pose=lib.bk_ending_normal_assets_pose(owner,4);m=lib.bk_actor_pose_model(pose).contents
      poses.append(pose);models.append(m);xan=read('bk3_03',expected.decode());xan_names.append(expected.decode())
      vm.byname[expected]=vm.extra_actor(m,xan,4)
     # Outer4cc582 starts with the retained special-background flag cleared;
     # group1's actual4cf318 sets it. Execute the real table/bypass and loader.
     assert vm.read(0x721ed8)==int(group==1)
     vm.uc.reg_write(UC_X86_REG_ESP,vm.stack-0x800);vm.uc.reg_write(UC_X86_REG_EBP,vm.stack)
     vm.uc.emu_start(0x4ce7bf,0x4ce80e,count=2000000000)
     assert vm.uc.reg_read(UC_X86_REG_EIP)==0x4ce80e
     assert vm.load_names==[s.encode() for s in xan_names],vm.load_names
     for actor,m in enumerate(models):
      for f in range(m.frame_count):
       for getter,off in [('local',0x80),('frame',0xc0),('parent_world',0x100)]:
        equal(getattr(lib,'bk_actor_pose_'+getter)(poses[actor],f)[:16],vm.floats(vm.bases[actor]+f*0x400+off,16),(group,variant,'background',actor,f,getter));background_matrices+=1
      st=ClipState();assert lib.bk_actor_pose_state(poses[actor],C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.clip_state(vm.clips[actor]),(group,variant,'background clock',actor))
     for i in range(3):
      assert C.string_at(lib.bk_ending_normal_assets_target(owner,i),12)==held_targets[i]
      equal(lib.bk_ending_normal_assets_target(owner,i)[:3],vm.floats(0x70c8d8+i*12,3),(group,'held target',i))
     equal(values(state),values(vm.camera()),(group,variant,'held camera'))
     assert lib.bk_ending_normal_assets_load_background(owner,store,err),err.value
    loads+=1;print('PASS',group,variant,'outer background' if a.background else '',flush=True)
   finally:
    for p in baseline:lib.bk_actor_pose_destroy(p)
    for p in base_clips:lib.bk_clip_set_destroy(p)
    lib.bk_model_morph_destroy(morph);lib.bk_ending_normal_assets_destroy(owner)
 finally:lib.bk_resources_destroy(store)
 report=dict(passed=True,loads=loads,matrices=matrices,morph_vertices=vertices,material_fields=material_fields,mapping_entries=maps,face_states=loads,background_matrices=background_matrices,max_relative_error=worst,exe_sha256=hashlib.sha256(exe).hexdigest(),scope=__doc__+(" With --background: original4ce7bf..4ce80e table/bypass and complete4d460b CPU load, all cached matrices/clocks/targets compared; fog device commands excluded." if a.background else ""))
 (ROOT/('local/original-ending-background-oracle.json' if a.background else 'local/original-ending-normal-assets-oracle.json')).write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
