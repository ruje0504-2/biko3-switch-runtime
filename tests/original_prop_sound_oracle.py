"""512853/512c0e prop sound policy vs original instructions.
Only release/load/play and DirectSound setters/status are service boundaries.
Car state machine, nearest train source, spatial gain and frequency arithmetic
run original x86; packed and loose file paths both covered. No device claim.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_prop_motion_oracle import State
from model_binding import ROOT, library
V=C.c_float*3
class Sound(C.Structure): _fields_=[('stage',C.c_int8)]
class Gain(C.Structure): _fields_=[('volume',C.c_int32),('pan',C.c_int32)]
class Input(C.Structure): _fields_=[('listener',V),('listener_yaw',C.c_float),('effect_volume',C.c_int32),('voice_present',C.c_int),('voice_playing',C.c_int)]
class Command(C.Structure): _fields_=[('file',C.c_char_p),('initial_volume',C.c_int32),('release',C.c_int),('loop',C.c_int),('play',C.c_int),('spatial',C.c_int),('frequency',C.c_int),('gain',Gain),('source',V)]
class Native(Base):
 sound,table=0x3008000,0x3009000
 volume,pan,freq,status=0x300b000,0x300b100,0x300b200,0x300b300
 def __init__(self,exe):
  super().__init__(exe)
  self.word(self.sound,self.table)
  for off,fn in [(0x3c,self.volume),(0x40,self.pan),(0x44,self.freq),(0x24,self.status)]:self.word(self.table+off,fn)
  for a in [0x50dac1,0x50d858,0x46435e,self.volume,self.pan,self.freq,self.status]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,x):self.u.mem_write(a,struct.pack('<I',x&0xffffffff))
 def string(self,a):return bytes(self.u.mem_read(a,260)).split(b'\0')[0] if a else None
 def hook(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<6I',u.mem_read(sp,24));clean=4
  if a==0x50dac1:
   assert args[0]==self.actor+0x338;self.events.append(('release',));self.word(self.actor+0x448,0)
  elif a==0x50d858:
   assert args[0]==self.actor+0x338
   file=self.string(args[2]);file=file.split(b'\\')[-1];self.events.append(('load',file,C.c_int32(args[3]).value,args[4]))
   self.word(self.actor+0x448,self.sound);self.word(self.actor+0x454,22050);u.mem_write(self.actor+0x451,bytes([args[4]==1]))
  elif a==0x46435e:
   assert args[:2]==[self.sound,0];self.events.append(('play',))
  else:
   clean=12;assert args[0]==self.sound
   if a==self.status:self.word(args[1],self.playing)
   else:self.events.append(({self.volume:'volume',self.pan:'pan',self.freq:'frequency'}[a],C.c_int32(args[1]).value))
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+clean);u.reg_write(UC_X86_REG_EIP,ret)
 def prepare(self,p,s,inp,packed):
  u=self.u;u.mem_write(self.actor,bytes(0x998));self.word(self.actor+0xc,p.kind);self.word(self.actor+0x10,p.action);u.mem_write(self.actor+0x14,bytes(p.actions));u.mem_write(self.actor+0x29c,bytes(p.path.position));u.mem_write(self.actor+0x2ac,struct.pack('<f',p.path.yaw));u.mem_write(self.actor+0x450,bytes([s.stage&255]));self.word(self.actor+0x448,self.sound if inp.voice_present else 0);self.word(self.actor+0x454,22050);u.mem_write(0x71b7ac,struct.pack('<5f',*inp.listener,0,inp.listener_yaw));self.word(0xbe9a10,inp.effect_volume);u.mem_write(0x5767c8,bytes([packed]));self.events=[];self.playing=inp.voice_playing
 def initial(self):self.call(0x512853,struct.pack('<I',self.actor))
 def step(self):
  # Native spatial setter unconditionally dereferences its buffer, so the
  # absent-buffer fixture is only used for the isolated car status routine.
  self.call(0x512c0e,struct.pack('<I',self.actor))
  return C.c_int8(self.u.mem_read(self.actor+0x450,1)[0]).value
 def train(self,p,inp):
  dest=0x300a000;self.call(0x514f0e,struct.pack('<I4fI',self.actor,*inp.listener,inp.listener_yaw,dest));return struct.unpack('<3f',self.u.mem_read(dest,12))
def commands(c):
 out=[]
 if c.release:out.append(('release',))
 if c.file:out.append(('load',c.file,c.initial_volume,c.loop))
 if c.play:out.append(('play',))
 if c.spatial:out += [('volume',c.gain.volume),('pan',c.gain.pan)]
 if c.frequency:out.append(('frequency',22050+2*c.gain.volume))
 return out

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x512c0e)
 lib.bk_prop_sound_initial.argtypes=[C.c_int32,C.c_int32,C.POINTER(Command)]
 lib.bk_prop_sound_step.argtypes=[C.POINTER(Sound),C.POINTER(State),C.POINTER(Input),C.POINTER(Command)]
 initials=trains=changes=0;worst=0
 for case in range(20000):
  p=State();p.kind=case%22-1;p.action=rng.randrange(4);p.actions[:]=[rng.randrange(4) for _ in range(4)];p.path.position[:]=[rng.uniform(-1000,1000) for _ in range(3)];p.path.yaw=rng.choice([0,90,-90,180,360,rng.uniform(-720,720)])
  inp=Input(V(*(rng.uniform(-1000,1000) for _ in range(3))),rng.uniform(-720,720),rng.choice([-10000,-6000,-1000,-1,0]),1,rng.randrange(2));s=Sound(rng.choice([-128,-1,0,1,2,3,4,5,127]));c=Command();packed=case%2
  n.prepare(p,s,inp,packed);n.initial();assert lib.bk_prop_sound_initial(p.kind,inp.effect_volume,C.byref(c));assert commands(c)==n.events,(case,commands(c),n.events);initials+=1
  n.prepare(p,s,inp,packed);wanted=n.step();assert lib.bk_prop_sound_step(C.byref(s),C.byref(p),C.byref(inp),C.byref(c));assert s.stage==wanted,(case,s.stage,wanted);assert commands(c)==n.events,(case,p.kind,commands(c),n.events);changes+=bool(c.file)
  if p.kind==1:
   expected=n.train(p,inp)
   for x,y in zip(c.source,expected):
    delta=abs(x-y)/max(1,abs(y));worst=max(worst,delta);assert delta<3e-6,(case,tuple(c.source),expected)
   trains+=1
 # Absent/status-failed buffer selects stage3, unless idle overrides with stage1.
 for present in [0,1]:
  for playing in [0,1]:
   for action in [0,1]:
    p=State();p.kind=0;p.action=action;p.actions[0]=0;p.actions[3]=1;s=Sound(2);inp=Input(V(0,0,0),0,0,present,playing);n.prepare(p,s,inp,1);n.call(0x514b9a,struct.pack('<I',n.actor));assert lib.bk_prop_sound_step(C.byref(s),C.byref(p),C.byref(inp),C.byref(c));assert s.stage==C.c_int8(n.u.mem_read(n.actor+0x450,1)[0]).value
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),initializations=initials,frames=20000,car_replacements=changes,train_sources=trains,missing_buffer_cases=8,max_relative_error=worst,hooks=['50dac1','50d858','46435e','DirectSound setters/status'],scope=__doc__)
 (ROOT/'local/original-prop-sound-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
