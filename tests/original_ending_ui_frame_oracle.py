"""Whole4d4979/4d499b with native UI children, pointer conversion, RNG and reload
selectors. Device/input/scene-resource/media leaves are observed boundaries.
Phase7's demonstrably uninitialized cursor is explicitly seeded -1 and counted
as a portable policy, separately from defined native frames. Loaders mutate
live scalar state here; this is not their complete model/resource execution.
Null-image device writes retain the previously documented policy.
"""
import ctypes as C,struct,json,hashlib,random,argparse
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
import original_ending_ui_tail_oracle as t
import original_ending_ui_hints_oracle as h
import original_ending_ui_select_oracle as s
import original_ending_reload_oracle as r
from original_ending_ui_toolbar_oracle import Bindings as Toolbar
from original_ending_ui_cursor_oracle import Bindings as Cursor
from original_detached_sprite_oracle import null_write
from model_binding import ROOT,library
m=t.m;I=t.I;F=t.F;B=t.B;P=C.POINTER
class Controller(C.Structure):_fields_=[('hints',h.State),('normal',t.Normal),('aux',t.Aux)]
class Bindings(C.Structure):_fields_=[('common',P(t.Common)),('toolbar',Toolbar),('hints',h.Bindings),('select',s.Bindings),('cursor',Cursor),('reload',r.Bindings),('tail',t.Bindings)]
Capture=C.CFUNCTYPE(C.c_int,C.c_void_p,P(F),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('position',Capture),('motion',Capture),('key',m.KEY),('voice',s.Voice),('reload',r.Ops),('tail',t.Ops)]
class Output(C.Structure):_fields_=[('sprites',t.DrawFrame),('curtain_after',C.c_uint),('curtain',F),('early',C.c_int),('complete',C.c_int)]
HINT=[('length',0x719b18),('fixed_scroll',0x6bbe44),('variable_scroll',0x6afd28),('movement_ready',0x6afd14),('once_flags',0x70c8cc),('meter_x',0x6e9fa4),('meter_once_flags',0x6ddea4)]
def fields(n,obj,layout,read=False,label=None):
 for name,addr in layout:
  typ=dict(obj._fields_)[name];off=getattr(type(obj),name).offset;raw=bytes(obj)[off:off+C.sizeof(typ)]
  if read:assert raw==bytes(n.u.mem_read(addr,len(raw))),(label,name,raw.hex(),bytes(n.u.mem_read(addr,len(raw))).hex())
  else:n.u.mem_write(addr,raw)
