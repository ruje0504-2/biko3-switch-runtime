"""Bound actor_forest assembly vs native global attachment/draw and original
D3DX matrices: two real models, hidden and cross-model reparenting, camera
prepass/view, and atomic invalid draws. Animation locals are shared inputs;
this validates the registry/publication adapter, not animation sampling.
Recursive423b01 draw-disable flags remain independent of423a99 visibility;
draw-suppressed children must still receive their original matrix updates.
Only native matrix-stack lifetime and GPU view-upload boundaries are hooked.
"""
import argparse,ctypes as C,hashlib,json,math,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_prop_route_oracle import Native
from original_actor_phase_oracle import bind
from original_frame_tree_oracle import Visit,NONE
from model_binding import ROOT,decode,Model
from playback_binding import library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];F16=C.c_float*16
class VM(Native):
 frames,links,heap,stack_object,matrices,vt,stub,device=0x4000000,0x4200000,0x4300000,0x4400000,0x4410000,0x4420000,0x4421000,0x4422000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x500000);self.alloc=self.heap
  self.u.mem_write(self.vt,bytes(self.u.mem_read(0x53fb10,0x48)));self.word(self.vt+8,self.stub)
  self.word(self.device,self.device+0x100);self.word(self.device+0x12c,self.stub+4);self.word(0x6455a0,self.device)
  for a in [0x52440d,0x42d217,0x42d270,self.stub,self.stub+4]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.word(0x645600,self.frames);self.word(0x645604,self.frames+0x400);self.word(0x63f104,1)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&NONE))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def vector(self,a,v):self.u.mem_write(a,struct.pack('<16f',*v))
 def floats(self,a):return struct.unpack('<16f',self.u.mem_read(a,64))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);extra=0;eax=0
  if a==0x52440d:
   u.mem_write(self.stack_object,struct.pack('<5I',self.vt,1024,self.matrices,0,1));self.vector(self.matrices,I);self.word(self.read(sp+8),self.stack_object);extra=8
  elif a==0x42d217:eax=self.alloc;self.alloc+=16;assert self.read(sp+4)==12 and self.alloc<self.stack_object
  elif a==self.stub:extra=4
  elif a==self.stub+4:extra=12
  elif a!=0x42d270:raise AssertionError(hex(a))
  u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,eax)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--output',type=Path,default=ROOT/'local/original-actor-forest-oracle.json');args=ap.parse_args();exe=args.exe.read_bytes();vm=VM(exe);lib=library();bind(lib);err=C.create_string_buffer(256);fp=C.POINTER(C.c_float)
 lib.bk_actor_pose_publish_node.argtypes=[C.c_void_p,C.c_uint32,fp,C.c_void_p]
 lib.bk_actor_pose_parent_world.argtypes=[C.c_void_p,C.c_uint32];lib.bk_actor_pose_parent_world.restype=fp
 lib.bk_actor_pose_advance.argtypes=[C.c_void_p,C.c_int,C.c_float,C.c_void_p]
 class Edit(C.Structure):_fields_=[('frame',C.c_uint32),('hidden',C.c_uint32)]
 lib.bk_actor_pose_visibility.argtypes=[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p]
 lib.bk_actor_pose_hidden.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
 lib.bk_actor_pose_draw_disabled.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
 lib.bk_actor_forest_draw_disable.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p]
 for name,types,result in [('create',[C.c_uint32],C.c_void_p),('destroy',[C.c_void_p],None),('attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(C.c_int)],C.c_int),('parent',[C.c_void_p,C.c_uint32],C.c_uint32),('first',[C.c_void_p,C.c_uint32],C.c_uint32),('next',[C.c_void_p,C.c_uint32],C.c_uint32),('refresh_walk',[C.c_void_p,C.POINTER(Visit),C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),('draw_walk',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.POINTER(Visit),C.c_uint32,C.POINTER(C.c_uint32)],C.c_int)]:
  fn=getattr(lib,'bk_frame_tree_'+name);fn.argtypes=types;fn.restype=result
 for name,types,result in [
 ('create',[C.POINTER(C.c_void_p),C.c_uint32,C.c_void_p],C.c_void_p),('destroy',[C.c_void_p],None),
 ('node',[C.c_void_p,C.c_uint32,C.c_uint32],C.c_uint32),('tree',[C.c_void_p],C.c_void_p),('world',[C.c_void_p,C.c_uint32],fp),('view',[C.c_void_p],fp),
 ('anchor',[C.c_void_p,C.c_uint32,fp,C.c_uint32,C.c_void_p],C.c_int),('attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p],C.c_int),
 ('draw',[C.c_void_p,C.c_uint32,C.POINTER(C.POINTER(Visit)),C.POINTER(C.c_uint32),C.c_void_p],C.c_int)]:
  fn=getattr(lib,'bk_actor_forest_'+name);fn.argtypes=types;fn.restype=result
 arc=Archive(args.data/'bk3_01.pp');objects=[];bindings=[None,None];roots=[];records=[];refresh=C.c_int();t=None;forest=None;matrices=steps=flags=0;worst=0
 manual_local=[I.copy(),I.copy()];manual_world=[I.copy(),I.copy()];manual_parent=[I.copy(),I.copy()]
 try:
  for name in ['h00_80','h01_80']:
   raw=arc.read(next(e for e in arc.entries if e.name==name+'.x'));xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'));ok,m,msg=decode(lib,raw);assert ok,msg;clips=lib.bk_clip_set_decode(xan,len(xan),err);assert clips,err.value
   root=next(i for i in range(m.contents.frame_count) if m.contents.frames[i].parent_index==NONE);pose=lib.bk_actor_pose_create(m,clips,root,None,(C.c_float*3)(0,0,0),0,0,1,err);assert pose,err.value
   start=len(bindings);objects.append((m,clips,pose,start));roots.append(start+root);bindings.extend((pose,i) for i in range(m.contents.frame_count));records.append(dict(name=name,frames=m.contents.frame_count,sha256=hashlib.sha256(raw).hexdigest()))
  n=len(bindings);assert n<2048
  forest=lib.bk_actor_forest_create((C.c_void_p*len(objects))(*[x[2] for x in objects]),len(objects),err);assert forest,err.value
  for root in roots:assert lib.bk_actor_forest_attach(forest,0,root,err),err.value
  t=lib.bk_actor_forest_tree(forest);visits=C.POINTER(Visit)();count=C.c_uint32();hidden=(C.c_uint32*n)()
  def world(node):return lib.bk_actor_forest_world(forest,node)[:16]
  def local(node):return manual_local[node] if node<2 else lib.bk_actor_pose_local(*bindings[node])[:16]
  lib.bk_matrix_multiply.argtypes=[fp,fp,fp]
  for node in range(n):
   f=vm.frames+node*0x400;vm.vector(f+0x80,local(node));vm.vector(f+0xc0,world(node));vm.vector(f+0x100,I if node<2 else lib.bk_actor_pose_parent_world(*bindings[node])[:16])
   p=lib.bk_frame_tree_parent(t,node);vm.word(f+0x22c,0 if p==NONE else vm.frames+p*0x400)
   children=[];c=lib.bk_frame_tree_first(t,node)
   while c!=NONE:children.append(c);c=lib.bk_frame_tree_next(t,c)
   vm.word(f+0x238,len(children));vm.word(f+0x230,vm.links+children[0]*16 if children else 0);vm.word(f+0x234,vm.links+children[-1]*16 if children else 0)
   for j,c in enumerate(children):vm.word(vm.links+c*16,vm.frames+c*0x400);vm.word(vm.links+c*16+4,vm.links+children[j-1]*16 if j else 0);vm.word(vm.links+c*16+8,vm.links+children[j+1]*16 if j+1<len(children) else 0)
  def check(label):
   nonlocal matrices,worst
   for node in range(n):
    actual=[world(node)] if node<2 else [world(node),lib.bk_actor_pose_parent_world(*bindings[node])[:16]]
    for off,values in zip([0xc0,0x100],actual):
     for j,(a,b) in enumerate(zip(values,vm.floats(vm.frames+node*0x400+off))):
      d=abs(a-b)/max(1,abs(b));worst=max(worst,d);assert math.isfinite(d) and d<2e-6,(label,node,hex(off),j,a,b,d)
     matrices+=1
  for step in range(160):
   for m,clips,pose,start in objects:
    assert lib.bk_actor_pose_advance(pose,-1,C.c_float([0,.016,.033,.1][step%4]),err),err.value
    edit=Edit(0,1 if step%11 in [3,4] else 0);assert lib.bk_actor_pose_visibility(pose,C.byref(edit),1,err)
   manual_local[0][12]=step*.0625;manual_local[1][13]=20+step*.1
   for node in [0,1]:
    assert lib.bk_actor_forest_anchor(forest,node,F16(*manual_local[node]),0,err),err.value
    vm.vector(vm.frames+node*0x400+0xc0,world(node))
   for node in range(n):
    if node>=2:assert lib.bk_actor_pose_hidden(*bindings[node],C.cast(C.byref(hidden,node*4),C.POINTER(C.c_uint32)))
    vm.vector(vm.frames+node*0x400+0x80,local(node));vm.word(vm.frames+node*0x400+0x70,hidden[node])
   if step%4==0:
    child=roots[1];parent=roots[0]+(step//4)%(objects[0][0].contents.frame_count) if step%8==0 else 0
    assert lib.bk_actor_forest_attach(forest,parent,child,err),err.value
    vm.call(0x422015,struct.pack('<II',vm.frames+parent*0x400,vm.frames+child*0x400));check(('attach',step))
   target=[0,roots[0],roots[1],roots[0]+10][step%4]
   disabled=[0,1,255,NONE][(step//4)%4]
   assert lib.bk_actor_forest_draw_disable(forest,target,disabled,err)
   vm.call(0x423b01,struct.pack('<II',vm.frames+target*0x400,disabled))
   for node in range(2,n):
    value=C.c_uint32();assert lib.bk_actor_pose_draw_disabled(*bindings[node],C.byref(value))
    assert value.value==vm.read(vm.frames+node*0x400+0x240),(step,node,'draw-disable')
    assert lib.bk_actor_pose_hidden(*bindings[node],C.byref(value)) and value.value==hidden[node]
    flags+=1
   assert not lib.bk_actor_forest_draw_disable(forest,n,0,err)
   assert lib.bk_actor_forest_draw(forest,target,C.byref(visits),C.byref(count),err),err.value
   vm.call(0x42261a,struct.pack('<I',vm.frames+target*0x400));check(('draw',step))
   for a,b in zip(lib.bk_actor_forest_view(forest)[:16],vm.floats(0x642fa8)):
    difference=abs(a-b)/max(1,abs(b));worst=max(worst,difference);assert difference<2e-6,(step,'view',a,b)
   steps+=1
  before=bytes(F16(*lib.bk_actor_forest_view(forest)[:16]));held=count.value
  assert not lib.bk_actor_forest_draw(forest,n,C.byref(visits),C.byref(count),err)
  assert before==bytes(F16(*lib.bk_actor_forest_view(forest)[:16])) and count.value==held
  # Invalid parent/frame cannot partially overwrite either cache.
  pose,frame=bindings[roots[0]];saved=bytes(F16(*world(roots[0])));parent_saved=bytes(F16(*lib.bk_actor_pose_parent_world(pose,frame)[:16]));bad=F16(*I);bad[3]=float('nan')
  assert not lib.bk_actor_pose_publish_node(pose,frame,bad,err);assert saved==bytes(F16(*world(roots[0]))) and parent_saved==bytes(F16(*lib.bk_actor_pose_parent_world(pose,frame)[:16]))
 finally:
  lib.bk_actor_forest_destroy(forest)
  for m,clips,pose,start in objects:lib.bk_actor_pose_destroy(pose);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(m)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=steps,matrices=matrices,draw_disable_checks=flags,max_relative_error=worst,records=records,scope=__doc__)
 args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
