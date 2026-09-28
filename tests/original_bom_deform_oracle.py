"""Original4aab5e mapping/grouping +4aaebb callback; native math unhooked.
Synthetic shared-target, duplicate, alias, NULL and raw disabled flags, plus
all seven real BOM/VIX mesh profiles in explicit aligned base-pose fixtures.
Also4a4dc3's node math422c49/4230bd, without a topology change.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine,multiply
from original_bom_oracle import Config,Archive
from model_binding import ROOT,library,decode,Vertex
NONE=0xffffffff
F16=C.c_float*16
class Node(C.Structure):_fields_=[('local',F16),('world',F16),('parent',F16)]
class View(C.Structure):_fields_=[('vertices',C.POINTER(Vertex)),('count',C.c_uint32),('world',C.POINTER(C.c_float))]
class Binding(C.Structure):_fields_=[('source',C.c_uint32),('target',C.c_uint32),('indices',C.POINTER(C.c_uint16)),('count',C.c_size_t)]
def identity():return [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def bind(lib):
 lib.bk_bom_deform_create.argtypes=[C.POINTER(View),C.c_size_t,C.POINTER(Binding),C.c_size_t,C.c_void_p];lib.bk_bom_deform_create.restype=C.c_void_p
 lib.bk_bom_deform_destroy.argtypes=[C.c_void_p]
 lib.bk_bom_deform_mapping.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(C.c_int32),C.POINTER(C.POINTER(C.c_uint32)),C.POINTER(C.c_size_t)]
 lib.bk_bom_deform_draw.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(View),C.c_size_t,C.POINTER(C.c_int32),C.c_void_p]
 lib.bk_node_reference_position.argtypes=[C.POINTER(Node),C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_void_p]
 lib.bk_node_reference_orientation.argtypes=[C.POINTER(Node),C.POINTER(C.c_float),C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_void_p]
class Native:
 stack,stop=0x200e000,0x300f000
 def __init__(self,exe):
  self.u=machine(exe);self.u.mem_map(0x4000000,0x2000000);self.heap=0x4100000;self.meshes={}
  for a in [0x46d9b4,0x445a8e,0x445ac1]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def alloc(self,data):
  address=self.heap;self.heap+=(len(data)+15)&~15;self.heap+=16
  assert self.heap<0x6000000
  self.u.mem_write(address,data);return address
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);p=self.read(sp+4)
  if a==0x46d9b4:u.reg_write(UC_X86_REG_EAX,self.alloc(bytes(p)))
  elif a==0x445a8e:
   count,vertices=self.meshes[p];self.word(self.read(sp+8),count);self.word(self.read(sp+12),vertices)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def call(self,a,args):
  self.u.mem_write(self.stack,struct.pack('<'+'I'*(len(args)+1),self.stop,*args));self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  self.u.emu_start(a,self.stop,count=1500000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(a)
 def node(self,node,reference,position=None,forward=None,up=None):
  self.u.mem_write(0x3001000+0x80,bytes(node));self.u.mem_write(0x3002000+0xc0,bytes(reference))
  floats=list(position) if position is not None else list(forward)+list(up)
  args=[0x3001000,0x3002000]+list(struct.unpack('<'+'I'*len(floats),struct.pack('<'+'f'*len(floats),*floats)))
  self.call(0x422c49 if position is not None else 0x4230bd,args)
  return Node.from_buffer_copy(bytes(self.u.mem_read(0x3001080,C.sizeof(Node))))
 def init(self,views,bindings):
  self.heap=0x4100000;self.meshes={};self.ptrs=[];self.frames=[]
  self.u.mem_write(0x7080e0,bytes(0x104));self.word(0x7080e0,len(bindings))
  for i,v in enumerate(views):
   p=0x4000000+i*0x400;frame=p+0x200;vertices=self.alloc(C.string_at(v.vertices,v.count*60));self.ptrs.append(p);self.frames.append(frame)
   self.meshes[p]=(v.count,vertices);self.word(p+0x100,frame);self.u.mem_write(frame+0xc0,C.string_at(v.world,64))
  for i,b in enumerate(bindings):
   self.word(0x7080e4+i*4,0 if b.source==NONE else self.ptrs[b.source]);self.word(0x708104+i*4,0 if b.target==NONE else self.ptrs[b.target])
   self.word(0x708164+i*4,b.count);self.word(0x708144+i*4,self.alloc(C.string_at(b.indices,b.count*2)))
  self.call(0x4aab5e,[])
 def draw(self,views,disabled,target):
  for i,v in enumerate(views):self.u.mem_write(self.frames[i]+0xc0,C.string_at(v.world,64))
  for i,x in enumerate(disabled):self.word(0x7081a4+i*4,x&NONE)
  self.call(0x4aaebb,[self.ptrs[target] if target<len(views) else (0 if target==NONE else 0xdeadbeef)])

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);rng=random.Random(0x4aab5e)
 worst=0;nodes=maps=callbacks=vertices=0;records=[];fixture=bytearray(b'BOM1');fixture_records=[]
 def compare_node(x,y,label):
  nonlocal worst,nodes
  assert bytes(x.parent)==bytes(y.parent)
  for name in ['local','world']:
   for i,(c,o) in enumerate(zip(getattr(x,name),getattr(y,name))):
    d=abs(c-o)/max(1,abs(o));worst=max(worst,d);assert math.isfinite(d) and d<2e-5,(label,name,i,c,o,d)
  nodes+=1
 def matrix():
  m=identity()
  for i in range(3):
   for j in range(3):m[i*4+j]=rng.uniform(-.2,.2)+(rng.uniform(.7,1.3) if i==j else 0)
  m[12:15]=[rng.uniform(-30,30) for _ in range(3)];return m
 for case in range(600):
  node=Node(F16(*matrix()),F16(*matrix()),F16(*matrix()));ref=F16(*matrix())
  if case%3==0:node.parent[15]=.85;ref[15]=1.000005
  if case%5==0:node.parent[3]=.001;node.local[7]=.002;node.world[11]=.003
  for step in range(4):
   if step%2==0:
    p=(C.c_float*3)(*[rng.uniform(-10,10) for _ in range(3)]);expected=n.node(node,ref,position=p);assert lib.bk_node_reference_position(C.byref(node),ref,p,error),error.value
   else:
    f=(C.c_float*3)(*[rng.uniform(-1,1) for _ in range(3)]);u=(C.c_float*3)(0,1,0)
    if case%17==0:f[:]=[0,0,0]
    if case%19==0:f[:]=u[:]
    expected=n.node(node,ref,forward=f,up=u);assert lib.bk_node_reference_orientation(C.byref(node),ref,f,u,error),error.value
   compare_node(node,expected,(case,step))
 print('PASS node reference synthetic',nodes,'max',worst,flush=True)
 def check(buffers,worlds,specs,label,steps,save=False):
  nonlocal maps,callbacks,vertices,worst
  views=(View*len(buffers))(*[View(buf,len(buf),w) for buf,w in zip(buffers,worlds)])
  indices=[(C.c_uint16*len(s[2]))(*s[2]) for s in specs]
  bindings=(Binding*len(specs))(*[Binding(s[0],s[1],ix,len(ix)) for s,ix in zip(specs,indices)])
  handle=lib.bk_bom_deform_create(views,len(views),bindings,len(bindings),error);assert handle,(label,error.value)
  if save:
   fixture_records.append(1);fixture.extend(struct.pack('<II',len(views),len(bindings)))
   for v in views:fixture.extend(struct.pack('<I',v.count)+C.string_at(v.world,64)+C.string_at(v.vertices,v.count*60))
   for b in bindings:fixture.extend(struct.pack('<III',b.source,b.target,b.count)+C.string_at(b.indices,b.count*2))
  try:
   n.init(views,bindings)
   for i,b in enumerate(bindings):
    group=C.c_int32();ptr=C.POINTER(C.c_uint32)();count=C.c_size_t();assert lib.bk_bom_deform_mapping(handle,i,C.byref(group),C.byref(ptr),C.byref(count))
    assert group.value==n.read(0x708184+i*4),(label,i,group.value)
    if b.source!=NONE and b.target!=NONE:
     want=struct.unpack('<'+'I'*b.count,n.u.mem_read(n.read(0x708124+i*4),b.count*4)) if b.count else ()
     assert tuple(ptr[:count.value])==want,(label,i,tuple(ptr[:count.value]),want);maps+=b.count
   for step in range(steps):
    target=([s[1] for s in specs]+[NONE,NONE-1])[step%(len(specs)+2)]
    flags=(C.c_int32*len(bindings))(*[([0,1,2,-1][(step+i)%4]) for i in range(len(bindings))])
    for i,w in enumerate(worlds):w[12]+=.002*(i+1);w[13]-=.001
    n.draw(views,flags,target);assert lib.bk_bom_deform_draw(handle,target,views,len(views),flags,error),(label,step,error.value)
    for i,b in enumerate(buffers):
     expected=bytes(n.u.mem_read(n.meshes[n.ptrs[i]][1],len(b)*60))
     got=bytes(b)
     # beta and all UV fields are byte-preserved, including NaNs.
     for j in range(len(b)):
      assert got[j*60+12:j*60+16]==expected[j*60+12:j*60+16] and got[j*60+28:(j+1)*60]==expected[j*60+28:(j+1)*60]
      for k in [0,4,8,16,20,24]:
       c=struct.unpack_from('<f',got,j*60+k)[0];o=struct.unpack_from('<f',expected,j*60+k)[0];d=abs(c-o)/max(1,abs(o));worst=max(worst,d);assert math.isfinite(d) and d<2e-5,(label,step,i,j,k,c,o,d)
     vertices+=len(b)
    callbacks+=1
  finally:lib.bk_bom_deform_destroy(handle)
 for case in range(300):
  buffers=[];worlds=[]
  for i in range(4):
   buf=(Vertex*(3+case%7))()
   for j,v in enumerate(buf):
    v.position[:]=[rng.uniform(-10,10) for _ in range(3)];v.normal[:]=[rng.uniform(-2,2) for _ in range(3)];v.beta=-.5;v.uv[3][0]=float('nan')
   if case%7==0:buf[1]=buf[0] # equal nearest candidates
   buffers.append(buf);w=F16(*identity());w[15]=[1,1.000005,1.00002,.85][case%4]
   if case%9==0:w[3]=.0001
   worlds.append(w)
  specs=[(0,2,[0,1,1,2]),(1,2,[0,2]),(2,2,[0,0,1]) if case%2 else (3,3,[0,1,0]),(NONE,1,[]),(0,NONE,[]),(NONE,NONE,[])]
  check(buffers,worlds,specs,('synthetic',case),10)
 print('PASS synthetic deformation',maps,callbacks,flush=True)
 # Actual assets: explicit metadata inventory; production loaders must pass
 # their selected models and sampled worlds, not search FAM/BOM metadata.
 packs=[Archive(a.data/f'bk3_{i:02}.pp') for i in range(8,15)];arc=Archive(a.data/'fambom.pp')
 lib.bk_bom_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Config),C.c_void_p]
 lib.bk_model_find_frame.argtypes=[C.c_void_p,C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p]
 lib.bk_model_find_submesh.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p]
 def read(pack,name):return pack.read(next(e for e in pack.entries if e.name.encode().lower()==name.lower()))
 for e in arc.entries:
  if not e.name.lower().endswith('.bom'):continue
  config=Config();raw=arc.read(e);assert lib.bk_bom_decode(raw,len(raw),C.byref(config),error)
  pack=next(p for p in packs if all(any(e.name.encode()==name for e in p.entries) for name in [config.primary,config.secondary]));models=[];names=[];world_arrays=[];locals_arrays=[]
  for name in [config.primary,config.secondary]:
   model_name=read(pack,name)[:256].split(b'\0')[0];ok,model,msg=decode(lib,read(pack,model_name));assert ok,msg;models.append(model);names.append(model_name)
   w=F16*model.contents.frame_count;w=w();flat=C.cast(w,C.POINTER(C.c_float));assert lib.bk_model_world_matrices(model,flat,len(w)*16,error);world_arrays.append(w);locals_arrays.append([list(f.local) for f in model.contents.frames[:model.contents.frame_count]])
  try:
   primary,secondary=models;world=world_arrays[1]
   for b in config.bindings[:config.count]:
    parent=C.c_uint32();child=C.c_uint32();assert lib.bk_model_find_frame(primary,b.parent,C.byref(parent),error),error.value;assert lib.bk_model_find_frame(secondary,b.child,C.byref(child),error),error.value
    f=secondary.contents.frames[child.value];node=Node(F16(*locals_arrays[1][child.value]),world[child.value],world[f.parent_index] if f.parent_index!=NONE else F16(*identity()));node.local[12:15]=[0,0,0]
    ref=world_arrays[0][parent.value];pos=(C.c_float*3)(0,0,0);fw=(C.c_float*3)(0,0,1);up=(C.c_float*3)(0,1,0)
    expected=n.node(node,ref,position=pos);assert lib.bk_node_reference_position(C.byref(node),ref,pos,error);compare_node(node,expected,(e.name,b.child,'position'))
    expected=n.node(node,ref,forward=fw,up=up);assert lib.bk_node_reference_orientation(C.byref(node),ref,fw,up,error);compare_node(node,expected,(e.name,b.child,'orientation'))
    locals_arrays[1][child.value]=list(node.local);world[child.value][:]=node.world[:]
   # 423be2 refresh, unchanged original hierarchy; native matrix function.
   pending=set(range(secondary.contents.frame_count))
   while pending:
    ready=[i for i in pending if secondary.contents.frames[i].parent_index not in pending]
    assert ready
    for i in ready:
     parent=secondary.contents.frames[i].parent_index;world[i][:]=multiply(n.u,locals_arrays[1][i],world[parent]) if parent!=NONE else locals_arrays[1][i];pending.remove(i)
   buffers=[];worlds=[];registry={};specs=[];missing=[]
   def mesh(which,name):
    key=(which,name)
    if key in registry:return registry[key]
    index=C.c_uint32();model=models[which];assert lib.bk_model_find_submesh(model,names[which],name,C.byref(index),error),(e.name,name,error.value)
    sm=model.contents.submeshes[index.value];frames=[j for j in range(model.contents.frame_count) if model.contents.frames[j].mesh_index==sm.mesh_index];assert len(frames)==1
    buffers.append((Vertex*sm.vertex_count).from_buffer_copy(C.string_at(sm.vertices,sm.vertex_count*60)));worlds.append(F16(*world_arrays[which][frames[0]]));registry[key]=len(buffers)-1;return len(buffers)-1
   for b in config.bindings[:config.count]:
    target=mesh(0,b.target_mesh);source=mesh(1,b.source_mesh)
    if any(x.name.encode().lower()==b.selection.lower() for x in pack.entries):vix=read(pack,b.selection)
    else:
     assert (e.name,b.selection)==('kahansin.bom',b'mata.vix'),(e.name,b.selection)
     missing.append(b.selection.decode());vix=b'' # explicit empty-selection fixture; full file loader not claimed
    selection=struct.unpack('<'+'H'*(len(vix)//2),vix[:len(vix)//2*2]);specs.append((source,target,selection))
   check(buffers,worlds,specs,e.name,20,save=True);records.append(dict(name=e.name,meshes=len(buffers),bindings=len(specs),selected=sum(len(s[2]) for s in specs),vertices=sum(len(x) for x in buffers),missing_selection_fixture=missing))
   print('PASS actual BOM',records[-1],flush=True)
  finally:
   for model in models:lib.bk_model_destroy(model)
 fixture[4:4]=struct.pack('<I',len(fixture_records));(ROOT/'local/bom-deform-fixture.bin').write_bytes(fixture)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),node_calls=nodes,mapped_vertices=maps,callbacks=callbacks,checked_vertices=vertices,max_normalized_error=worst,actual=records,fixture_sha256=hashlib.sha256(fixture).hexdigest(),scope=__doc__)
 (ROOT/'local/original-bom-deform-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
