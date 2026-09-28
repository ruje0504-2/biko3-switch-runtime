"""Prop motion/point/material policy vs original512595/4f56a3.
Only recursive material writes are intercepted as commands; visibility bytes,
shared alpha/distance, wait state and player/NPC-dependent branches run x86.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base,Motion
from model_binding import ROOT,library
class Timer(C.Structure):_fields_=[('duration',C.c_uint32),('deadline',C.c_uint32),('armed',C.c_uint8)]
class State(C.Structure):_fields_=[('kind',C.c_int32),('action',C.c_int32),('actions',C.c_int32*4),('path',Motion),('target_yaw',C.c_float),('wait',Timer),('background_wait',C.c_int8),('route_flag',C.c_int8),('hidden',C.c_uint8)]
class Input(C.Structure):_fields_=[('seconds',C.c_float),('player_action',C.c_int32),('player_actions',C.c_int32*21),('background_clip',C.c_int32),('npc_last_crossed',C.c_int32),('npc_previous_flags',C.c_int8*2),('player_mode',C.c_int8)]
class Shared(C.Structure):_fields_=[('distance',C.c_float),('alpha',C.c_float),('last_crossed',C.c_int32)]
class Effects(C.Structure):_fields_=[('allowed',C.c_int),('material',C.c_int),('alpha',C.c_float)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x4b0de5,0x4b124d]:self.u.hook_add(UC_HOOK_CODE,self.boundary,begin=a,end=a)
 def boundary(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret,frame,alpha,policy=struct.unpack('<II f I',u.mem_read(sp,16));assert frame==0x3007000
  self.command=(-1 if a==0x4b0de5 else policy,alpha);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def prepare(self,s,sh,inp):
  u=self.u;u.mem_write(self.actor,bytes(0x998));self.word(self.actor,0x3005000);self.word(0x3005160,0x3006000);self.word(0x3006014,0x3007000)
  for off,v in [(0xc,s.kind),(0x10,s.action),(0x840,s.wait.duration),(0x844,s.wait.deadline)]:self.word(self.actor+off,v)
  u.mem_write(self.actor+0x14,bytes(s.actions))
  for off,v in [(0x328,s.hidden),(0x330,s.background_wait),(0x848,s.wait.armed),(0x84c,s.route_flag)]:u.mem_write(self.actor+off,bytes([v&255]))
  self.word(0x725718,0x3004000);self.word(0x3004140,inp.background_clip)
  self.word(0x71b520,inp.player_action);u.mem_write(0x71b524,bytes(inp.player_actions));u.mem_write(0x71ba89,bytes([inp.player_mode&255]));u.mem_write(0x733700,struct.pack('<f',inp.seconds));u.mem_write(0xbf4b58,struct.pack('<f',sh.alpha));u.mem_write(0x3002000,struct.pack('<f',sh.distance));self.word(0xbf3c74,inp.npc_last_crossed)
  if inp.npc_last_crossed>2:
   for i in range(2):u.mem_write(0xbe9a28+(inp.npc_last_crossed-1-i)*20,bytes([inp.npc_previous_flags[i]&255]))
  self.command=(-2,0)
 def read(self):
  u=self.u;return dict(action=struct.unpack('<i',u.mem_read(self.actor+0x10,4))[0],hidden=u.mem_read(self.actor+0x328,1)[0],background_wait=C.c_int8(u.mem_read(self.actor+0x330,1)[0]).value,route_flag=C.c_int8(u.mem_read(self.actor+0x84c,1)[0]).value,wait=(struct.unpack('<2I',u.mem_read(self.actor+0x840,8))+(u.mem_read(self.actor+0x848,1)[0],)))
 def motion(self,s,sh,inp):
  self.prepare(s,sh,inp);self.call(0x512595,struct.pack('<II',self.actor,0x3002000));out=self.read();out.update(allowed=self.u.reg_read(UC_X86_REG_EAX)&255,distance=struct.unpack('<f',self.u.mem_read(0x3002000,4))[0],alpha=struct.unpack('<f',self.u.mem_read(0xbf4b58,4))[0],command=self.command);return out
 def point(self,s,sh,inp,flag):
  self.prepare(s,sh,inp);self.call(0x4f56a3,struct.pack('<I4fB3x',self.actor,1,2,3,4,flag&255));return self.read()
def bind(lib):
 lib.bk_prop_motion_select.argtypes=[C.POINTER(State),C.POINTER(Shared),C.POINTER(Input),C.POINTER(Effects)]
 lib.bk_prop_point_apply.argtypes=[C.POINTER(State),C.c_int8,C.POINTER(Input)]
 lib.bk_prop_smooth_heading.argtypes=[C.c_int32];lib.bk_prop_needs_ground.argtypes=[C.c_int32,C.c_int32]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x512595);commands={-2:0,-1:0,0:0,1:0}
 def check(s,w):
  for k in ['action','hidden','background_wait','route_flag']:assert getattr(s,k)==w[k],(k,getattr(s,k),w[k])
  assert (s.wait.duration,s.wait.deadline,s.wait.armed)==w['wait']
 for case in range(20000):
  s=State();s.kind=case%20;s.action=rng.randrange(8);s.actions[:]=[rng.randrange(8) for _ in range(4)];s.wait=Timer(1234,0xfddc1234,rng.randrange(4));s.background_wait=rng.choice([-1,0,1,2]);s.route_flag=rng.choice([-1,0,2,3,5]);s.hidden=rng.randrange(4)
  sh=Shared(rng.uniform(0,30),rng.uniform(-.5,1.5),39);inp=Input();inp.seconds=[0,1/60,.5,2,3][case%5];inp.player_actions[:]=[rng.randrange(24) for _ in range(21)];inp.player_action=rng.randrange(24);inp.background_clip=rng.randrange(5);inp.npc_last_crossed=rng.randrange(0,20);inp.npc_previous_flags[:]=[rng.randrange(5) for _ in range(2)];inp.player_mode=rng.choice([-1,0,1,2,3]);wanted=n.motion(s,sh,inp);old=State.from_buffer_copy(s);oldsh=Shared.from_buffer_copy(sh);out=Effects()
  assert lib.bk_prop_motion_select(C.byref(s),C.byref(sh),C.byref(inp),C.byref(out));check(s,wanted)
  assert (sh.distance,sh.alpha,out.allowed,out.material,out.alpha)==(wanted['distance'],wanted['alpha'],wanted['allowed'],*wanted['command']),(case,sh.distance,sh.alpha,out.allowed,out.material,out.alpha,wanted)
  assert sh.last_crossed==39;commands[out.material]+=1
  s=old;flag=rng.choice([-128,-1,0,1,2,3,4,5,6,127]);wanted=n.point(s,oldsh,inp,flag);assert lib.bk_prop_point_apply(C.byref(s),flag,C.byref(inp));check(s,wanted)
  n.word(n.actor+0xc,s.kind);n.call(0x5127eb,struct.pack('<I',n.actor));assert lib.bk_prop_smooth_heading(s.kind)==n.u.reg_read(UC_X86_REG_EAX)&255
 for g in range(-1,7):
  for a in range(-1,12):
   n.call(0x5149e0,struct.pack('<2i',g,a));assert lib.bk_prop_needs_ground(g,a)==n.u.reg_read(UC_X86_REG_EAX)&255
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),motion_cases=20000,point_cases=20000,heading_cases=20000,ground_dispatches=104,commands=commands,hooks=['4b0de5','4b124d'],scope=__doc__)
 (ROOT/'local/original-prop-motion-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
