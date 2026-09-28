"""Normal ending regular draws: actual bound topology, original42261a/42273b
publication and mesh-frame submission order. Matrix stack/D3DX execute natively;
GPU queue insertion is a marker per mesh-bearing frame, not rasterization.
Component animation/local edits are explicit shared inputs. Verify that a
visible final background publishes all caches needed by later regular draws,
allowing one GPU geometry snapshot without changing original per-flush walks.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_actor_forest_oracle import VM,Visit,NONE,F16,I
from original_ending_normal_assets_oracle import bind as normal_bind,State,Presets
from original_menu_track_oracle import Edit
from model_binding import ROOT
from playback_binding import library

def bind(lib):
 normal_bind(lib);fp=C.POINTER(C.c_float)
 for name,args,result in [
 ('bk_actor_pose_advance',[C.c_void_p,C.c_int,C.c_float,C.c_void_p],C.c_int),
 ('bk_ending_normal_assets_forest',[C.c_void_p],C.c_void_p),
 ('bk_actor_forest_tree',[C.c_void_p],C.c_void_p),
 ('bk_actor_forest_world',[C.c_void_p,C.c_uint32],fp),
 ('bk_actor_forest_view',[C.c_void_p],fp),
 ('bk_frame_tree_count',[C.c_void_p],C.c_uint32),
 ('bk_frame_tree_parent',[C.c_void_p,C.c_uint32],C.c_uint32),
 ('bk_frame_tree_first',[C.c_void_p,C.c_uint32],C.c_uint32),
 ('bk_frame_tree_next',[C.c_void_p,C.c_uint32],C.c_uint32),
 ('bk_actor_forest_binding',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.POINTER(C.c_uint32)],C.c_int),
 ('bk_actor_pose_visibility',[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p],C.c_int),
 ('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
 ('bk_actor_forest_draw',[C.c_void_p,C.c_uint32,C.POINTER(C.POINTER(Visit)),C.POINTER(C.c_uint32),C.c_void_p],C.c_int)]:
  f=getattr(lib,name);f.argtypes=args;f.restype=result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();lib=library();bind(lib);err=C.create_string_buffer(256)
 store=lib.bk_resources_create(err);assert store
 for pack in ['bk3_08','bk3_03','bk3_04','fambom']:
  assert lib.bk_resources_mount(store,pack.encode(),str(a.data/(pack+'.pp')).encode(),err),err.value
 matrices=walks=submitted=frames=0;worst=0
 try:
  for g in range(5):
   for v in range(2):
    camera=State();camera.pose.world[:]=camera.matrix[:]=I;presets=Presets();rng=C.c_uint32(123)
    owner=lib.bk_ending_normal_assets_create(store,g,v,(C.c_uint32*4)(100,110,120,130),C.byref(rng),C.byref(camera),C.byref(presets),err);assert owner,err.value
    try:
     assert lib.bk_ending_normal_assets_load_background(owner,store,err),err.value
     forest=lib.bk_ending_normal_assets_forest(owner);tree=lib.bk_actor_forest_tree(forest);n=lib.bk_frame_tree_count(tree)
     poses=[lib.bk_ending_normal_assets_pose(owner,i) for i in range(5)];models=[lib.bk_actor_pose_model(p).contents for p in poses]
     bindings=[None,None];roots=[]
     for i,m in enumerate(models):
      roots.append(lib.bk_actor_forest_node(forest,i,next(j for j in range(m.frame_count) if m.frames[j].parent_index==NONE)))
      bindings.extend((i,j) for j in range(m.frame_count))
     assert len(bindings)==n and n<2048
     vm=VM(exe);vm.word(0x63f104,0);queued=[];marker=vm.stub+0x400
     vm.word(marker+4,0x3ea)
     def queue(u,address,size,unused):
      sp=u.reg_read(UC_X86_REG_ESP);queued.append((vm.read(sp+8)-vm.frames)//0x400)
      u.reg_write(UC_X86_REG_EIP,vm.read(sp));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EAX,0)
     for address in [0x42a65f,0x42b582]:vm.u.hook_add(UC_HOOK_CODE,queue,begin=address,end=address)
     for node in range(n):
      f=vm.frames+node*0x400;b=bindings[node]
      local=I if node==0 else list(camera.pose.world) if node==1 else lib.bk_actor_pose_local(poses[b[0]],b[1])[:16]
      vm.vector(f+0x80,local);vm.vector(f+0xc0,lib.bk_actor_forest_world(forest,node)[:16]);vm.vector(f+0x100,I if node<2 else lib.bk_actor_pose_parent_world(poses[b[0]],b[1])[:16])
      p=lib.bk_frame_tree_parent(tree,node);vm.word(f+0x22c,0 if p==NONE else vm.frames+p*0x400)
      if b and models[b[0]].frames[b[1]].mesh_index!=NONE:vm.word(f+0x244,marker)
      children=[];c=lib.bk_frame_tree_first(tree,node)
      while c!=NONE:children.append(c);c=lib.bk_frame_tree_next(tree,c)
      vm.word(f+0x238,len(children));vm.word(f+0x230,vm.links+children[0]*16 if children else 0);vm.word(f+0x234,vm.links+children[-1]*16 if children else 0)
      for j,c in enumerate(children):
       vm.word(vm.links+c*16,vm.frames+c*0x400);vm.word(vm.links+c*16+4,vm.links+children[j-1]*16 if j else 0);vm.word(vm.links+c*16+8,vm.links+children[j+1]*16 if j+1<len(children) else 0)
     for step in range(24):
      for i in [0,1,4]:
       assert lib.bk_actor_pose_advance(poses[i],-1,[0,.016,.033,.1][step%4],err),err.value
       root=next(j for j in range(models[i].frame_count) if models[i].frames[j].parent_index==NONE)
       edit=Edit(root,int(i!=4 and step%8==(3 if i==0 else 5)))
       assert lib.bk_actor_pose_visibility(poses[i],C.byref(edit),1,err)
      for node,b in enumerate(bindings):
       if b:
        vm.vector(vm.frames+node*0x400+0x80,lib.bk_actor_pose_local(poses[b[0]],b[1])[:16]);hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(poses[b[0]],b[1],C.byref(hidden));vm.word(vm.frames+node*0x400+0x70,hidden.value)
      snapshot=None
      for target in ([roots[4],roots[0],roots[1]] if step%2 else [roots[4],roots[0]]):
       visits=C.POINTER(Visit)();count=C.c_uint32();assert lib.bk_actor_forest_draw(forest,target,C.byref(visits),C.byref(count),err),err.value
       queued.clear();vm.call(0x42261a,struct.pack('<I',vm.frames+target*0x400))
       expected=[x.node for x in visits[:count.value] if x.submit and bindings[x.node] and models[bindings[x.node][0]].frames[bindings[x.node][1]].mesh_index!=NONE]
       assert queued==expected,(g,v,step,target,queued,expected);submitted+=len(queued)
       current=[]
       for node,b in enumerate(bindings):
        values=[lib.bk_actor_forest_world(forest,node)[:16]] if not b else [lib.bk_actor_forest_world(forest,node)[:16],lib.bk_actor_pose_parent_world(poses[b[0]],b[1])[:16]]
        for off,got in zip([0xc0,0x100],values):
         want=vm.floats(vm.frames+node*0x400+off)
         for x,y in zip(got,want):
          d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<2e-6,(g,v,step,target,node,off,x,y)
         matrices+=1;current.extend(got)
       if snapshot is None:snapshot=current
       else:assert current==snapshot,(g,v,step,'later regular walk changed cached pose')
       walks+=1
      frames+=1
    finally:lib.bk_ending_normal_assets_destroy(owner)
 finally:lib.bk_resources_destroy(store)
 report=dict(passed=True,frames=frames,walks=walks,matrices=matrices,mesh_frame_submissions=submitted,max_relative_error=worst,exe_sha256=hashlib.sha256(exe).hexdigest(),scope=__doc__)
 (ROOT/'local/original-ending-regular-walk-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report)
if __name__=='__main__':main()
