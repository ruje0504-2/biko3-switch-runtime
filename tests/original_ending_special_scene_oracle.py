"""Actual normal-ending forest with full native4d9898 camera/node/material
math.4a4701 is reduced to its ordered root walks (original42261a executes);
GPU viewport/clear/pass boundaries are observed, not rendered. Shared actor
locals are explicit component inputs. Includes a separate named material
fixture because ordinary normal assets do not contain the group4 material.
"""
import argparse,ctypes as C,hashlib,json,math,struct,random
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_ending_regular_walk_oracle import bind,VM,Visit,NONE,F16,I,State,Presets
from original_ending_special_oracle import Bindings,Draw,Render,Frame
from original_material_animation_oracle import bind as material_bind
from model_binding import ROOT,Model,Material,Frame as ModelFrame
from playback_binding import library
FP=C.POINTER(C.c_float)
class Ref(C.Structure):_fields_=[('local',C.c_float*16),('world',C.c_float*16),('parent',C.c_float*16)]
class MaterialBinding(C.Structure):_fields_=[('pose',C.c_void_p),('index',C.c_uint32)]
SceneDraw=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Draw),C.POINTER(State),C.c_void_p)
class Scene(C.Structure):
 _fields_=[('forest',C.c_void_p),('camera',C.POINTER(State)),('materials',C.POINTER(MaterialBinding)),('material_count',C.c_size_t),('context',C.c_void_p),('draw',SceneDraw),('render',Render)]

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind(lib);material_bind(lib);err=C.create_string_buffer(256)
 lib.bk_ending_special_scene_draw.argtypes=[C.POINTER(Scene),C.POINTER(Bindings),C.POINTER(Draw),C.c_void_p]
 lib.bk_ending_special_cameras.argtypes=[C.POINTER(C.c_float*4),C.c_uint]
 lib.bk_actor_forest_anchor_reference.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Ref),C.c_void_p]
 lib.bk_actor_forest_commit_anchor_reference.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Ref),C.c_void_p]
 lib.bk_actor_forest_camera_publish.argtypes=[C.c_void_p,C.c_void_p]
 lib.bk_node_reference_aim.argtypes=[C.POINTER(Ref),FP,C.c_void_p]
 lib.bk_actor_forest_visibility.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p]
 lib.bk_actor_forest_find.argtypes=[C.c_void_p,C.c_uint32,C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p]
 worst=0.;matrix_count=material_fields=float_fields=frames=snapshots=walks=0;raw=hashlib.sha256()
 def equal(got,want,label):
  nonlocal worst,float_fields
  assert len(got)==len(want)
  for i,(x,y) in enumerate(zip(struct.unpack('<'+'f'*(len(got)//4),got),struct.unpack('<'+'f'*(len(want)//4),want))):
   delta=abs(x-y)/max(1,abs(y));worst=max(worst,delta);assert math.isfinite(delta) and delta<3e-6,(label,i,x,y,delta)
  float_fields+=len(got)//4
 #425196 preserves own world; cached parent inversion only affects local.
 vm=VM(exe);rng=random.Random(0x425196)
 for case in range(4000):
  ref=Ref();ref.local[:]=ref.world[:]=ref.parent[:]=I
  ref.world[12:15]=[rng.uniform(-300,300) for _ in range(3)]
  for i in [0,5,10,15]:ref.parent[i]=rng.choice([.5,.9,1,1.1,2]);ref.world[i]=rng.choice([.5,1,2])
  ref.parent[12:15]=[rng.uniform(-50,50) for _ in range(3)]
  target=(C.c_float*3)(*[rng.uniform(-500,500) for _ in range(3)])
  if case%7==0:target[:]=ref.world[12:15]
  if case%11==0:target[:]=ref.world[12:15];target[1]+=10
  for field,off in [('local',0x80),('world',0xc0),('parent',0x100)]:vm.vector(vm.frames+0x400+off,getattr(ref,field))
  vm.u.mem_write(0x3000200,bytes(target));vm.call(0x425196,struct.pack('<4I',vm.frames+0x400,0x3000200,vm.frames,0))
  assert lib.bk_node_reference_aim(C.byref(ref),target,err),err.value
  for field,off in [('local',0x80),('world',0xc0),('parent',0x100)]:equal(bytes(getattr(ref,field)),bytes(vm.u.mem_read(vm.frames+0x400+off,64)),('aim',case,field));matrix_count+=1
 store=lib.bk_resources_create(err);assert store
 for pack in ['bk3_08','bk3_03','bk3_04','fambom']:assert lib.bk_resources_mount(store,pack.encode(),str(a.data/(pack+'.pp')).encode(),err),err.value
 try:
  for g in range(5):
   for variant in range(2):
    camera=State();camera.pose.world[:]=camera.matrix[:]=I;presets=Presets();seed=C.c_uint32(123);materials=[]
    owner=lib.bk_ending_normal_assets_create(store,g,variant,(C.c_uint32*4)(100,110,120,130),C.byref(seed),C.byref(camera),C.byref(presets),err);assert owner,err.value
    try:
     assert lib.bk_ending_normal_assets_load_background(owner,store,err),err.value
     forest=lib.bk_ending_normal_assets_forest(owner);tree=lib.bk_actor_forest_tree(forest);count=lib.bk_frame_tree_count(tree)
     poses=[lib.bk_ending_normal_assets_pose(owner,i) for i in range(5)];models=[lib.bk_actor_pose_model(p).contents for p in poses];nodes=[None,None];roots=[];bindings=[]
     for i,m in enumerate(models):
      roots.append(lib.bk_actor_forest_node(forest,i,next(j for j in range(m.frame_count) if m.frames[j].parent_index==NONE)));nodes.extend((i,j) for j in range(m.frame_count))
      pose=lib.bk_material_pose_create(C.pointer(m),err);assert pose;materials.append(pose);bindings.extend(MaterialBinding(pose,j) for j in range(m.material_count))
     fixture=(Material*2)()
     for i in range(2):
      C.memmove(C.addressof(fixture[i]),C.addressof(models[0].materials[0]),C.sizeof(Material));fixture[i].name=b'Om_syokusyu_maki';fixture[i].id=900+i;fixture[i].diffuse[3]=.3+i
     fixture_root=ModelFrame();fixture_root.parent_index=fixture_root.mesh_index=NONE;fixture_root.local[:]=I
     synthetic=Model();synthetic.materials=fixture;synthetic.material_count=2;synthetic.frames=C.pointer(fixture_root);synthetic.frame_count=1
     mat=lib.bk_material_pose_create(C.byref(synthetic),err);assert mat,err.value;materials.append(mat);bindings.extend([MaterialBinding(mat,0),MaterialBinding(mat,1)])
     registry=(MaterialBinding*len(bindings))(*bindings);vm=VM(exe);trace=[];draw_count=[0];native_materials=[]
     def ptr(node):return vm.frames+node*0x400
     vm.word(0x721b28,0x3001000);vm.word(0x3001160,0x3001400);vm.word(0x3001414,ptr(roots[0]));vm.word(0x721b2c,0x3001800);vm.word(0x3001960,0x3001c00);vm.word(0x3001c14,ptr(roots[1]));head=lib.bk_actor_forest_node(forest,0,lib.bk_ending_normal_assets_node(owner,0));vm.word(0x721ef4,ptr(head))
     # Real, case-sensitive native resource registry; refcount starts at1.
     for i,b in enumerate(bindings):
      m=lib.bk_material_pose_material(b.pose,b.index).contents;p=0x4300000+i*0x100;entry=0x43a0000+i*0x88
      vm.u.mem_write(entry,m.name+b'\0');vm.word(entry+0x80,p);vm.word(entry+0x84,0x3ee);vm.word(p+4,0x3ee);vm.word(p+0x6c,1);vm.u.mem_write(p+0x70,C.string_at(C.addressof(m)+Material.diffuse.offset,68));native_materials.append(p)
     vm.word(0x645614,0x43a0000);vm.word(0x645618,len(bindings));vm.word(vm.device+0x134,vm.stub+0x20)
     for node,b in enumerate(nodes):
      f=ptr(node)
      if b:vm.u.mem_write(f+8,models[b[0]].frames[b[1]].name+b'\0')
      p=lib.bk_frame_tree_parent(tree,node);vm.word(f+0x22c,0 if p==NONE else ptr(p));children=[];child=lib.bk_frame_tree_first(tree,node)
      while child!=NONE:children.append(child);child=lib.bk_frame_tree_next(tree,child)
      vm.word(f+0x238,len(children));vm.word(f+0x230,vm.links+children[0]*16 if children else 0);vm.word(f+0x234,vm.links+children[-1]*16 if children else 0)
      for j,c in enumerate(children):vm.word(vm.links+c*16,ptr(c));vm.word(vm.links+c*16+4,vm.links+children[j-1]*16 if j else 0);vm.word(vm.links+c*16+8,vm.links+children[j+1]*16 if j+1<len(children) else 0)
     def cpu_snapshot():
      mats=bytearray();hidden=[]
      for node,b in enumerate(nodes):
       if b:
        for getter in ['local','frame','parent_world']:mats.extend(bytes(F16(*getattr(lib,'bk_actor_pose_'+getter)(poses[b[0]],b[1])[:16])))
        h=C.c_uint32();assert lib.bk_actor_pose_hidden(poses[b[0]],b[1],C.byref(h));hidden.append(h.value)
       else:
        ref=Ref();assert lib.bk_actor_forest_anchor_reference(forest,node,C.byref(ref),err);mats.extend(bytes(ref));hidden.append(0)
      mats.extend(bytes(F16(*lib.bk_actor_forest_view(forest)[:16])))
      for b in bindings:
       m=lib.bk_material_pose_material(b.pose,b.index).contents;mats.extend(C.string_at(C.addressof(m)+Material.diffuse.offset,68))
      return bytes(mats),hidden
     def native_snapshot():
      mats=bytearray();hidden=[]
      for node in range(count):
       for off in [0x80,0xc0,0x100]:mats.extend(vm.u.mem_read(ptr(node)+off,64))
       hidden.append(vm.read(ptr(node)+0x70))
      mats.extend(vm.u.mem_read(0x642fa8,64))
      for p in native_materials:mats.extend(vm.u.mem_read(p+0x70,68))
      return bytes(mats),hidden
     def service(u,address,size,unused):
      sp=u.reg_read(UC_X86_REG_ESP);ret=vm.read(sp);extra=0
      if address==0x4a4701:
       d=Draw.from_buffer_copy(bytes(u.mem_read(sp+4,216))+bytes(4));tokens=[(x-vm.frames)//0x400 if x else 0 for x in d.objects];d.objects[:]=tokens;event=('draw',bytes(d)[:216]);trace.append((event,native_snapshot()));draw_count[0]+=1
       code=bytearray();pc=0x4430000
       for root in (tokens[:20] if d.mode==1 else tokens[20:]):
        if root:
         code.extend(b'\x68'+struct.pack('<I',ptr(root)));code.extend(b'\xe8'+struct.pack('<i',0x42261a-(pc+len(code)+5)));code.extend(b'\x83\xc4\x04')
       code.extend(b'\xc3');u.mem_write(pc,bytes(code));u.reg_write(UC_X86_REG_EIP,pc);return
      elif address==0x429fd6:event=('render',0,0)
      elif address==0x429f86:event=('render',3,0)
      elif address==0x42a08b:event=('render',2,vm.read(sp+4))
      else:
       assert address==vm.stub+0x20;event=('render',1,int(vm.read(sp+8)==0x6886a8));extra=8
      trace.append((event,native_snapshot()));u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
     for addr in [0x4a4701,0x429fd6,0x429f86,0x42a08b,vm.stub+0x20]:vm.u.hook_add(UC_HOOK_CODE,service,begin=addr,end=addr)
     table=((C.c_float*4)*108)();assert lib.bk_ending_special_cameras(table,g)
     f=Frame();f.group=g;action=C.c_int32(variant);cv=C.c_uint8();restore=C.c_uint8();index=C.c_int32();mode=C.c_int32();primary=C.c_uint32(roots[0]);aux=C.c_uint32(roots[1]);b=Bindings(C.pointer(f),C.pointer(action),C.pointer(cv),C.pointer(restore),C.pointer(index),C.pointer(mode),table,C.cast(C.addressof(lib.bk_actor_forest_world(forest,head).contents)+48,FP),C.pointer(primary),C.pointer(aux))
     portable=[];callback_errors=[]
     @SceneDraw
     def draw(_,desc,cam,e):
      nonlocal walks
      try:
       portable.append((('draw',bytes(desc.contents)[:216]),cpu_snapshot()))
       roots_now=desc.contents.objects[:20] if desc.contents.mode==1 else desc.contents.objects[20:]
       for root in roots_now:
        if root:
         visits=C.POINTER(Visit)();n=C.c_uint32();assert lib.bk_actor_forest_draw(forest,root,C.byref(visits),C.byref(n),err),err.value;walks+=1
       return 1
      except Exception as exc:callback_errors.append(repr(exc));return 0
     @Render
     def render(_,kind,arg,e):
      try:portable.append((('render',kind,arg),cpu_snapshot()));return 1
      except Exception as exc:callback_errors.append(repr(exc));return 0
     scene=Scene(forest,C.pointer(camera),registry,len(bindings),None,draw,render)
     for step in range(24):
      for p in poses:assert lib.bk_actor_pose_advance(p,-1,[0,.016,.033][step%3],err),err.value
      for root in roots[:2]:assert lib.bk_actor_forest_visibility(forest,root,0,err)
      ref=Ref();assert lib.bk_actor_forest_anchor_reference(forest,1,C.byref(ref),err)
      ref.local[12]+=C.c_float(.125).value;ref.world[12]+=C.c_float(.125).value
      assert lib.bk_actor_forest_commit_anchor_reference(forest,1,C.byref(ref),err)
      f.phase=[1,3,4,5,6,8][step%6];f.state_721ee0=[4,6,7,8][step%4];cv.value=step%10;restore.value=[0,1,255][step%3];index.value=(step*7)%108;mode.value=step%3
      if step%5==0:index.value=47 if g==4 else 104;mode.value=1 if g==4 else 2
      for addr,val in [(0x721e00,f.phase),(0x721ee0,f.state_721ee0),(0x721e04,variant),(0x721edc,index.value),(0x7220f4,mode.value)]:vm.word(addr,val)
      vm.u.mem_write(0x721b3c,bytes([g,cv.value]));vm.u.mem_write(0x7220f9,bytes([restore.value]));vm.u.mem_write(0x71944c,bytes(table))
      for node,nb in enumerate(nodes):
       if nb:
        for getter,off in [('local',0x80),('frame',0xc0),('parent_world',0x100)]:vm.vector(ptr(node)+off,getattr(lib,'bk_actor_pose_'+getter)(poses[nb[0]],nb[1])[:16])
        h=C.c_uint32();assert lib.bk_actor_pose_hidden(poses[nb[0]],nb[1],C.byref(h));vm.word(ptr(node)+0x70,h.value)
       else:
        ref=Ref();assert lib.bk_actor_forest_anchor_reference(forest,node,C.byref(ref),err)
        for field,off in [('local',0x80),('world',0xc0),('parent',0x100)]:vm.vector(ptr(node)+off,getattr(ref,field))
      vm.vector(0x642fa8,lib.bk_actor_forest_view(forest)[:16])
      for i,mb in enumerate(bindings):m=lib.bk_material_pose_material(mb.pose,mb.index).contents;vm.u.mem_write(native_materials[i]+0x70,C.string_at(C.addressof(m)+Material.diffuse.offset,68))
      d=Draw();d.mode=1;d.objects[0]=roots[4];d.objects[4]=roots[0];d.objects[5]=roots[1]
      native=Draw.from_buffer_copy(d);native.objects[:]=[ptr(x) if x else 0 for x in d.objects];vm.u.mem_write(0x3006000,bytes(native)[:216]);trace.clear();portable.clear()
      try:
       vm.u.mem_write(vm.stack,struct.pack('<II',vm.stop,0x3006000));vm.u.reg_write(UC_X86_REG_ESP,vm.stack);vm.u.reg_write(UC_X86_REG_FPCW,0x037f)
       vm.u.emu_start(0x4d9898,vm.stop,count=20000000);assert vm.u.reg_read(UC_X86_REG_EIP)==vm.stop
      except Exception:
       print('native failure',g,variant,step,'eip',hex(vm.u.reg_read(UC_X86_REG_EIP)),'events',len(trace),flush=True);raise
      final=native_snapshot()
      old_camera=bytes(camera)[64:];result=lib.bk_ending_special_scene_draw(C.byref(scene),C.byref(b),C.byref(d),err)
      assert not callback_errors,callback_errors
      assert result,(g,variant,step,err.value)
      assert bytes(camera)[64:]==old_camera
      assert len(portable)==len(trace)
      for j,(got,want) in enumerate(zip(portable+[(('final',),cpu_snapshot())],trace+[(('final',),final)])):
       assert got[0]==want[0],(g,variant,step,j,got[0],want[0]);assert got[1][1]==want[1][1],(g,variant,step,j,'hidden')
       equal(got[1][0],want[1][0],(g,variant,step,j));snapshots+=1;matrix_count+=count*3+1;material_fields+=len(bindings)*17;raw.update(got[1][0])
      frames+=1
    finally:
     for p in materials:lib.bk_material_pose_destroy(p)
     lib.bk_ending_normal_assets_destroy(owner)
 finally:lib.bk_resources_destroy(store)
 assert float_fields==matrix_count*16+material_fields
 report=dict(passed=True,profiles=10,frames=frames,aim_cases=4000,snapshots=snapshots,matrices=matrix_count,material_fields=material_fields,compared_float_fields=float_fields,walks=walks,max_relative_error=worst,exe_sha256=hashlib.sha256(exe).hexdigest(),component_sha256=raw.hexdigest(),scope=__doc__)
 (ROOT/'local/original-ending-special-scene-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
