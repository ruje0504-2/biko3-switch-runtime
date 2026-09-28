"""Full4d9898 orchestration against original instructions. Camera/node/GPU
services are observed boundaries, not an assertion of native rasterization.
Both draw descriptors, names (including empty), duplicate/missing lookups,
ordered visibility/material calls, live state edits, target arithmetic,
viewport/pass boundaries and camera restoration are compared exactly.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame
from model_binding import ROOT, library
F=C.c_float; FP=C.POINTER(F); IP=C.POINTER(C.c_int32); BP=C.POINTER(C.c_uint8); UP=C.POINTER(C.c_uint32)
class Draw(C.Structure):
 _fields_=[('objects',C.c_uint32*52),('mode',C.c_int32),('shadow',C.c_int32),('event',C.c_uint32)]
class Bindings(C.Structure):
 _fields_=[('frame',C.POINTER(Frame)),('action',IP),('variant',BP),('restore',BP),('index',IP),('mode',IP),('cameras',C.POINTER(F*4)),('target',FP),('primary',UP),('auxiliary',UP)]
D=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Draw),C.c_void_p)
Read=C.CFUNCTYPE(C.c_int,C.c_void_p,FP,FP,FP,C.c_void_p)
Vec=C.CFUNCTYPE(C.c_int,C.c_void_p,FP,C.c_void_p)
Orient=C.CFUNCTYPE(C.c_int,C.c_void_p,FP,FP,C.c_void_p)
Publish=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Render=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int,C.c_uint,C.c_void_p)
Find=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.c_char_p,UP,C.c_void_p)
Hide=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p)
Material=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_char_p,C.c_int,F,C.c_void_p)
class Ops(C.Structure):
 _fields_=[('context',C.c_void_p),('draw',D),('read',Read),('position',Vec),('orientation',Orient),('aim',Vec),('publish',Publish),('render',Render),('find',Find),('hide',Hide),('material',Material)]
# Live externally owned words in exactly the binding order below.
WORDS=[0x721e00,0x721ee0,0x721e04,0x721edc,0x7220f4]
BYTES=[0x721b3c,0x721b3d,0x7220f9]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4a4701,0x422ea4,0x423564,0x429fd6,0x429f86,0x42a08b,0x422c49,0x4230bd,0x425904,0x423a99,0x4a7d10,0x425196,0x423e72,0x422d38,0x4232a8,0x300f100]:
   self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.word(0x6455a0,0x3007000);self.word(0x3007000,0x3007100);self.word(0x3007100+0x34,0x300f100)
  self.word(0x721b28,0x3001000);self.word(0x3001160,0x3001400);self.word(0x3001414,7)
  self.word(0x3001960,0x3001c00);self.word(0x3001c14,8);self.word(0x721ef4,0x3002000)
  self.word(0x645600,0x3003000);self.word(0x645604,0x3003400)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def floats(self,a,v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def read_f(self,a,n=3):return tuple(struct.unpack('<'+'f'*n,self.u.mem_read(a,n*4)))
 def name(self,p):return bytes(self.u.mem_read(p,260)).split(b'\0',1)[0]
 def state(self,values):
  for a,v in zip(WORDS,values[:5]):self.word(a,v)
  for a,v in zip(BYTES,values[5:8]):self.u.mem_write(a,bytes([v]))
  self.word(0x721b2c,0x3001800 if values[8] else 0)
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);words=struct.unpack('<10I',u.mem_read(sp,40));ret=words[0];args=words[1:];skip=4;event=None
  if a==0x4a4701:
   event=('draw',bytes(u.mem_read(sp+4,216)))
   self.draws+=1
  elif a==0x422ea4:self.floats(args[2],self.saved[:3])
  elif a==0x423564:
   self.floats(args[2],self.saved[3:6]);self.floats(args[3],self.saved[6:9]);event=('read',self.saved)
  elif a in (0x429fd6,0x429f86):event=('render',0 if a==0x429fd6 else 3,0)
  elif a==0x42a08b:event=('render',2,args[0])
  elif a==0x300f100:
   assert args[0]==0x3007000 and args[1] in (0x688690,0x6886a8)
   event=('render',1,int(args[1]==0x6886a8));skip+=8
  elif a==0x422c49:event=('position',self.read_f(sp+12))
  elif a==0x422d38:event=('position',self.read_f(args[2]))
  elif a==0x4230bd:event=('orientation',self.read_f(sp+12),self.read_f(sp+24))
  elif a==0x4232a8:event=('orientation',self.read_f(args[2]),self.read_f(args[3]))
  elif a==0x425904:
   name=self.name(args[1]);node=self.lookup[name];self.word(args[2],node);event=('find',args[0],name,node)
  elif a==0x423a99:event=('hide',args[0],args[1])
  elif a==0x4a7d10:event=('material',self.name(args[0]),args[1],self.read_f(sp+12,1)[0])
  elif a==0x425196:
   assert args[3]==0;event=('aim',self.read_f(args[1]))
  elif a==0x423e72:event=('publish',)
  else:raise AssertionError(hex(a))
  if event:
   self.trace.append(event)
   if a==0x4a4701 and self.changes[self.draws-1] is not None:self.state(self.changes[self.draws-1])
   if a in (0x4230bd,0x4232a8):
    self.orientations+=1
    if self.orientations==2 and self.reset_change is not None:self.state(self.reset_change)
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+skip);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,values,d,cameras,target,saved,lookup,changes,reset_change):
  self.trace=[];self.draws=0;self.orientations=0;self.state(values);self.saved=saved;self.lookup=lookup;self.changes=changes;self.reset_change=reset_change
  self.u.mem_write(0x71944c,bytes(cameras));self.floats(0x30020f0,target);self.u.mem_write(0x3006000,bytes(d)[:216])
  self.call(0x4d9898,struct.pack('<I',0x3006000))
  return self.trace,bytes(self.u.mem_read(0x3006000,216))+bytes(d)[216:]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256)
 lib.bk_ending_special_draw.argtypes=[C.POINTER(Bindings),C.POINTER(Draw),C.POINTER(Ops),C.c_void_p]
 lib.bk_ending_special_cameras.argtypes=[C.POINTER(F*4),C.c_uint]
 lib.bk_ending_special_hidden_name.argtypes=[C.c_uint]*3;lib.bk_ending_special_hidden_name.restype=C.c_char_p
 tables=[];names=set([b'M_pantsu',b'M_pantsuU',b'oisu_005B_Layer1',b'bunben_semotare',b'bunben_dai',b'bunben_te',b'OYU',b'M_syokusyu_kuch_gawa',b'M_syokusyu_kuch_moza'])
 for g,addr in enumerate([0x571238,0x5718f8,0x571fb8,0x572678,0x572d38]):
  table=((F*4)*108)();assert lib.bk_ending_special_cameras(table,g);assert bytes(table)==bytes(n.u.mem_read(addr,1728));tables.append(table)
  for v in range(10):
   for i in range(3):
    wanted=n.name(0x564b10+(g*30+v*3+i)*260);assert lib.bk_ending_special_hidden_name(g,v,i)==wanted;names.add(wanted)
 rng=random.Random(0x4d9898);calls=draws=hides=materials=missing=aliases=0;axes=[0,0,0,0]
 def values(case):
  group=case%5;index=rng.randrange(108);mode=rng.choice([0,1,2,3,-1])
  if group==4 and case%3:index=47;mode=1
  if group==0 and case%3:index=104;mode=2
  return [rng.choice([1,2,3,4,5,6,7,8,8,8,9]),rng.choice([0,3,4,6,7,8]),rng.choice([0,1,2,-1]),index,mode,group,rng.randrange(10),rng.choice([0,1,2,255]),rng.randrange(2)]
 for case in range(12000):
  v=values(case);changes=[values(case+17) if case%7==0 else None,values(case+31) if case%4==0 else None];reset_change=values(case+43) if case%11==0 else None
  table=((F*4)*108).from_buffer_copy(tables[v[5]])
  if case%3==0:
   for row in table:row[:]=[rng.uniform(-500,500) for _ in range(4)]
  target=(F*3)(*[rng.uniform(-1000,1000) for _ in range(3)]);saved=tuple(F(rng.uniform(-100,100)).value for _ in range(9))
  lookup={name:rng.choice([0,0,31,31,31,32,33,34,35]) for name in sorted(names)}
  missing+=sum(x==0 for x in lookup.values());aliases+=len(lookup)-len(set(lookup.values()))
  d=Draw();d.objects[:]=[rng.getrandbits(32) for _ in range(52)];d.mode=rng.choice([0,1,2]);d.shadow=rng.randrange(3);d.event=123
  want,final=n.run(v,d,table,target,saved,lookup,changes,reset_change)
  frame=Frame();action=C.c_int32();index=C.c_int32();mode=C.c_int32();variant=C.c_uint8();restore=C.c_uint8();primary=C.c_uint32(7);aux=C.c_uint32()
  def state(v):
   frame.phase,frame.state_721ee0,action.value,index.value,mode.value=v[:5];frame.group,variant.value,restore.value=v[5:8];aux.value=8 if v[8] else 0
  state(v);b=Bindings(C.pointer(frame),C.pointer(action),C.pointer(variant),C.pointer(restore),C.pointer(index),C.pointer(mode),table,target,C.pointer(primary),C.pointer(aux));trace=[];nd=[0];no=[0]
  @D
  def draw(_,desc,err):
   trace.append(('draw',bytes(desc.contents)[:216]));c=changes[nd[0]];nd[0]+=1
   if c is not None:state(c)
   return 1
  @Read
  def read(_,p,f,u,err):
   
   for i in range(3):p[i]=saved[i];f[i]=saved[i+3];u[i]=saved[i+6]
   trace.append(('read',saved));return 1
  @Vec
  def position(_,p,err):trace.append(('position',tuple(p[:3])));return 1
  @Orient
  def orientation(_,f,u,err):
   trace.append(('orientation',tuple(f[:3]),tuple(u[:3])));no[0]+=1
   if no[0]==2 and reset_change is not None:state(reset_change)
   return 1
  @Vec
  def aim(_,p,err):trace.append(('aim',tuple(p[:3])));return 1
  @Publish
  def publish(_,err):trace.append(('publish',));return 1
  @Render
  def render(_,kind,arg,err):trace.append(('render',kind,arg));return 1
  @Find
  def find(_,root,name,node,err):node[0]=lookup[name];trace.append(('find',root,name,node[0]));return 1
  @Hide
  def hide(_,node,hidden,err):trace.append(('hide',node,hidden));return 1
  @Material
  def material(_,name,hidden,alpha,err):trace.append(('material',name,hidden,alpha));return 1
  ops=Ops(None,draw,read,position,orientation,aim,publish,render,find,hide,material)
  assert lib.bk_ending_special_draw(C.byref(b),C.byref(d),C.byref(ops),e),(case,e.value)
  if trace!=want:
   for i,(x,y) in enumerate(zip(trace,want)):
    if x!=y:raise AssertionError((case,v,changes,reset_change,i,x,y))
   raise AssertionError((case,len(trace),len(want)))
  assert bytes(d)==final,(case,'descriptor')
  for event in trace:
   draws+=event[0]=='draw';hides+=event[0]=='hide';materials+=event[0]=='material'
   if event[0]=='aim':
    changed=[i for i in range(3) if event[1][i]!=target[i]];assert len(changed)<=1;axes[changed[0] if changed else 3]+=1
  calls+=len(trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=12000,services=calls,draws=draws,visibility_calls=hides,material_calls=materials,camera_table_rows=540,base_names=150,target_axes=axes,missing_lookup_fixtures=missing,alias_fixtures=aliases,byte_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-special-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
