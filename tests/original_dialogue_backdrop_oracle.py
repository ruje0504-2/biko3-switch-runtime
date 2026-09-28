"""Full4f1503 with original50e633/50e6ba fade state machines.

4f19ec resource replacement is an explicit service boundary resetting its
constructed fade. No file/GPU output or full flow8 claim in this oracle.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class State(C.Structure):
 _fields_=[('image',Fade),('saved_expression',C.c_int32),('cycle',C.c_uint8),('kind',C.c_uint8),('wanted',C.c_uint8)]
class Bindings(C.Structure):
 _fields_=[('phase',C.POINTER(C.c_uint8)),('expression',C.POINTER(C.c_int32)),('curtain',C.POINTER(Fade)),('wanted',C.POINTER(C.c_uint8)),('name',C.c_char_p)]
Replace=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_char_p,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('replace',Replace)]
class Frame(C.Structure):_fields_=[('image',C.c_float),('curtain',C.c_float),('replaced',C.c_int)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4f19ec,0x50e633,0x50e6ba]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def sprite(self,addr,f):
  b=bytearray(0x16c);struct.pack_into('<f',b,0x12c,f.alpha);struct.pack_into('<f',b,0x138,f.speed);b[0x134]=f.stage;b[0x148:0x14b]=bytes([1,1,0]);self.u.mem_write(addr,bytes(b))
 def read_fade(self,addr):return Fade(struct.unpack('<f',self.u.mem_read(addr+0x12c,4))[0],struct.unpack('<f',self.u.mem_read(addr+0x138,4))[0],self.u.mem_read(addr+0x134,1)[0])
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<II',u.mem_read(sp+4,8))
  if a==0x4f19ec:
   assert args[0]==0xbefeec;self.events.append('replace')
   #50de40 writes fade fields but preserves its external+167 request byte.
   u.mem_write(0x73449c+0x12c,struct.pack('<f',1));u.mem_write(0x73449c+0x134,b'\0');u.mem_write(0x73449c+0x138,struct.pack('<f',2))
   ret=struct.unpack('<I',u.mem_read(sp,4))[0];u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
  else:self.events.append((a,args[0],args[1]&255))
 def run(self,s,phase,expr,curtain,wanted,name,dt):
  self.events=[];u=self.u;self.sprite(0x73449c,s.image);self.sprite(0x729c10,curtain)
  u.mem_write(0xbf00f0,struct.pack('<iBB',s.saved_expression,s.cycle,s.kind));u.mem_write(0x734603,bytes([s.wanted]));u.mem_write(0x729d77,bytes([wanted]));u.mem_write(0xbef798,bytes([phase]));u.mem_write(0x734034,struct.pack('<i',expr));u.mem_write(0xbefeec,name+bytes(256-len(name)));u.mem_write(0x733700,struct.pack('<f',dt))
  self.call(0x4f1503,b'');out=State.from_buffer_copy(s);out.image=self.read_fade(0x73449c);out.saved_expression,out.cycle,out.kind=struct.unpack('<iBB',u.mem_read(0xbf00f0,6));out.wanted=u.mem_read(0x734603,1)[0]
  return out,u.mem_read(0xbef798,1)[0],struct.unpack('<i',u.mem_read(0x734034,4))[0],self.read_fade(0x729c10),u.mem_read(0x729d77,1)[0]
def values(s):return (s.image.alpha,s.image.speed,s.image.stage,s.saved_expression,s.cycle,s.kind,s.wanted)
def fv(s):return (s.alpha,s.speed,s.stage)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();assert hashlib.sha256(exe).hexdigest()=='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
 n=Native(exe);lib=library();fn=lib.bk_dialogue_backdrop_step;fn.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Ops),C.c_float,C.POINTER(Frame),C.c_void_p];fn.restype=C.c_int
 rng=random.Random(0x4f1503);frames=replacements=0;phases=set();err=C.create_string_buffer(256)
 def check(s,p,x,c,w,name,dt):
  nonlocal frames,replacements
  expected,np,nx,nc,nw=n.run(s,p.value,x.value,c,w.value,name,dt)
  events=[]
  @Replace
  def replace(_,filename,e):events.append(filename);return 1
  ops=Ops(None,replace);b=Bindings(C.pointer(p),C.pointer(x),C.pointer(c),C.pointer(w),name);out=Frame()
  assert fn(C.byref(s),C.byref(b),C.byref(ops),dt,C.byref(out),err),err.value
  assert values(s)==values(expected),(frames,values(s),values(expected))
  assert (p.value,x.value,w.value)==(np,nx,nw),(frames,p.value,x.value,w.value,np,nx,nw)
  assert fv(c)==fv(nc),(frames,fv(c),fv(nc))
  assert (out.image,out.curtain,out.replaced)==(s.image.alpha,c.alpha,len(events))
  assert events==([name] if n.events[0]=='replace' else [])
  expected_events=(['replace'] if events else [])+[(0x50e633,0x73449c,s.wanted),(0x50e6ba,0x73449c,0),(0x50e6ba,0x729c10,0),(0x50e633,0x729c10,w.value)]
  assert n.events==expected_events,(frames,n.events,expected_events)
  frames+=1;replacements+=len(events);phases.add(p.value)
 names=[b'SG00001.bmp',b'sg00001.bmp',b'SG00001_extra.bmp',b'SG0000',b'SG10000.bmp',b'g01_10.bmp',b'']
 for i in range(15000):
  s=State(Fade(rng.random(),rng.choice([0,.1,2,7,100]),rng.randrange(6)),rng.randrange(-3,57),rng.choice([0,1,2,3,255]),rng.choice([0,1,2,255]),rng.choice([0,1,2,255]))
  c=Fade(rng.random(),rng.choice([0,.1,2,7]),rng.randrange(6));p=C.c_uint8(rng.choice([0,1,2,3,4,5,10,11,127,128,255]));x=C.c_int32(rng.randrange(-3,57));w=C.c_uint8(rng.choice([0,1,2,255]));check(s,p,x,c,w,names[i%len(names)],C.c_float(rng.choice([0,1/60,.1,1,10])).value)
 # Real order: actor acknowledges phase1 and5 before backdrop; traverse two
 # successive special-image cycles and a normal swap at15/30/53/60Hz.
 sequence_completions=0
 for hz in [15,30,53,60]:
  s=State(Fade(1,2,3),3,0,0,1);c=Fade(0,2,0);p=C.c_uint8(0);x=C.c_int32(3);w=C.c_uint8(0)
  for name in [b'SG00001.bmp',b'SG10000.bmp',b'g01_10.bmp']:
   p.value=1;s.saved_expression=4;s.kind=1 if name.startswith(b'g') else 0
   for i in range(hz*8):
    if x.value==-1 and p.value==1:p.value=2
    # Native actor can complete phase5 itself when a new expression is set;
    # keep -1 here to exercise backdrop's own restore branch.
    check(s,p,x,c,w,name,C.c_float(1/hz).value)
    if p.value==0 and i>1:sequence_completions+=1;break
   else:raise AssertionError(('transition did not finish',hz,name,p.value))
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,replacements=replacements,sequence_completions=sequence_completions,observed_phases=sorted(phases),max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-backdrop-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
