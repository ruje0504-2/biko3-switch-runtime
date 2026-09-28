"""Complete495d92 scalar/clip-write dispatch vs original instructions.
Clip request4018c8 and media are observing service boundaries here; per-instance
real scheduler and assets are separately exercised by ending-auxiliary-probe.
4dfb96/4a07a9 run original instructions with explicit eye-buffer fixtures.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame,FIELDS,BYTES
from model_binding import ROOT,library
class State(C.Structure):
 _fields_=[(n,C.c_int32) for n in ['gate','variant','selection','base','index','pending','direction']]+[('progress',C.c_float)]+[(n,C.c_int32) for n in ['expression_a','expression_b']]
class Call(C.Structure):
 _fields_=[('operation',C.c_int),('slot',C.c_uint)]+[(n,C.c_int32) for n in ['cue','bank','flags','volume']]
Active=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int32),C.c_void_p)
Write=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int,C.c_int32,C.c_void_p)
Request=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Audio=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Call),C.POINTER(C.c_int),C.c_void_p)
Eyes=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('active',Active),('write',Write),('request',Request),('audio',Audio),('eyes',Eyes)]
ADDR=[0x721ef0,0x721e04,0x7220f4,0x6ea024,0x721edc,0x722100,0x6ea35c,0x721e20,0x721df4,0x721df0]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4018c8,0x4946b4,0x49490a,0x4a07a9,0x4ad2bf,0x300d100,0x300d200]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.word(0x721b28,self.actor)
  self.word(0x300b000+0x24,0x300d100);self.word(0x300b000+0x48,0x300d200)
  for i in range(6):self.word(0x300a000+i*32,0x300b000)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def integer(self,a):return struct.unpack('<i',self.u.mem_read(a,4))[0]
 def read(self):
  s=State();f=Frame()
  for (name,typ),a in zip(State._fields_,ADDR):setattr(s,name,struct.unpack('<f' if typ==C.c_float else '<i',self.u.mem_read(a,4))[0])
  for name,a in FIELDS:setattr(f,name,self.integer(a))
  for name,a in BYTES:setattr(f,name,self.u.mem_read(a,1)[0])
  return bytes(s),bytes(f),self.integer(self.actor+0x140),tuple(bytes(self.u.mem_read(self.actor+0x190+i*156+o,4)) for i in range(128) for o in [0x54,0x60,0x70,0x74]),self.integer(0x300c098)
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,=struct.unpack('<I',u.mem_read(sp,4));result=0;pop=4
  def words(n):return struct.unpack('<'+'i'*n,u.mem_read(sp+4,n*4))
  if a==0x4a07a9:
   _,slot=words(2);self.trace.append((('eyes',slot),self.read()));return
  if a==0x4018c8:
   _,slot=words(2);self.trace.append((('request',slot),self.read()));self.word(self.actor+0x140,slot)
  elif a==0x4946b4:
   cue,slot,flags=words(3);self.trace.append((('audio',3,slot,cue,0,flags,self.voice),self.read()));self.present[slot]=True;self.playing[slot]=True
  elif a==0x49490a:
   cue,bank,slot,flags=words(4);self.trace.append((('audio',4,slot,cue,bank,flags&255,self.voice),self.read()));self.present[slot]=True;self.playing[slot]=True
  elif a==0x4ad2bf:
   ptr,flags,volume=words(3);slot=5;assert ptr==(0x300a000+slot*32 if self.present[slot] else 0)
   self.trace.append((('audio',2,slot,0,0,flags,volume),self.read()));self.playing[slot]=self.present[slot]
  else:
   ptr,=words(1);slot=(ptr-0x300a000)//32
   if a==0x300d100:
    _,out=words(2);self.word(out,1 if self.playing[slot] else 2);result=self.hr[slot];pop=12
    self.trace.append((('audio',0,slot,0,0,0,0),self.read()))
   else:
    self.trace.append((('audio',1,slot,0,0,0,0),self.read()));self.playing[slot]=False;pop=8
  u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,f,slot,rows,proposed,present,playing,hr,voice,effect,eye):
  self.u.mem_write(self.actor,bytes(0x5100));self.word(self.actor+0x140,slot)
  for i,row in enumerate(rows):
   for value,off in zip(row,[0x54,0x60,0x70,0x74]):self.word(self.actor+0x190+i*156+off,value)
  for (name,_),addr in zip(State._fields_,ADDR):self.u.mem_write(addr,bytes(s)[getattr(State,name).offset:getattr(State,name).offset+4])
  for name,a in FIELDS:self.word(a,getattr(f,name))
  for name,a in BYTES:self.u.mem_write(a,bytes([getattr(f,name)]))
  self.voice=voice;self.word(0xbe9a08,voice);self.word(0xbe9a10,effect)
  self.present=list(present);self.playing=list(playing);self.hr=hr
  for i in range(6):self.word(0x722334+i*0x120,0x300a000+i*32 if present[i] else 0)
  self.word(0x70d374,77 if eye else 0);self.word(0x70d384,0x300c000);self.word(0x300c098,31)
  self.trace=[];self.call(0x495d92,struct.pack('<i',proposed))
  return self.u.reg_read(UC_X86_REG_EAX),self.trace,self.read()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x495d92)
 lib.bk_ending_auxiliary_change.argtypes=[C.POINTER(State),C.POINTER(Frame),C.c_int32,C.c_int32,C.c_int32,C.POINTER(Ops),C.POINTER(C.c_int32),C.c_void_p]
 frames=20000;calls=accepted=0;branches=[0]*4
 for case in range(frames):
  s=State(1 if case%4 else rng.choice([0,2,-1]),case%2,case%10,rng.choice([-0x80000000,0x7fffffff,7]),8,9,10,rng.choice([-.1,0,.59,.6,.79,.8,1,float('nan'),float('inf'),-float('inf')]),11,12)
  f=Frame();f.group=case%5;f.phase=rng.choice([0,1,6,7,9]);f.camera_cached=55
  slot=[rng.randrange(-2,17)];proposed=rng.choice([-0x80000000,-1,0,0,1,1,2,2,3,3,4,0x7fffffff]);voice=-800;effect=-500;eye=case%3!=0
  rows=[[struct.unpack('<I',struct.pack('<f',rng.uniform(0,500)))[0],struct.unpack('<I',struct.pack('<f',rng.uniform(0,500)))[0],rng.randrange(2),rng.randrange(128)] for _ in range(128)]
  present=[rng.choice([False,True,True]) for _ in range(6)];playing=[rng.choice([False,True]) for _ in range(6)];hr=[rng.choice([0,0,0,-1,1]) for _ in range(6)]
  expected=n.run(s,f,slot[0],rows,proposed,present,playing,hr,voice,effect,eye);trace=[];selected=[31]
  def snap():return bytes(s),bytes(f),slot[0],tuple(struct.pack('<I',v&0xffffffff) for row in rows for v in row),selected[0]
  @Active
  def active(_,out,err):out[0]=slot[0];return 1
  @Write
  def write(_,i,k,v,err):rows[i][{0:2,1:3,2:1}[k]]=rows[i][0] if k==2 else v;return 1
  @Request
  def request(_,i,err):trace.append((('request',i),snap()));slot[0]=i;return 1
  @Audio
  def audio(_,cp,out,err):
   c=cp.contents
   if c.operation!=0 or present[c.slot]:trace.append((('audio',*[getattr(c,k) for k,_ in Call._fields_]),snap()))
   out[0]=int(present[c.slot] and playing[c.slot] and hr[c.slot]==0)
   if c.operation==1:playing[c.slot]=False
   elif c.operation==2:playing[c.slot]=present[c.slot]
   elif c.operation in [3,4]:present[c.slot]=True;playing[c.slot]=True
   return 1
  @Eyes
  def eyes(_,i,err):trace.append((('eyes',i),snap()));selected[0]=77 if eye else selected[0];return 1
  result=C.c_int32(-1);ops=Ops(None,active,write,request,audio,eyes)
  assert lib.bk_ending_auxiliary_change(C.byref(s),C.byref(f),proposed,voice,effect,C.byref(ops),C.byref(result),e),(case,e.value)
  actual=(result.value,trace,snap())
  if actual!=expected:
   for idx,(x,y) in enumerate(zip(trace+[(None,snap())],expected[1]+[(None,expected[2])])):
    if x!=y:raise AssertionError((case,proposed,idx,x,y))
   raise AssertionError((case,result.value,expected[0],len(trace),len(expected[1])))
  calls+=len(trace);accepted+=result.value
  if 0<=proposed<=3 and result.value:branches[proposed]+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,service_calls=calls,accepted=accepted,branches=branches,byte_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-auxiliary-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
