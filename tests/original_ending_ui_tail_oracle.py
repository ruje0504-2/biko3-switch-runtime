"""Actual4d6e6e..7432 including495567/48cb8a/4dfb96, real UI math and RNG.
Clip requests, eye and media IO are observed services; filenames4e0032/4e0818
execute unchanged with Win32 string helper boundaries. Same retained UI state,
ordered commands, wrap counters and vertices. Explicit controller fixtures,
not a complete ending lifecycle or hardware rendering test.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_ending_stage_ui_oracle import Native as Base,Stage,bind,install,same
from original_detached_sprite_oracle import Frame as DrawFrame
from original_ending_frame_oracle import State as Frame,FIELDS,BYTES
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Auxiliary,ADDR,Ops as ActorOps,Active,Write,Request,Audio,Eyes
from original_ending_ui_cursor_oracle import Notices
from original_effect_sprite_oracle import Effect
from original_item_notice_oracle import Fade
from original_player_hud_oracle import Timer
import original_ending_ui_oracle as m
from model_binding import ROOT,library
I=C.c_int32;F=C.c_float;B=C.c_uint8;S=C.c_int8;P=C.POINTER
class Normal(C.Structure):_fields_=[('frame',I),('cycles',I),('elapsed',F),('uv_right',F)]
class Aux(C.Structure):
 _fields_=[('mode',I),('choice',I),('once',I),('random_latch',I),('processed',I*2),('group_seen',I*5),('cycles',I),('reset_a',I),('reset_b',I),('reset_c',I),('elapsed',F),('sequence_elapsed',F),('sequence',B),('sequence_count',B)]
class Bindings(C.Structure):
 _fields_=[('frame',P(Frame)),('control',P(Control)),('auxiliary',P(Auxiliary)),('notices',P(Notices)),('flash_wanted',P(B)),('final_state',P(S)),('normal_side',P(S)),('normal_target',P(I)),('normal_inputs',P(I)),('normal_processed',P(I)),('gauge_y',P(F)),('contact_index',P(I)),('aux_inputs',P(I)),('aux_config',P(I*6)),('random',P(C.c_uint32)),('speech_names',P(C.c_char*32)),('voice_volume',P(I))]
Speech=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_char_p,I,C.c_void_p)
class Ops(C.Structure):_fields_=[('actor',ActorOps),('speech',Speech)]
class Common(C.Structure):_fields_=[('curtain',Fade),('wait',Timer),('gate',B),('action',B),('blocked',B)]
NORMAL=[0x719c3c,0x719c40,0x719c44,0x575638]
AUX=[('mode',0x6dde94),('choice',0x722104),('once',0x6ea310),('random_latch',0x6ddea0),('cycles',0x6ea384),('reset_a',0x6ea344),('reset_b',0x5546a4),('reset_c',0x6ddea8),('elapsed',0x6ea388),('sequence_elapsed',0x6ea350),('sequence',0x6ea34c),('sequence_count',0x6ea34d)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  self.wi(0x53f2f0,0x300d000);self.wi(0x53f214,0x300d010)
  self.wi(0x300b024,0x300d100);self.wi(0x300a000,0x300b000)
  self.wi(0x300a020,0x300b000)
  for a in [0x300d000,0x300d010,0x300d100,0x4e0956,0x4ad2bf,0x4946b4,0x4a07a9,0x4018c8]:
   self.u.hook_add(UC_HOOK_CODE,self.service,begin=a,end=a)
 def raw(self,a):return bytes(self.u.mem_read(a,260)).split(b'\0')[0]
 def service(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.ri(sp);pop=4;result=0
  def words(n):return struct.unpack('<'+'i'*n,u.mem_read(sp+4,n*4))
  if a==0x300d000:
   dst,fmt=words(2);fmt=self.raw(fmt).decode();count=fmt.count('%');args=words(count+2)[2:];value=(fmt%args).encode();u.mem_write(dst,value+b'\0');result=len(value)
  elif a==0x300d010:
   dst,src=words(2);u.mem_write(dst,self.raw(src)+b'\0');pop=12;result=dst
  elif a==0x4e0956:
   name,slot=words(2);self.trace.append(('speech',slot,self.raw(name).decode(),I(self.ri(0xbe9a08)).value));self.wi(0x722334+slot*0x120,0x300a000+slot*32)
   if self.mutate:self.wi(0x721ed0,9);self.u.mem_write(0x719b4c,b'\0');self.wi(0x719444,0)
  elif a==0x4ad2bf:
   ptr,loop,volume=words(3);assert ptr in [0x300a000,0x300a020] and loop==0
  elif a==0x4946b4:
   cue,slot,flags=words(3);assert flags==0;self.trace.append(('audio',slot,cue,I(self.ri(0xbe9a08)).value))
   if self.mutate:self.wi(0x721e00,8)
  elif a==0x4a07a9:
   ptr,slot=words(2);assert ptr==0x70d370;self.trace.append(('eyes',slot,self.ri(0x721df4),self.ri(0x721df0)))
  elif a==0x4018c8:
   ptr,slot=words(2);assert ptr==self.actor;self.trace.append(('request',slot));self.wi(ptr+0x140,slot)
  else:
   ptr,dst=words(2);assert ptr==0x300a020;self.trace.append(('status',));self.wi(dst,self.flags);result=self.hr;pop=12
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256);rng=random.Random(0x4d6e6e)
 lib.bk_ending_ui_tail.argtypes=[P(m.Ui),P(Stage),P(Normal),P(Aux),P(Bindings),P(Ops),F,F,P(DrawFrame),C.c_void_p]
 lib.bk_ending_ui_curtain.argtypes=[P(Common),F,P(F),C.c_void_p]
 lib.bk_ending_sound_normal_voice.argtypes=[C.c_uint,I,I,C.c_void_p,C.c_void_p]
 lib.bk_ending_sound_contact_voice.argtypes=[C.c_uint,I,I,I,C.c_void_p,C.c_void_p]
 # Real string selection, all recovered table entries and nonboolean modes.
 names=0
 for g in range(5):
  n.u.mem_write(0x721b3c,bytes([g]));dst=0x300e000
  for target in [1,11,12,9,10,26,27,5]:
   for mode in [0,1,2,-1,255]:
    out=C.create_string_buffer(32);n.wi(0x721ed0,target)
    if target==1 and mode not in [0,1]:assert not lib.bk_ending_sound_normal_voice(g,target,mode,out,e);continue
    n.call(0x4e0032,struct.pack('<Ii',dst,mode));assert lib.bk_ending_sound_normal_voice(g,target,mode,out,e)
    assert out.value==n.raw(dst)==n.raw(0x722224);names+=1
  for kind in [0,1,-1,255]:
   for alt in [0,1,-1]:
    for i in range(3 if kind==0 else 14-int(bool(alt))):
     out=C.create_string_buffer(32);n.call(0x4e0818,struct.pack('<Iiii',dst,kind,i,alt));assert lib.bk_ending_sound_contact_voice(g,kind,i,alt,out,e)
     assert out.value==n.raw(dst)==n.raw(0x722344);names+=1
 print('PASS names',names,flush=True)
 draws=vertices=services=mutations=0;phases={};modes={};trace_lines=[]
 for case in range(4200):
  ui,stage=m.Ui(),Stage();f,c,a=Frame(),Control(),Auxiliary();ns,ax=Normal(),Aux();notices=Notices();scale=F([.25,.5,961/1280,1][case%4]).value;gauge=F()
  assert lib.bk_ending_ui_initialize(C.byref(ui),int(scale*1280),c.pause_flags,C.byref(gauge),e)
  assert lib.bk_ending_stage_ui_initialize(C.byref(ui),C.byref(stage),2,case%5,case%2,int(scale*1280),e)
  f.phase=[1,1,5,6,8,9][case%6];f.group=case%5;f.camera_cached=rng.choice([11,12,9,10,26,27,5]);a.gate=rng.choice([1,3,3,5]);a.pending=rng.choice([0,6,23]);a.variant=case%2;a.selection=rng.randrange(3);a.progress=rng.choice([0,.1,.59,.6,.79,.8,.99,1,1.2]);a.expression_a=17;a.expression_b=19;c.toggles[7]=rng.choice([0,1,255])
  side=S(rng.randrange(2));target=I(rng.randrange(7));inputs=(I*14)(*[rng.choice([0,1,1,2]) for _ in range(14)]);processed=(I*14)(*[rng.choice([0,0,1]) for _ in range(14)]);contact=I(rng.choice([0,2,4,6,8,10,12]));volume=I(-500)
  ns.frame=rng.choice([-5,-2,0,0,0,1,2,3,4,7]);ns.cycles=rng.choice([0,1,3,3,4,0x7fffffff]);ns.elapsed=rng.choice([-.1,0,.001,.5]);ns.uv_right=.8
  ax.mode=(case//6)%8;ax.choice=rng.randrange(2);ax.once=rng.randrange(2);ax.random_latch=7;ax.processed[:]=[rng.randrange(2),rng.randrange(2)];ax.group_seen[:]=[rng.randrange(2) for _ in range(5)]
  ax.cycles=rng.choice([0,1,3,3,4,0x7fffffff]);ax.reset_a=11;ax.reset_b=12;ax.reset_c=13;ax.elapsed=rng.choice([-.1,0,.001]);ax.sequence_elapsed=rng.choice([0,.001]);ax.sequence=rng.choice([0,1,3,127,255]);ax.sequence_count=rng.choice([0,3,4,127,255]);auxinputs=(I*2)(rng.choice([0,1,2]),rng.choice([0,1,2]));config=((I*6)*5)(*[(I*6)(*[rng.randrange(2) for _ in range(6)]) for _ in range(5)])
  seed=C.c_uint32(rng.getrandbits(32));flash=B(rng.choice([0,1,2,255]));final=S(rng.choice([0,8,9,9,-1]));dt=F(rng.choice([0,1/60,.1,.5])).value;oldseed=seed.value
  speech_names=((C.c_char*32)*2)();speech_names[0].value=b'old0';speech_names[1].value=b'old1';clip=I(rng.choice([0,1,6,7,10,11,13,14,15,16]));chain=[8,9]
  for i in range(4):notices.notices[i]=rng.choice([0,1,1,2])
  for i in range(2):notices.popups[i]=rng.randrange(2)
  for i in range(52,57):
   ui.sprites[i].transform.fade.stage=rng.choice([0,1,2,3,3,3,4,5]);ui.sprites[i].transform.fade.alpha=rng.random();ui.sprites[i].transform.fade.speed=rng.choice([.5,1,2,5])
  # Directed terminal branches improve coverage without replacing their logic.
  if case%12==0:
   notices.notices[0]=1;ui.sprites[53].transform.fade.stage=3;ns.frame=0;ns.cycles=3;dt=F(1/60).value;processed[:]=[1]*14
  if case%48==2:
   ax.mode=6;a.gate=3;ax.once=1;ax.sequence_count=3;notices.notices[0]=1;ui.sprites[53].transform.fade.stage=3;dt=F(1/60).value
  b=Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(notices),C.pointer(flash),C.pointer(final),C.pointer(side),C.pointer(target),inputs,processed,C.pointer(gauge),C.pointer(contact),auxinputs,config,C.pointer(seed),speech_names,C.pointer(volume))
  install(n,ui,stage)
  for name,addr in FIELDS:n.wi(addr,getattr(f,name))
  for name,addr in BYTES:n.u.mem_write(addr,bytes([getattr(f,name)]))
  for (name,typ),addr in zip(Auxiliary._fields_,ADDR):n.u.mem_write(addr,bytes(a)[getattr(Auxiliary,name).offset:getattr(Auxiliary,name).offset+C.sizeof(typ)])
  for (name,typ),addr in zip(Normal._fields_,NORMAL):n.u.mem_write(addr,bytes(ns)[getattr(Normal,name).offset:getattr(Normal,name).offset+4])
  for name,addr in AUX:
   typ=dict(Aux._fields_)[name];n.u.mem_write(addr,bytes(ax)[getattr(Aux,name).offset:getattr(Aux,name).offset+C.sizeof(typ)])
  n.u.mem_write(0x6ea178,bytes(ax.processed))
  for i in range(5):n.wi(0x6ea18c+i*64,ax.group_seen[i])
  for addr,value in [(0x719b4c,side),(0x719444,target),(0x709c70,inputs),(0x719b64,processed),(0x721e24,gauge),(0x721ed4,contact),(0xbe9a08,volume),(0x6ea170,auxinputs),(0x6e9fa8,config),(0x58edd8,seed),(0x738baf,flash),(0x6d1c0c,final)]:n.u.mem_write(addr,bytes(value))
  n.u.mem_write(0x7220f8,bytes(c.toggles));n.u.mem_write(0x722224,b'old0\0');n.u.mem_write(0x722344,b'old1\0')
  for i in range(4):n.u.mem_write(m.BASE+(53+i)*0x16c+0x167,bytes([notices.notices[i]]))
  for i in range(2):n.u.mem_write(m.BASE+(72+i)*0x16c+0x167,bytes([notices.popups[i]]))
  n.wi(0x721b28,n.actor);n.wi(n.actor+0x140,clip.value);n.wi(n.actor+0xb24,chain[0]);n.wi(n.actor+0xbc0,chain[1]);n.wf(0x721ad0,scale);n.wf(0x733700,dt)
  n.wi(0x722454,0x300a020);n.flags=rng.choice([0,1,2,3]);n.hr=rng.choice([0,0,1]);n.trace=[];n.draws=[];n.mutate=case%37==0
  n.segment(0x4d6e6e,0x4d7432)
  trace=[]
  @Active
  def active(_,out,err):out[0]=clip.value;return 1
  @Write
  def write(_,slot,kind,value,err):assert slot in [15,16] and kind==0;chain[slot-15]=value;return 1
  @Request
  def request(_,slot,err):trace.append(('request',slot));clip.value=slot;return 1
  @Audio
  def audio(_,cp,playing,err):
   call=cp.contents
   if call.operation==0:trace.append(('status',));playing[0]=int(not n.hr and n.flags&1)
   else:
    assert call.operation==3;trace.append(('audio',call.slot,call.cue,call.volume));playing[0]=0
    if n.mutate:f.phase=8
   return 1
  @Eyes
  def eyes(_,slot,err):trace.append(('eyes',slot,a.expression_a,a.expression_b));return 1
  @Speech
  def speech(_,slot,name,vol,err):
   trace.append(('speech',slot,name.decode(),vol))
   if n.mutate:f.camera_cached=9;side.value=0;target.value=0
   return 1
  ops=Ops(ActorOps(None,active,write,request,audio,eyes),speech);out=DrawFrame()
  assert lib.bk_ending_ui_tail(C.byref(ui),C.byref(stage),C.byref(ns),C.byref(ax),C.byref(b),C.byref(ops),scale,dt,C.byref(out),e),(case,e.value)
  assert trace==n.trace,(case,trace,n.trace)
  assert clip.value==I(n.ri(n.actor+0x140)).value and chain==[n.ri(n.actor+0xb24),n.ri(n.actor+0xbc0)],case
  for obj,fields in [(ns,list(zip([x[0] for x in Normal._fields_],NORMAL))),(ax,AUX),(a,list(zip([x[0] for x in Auxiliary._fields_],ADDR)))]:
   for name,addr in fields:
    typ=dict(obj._fields_)[name];off=getattr(type(obj),name).offset;got=bytes(obj)[off:off+C.sizeof(typ)];assert got==bytes(n.u.mem_read(addr,len(got))),(case,name,got.hex(),bytes(n.u.mem_read(addr,len(got))).hex())
  for addr,value in [(0x719b64,processed),(0x721e24,gauge),(0x58edd8,seed),(0x738baf,flash),(0x6ea178,ax.processed)]:assert bytes(value)==bytes(n.u.mem_read(addr,C.sizeof(value))),(case,hex(addr))
  for i in range(5):assert ax.group_seen[i]==n.ri(0x6ea18c+i*64)
  for i in range(4):assert notices.notices[i]==n.u.mem_read(m.BASE+(53+i)*0x16c+0x167,1)[0]
  for i in range(2):
   assert notices.popups[i]==n.u.mem_read(m.BASE+(72+i)*0x16c+0x167,1)[0]
   assert speech_names[i].value==n.raw(0x722224+i*0x120)
  same(n,ui,stage,case);assert out.count==len(n.draws),(case,out.count,len(n.draws))
  for j,(slot,_,raw) in enumerate(n.draws):
   d=out.draws[j];assert d.slot==slot
   for k,q in enumerate([0,1,2,3,0,2]):
    assert tuple(d.xy[2*q:2*q+2])==struct.unpack_from('<2f',raw,32*k),(case,'vertices')
    assert struct.unpack_from('<I',raw,32*k+16)[0]==(int(d.alpha*255)<<24)|d.rgb,(case,'alpha')
    x,y=[(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)][k];assert (d.uv[x],d.uv[y])==struct.unpack_from('<2f',raw,32*k+24)
    vertices+=1
  draws+=out.count;services+=len(trace);mutations+=bool(n.mutate and trace);phases[f.phase]=phases.get(f.phase,0)+1;modes[ax.mode]=modes.get(ax.mode,0)+1
  if case%600==0:print('PASS tail',case,draws,services,flush=True)
 # Isolated shared curtain segment, including values2/255 and exact endpoints.
 for case in range(600):
  s=Common();s.curtain=Fade(rng.choice([0,.001,.3,1]),rng.choice([0,.5,2,5]),case%6);s.blocked=rng.choice([0,1,2,255]);dt=F(rng.choice([0,1/60,.5,1])).value
  effect=Effect();effect.fade=s.curtain;effect.scale[:]=[1,1];effect.enter=effect.exit=1
  n.install(0xbeea18,74,effect,(F*4)(0,0,1280,960));n.u.mem_write(0xbeeb7f,bytes([s.blocked]));n.wf(0x733700,dt);n.draws=[];n.segment(0x4d677b,0x4d679d)
  alpha=F();assert lib.bk_ending_ui_curtain(C.byref(s),dt,C.byref(alpha),e)
  assert (s.curtain.alpha,s.curtain.speed,s.curtain.stage)==(n.rf(0xbeeb44),n.rf(0xbeeb50),n.u.mem_read(0xbeeb4c,1)[0])
  assert int(alpha.value*255)==struct.unpack_from('<I',n.draws[0][2],16)[0]>>24
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),names=names,tail_frames=4200,curtain_frames=600,draws=draws,vertices=vertices,services=services,mutable_callbacks=mutations,phases=phases,modes=modes,max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-ui-tail-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
