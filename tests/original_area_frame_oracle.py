"""Complete4f3d34 including original4f64ae/4f5e24/4f607a/50d2a0.
Only sound load/play and DirectSound setters are service boundaries. Retains
absent actor state, all16 props, last-prop ambient gate, NPC/player voice order.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_prop_motion_oracle import State as Prop
from original_npc_spatial_oracle import State as Npc
from original_npc_ai_oracle import Shared
from original_area_boundary_oracle import Bounds
from model_binding import ROOT,library
class State(C.Structure):_fields_=[('gate',C.c_uint8),('npc_played',C.c_uint8),('player_played',C.c_uint8)]
class Input(C.Structure):_fields_=[('group',C.c_uint32),('area',C.c_uint32),('present',C.c_uint32),('npc_present',C.c_int),('npc_group',C.c_int32),('volume',C.c_int32),('suppressed',C.c_uint8),('player',C.c_float*3),('yaw',C.c_float),('wall',C.c_char_p),('boundary_wall',C.c_char_p),('bounds',Bounds)]
class Gain(C.Structure):_fields_=[('volume',C.c_int32),('pan',C.c_int32)]
class Command(C.Structure):_fields_=[('recipient',C.c_int),('file',C.c_char_p),('gain',Gain)]
class Commands(C.Structure):_fields_=[('count',C.c_uint),('commands',Command*2)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  self.word(0x300a000,0x300a100);self.word(0x300a13c,0x300e100);self.word(0x300a140,0x300e110)
  for a in [0x4ff5ad,0x50d858,0x46435e,0x300e100,0x300e110]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<6I',u.mem_read(sp,24));pop=0
  if a==0x4ff5ad:
   assert args[0]==0x729240;self.npcfile=bytes(u.mem_read(args[2],260)).split(b'\0')[0];self.word(0x729350,0x300a000);self.gain=[None,None];self.trace.append('npc-load')
  elif a==0x50d858:
   assert args[0]==0x71b968;self.playerfile=bytes(u.mem_read(args[2],260)).split(b'\0')[0];self.word(0x71ba78,0x300b000);self.playergain=struct.unpack('<i',struct.pack('<I',args[3]))[0];self.trace.append('player-load')
  elif a==0x46435e:
   assert args[1]==0
   if args[0]==0x300a000:self.commands.append((0,self.npcfile,*self.gain));self.trace.append('npc-play')
   else:assert args[0]==0x300b000;self.commands.append((1,self.playerfile,self.playergain,0));self.trace.append('player-play')
  else:
   assert args[0]==0x300a000;index=0 if a==0x300e100 else 1;self.gain[index]=struct.unpack('<i',struct.pack('<I',args[1]))[0];pop=8;self.trace.append(['volume','pan'][index])
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,state,props,npc,shared,inp):
  u=self.u;self.commands=[];self.trace=[]
  for i,p in enumerate(props):
   a=0x729d80+i*0x998;u.mem_write(a,bytes(0x998));self.word(a,1 if inp.present&(1<<i) else 0);self.word(a+0xc,p.kind);u.mem_write(a+0x29c,bytes(p.path.position));u.mem_write(a+0x2ac,struct.pack('<f',p.path.yaw));u.mem_write(a+0x328,bytes([p.hidden]))
  self.word(0x728de8,inp.npc_present);self.word(0x728df4,inp.npc_group);self.word(0x729618,npc.path.cursor);u.mem_write(0x729084,bytes(npc.path.position));u.mem_write(0x729094,struct.pack('<f',npc.path.yaw_degrees));u.mem_write(0x729110,bytes([npc.ai.point.motion.hidden]));u.mem_write(0x72911a,bytes([state.npc_played]))
  u.mem_write(0x71b7ac,bytes(inp.player));u.mem_write(0x71b7bc,struct.pack('<f',inp.yaw));u.mem_write(0x71b840,bytes([inp.suppressed]));u.mem_write(0x71b842,bytes([state.player_played]));u.mem_write(0x71bcda,bytes([shared.response]));u.mem_write(0xbf3be4,bytes([state.gate]));u.mem_write(0x71bbd4,inp.wall+b'\0');u.mem_write(0x725a30,inp.boundary_wall+b'\0');u.mem_write(0x725920,bytes(inp.bounds));u.mem_write(0x7219a8,struct.pack('<2I',inp.group,inp.area));self.word(0xbe9a10,inp.volume);u.mem_write(0x5767c8,b'\1')
  self.call(0x4f3d34,b'')
  ws=State(u.mem_read(0xbf3be4,1)[0],u.mem_read(0x72911a,1)[0],u.mem_read(0x71b842,1)[0]);wp=(Prop*16).from_buffer_copy(props);wn=Npc.from_buffer_copy(npc);wi=Shared.from_buffer_copy(shared)
  for i,p in enumerate(wp):p.hidden=u.mem_read(0x729d80+i*0x998+0x328,1)[0]
  wn.ai.point.motion.hidden=u.mem_read(0x729110,1)[0];wi.response=u.mem_read(0x71bcda,1)[0]
  return ws,wp,wn,wi

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4f3d34)
 lib.bk_area_boundary_step.argtypes=[C.POINTER(State),C.POINTER(Prop),C.POINTER(Npc),C.POINTER(Shared),C.POINTER(Input),C.POINTER(Commands)]
 triggers={(0,6):234,(0,7):55,(1,6):132,(1,7):26,(2,4):165,(2,5):280,(2,6):230,(3,6):276,(3,7):241,(4,7):329};voices=both=propvis=0
 for case in range(4500):
  g=case%5;a=(case//5)%9;ng=g if case%3 else rng.randrange(5)
  props=(Prop*16)();npc=Npc();shared=Shared(rng.randrange(256),rng.randrange(256),rng.randrange(256));state=State(*[rng.choice([0,0,1,2,255]) for _ in range(3)])
  inp=Input(g,a,rng.getrandbits(16),case%7!=0,ng,rng.randrange(-6000,1),rng.choice([0,0,1,255]),(C.c_float*3)(rng.uniform(-800,800),rng.uniform(-50,50),rng.uniform(-800,800)),rng.uniform(-360,360),b'exit' if case%3 else b'Exit',b'exit',Bounds(-200,200,200,-200))
  npc.path.position[:]=[rng.uniform(-400,400),rng.uniform(-50,50),rng.uniform(-400,400)];npc.path.yaw_degrees=12;npc.path.cursor=triggers.get((ng,a),0) if case%4 else 0;npc.ai.point.motion.hidden=rng.choice([0,1,1,2,255]);npc.ai.point.motion.behavior=37
  for p in props:p.kind=rng.randrange(20);p.path.position[:]=[rng.uniform(-700,700),0,rng.uniform(-700,700)];p.path.yaw=rng.uniform(-360,360);p.hidden=rng.randrange(256)
  expected=n.run(state,props,npc,shared,inp);out=Commands();assert lib.bk_area_boundary_step(C.byref(state),props,C.byref(npc),C.byref(shared),C.byref(inp),C.byref(out))
  for label,got,want in zip(['state','props','npc','shared'],[state,props,npc,shared],expected):assert bytes(got)==bytes(want),(case,label,bytes(got).hex(),bytes(want).hex())
  commands=[(c.recipient,c.file,c.gain.volume,c.gain.pan) for c in out.commands[:out.count]];assert commands==n.commands,(case,commands,n.commands)
  trace=[]
  for cmd in commands:trace+=['npc-load','volume','pan','npc-play'] if cmd[0]==0 else ['player-load','player-play']
  assert n.trace==trace
  voices+=out.count;both+=out.count==2;propvis+=sum(bool(inp.present&(1<<i)) for i in range(16))
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=4500,prop_queries=propvis,sounds=voices,both_voice_frames=both,scope=__doc__)
 (ROOT/'local/original-area-frame-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
