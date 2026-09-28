"""Complete4f737b background frame policy against original x86.
Timer/RNG, music fade, ambient event/edge policy, spatial gain, door proximity
and weather timing run natively. Only clock, sound and animation services are
captured; asset loading, rendering and the actual game driver are separate.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_prop_motion_oracle import Timer
from model_binding import ROOT,library
V=C.c_float*3
class State(C.Structure): _fields_=[('ambient_timer',Timer),('music_volume',C.c_int32),('music_mode',C.c_int8),('ambient_latch',C.c_int8*8)]
class Ambient(C.Structure): _fields_=[('present',C.c_int),('playing',C.c_int),('loop',C.c_int),('position',V),('trigger',C.c_float)]
class Input(C.Structure): _fields_=[('now',C.c_uint32),('seconds',C.c_float),('group',C.c_int32),('area',C.c_int32),('background_clip',C.c_int32),('door_clip',C.c_int32),('music_master',C.c_int32),('effect_master',C.c_int32),('player',V),('player_yaw',C.c_float),('npc',V),('background_present',C.c_int),('weather_enabled',C.c_int),('weather_present',C.c_int),('ambient_gate',C.c_int8),('ambient',Ambient*8)]
class Gain(C.Structure): _fields_=[('volume',C.c_int32),('pan',C.c_int32)]
class AmbientCommand(C.Structure): _fields_=[('action',C.c_int),('loop',C.c_int),('spatial',C.c_int),('gain',Gain)]
class Commands(C.Structure): _fields_=[('music_update',C.c_int),('music_volume',C.c_int32),('random_choice',C.c_int32),('ambient',AmbientCommand*8),('door_request',C.c_int32),('door_advance',C.c_int),('background_advance',C.c_int),('weather_advance',C.c_int),('seconds',C.c_float),('weather_seconds',C.c_float)]
class Native(Base):
 bg,door,weather,model,root=0x3003000,0x3004000,0x3005000,0x3006000,0x3007000
 sound,table=0x3008000,0x3009000
 volume,pan,status,stopvoice,clock=0x300b000,0x300b100,0x300b200,0x300b300,0x300e000
 def __init__(self,exe):
  super().__init__(exe)
  for i in range(9):self.word(self.sound+i*16,self.table)
  for off,fn in [(0x3c,self.volume),(0x40,self.pan),(0x24,self.status),(0x48,self.stopvoice)]:self.word(self.table+off,fn)
  self.word(0x53f10c,self.clock)
  for a in [self.volume,self.pan,self.status,self.stopvoice,self.clock,0x46435e,0x401b0a,0x4026fe,0x42403c]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,x):self.u.mem_write(a,struct.pack('<I',x&0xffffffff))
 def hook(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<5I',u.mem_read(sp,20));clean=4;value=0
  if a==self.clock:value=self.inp.now
  elif a==0x401b0a:self.events.append(('request',args[0],args[1]))
  elif a==0x4026fe:self.events.append(('advance',args[0],struct.unpack('<f',u.mem_read(sp+8,4))[0]))
  elif a==0x42403c:
   assert args[0]==self.root
   self.events.append(('place',tuple(struct.unpack('<16f',u.mem_read(args[1],64)))))
  else:
   index=(args[0]-self.sound)//16;assert 0<=index<=8
   if a==0x46435e:self.events.append(('play',index,args[1]))
   elif a==self.stopvoice:clean=8;self.events.append(('stop',index))
   elif a==self.status:clean=12;self.word(args[1],self.inp.ambient[index].playing)
   else:clean=12;self.events.append(('volume' if a==self.volume else 'pan',index,C.c_int32(args[1]).value))
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+clean);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,seed,inp,entry=0x4f737b):
  u=self.u;self.inp=inp;self.events=[];u.mem_write(self.actor,bytes(0x1000));self.word(self.actor,self.bg if inp.background_present else 0);self.word(0x725718,self.bg);self.word(self.bg+0x140,inp.background_clip);self.word(0xbf3bd0,self.door);self.word(self.door+0x140,inp.door_clip);self.word(0xb54730,inp.weather_enabled);self.word(0xb53430,self.weather if inp.weather_present else 0);self.word(self.weather+0x160,self.model);self.word(self.model+0x14,self.root)
  u.mem_write(0x7219a8,struct.pack('<2i',inp.group,inp.area));u.mem_write(0x733700,struct.pack('<f',inp.seconds));u.mem_write(0x71b7ac,struct.pack('<5f',*inp.player,0,inp.player_yaw));u.mem_write(0x729084,bytes(inp.npc));u.mem_write(0xbf3be4,bytes([inp.ambient_gate&255]));self.word(0xbe9a0c,inp.music_master);self.word(0xbe9a10,inp.effect_master);u.mem_write(0xbf3bd8,bytes(s.ambient_timer));self.word(0x58edd8,seed)
  self.word(self.actor+0x618,self.sound+8*16);self.word(self.actor+0x61c,s.music_volume);u.mem_write(self.actor+0x620,bytes([s.music_mode&255]))
  for i,a in enumerate(inp.ambient):
   p=self.actor+0x628+i*0x120;u.mem_write(p+0x100,struct.pack('<4f',*a.position,a.trigger));self.word(p+0x110,self.sound+i*16 if a.present else 0);u.mem_write(p+0x118,bytes([s.ambient_latch[i]&255,a.loop]))
  self.call(entry,struct.pack('<I',self.actor));result=State.from_buffer_copy(s);result.ambient_timer=Timer.from_buffer_copy(u.mem_read(0xbf3bd8,C.sizeof(Timer)));result.music_volume=struct.unpack('<i',u.mem_read(self.actor+0x61c,4))[0]
  for i in range(8):result.ambient_latch[i]=C.c_int8(u.mem_read(self.actor+0x740+i*0x120,1)[0]).value
  return result,struct.unpack('<I',u.mem_read(0x58edd8,4))[0],struct.unpack('<i',u.mem_read(self.stack-8,4))[0]
def events(c,n):
 out=[]
 if c.music_update:out.append(('volume',8,c.music_volume))
 for i,a in enumerate(c.ambient):
  if a.action==1:out.append(('play',i,a.loop))
  elif a.action==2:out.append(('stop',i))
  if a.spatial:out += [('volume',i,a.gain.volume),('pan',i,a.gain.pan)]
 if c.door_request>=0:out.append(('request',n.door,c.door_request))
 if c.door_advance:out.append(('advance',n.door,c.seconds))
 if c.background_advance:out.append(('advance',n.bg,c.seconds))
 if c.weather_advance:
  out += [('place',(1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,10.,1.)),('request',n.weather,0),('advance',n.weather,c.weather_seconds)]
 return out

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();lib.bk_background_step.argtypes=[C.POINTER(State),C.POINTER(C.c_uint32),C.POINTER(Input),C.POINTER(Commands)];rng=random.Random(0x4f737b);total=0;random_steps=0;plays=stops=0
 for case in range(18000):
  s=State(Timer(rng.getrandbits(32),rng.getrandbits(32),rng.randrange(3)),rng.randrange(-10000,1),rng.choice([-1,0,1,2]),(C.c_int8*8)(*(rng.choice([-1,0,1,2]) for _ in range(8))))
  inp=Input();inp.now=rng.getrandbits(32);inp.seconds=rng.choice([0,.0001,1/60,.5,2,10]);inp.group=2 if case%3==0 else rng.randrange(5);inp.area=8 if case%3==0 else rng.randrange(9);inp.background_clip=rng.randrange(6);inp.door_clip=rng.randrange(6);inp.music_master=rng.randrange(-10000,1);inp.effect_master=rng.randrange(-10000,1);inp.player[:]=[rng.uniform(-200,200) for _ in range(3)];inp.npc[:]=[rng.uniform(-200,200) for _ in range(3)];inp.player_yaw=rng.uniform(-720,720);inp.background_present=case%5!=0;inp.weather_enabled=rng.randrange(2);inp.weather_present=rng.randrange(2);inp.ambient_gate=rng.choice([-1,0,1,2]);seed=rng.getrandbits(32)
  if case%6==0:inp.player[:]=[124,0,0]
  if case%6==1:inp.npc[:]=[84,99,0]
  for a in inp.ambient:
   a.present=rng.randrange(4)!=0;a.playing=rng.randrange(2);a.loop=rng.randrange(2);a.position[:]=[rng.uniform(-200,200) for _ in range(3)];a.trigger=rng.choice([-1,0,1,2,37,99,99.9,100,100.5,101,102,103,150])
  wanted,random_out,choice=n.run(s,seed,inp);result=State.from_buffer_copy(s);random_state=C.c_uint32(seed);cmd=Commands();assert lib.bk_background_step(C.byref(result),C.byref(random_state),C.byref(inp),C.byref(cmd));assert bytes(result)==bytes(wanted),(case,bytes(result).hex(),bytes(wanted).hex());assert random_state.value==random_out and cmd.random_choice==choice;assert events(cmd,n)==n.events,(case,events(cmd,n),n.events)
  random_steps+=seed!=random_out;total+=len(n.events);plays+=sum(e[0]=='play' for e in n.events);stops+=sum(e[0]=='stop' for e in n.events)
 # Invalid plans leave state/RNG/output untouched.
 old=bytes(result);old_rng=random_state.value;old_cmd=bytes(cmd);inp.ambient[0].present=1;inp.ambient[0].trigger=float('nan');assert not lib.bk_background_step(C.byref(result),C.byref(random_state),C.byref(inp),C.byref(cmd));assert bytes(result)==old and random_state.value==old_rng and bytes(cmd)==old_cmd
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=18000,commands=total,random_steps=random_steps,plays=plays,stops=stops,max_relative_error=0,scope=__doc__)
 (ROOT/'local/original-background-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
