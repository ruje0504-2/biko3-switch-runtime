"""Full native4ec9f0 loader/4ef168 positions, retained yaw/visibility and
original root rotation/setter vs item config/initialization. File/path context,
model allocation and sound loading are explicit service hooks; no gameplay.
"""
import sys,struct,json
from pathlib import Path
import ctypes as C, hashlib, random
from model_binding import ROOT, library
from original_prop_route_oracle import Native as Base
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x100000)
  for a in [0x4ad8ec,0x4ad97e,0x401074,0x42892a,0x428a70,0x428bb6,0x4a7109,0x428caf,0x428b69,0x428a23,0x50d858]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def floats(self,a,n):return struct.unpack('<'+'f'*n,self.u.mem_read(a,4*n))
 def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0] if a else b''
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);args=struct.unpack('<5I',u.mem_read(sp+4,20));value=0
  if a==0x4ad8ec:
   dest,prefix,suffix=args[:3];u.mem_write(dest,self.string(prefix)+self.string(suffix)+b'\0');value=dest
  elif a==0x4ad97e:
   directory,filename,src=args[:3];folder,_,name=self.string(src).rpartition(b'\\');u.mem_write(directory,folder+b'\\\0');u.mem_write(filename,name+b'\0')
  elif a==0x401074:
   slot=len(self.loads);value=0x4000000+slot*0x8000;model=value+0x6000;root=value+0x6400
   self.word(value+0x160,model);self.word(model+0x14,root)
   for off in [0x80,0xc0,0x100]:u.mem_write(root+off,struct.pack('<16f',*I))
   self.loads.append(self.string(args[0]).decode())
  elif a==0x50d858:self.sounds.append([self.string(args[1]).decode(),self.string(args[2]).decode()])
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def profile(self,g,a,states=None,collected=None):
  self.u.mem_write(0x726650,bytes(0x268*16));self.u.mem_write(0x71bcdc,bytes(5));self.u.mem_write(0x5767c8,b'\1');self.u.mem_write(0xbef778,b'\0')
  for i in range(16):self.u.mem_write(0x7268a0+i*0x268,struct.pack('<3f',1001,1002,1003))
  if states is not None:
   for i,s in enumerate(states):
    p=0x726650+i*0x268;self.word(p+4,s.id);self.u.mem_write(p+0x250,bytes(s.position));self.u.mem_write(p+0x260,struct.pack('<f',s.yaw));self.u.mem_write(p+0x20c,bytes([s.hidden]))
  if collected is not None:self.u.mem_write(0x71bcdc,bytes(collected))
  self.loads=[];self.sounds=[];self.call(0x4ec9f0,struct.pack('<2I',g,a));n=self.read(0xbef78c);assert n==len(self.loads)
  items=[]
  for i in range(n):
   p=0x726650+i*0x268
   items.append(dict(id=self.read(p+4),clip=self.loads[i],position=self.floats(p+0x250,3),yaw=self.floats(p+0x260,1)[0],hidden=bytes(self.u.mem_read(p+0x20c,1))[0],world=self.floats(self.read(self.read(self.read(p)+0x160)+0x14)+0xc0,16)))
  return dict(group=g,area=a,items=items,sounds=self.sounds)

class Config(C.Structure):
 _fields_=[('id',C.c_uint32),('clip',C.c_char_p),('position',C.c_float*3)]
class State(C.Structure):
 _fields_=[('id',C.c_uint32),('position',C.c_float*3),('yaw',C.c_float),('hidden',C.c_uint8)]
class Placement(C.Structure):
 _fields_=[('position',C.c_float*3),('yaw',C.c_float),('world',C.c_float*16)]
def main():
 import argparse
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();vm=Native(exe);lib=library();rng=random.Random(0x4ec9f0)
 lib.bk_item_config.argtypes=[C.POINTER(C.POINTER(Config)),C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32]
 lib.bk_item_initialize.argtypes=[C.POINTER(State),C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32,C.POINTER(C.c_uint8)]
 lib.bk_actor_placement.argtypes=[C.POINTER(Placement),C.POINTER(C.c_float),C.c_float]
 cases=items=0;worst=0
 for g in range(5):
  for a in range(9):
   config=C.POINTER(Config)();n=C.c_uint32()
   assert lib.bk_item_config(C.byref(config),C.byref(n),g,a)
   for case in range(64):
    states=(State*16)(*[State(rng.randrange(100),(C.c_float*3)(*[rng.uniform(-1000,1000) for _ in range(3)]),rng.uniform(-720,720),rng.randrange(256)) for _ in range(16)])
    flags=(C.c_uint8*5)(*[rng.choice([0,1,2,127,128,255]) for _ in range(5)])
    wanted=vm.profile(g,a,states,flags);assert n.value==len(wanted['items'])
    before=bytes(states);count=C.c_uint32(0xdead)
    assert lib.bk_item_initialize(states,C.byref(count),g,a,flags)
    assert count.value==n.value
    assert bytes(states)[n.value*C.sizeof(State):]==before[n.value*C.sizeof(State):]
    for i,it in enumerate(wanted['items']):
     c=config[i];s=states[i]
     assert c.id==it['id']==s.id
     assert c.clip.decode()==it['clip'].split('\\')[-1]
     assert list(c.position)==list(s.position)==list(it['position'])
     assert s.yaw==it['yaw'] and s.hidden==it['hidden']
     p=Placement();assert lib.bk_actor_placement(C.byref(p),s.position,s.yaw)
     for x,y in zip(p.world,it['world']):
      d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert d<3e-6,(g,a,case,i,x,y,d)
     items+=1
    assert bytes(vm.u.mem_read(0x71bcdc,5))==bytes(flags)
    assert wanted['sounds']==[['bk3_02.pp','se101.wav']]
    cases+=1
   print(g,a,'PASS',n.value,flush=True)
 snapshot=bytes(states);count=C.c_uint32(123)
 assert not lib.bk_item_initialize(states,C.byref(count),5,0,flags)
 assert bytes(states)==snapshot and count.value==123
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=cases,items=items,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-item-loader-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
