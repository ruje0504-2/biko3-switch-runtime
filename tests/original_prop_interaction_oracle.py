"""Full4f4306 vs portable shared state. Only DirectSound Play/Stop and the
millisecond clock are substituted. Radius/heading/trig/strcmp, matrix copies,
timer and ascending actor loop execute original instructions.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Base
from original_prop_motion_oracle import State as Prop, Timer
from original_prop_sound_oracle import Sound
from original_player_control_oracle import State as Player
from model_binding import ROOT, library
M=C.c_float*16
class Contact(C.Structure): _fields_=[('alarm',Timer),('car_hit',C.c_uint8)]
class Shared(C.Structure): _fields_=[('selected',C.c_int32),('available',C.c_uint8),('matrix',M)]
class Actor(C.Structure): _fields_=[('motion',C.POINTER(Prop)),('sound',C.POINTER(Sound)),('interaction',C.POINTER(Contact)),('world',C.POINTER(C.c_float))]
class Input(C.Structure): _fields_=[('actions',C.c_int32*21),('wall',C.c_char_p),('now',C.c_uint32),('blocked',C.c_uint8)]
class Command(C.Structure): _fields_=[('index',C.c_uint),('play',C.c_int),('loop',C.c_int)]
class Commands(C.Structure): _fields_=[('count',C.c_uint),('sounds',Command*16)]
class Native(Base):
 props=0x729d80; player=0x71b510; play=0x300e100; stop_sound=0x300e200; clock=0x300e300; table=0x3007000
 def __init__(self,exe):
  super().__init__(exe)
  self.word(0x53f10c,self.clock);self.word(self.table+0x30,self.play);self.word(self.table+0x48,self.stop_sound)
  for a in [self.play,self.stop_sound,self.clock]:self.u.hook_add(UC_HOOK_CODE,self.service,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def service(self,u,a,size,data):
  sp=u.reg_read(UC_X86_REG_ESP);ret,handle=struct.unpack('<2I',u.mem_read(sp,8));result=0;clean=4
  if a==self.clock:self.clock_calls+=1;result=self.now
  else:
   i=(handle-0x3005000)//0x100;assert 0<=i<16 and handle==0x3005000+i*0x100
   loop=0
   if a==self.play:
    x,y,loop=struct.unpack('<3I',u.mem_read(sp+8,12));assert x==y==0 and loop in [0,1];clean=20
   else:clean=8
   self.events.append((i,int(a==self.play),loop))
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+clean);u.reg_write(UC_X86_REG_EIP,ret)
 def prepare(self,p,props,sounds,contacts,worlds,present,sh,inp,noise,outcome):
  u=self.u;self.now=inp.now;self.clock_calls=0;self.events=[]
  u.mem_write(self.props,bytes(0x998*16));u.mem_write(self.player,bytes(0x998))
  self.word(0x71b520,p.spatial.movement.action);u.mem_write(0x71b524,bytes(inp.actions));u.mem_write(0x71b7ac,bytes(p.spatial.movement.position));u.mem_write(0x71bce8,bytes(p.interaction.trigger.origin)+bytes(p.interaction.trigger.target));u.mem_write(0x71bd08,bytes([p.interaction.script_phase&255]));u.mem_write(0x71bcdb,bytes([p.completion_mode&255]));u.mem_write(0x71bbd4,inp.wall+b'\0')
  self.word(0x728de4,sh.selected);u.mem_write(0x71ba8b,bytes([sh.available]));u.mem_write(0x71ba90,bytes(sh.matrix));u.mem_write(0x729101,bytes([noise&255]));u.mem_write(0x71bcd8,bytes([outcome]));u.mem_write(0xbeeb7f,bytes([inp.blocked]))
  for i in range(16):
   a=self.props+i*0x998;b=0x3000000+i*0x400;h=0x3005000+i*0x100;s=props[i]
   self.word(a,b if present[i] else 0);self.word(b+0x160,b+0x200);self.word(b+0x214,b+0x240);u.mem_write(b+0x300,bytes(worlds[i]));self.word(a+0x448,h);self.word(h,self.table)
   self.word(a+0xc,s.kind);self.word(a+0x10,s.action);u.mem_write(a+0x29c,bytes(s.path.position));u.mem_write(a+0x2ac,struct.pack('<f',s.path.yaw));u.mem_write(a+0x2b4,bytes(s.path.velocity));u.mem_write(a+0x328,bytes([s.hidden]));u.mem_write(a+0x450,bytes(sounds[i]));u.mem_write(a+0x337,bytes([contacts[i].car_hit]));u.mem_write(a+0x854,bytes(contacts[i].alarm))
 def read(self):
  u=self.u
  return dict(action=struct.unpack('<i',u.mem_read(0x71b520,4))[0],trigger=struct.unpack('<8f',u.mem_read(0x71bce8,32)),phase=C.c_int8(u.mem_read(0x71bd08,1)[0]).value,mode=C.c_int8(u.mem_read(0x71bcdb,1)[0]).value,available=u.mem_read(0x71ba8b,1)[0],selected=struct.unpack('<i',u.mem_read(0x728de4,4))[0],matrix=bytes(u.mem_read(0x71ba90,64)),noise=C.c_int8(u.mem_read(0x729101,1)[0]).value,outcome=u.mem_read(0x71bcd8,1)[0])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256)
 lib.bk_prop_interaction_step.argtypes=[C.POINTER(Actor),C.POINTER(Player),C.POINTER(C.c_int8),C.POINTER(C.c_uint8),C.POINTER(Shared),C.POINTER(Input),C.POINTER(Commands),C.c_void_p]
 lib.bk_prop_interaction_wall.argtypes=[C.c_int32];lib.bk_prop_interaction_wall.restype=C.c_char_p
 names=[bytes(n.u.mem_read(0x578a30+i*256,256)).split(b'\0')[0] for i in range(20)]
 assert all(lib.bk_prop_interaction_wall(i)==names[i] for i in range(20))
 rng=random.Random(0x4f4306);totals=dict(frames=0,present_props=0,play_loop=0,play_once=0,pauses=0,clocks=0,car_hits=0,alarms=0,prompts=0,multiple_commands=0);worst=0
 for case in range(6000):
  props=(Prop*16)();sounds=(Sound*16)();contacts=(Contact*16)();worlds=(M*16)();actors=(Actor*16)();p=Player();sh=Shared();inp=Input();out=Commands()
  C.memset(C.byref(p),0x31,C.sizeof(p))
  inp.actions[:]=[i+100 if case%3 else rng.randrange(10) for i in range(21)]
  p.spatial.movement.action=rng.choice([*inp.actions[:14],999]);p.spatial.movement.position[:]=[rng.choice([0.,-123.75,890.25]) for _ in range(3)];p.interaction.trigger.origin[:]=[13,27,-34,79];p.interaction.trigger.target[:]=[-100,-23,53,137];p.interaction.script_phase=rng.choice([-1,0,1,2]);p.completion_mode=rng.randrange(4)
  inp.now=rng.choice([0,500,1000,2000,0x7fffffff,0x80000000,0xffffffff]);inp.blocked=rng.choice([0,0,0,1,255]);first_kind=case%20;inp.wall=names[first_kind] if case%4 else b'unmatched'
  sh.selected=rng.randrange(-4,16);sh.available=rng.randrange(256);sh.matrix[:]=[rng.uniform(-10,10) for _ in range(16)];noise=C.c_int8(rng.choice([-1,0,1,2,3]));outcome=C.c_uint8(rng.choice([0,0,1,2,5,255]));present=[]
  for i in range(16):
   present.append(i==0 or (case>=3000 and rng.randrange(3)!=0));s=props[i];s.kind=first_kind if i==0 else rng.randrange(-2,22);s.action=rng.choice([0,0,0,1,2,8,10,99]);s.hidden=rng.choice([0,1,255]);s.actions[:]=[rng.randrange(20) for _ in range(4)]
   distance=rng.choice([0.,.1,19.999,20,20.001,39.999,40,40.001,49.999,50,50.001,99.999,100,100.001]);angle=rng.choice([0,math.pi/2,math.pi,3*math.pi/2]);s.path.position[:]=[p.spatial.movement.position[0]+distance*math.sin(angle),p.spatial.movement.position[1]+rng.choice([-5.001,-5,0,5,5.001]),p.spatial.movement.position[2]+distance*math.cos(angle)];s.path.yaw=rng.choice([0,90,180,270,360,-180,720,rng.uniform(-720,720)]);s.path.velocity[:]=[rng.choice([0,0,-1,2]),rng.random(),rng.choice([0,0,-1,2])]
   sounds[i].stage=rng.choice([-1,0,0,1,2,4,5]);contacts[i].car_hit=rng.choice([0,0,0,1,2]);contacts[i].alarm=Timer(rng.choice([0,100,500,2000]),rng.choice([0,1000,2000,0x7fffffff,0x80000000,0xffffffff]),rng.choice([0,1,2,255]));worlds[i][:]=[rng.uniform(-20,20) for _ in range(16)]
   actors[i]=Actor(C.pointer(s) if present[-1] else None,C.pointer(sounds[i]),C.pointer(contacts[i]),worlds[i])
  oldprops=bytes(props);oldplayer=Player.from_buffer_copy(p);oldcontacts=bytes(contacts);oldoutcome=outcome.value
  n.prepare(p,props,sounds,contacts,worlds,present,sh,inp,noise.value,outcome.value);n.call(0x4f4306,b'');w=n.read()
  assert lib.bk_prop_interaction_step(actors,C.byref(p),C.byref(noise),C.byref(outcome),C.byref(sh),C.byref(inp),C.byref(out),error),(case,error.value)
  assert (p.spatial.movement.action,p.interaction.script_phase,p.completion_mode,noise.value,outcome.value,sh.selected,sh.available)==(w['action'],w['phase'],w['mode'],w['noise'],w['outcome'],w['selected'],w['available']),(case,w)
  assert bytes(sh.matrix)==w['matrix'],(case,'matrix')
  for a,b in zip([*p.interaction.trigger.origin,*p.interaction.trigger.target],w['trigger']):
   delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta);assert delta<3e-6,(case,a,b,delta)
  # Check every unused portable player field remains untouched.
  oldplayer.spatial.movement.action=p.spatial.movement.action;oldplayer.interaction.trigger.origin[:]=p.interaction.trigger.origin;oldplayer.interaction.trigger.target[:]=p.interaction.trigger.target;oldplayer.interaction.script_phase=p.interaction.script_phase;oldplayer.completion_mode=p.completion_mode
  assert bytes(oldplayer)==bytes(p)
  expected_props=(Prop*16).from_buffer_copy(oldprops)
  for i in range(16):
   a=n.props+i*0x998;expected_props[i].action=struct.unpack('<i',n.u.mem_read(a+0x10,4))[0]
   assert sounds[i].stage==C.c_int8(n.u.mem_read(a+0x450,1)[0]).value
   assert contacts[i].car_hit==n.u.mem_read(a+0x337,1)[0]
   assert bytes(contacts[i].alarm)==bytes(n.u.mem_read(a+0x854,12))
   totals['car_hits']+=contacts[i].car_hit==1 and Contact.from_buffer_copy(oldcontacts[i*C.sizeof(Contact):]).car_hit==0
  assert bytes(props)==bytes(expected_props)
  commands=[(c.index,c.play,c.loop) for c in out.sounds[:out.count]];assert commands==n.events,(case,commands,n.events)
  totals['frames']+=1;totals['present_props']+=sum(present);totals['clocks']+=n.clock_calls;totals['alarms']+=outcome.value==5 and oldoutcome!=5;totals['prompts']+=bool(sh.available);totals['multiple_commands']+=out.count>1
  for _,play,loop in commands:totals['play_loop' if play and loop else 'play_once' if play else 'pauses']+=1
 assert all(totals[k]>0 for k in totals)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),**totals,max_relative_error=worst,hooks=['DirectSound Play/Stop','GetTickCount'],scope=__doc__)
 (ROOT/'local/original-prop-interaction-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
