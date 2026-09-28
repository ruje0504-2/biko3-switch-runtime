"""Original422015/4222d4 ordered reparenting and42261a draw traversal.
Matrix-stack COM services use a path-only fixture: this checks topology,
publication visits, hidden pruning, camera ancestry and submitted frame IDs,
not matrix values (covered by separate matrix/pose oracles). Billboard,
projection cameras and dynamic light nodes are disabled in the fixture.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP
from original_prop_route_oracle import Native
from model_binding import ROOT,library
NONE=0xffffffff
class Visit(C.Structure):_fields_=[('node',C.c_uint32),('submit',C.c_uint32)]
class VM(Native):
 frames,mesh,heap,matrix,vt,top,stubs=0x4000000,0x4010000,0x4020000,0x4060000,0x4061000,0x4062000,0x4063000
 def __init__(self,exe,n):
  super().__init__(exe);self.n=n;self.alloc=self.heap;self.u.mem_map(0x4000000,0x70000);self.trace=[];self.refresh=[];self.queued=[];self.camera_path=[];self.paths=[[]]
  self.word(self.matrix,self.vt);self.u.mem_write(self.top,struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
  for offset in [8,12,16,24,32,68]:self.word(self.vt+offset,self.stubs+offset);self.u.hook_add(UC_HOOK_CODE,self.hook,begin=self.stubs+offset,end=self.stubs+offset)
  for a in [0x42d217,0x42d270,0x52440d,0x4227df,0x423dfd,0x42a65f,0x42b582,0x4224cd]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.word(0x645600,self.frames);self.word(0x645604,self.frames+(n-1)*0x400)
  for i in range(n):
   f=self.frames+i*0x400;self.u.mem_write(f+0x80,bytes(self.u.mem_read(self.top,64)));self.u.mem_write(f+0xc0,bytes(self.u.mem_read(self.top,64)));self.word(f+0x244,self.mesh)
  self.word(self.mesh+4,0x3ea)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&NONE))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def identity(self,p):return (p-self.frames)//0x400
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);extra=0;eax=0
  if a in [0x4227df,0x423dfd]:
   node=self.identity(self.read(u.reg_read(UC_X86_REG_EBP)+8));(self.trace if a==0x4227df else self.refresh).append(node);return
  if a==0x42d217:
   size=self.read(sp+4);assert size==12;eax=self.alloc;self.alloc+=16;assert self.alloc<self.matrix
  elif a==0x42d270:pass
  elif a==0x52440d:self.word(self.read(sp+8),self.matrix);self.paths=[[]];extra=8
  elif a in [0x42a65f,0x42b582]:self.queued.append(self.identity(self.read(sp+8)))
  elif a==0x4224cd:self.camera_path=self.paths[-1].copy()
  else:
   offset=a-self.stubs;extra=4
   if offset==16:self.paths.append(self.paths[-1].copy())
   elif offset==12:self.paths.pop()
   elif offset==24:self.paths[-1]=[];extra=8
   elif offset==32:self.paths[-1].append(self.identity(self.read(sp+8)-0x80));extra=8
   elif offset==68:eax=self.top
   elif offset!=8:raise AssertionError(hex(a))
  u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,eax)
 def attach(self,p,c):
  self.refresh=[];self.call(0x422015,struct.pack('<II',self.frames+p*0x400,self.frames+c*0x400))
 def detach(self,p,c):self.call(0x4222d4,struct.pack('<II',self.frames+p*0x400,self.frames+c*0x400))
 def draw(self,target,hidden):
  self.trace=[];self.queued=[];self.camera_path=[]
  for i,h in enumerate(hidden):self.word(self.frames+i*0x400+0x70,h)
  self.call(0x42261a,struct.pack('<I',self.frames+target*0x400))
 def parent(self,i):
  v=self.read(self.frames+i*0x400+0x22c);return self.identity(v) if v else NONE
 def children(self,i):
  f=self.frames+i*0x400;p=self.read(f+0x230);out=[]
  for j in range(self.read(f+0x238)):out.append(self.identity(self.read(p)));p=self.read(p+8)
  assert p==0
  return out

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=40;vm=VM(exe,n);lib=library();rng=random.Random(0x422015)
 for name,types,result in [
 ('create',[C.c_uint32],C.c_void_p),('destroy',[C.c_void_p],None),('parent',[C.c_void_p,C.c_uint32],C.c_uint32),('first',[C.c_void_p,C.c_uint32],C.c_uint32),('next',[C.c_void_p,C.c_uint32],C.c_uint32),
 ('attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(C.c_int)],C.c_int),('detach',[C.c_void_p,C.c_uint32,C.c_uint32],C.c_int),
 ('refresh_walk',[C.c_void_p,C.POINTER(Visit),C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
 ('draw_walk',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.POINTER(Visit),C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
 ('camera_path',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.c_uint32,C.POINTER(C.c_uint32)],C.c_int)]:
  fn=getattr(lib,'bk_frame_tree_'+name);fn.argtypes=types;fn.restype=result
 t=lib.bk_frame_tree_create(n);assert t;visits=(Visit*n)();count=C.c_uint32();refresh=C.c_int();path=(C.c_uint32*n)();hidden=(C.c_uint32*n)();steps=draws=published=submitted=refreshes=cameras=rejects=0
 try:
  for case in range(5000):
   child=rng.randrange(1,n);parent=rng.randrange(n)
   if case<n-1:child=case+1;parent=child-1 if child%4 else 0
   if case%7==0 and case>=n:
    assert lib.bk_frame_tree_detach(t,parent,child);vm.detach(parent,child)
   else:
    ancestor=parent;cycle=False
    while ancestor!=NONE:
     if ancestor==child:cycle=True;break
     ancestor=lib.bk_frame_tree_parent(t,ancestor)
    previous=lib.bk_frame_tree_parent(t,child);refresh.value=-1
    ok=lib.bk_frame_tree_attach(t,parent,child,C.byref(refresh))
    if cycle:assert not ok and refresh.value==-1;rejects+=1
    else:
     assert ok and refresh.value==int(previous!=parent);vm.attach(parent,child)
     assert lib.bk_frame_tree_refresh_walk(t,visits,n,C.byref(count))
     if refresh.value:assert [v.node for v in visits[:count.value]]==vm.refresh;refreshes+=len(vm.refresh)
     else:assert vm.refresh==[]
   for i in range(n):
    assert lib.bk_frame_tree_parent(t,i)==vm.parent(i),(case,i)
    children=[];c=lib.bk_frame_tree_first(t,i)
    while c!=NONE:children.append(c);c=lib.bk_frame_tree_next(t,c)
    assert children==vm.children(i),(case,i,children,vm.children(i))
   for i in range(n):hidden[i]=rng.choice([0,0,0,0,1,0xffffffff]) if case%3 else 0
   target=rng.randrange(n) if case%4 else 0
   assert lib.bk_frame_tree_draw_walk(t,target,hidden,visits,n,C.byref(count));vm.draw(target,hidden)
   assert [v.node for v in visits[:count.value]]==vm.trace,(case,target,[(v.node,v.submit) for v in visits[:count.value]],vm.trace)
   assert [v.node for v in visits[:count.value] if v.submit]==vm.queued,(case,target,vm.queued)
   published+=count.value;submitted+=len(vm.queued);draws+=1
   assert lib.bk_frame_tree_camera_path(t,n-1,path,n,C.byref(count));assert list(path[:count.value])==vm.camera_path,(case,list(path[:count.value]),vm.camera_path);cameras+=count.value;steps+=1
 finally:lib.bk_frame_tree_destroy(t)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),mutations=steps,cycle_rejections=rejects,refresh_visits=refreshes,draws=draws,published_visits=published,submissions=submitted,camera_path_nodes=cameras,scope=__doc__)
 (ROOT/'local/original-frame-tree-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
