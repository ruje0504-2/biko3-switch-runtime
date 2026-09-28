"""Original4a4159/4a4438/4a4701: light registration, ambient priority and
ordered light/object/flush/shadow commands. Only native string and graphics
service boundaries are supplied. Actual drawing remains separately tested.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Light(C.Structure):_fields_=[('diffuse',C.c_float*3),('group',C.c_int32),('ambient_rank',C.c_int32)]
class Input(C.Structure):_fields_=[('lights',Light*16),('light_count',C.c_uint32),('objects',C.c_uint32*52),('scene_root',C.c_uint32),('mode',C.c_int32),('shadow_mode',C.c_int32)]
class Command(C.Structure):_fields_=[('kind',C.c_uint32),('target',C.c_uint32),('value',C.c_uint32)]
class Pass(C.Structure):_fields_=[('count',C.c_uint32),('commands',Command*128)]
class Native:
 device=0x3000100;table=0x3000200;light=0x3001000;frame=0x3003000;parent=0x3003500;key=0x3003800
 renderstate=0x300d000;copy=0x300d100;compare=0x300d200;stack=0x2008000;stop=0x300f000
 def __init__(self,exe):
  self.u=machine(exe);self.word(0x6455a0,self.device);self.word(self.device,self.table);self.word(self.table+0x50,self.renderstate);self.word(0x53f214,self.copy);self.word(0x53f0f8,self.compare)
  for a in [self.renderstate,self.copy,self.compare,0x425f40,0x42261a,0x42aa47,0x4b07b9]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,p,x):self.u.mem_write(p,struct.pack('<I',x&0xffffffff))
 def string(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0]
 def hook(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<4I',u.mem_read(sp,16));clean=4;value=1
  if a==self.copy:
   u.mem_write(args[0],self.string(args[1])+b'\0');clean=12;value=args[0]
  elif a==self.compare:
   value=int(self.string(args[0]).lower()!=self.string(args[1]).lower());clean=12
  elif a==self.renderstate:
   assert args[0]==self.device and args[1]==139
   self.commands.append((0,0,args[2]));clean=16
  elif a==0x425f40:
   index=(args[0]-self.light)//0x100;assert 0<=index<16;self.commands.append((1,index,args[1]))
  elif a==0x42261a:self.commands.append((2,args[0],0))
  elif a==0x42aa47:self.commands.append((3,0,args[0]))
  else:self.commands.append((4,0,0))
  u.reg_write(UC_X86_REG_ESP,sp+clean);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,value)
 def call(self,fn,args):
  self.u.mem_write(self.stack,struct.pack('<I',self.stop)+args);self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f);self.u.emu_start(fn,self.stop,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop;return self.u.reg_read(UC_X86_REG_EAX)
 def bind(self,inp,names=None):
  self.commands=[];self.word(0x705738,inp.light_count);self.word(0x721ad8,inp.shadow_mode);self.word(0x645600,inp.scene_root)
  for i,l in enumerate(inp.lights):
   self.word(0x7056b8+i*4,self.light+i*0x100);self.word(0x7056f8+i*4,l.group);self.word(0x70573c+i*4,l.ambient_rank)
   self.u.mem_write(self.light+i*0x100+0x7c,bytes(l.diffuse)+bytes(4));self.u.mem_write(self.light+i*0x100+8,b'Light_'+(b'A' if names is not None and names[i] else b'P')+b'\0')
 def run(self,inp):
  self.bind(inp);self.call(0x4a4701,bytes(inp.objects)+struct.pack('<iI',inp.mode,0));return self.commands

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();n=Native(exe);rng=random.Random(0x4a4701)
 lib.bk_lighting_pass.argtypes=[C.POINTER(Input),C.POINTER(Pass)]
 lib.bk_light_group.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_int32)]
 lib.bk_light_ambient_initialize.argtypes=[C.POINTER(Light),C.POINTER(C.c_uint8),C.c_uint32,C.POINTER(C.c_uint32)]
 total=draws=shadows=0
 for case in range(16000):
  inp=Input();inp.light_count=case%17;inp.mode=rng.choice([-1,0,1,2,3,9,10,11,0x7fffffff]);inp.shadow_mode=rng.randrange(4);inp.scene_root=rng.getrandbits(32)
  for l in inp.lights:l.diffuse[:]=[rng.random() for _ in range(3)];l.group=rng.choice([1,2]);l.ambient_rank=rng.randrange(3)
  inp.objects[:]=[rng.getrandbits(32) if rng.randrange(3) else 0 for _ in range(52)]
  expected=n.run(inp);out=Pass();assert lib.bk_lighting_pass(C.byref(inp),C.byref(out));actual=[(c.kind,c.target,c.value) for c in out.commands[:out.count]];assert actual==expected,(case,inp.mode,actual,expected)
  total+=len(actual);draws+=sum(c[0]==2 for c in actual);shadows+=sum(c[0]==4 for c in actual)
 # Ambient initialization preserves non-ambient slots and prioritizes group1
 # for the initial color, but marks all ambient names rank1 for later passes.
 for case in range(4000):
  inp.light_count=case%17;names=(C.c_uint8*16)(*[rng.randrange(2) for _ in range(16)])
  for l in inp.lights:l.diffuse[:]=[rng.random() for _ in range(3)];l.group=rng.choice([1,2]);l.ambient_rank=rng.randrange(3)
  n.bind(inp,names);n.call(0x4a4438,b'');argb=C.c_uint32();assert lib.bk_light_ambient_initialize(inp.lights,names,inp.light_count,C.byref(argb));assert argb.value==n.commands[-1][2]
  assert [l.ambient_rank for l in inp.lights[:inp.light_count]]==list(struct.unpack('<'+'i'*inp.light_count,n.u.mem_read(0x70573c,inp.light_count*4)))
 for case in range(4000):
  parent=rng.choice([b'LightGroup_BK3_L',b'BK3_L',b'bk3_l',b'BBK3_L',b'',b'abc',b'LightGroup_Bk3_l',b'BK3_P',b'z'*64]);key=rng.choice([b'BK3_L',b'bk3_l',b''])
  n.u.mem_write(n.parent,bytes(0x400));n.u.mem_write(n.key,key+b'\0');n.u.mem_write(n.parent+8,parent+b'\0');n.word(n.frame+0x22c,n.parent);n.word(n.light+0xe0,n.frame)
  wanted=n.call(0x4a4159,struct.pack('<II',n.light,n.key));actual=C.c_int32();assert lib.bk_light_group(parent,key,C.byref(actual));assert actual.value==wanted,(parent,key,actual.value,wanted)
 # Failed plans cannot leak partial commands.
 before=bytes(out);inp.light_count=17;assert not lib.bk_lighting_pass(C.byref(inp),C.byref(out));assert bytes(out)==before
 inp.light_count=1;inp.lights[0].diffuse[1]=float('nan');assert not lib.bk_lighting_pass(C.byref(inp),C.byref(out));assert bytes(out)==before
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),passes=16000,commands=total,objects=draws,projected_shadow_calls=shadows,ambient_initializations=4000,name_classifications=4000,max_error=0,scope=__doc__)
 (ROOT/'local/original-lighting-pass-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