def ptr(obj,name,typ):return C.cast(C.byref(obj,getattr(type(obj),name).offset),P(typ))
class Native(t.Native):
 def __init__(self,exe):
  super().__init__(exe);null_write(self);m.COUNT=76
  self.children={a:0 for a in [0x4d901c,0x4971bd,0x4db3e4,0x4797dc,0x4da76f,0x495567,0x48cb8a,0x4d9354,0x4d9575]}
  for a in [*r.LOAD,*r.RELEASE,*r.LEAVE,*r.LIGHT,0x51c47e,0x4d959c,0x4d6b84,0x4b76c2,0x4d49a4,0x4b75aa,0x4b757e,0x4d4a03,0x4d679d,0x4d6e6e,0x50de40,0x50e7c1,*self.children]:self.u.hook_add(UC_HOOK_CODE,self.extra,begin=a,end=a)
 def sprite(self,i):
  if i!=75:return super().sprite(i)
  value=m.Sprite();value.alpha=self.rf(0xbeeb44);return value
 def extra(self,u,a,size,_):
  if a in self.children:self.children[a]+=1;return
  if a==0x4d49a4:
   self.bp=u.reg_read(UC_X86_REG_EBP)
   if self.policy:self.wi(self.bp-0x11c,-1)
   return
  if a in [0x4b75aa,0x4b757e]:
   self.trace.append(('position' if a==0x4b75aa else 'motion',self.ri(0x719b18),self.u.mem_read(0x70c8cc,1)[0]));return
  if a==0x4d4a03:
   self.pointer=[self.rf(self.bp-0x124),self.rf(self.bp-0x128)];self.motion=[self.rf(self.bp-0x110),self.rf(self.bp-0x114)];return
  if a==0x4d679d:self.boundary=len(self.draws)-1;return
  if a==0x4d6e6e:self.tail_entered=True;return
  if a in [0x4d959c,0x4d6b84]:
   slot=2+u.reg_read(UC_X86_REG_ECX)//0x120 if a==0x4d959c else 0
   self.trace.append(('present',slot,0));return
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.ri(sp)
  def words(n):return struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4))
  value=0
  if a==0x4b76c2:
   code,mode,extra=words(3);assert mode==2 and extra==0
   self.trace.append(('key',code,mode));value=self.keys[code]
   if self.key_mutation:self.wi(0x721ee4,2);self.wi(0x72210c,0)
  elif a in r.LOAD:
   kind=r.LOAD.index(a);arg=I(words(1)[0]).value if kind==2 else -1;self.trace.append(('load',kind,arg))
   if self.load_mutation:self.wi(0x721e00,[1,2,5,3,4][kind]);self.u.mem_write(0xbeeb7f,b'\0')
  elif a in r.RELEASE:self.trace.append(('release',r.RELEASE.index(a)))
  elif a in r.LEAVE:self.trace.append(('leave',r.LEAVE.index(a)))
  elif a in r.LIGHT:self.trace.append(('light',r.LIGHT.index(a)))
  elif a==0x51c47e:self.trace.append(('schedule',*words(2)))
  elif a==0x50de40:
   if words(1)[0]==0x73a990:self.trace.append(('image',1,self.u.mem_read(0x721b3c,1)[0]))
   return # actual image constructor and setters, device IO only
  elif a==0x50e7c1:
   if words(1)[0]==0x73a990:self.trace.append(('image',0,0))
   return
  else:raise AssertionError(hex(a))
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--frames',type=int,default=1800);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();t.bind(lib);e=C.create_string_buffer(256);rng=random.Random(0x4d4979)
 lib.bk_ending_ui_frame.argtypes=[P(m.Ui),P(t.Stage),P(Controller),P(Bindings),P(Ops),F,F,P(Output),C.c_void_p]
 draws=vertices=services=policies=nullframes=early=reloads=mutations=0;phases={}
 for case in range(args.frames):
  width=[320,640,961,1280][case%4];scale=F(width/1280).value;dt=F([0,1/60,.1,.5][(case//4)%4]).value
  ui,stage=m.Ui(),t.Stage();state=r.State();f,c,a=state.frame,state.control,state.auxiliary
  ctl=Controller();common=t.Common();notices=t.Notices();gauge=F()
  assert lib.bk_ending_ui_initialize(C.byref(ui),width,c.pause_flags,C.byref(gauge),e)
  f.phase=case%9+1;f.group=(case//9)%5;c.variant=1 if f.phase==2 else 0;a.variant=case%2;a.selection=case%3;a.gate=[1,2,3,5][case%4];a.progress=.5
  f.camera_cached=11;f.camera_request=0;f.camera_event=case%4;f.state_721ee0=1;f.state_721ee4=3;c.state_721eec=3
  state.previous=8 if case%2 else 24;state.next_mode=case%2
  kind={1:0,2:1,3:3,4:4,5:2,6:2,7:5,8:2,9:0}[f.phase]
  assert lib.bk_ending_stage_ui_initialize(C.byref(ui),C.byref(stage),kind,f.group,a.variant,width,e),e.value
  # Show a subset before request, not just transparent construction frames.
  for slot in range(75):
   p=ui.sprites[slot] if slot<63 else stage.sprites[slot-63]
   p.transform.fade.alpha=rng.choice([0,.25,.5,1]);p.transform.fade.stage=rng.choice([0,1,3,4]);p.transform.fade.speed=2
  common.curtain=t.Fade([0,.25,1][case%3],2,3 if case%3==2 else case%3);common.blocked=1 if case%3==2 else 0
  common.action=[6,7,8,63,74,45,47,49,0,255][(case//9)%10]
  # Deliberately stale diagnostic aliases; only the common owner is live.
  f.curtain_wanted=255;f.transition_action=255;f.finish_blocked=255
  ctl.hints=h.State(12,.2,.3,-33,case%2,21,case%2);ctl.normal=t.Normal(0,case%4,0,.8);ctl.aux.mode=0
  opened=I(case%2);s3=I(3);clip=I([0,1,3,6,7,10][case%6]);ready=I(1);timing=h.Timing(0,100,25)
  points=(h.Pair*3)(h.Pair(50,60),h.Pair(90,80),h.Pair(100,110));choices=(I*3)(0,1,2)
  targets=(h.Pair*39)(*[h.Pair(i*7,i*5) for i in range(39)]);alternate=h.Pair(70,100);unavail=h.Pair(0,1);item=B(1)
  actions=(I*80).from_buffer_copy(bytes(n.u.mem_read(0x56f7f4+(f.group*2+1)*320,320)))
  cam=s.ident();matrix=s.ident();world=(s.M*39)(*[s.ident() for _ in range(39)]);present=(B*39)(*[1]*39);pos=(F*3)(0,0,-100)
  for i in range(39):world[i][12]=i*10;world[i][13]=i*3
  pick=s.Pick(world,present,39,pos,matrix,matrix,matrix,ui.sprites[50].rect[2])
  flash=B(case%2);final=t.S(9);side=t.S(0);target=I(0);inputs=(I*14)();processed=(I*14)();contact=I(2);volume=I(-500);auxinputs=(I*2)();config=((I*6)*5)();seed=C.c_uint32(case+23);names=((C.c_char*32)*2)()
  for i in range(4):notices.notices[i]=int(case%5==i)
  for i in range(2):notices.popups[i]=case%2
  tb=Toolbar(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(opened))
  hb=h.Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(s3),C.pointer(clip),points,choices,targets,39,alternate,C.pointer(gauge),C.pointer(timing))
  sb=s.Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(s3),C.pointer(clip),C.pointer(opened),actions,targets,39,cam,C.pointer(pick),unavail,C.pointer(item))
  cb=Cursor(C.pointer(f),C.pointer(a),C.pointer(clip),C.pointer(ready),C.pointer(notices))
  rb=r.bindings(state);rb.action=ptr(common,'action',B);rb.wanted=ptr(common,'blocked',B)
  tail=t.Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(notices),C.pointer(flash),C.pointer(final),C.pointer(side),C.pointer(target),inputs,processed,C.pointer(gauge),C.pointer(contact),auxinputs,config,C.pointer(seed),names,C.pointer(volume))
  b=Bindings(C.pointer(common),tb,hb,sb,cb,rb,tail)
  t.install(n,ui,stage)
  for off,addr,size in r.REGIONS:
   if addr!=0x73aac4:n.u.mem_write(addr,bytes(state)[off:off+size])
  for obj,layout in [(ctl.hints,HINT),(ctl.normal,list(zip([x[0] for x in t.Normal._fields_],t.NORMAL))),(ctl.aux,t.AUX)]:fields(n,obj,layout)
  n.u.mem_write(0x6ea178,bytes(ctl.aux.processed))
  for i in range(5):n.wi(0x6ea18c+i*64,ctl.aux.group_seen[i])
  for addr,value in [(0x719b4c,side),(0x719444,target),(0x709c70,inputs),(0x719b64,processed),(0x721e24,gauge),(0x721ed4,contact),(0xbe9a08,volume),(0x6ea170,auxinputs),(0x6e9fa8,config),(0x58edd8,seed),(0x738baf,flash),(0x6d1c0c,final),(0x7220c8,points),(0x7220e4,choices),(0x721f90,targets),(0x6afd38,alternate),(0x709db8,actions),(0x6afd0c,unavail),(0x71bcdc,item)]:n.u.mem_write(addr,bytes(value))
  for i in range(4):n.u.mem_write(m.BASE+(53+i)*0x16c+0x167,bytes([notices.notices[i]]))
  for i in range(2):n.u.mem_write(m.BASE+(72+i)*0x16c+0x167,bytes([notices.popups[i]]))
  n.wi(0x721ee8,s3.value);n.wi(0x72210c,opened.value);n.wi(0x719b0c,ready.value);n.wi(0x721b28,n.actor);n.wi(n.actor+0x140,clip.value);n.wi(n.actor+0xb24,8);n.wi(n.actor+0xbc0,9)
  n.wf(n.actor+0x190+clip.value*156+0x54,timing.start);n.wf(n.actor+0x190+clip.value*156+0x60,timing.source)
  n.wi(0x645604,0x300c000);n.u.mem_write(0x300c080,bytes(cam));n.u.mem_write(0x300c0c0,bytes(matrix))
  for i in range(39):n.wi(0x721ef4+i*4,0x3001000+i*0x100);n.u.mem_write(0x3001000+i*0x100+0xc0,bytes(world[i]))
  for addr,value in [(0x71b40c,pos),(0x642fa8,matrix),(0x642830,matrix),(0x642af0,matrix)]:n.u.mem_write(addr,bytes(value))
  effect=t.Effect();effect.fade=common.curtain;effect.scale[:]=[1,1];effect.enter=effect.exit=1
  n.install(0xbeea18,75,effect,(F*4)(0,0,width,width*.75));n.u.mem_write(0xbeeb7f,bytes([common.blocked]));n.u.mem_write(0xbeeb7e,bytes([common.action]))
  for i in range(48):n.wi(0x722334+i*0x120 if i<47 else 0x722214,0)
  n.u.mem_write(0x722224,bytes(32));n.u.mem_write(0x722344,bytes(32))
  n.wi(0x709964,int([30,1100,1200][case%3]*scale));n.wi(0x709968,int(100*scale));n.wi(0x709974,40);n.wi(0x709978,50);n.wi(0x70996c,case%7-3);n.wi(0x709970,case%9-4)
  n.wf(0x721ad0,scale);n.wf(0x733700,dt);n.u.mem_write(0x5767c8,b'\1')
  n.keys=[rng.choice([0,1,256,255]),rng.choice([0,1,256,255])];n.key_mutation=case%17==0;n.load_mutation=case%2==0;n.policy=f.phase==7;n.mutate=False;n.hr=0;n.flags=0
  n.trace=[];n.draws=[];n.null_calls=[];n.tail_entered=False;n.loaded=[]
  try:n.call(0x4d4979,b'')
  except Exception:
   print('native failure',case,hex(n.u.reg_read(UC_X86_REG_EIP)),n.trace,flush=True);raise
  trace=[];chain=[8,9]
  def capture(which,out,values):
   trace.append((which,struct.unpack('<I',struct.pack('<f',ctl.hints.length))[0],ctl.hints.once_flags));out[0],out[1]=values;return 1
  @Capture
  def position(_,out,err):return capture('position',out,n.pointer)
  @Capture
  def motion(_,out,err):return capture('motion',out,n.motion)
  @m.KEY
  def key(_,code,mode,out,err):
   trace.append(('key',code,mode));out[0]=n.keys[code]
   if n.key_mutation:f.state_721ee4=2;opened.value=0
   return 1
  @s.Voice
  def voice(_,out,err):out[0]=0;return 1
  @r.Load
  def load(_,kind,arg,err):
   trace.append(('load',kind,arg))
   if n.load_mutation:f.phase=[1,2,5,3,4][kind];common.blocked=0
   return 1
  @r.One
  def release(_,kind,err):trace.append(('release',kind));return 1
  @r.Image
  def image(_,create,group,err):
   trace.append(('image',create,group))
   return lib.bk_ending_stage_ui_initialize(C.byref(ui),C.byref(stage),5,group,0,width,err) if create else lib.bk_ending_stage_ui_release(C.byref(ui),C.byref(stage),5,err)
  @r.One
  def leave(_,kind,err):trace.append(('leave',kind));return 1
  @r.One
  def light(_,kind,err):trace.append(('light',kind));return 1
  @r.Schedule
  def schedule(_,target,mode,err):trace.append(('schedule',target,mode));return 1
  @r.Query
  def pres(_,slot,out,err):trace.append(('present',slot,0));out[0]=0;return 1
  @r.Query
  def status(_,slot,out,err):raise AssertionError('absent')
  @r.Pause
  def pause(_,slot,err):raise AssertionError('absent')
  @t.Active
  def active(_,out,err):out[0]=clip.value;return 1
  @t.Write
  def write(_,slot,kind,value,err):assert slot in [15,16] and kind==0;chain[slot-15]=value;return 1
  @t.Request
  def request(_,slot,err):trace.append(('request',slot));clip.value=slot;return 1
  @t.Audio
  def audio(_,cp,out,err):
   call=cp.contents;out[0]=0
   if call.operation!=0:trace.append(('audio',call.slot,call.cue,call.volume))
   return 1
  @t.Eyes
  def eyes(_,slot,err):trace.append(('eyes',slot,a.expression_a,a.expression_b));return 1
  @t.Speech
  def speech(_,slot,name,vol,err):trace.append(('speech',slot,name.decode(),vol));return 1
  ops=Ops(None,position,motion,key,voice,r.Ops(None,load,release,image,leave,light,schedule,pres,status,pause),t.Ops(t.ActorOps(None,active,write,request,audio,eyes),speech));out=Output()
  assert lib.bk_ending_ui_frame(C.byref(ui),C.byref(stage),C.byref(ctl),C.byref(b),C.byref(ops),scale,dt,C.byref(out),e),(case,f.phase,e.value)
  assert out.complete and out.early==int(not n.tail_entered) and out.curtain_after==n.boundary,(case,'completion',out.early,n.tail_entered,out.curtain_after,n.boundary)
  assert trace==n.trace,(case,'trace',trace,n.trace)
  assert common.blocked==n.u.mem_read(0xbeeb7f,1)[0] and common.action==n.u.mem_read(0xbeeb7e,1)[0]
  assert (common.curtain.alpha,common.curtain.speed,common.curtain.stage)==(n.rf(0xbeeb44),n.rf(0xbeeb50),n.u.mem_read(0xbeeb4c,1)[0])
  for off,addr,size in r.REGIONS:
   if addr in [0xbeeb7f,0xbeeb7e,0xbeeb4c,0x73aac4]:continue
   assert bytes(state)[off:off+size]==bytes(n.u.mem_read(addr,size)),(case,'state',hex(addr),bytes(state)[off:off+size].hex(),bytes(n.u.mem_read(addr,size)).hex())
  for obj,layout in [(ctl.hints,HINT),(ctl.normal,list(zip([x[0] for x in t.Normal._fields_],t.NORMAL))),(ctl.aux,t.AUX)]:fields(n,obj,layout,True,case)
  t.same(n,ui,stage,case);assert out.sprites.count+1==len(n.draws),(case,'count',out.sprites.count,len(n.draws))
  j=0
  for k,(slot,sp,raw) in enumerate(n.draws):
   if slot==75:
    assert j==out.curtain_after and int(out.curtain*255)==struct.unpack_from('<I',raw,16)[0]>>24;continue
   d=out.sprites.draws[j];j+=1;assert d.slot==slot,(case,k,d.slot,slot)
   for v,q in enumerate([0,1,2,3,0,2]):
    assert tuple(d.xy[2*q:2*q+2])==struct.unpack_from('<2f',raw,v*32),(case,k,'xy')
    assert (int(d.alpha*255)<<24)|d.rgb==struct.unpack_from('<I',raw,v*32+16)[0],(case,k,'rgba')
    x,y=[(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)][v];assert (d.uv[x],d.uv[y])==struct.unpack_from('<2f',raw,v*32+24),(case,k,'uv');vertices+=1
  for addr,value in [(0x719b64,processed),(0x721e24,gauge),(0x58edd8,seed),(0x738baf,flash),(0x6ea178,ctl.aux.processed),(0x72210c,opened)]:assert bytes(value)==bytes(n.u.mem_read(addr,C.sizeof(value))),(case,hex(addr))
  for i in range(4):assert notices.notices[i]==n.u.mem_read(m.BASE+(53+i)*0x16c+0x167,1)[0]
  draws+=out.sprites.count;services+=len(trace);policies+=n.policy;nullframes+=bool(n.null_calls);early+=out.early;reloads+=sum(ev[0] in ['load','image'] for ev in trace);mutations+=n.load_mutation and any(ev[0]=='load' for ev in trace);phases[case%9+1]=phases.get(case%9+1,0)+1
  if case%150==149:print('PASS full UI',case+1,'draws',draws,'reloads',reloads,flush=True)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=args.frames,defined_selection_frames=args.frames-policies,phase7_selection_policy_frames=policies,null_device_policy_frames=nullframes,draws=draws,vertices=vertices,services=services,reloads=reloads,mutated_load_callbacks=mutations,early_returns=early,phases=phases,native_child_calls={hex(k):v for k,v in n.children.items()},max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-ui-frame-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
