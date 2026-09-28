"""Original502480 UI construction,504335 draw and504802 control incl506629.
Resource-replacement block505b87..50638e is one explicit service boundary;
not actor/face/lighting/voice loading evidence. Texture, input, audio and
release APIs are substituted. Original50e6ba/43ed45/43f558 run unmodified.
Fixed Chinese EXE, not protected Japanese-EXE instruction evidence.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_zoom_sprite_oracle import Native as Base,Zoom,snapshot as zs
from original_pause_oracle import Cursor,Sprite as Flat,KEYS,ss
from original_common_hud_oracle import State as Common,Flow,Timer
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class Sprite(C.Structure):_fields_=[('transform',Zoom),('rect',C.c_float*4),('uv',C.c_float*4)]
class State(C.Structure):
 _fields_=[('sprites',Sprite*48),('pointer',C.c_float*2),('motion',C.c_float*2),('reveal',C.c_float*5),('scroll',C.c_float),('character_hover',C.c_int32),('camera_hover',C.c_int32),('action_hover',C.c_int32),('info',C.c_int32),('row',C.c_int32),('column',C.c_int32),('selected',C.c_int32),('camera_mode',C.c_int32),('music_volume',C.c_int32),('voice_active',C.c_uint8),('music_mode',C.c_uint8),('loaded',C.c_uint64)]
class Bindings(C.Structure):_fields_=[('common',C.POINTER(Common)),('flow',C.POINTER(Flow)),('cursor',C.POINTER(Cursor)),('latch',C.POINTER(C.c_uint8)),('photos',C.POINTER(C.c_int32)),('group',C.POINTER(C.c_int32)),('area',C.POINTER(C.c_int32)),('count',C.POINTER(C.c_int32))]
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Gain=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int32,C.c_void_p)
Pointer=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_float),C.c_void_p)
Warp=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_float,C.c_float,C.c_void_p)
Stop=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Status=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int),C.c_void_p)
Release=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('sound',Sound),('gain',Gain),('position',Pointer),('motion',Pointer),('warp',Warp),('voice_stop',Stop),('voice_play',Gain),('status',Status),('replace',Sound),('release',Release)]
class Input(C.Structure):_fields_=[('buttons',C.c_uint32),('now',C.c_uint32),('seconds',C.c_float),('scale',C.c_float),('music_master',C.c_int32),('voice_master',C.c_int32),('special',C.c_uint8),('voice_present',C.c_uint8)]
class Draw(C.Structure):_fields_=[('slot',C.c_uint),('corners',C.c_float*4),('uv',C.c_float*4),('alpha',C.c_float)]
class Frame(C.Structure):_fields_=[('count',C.c_uint),('draws',Draw*22)]
ADDR=[0x734058+i*0x16c for i in range(48)]+[0xb537e8,0xbeea18]
PROGRESS=[0xbf40ec,0xbf4104,0xbf40c4,0xbf40e4,0xbf40e0]
INTS=[('character_hover',0xbf40fc),('camera_hover',0xbf40f0),('action_hover',0xbf40f4),('info',0xbf40f8),('row',0xbf40e8),('column',0xbf40c8),('selected',0xbf9b94),('camera_mode',0xbfbba4),('music_volume',0x728dd4)]
def sv(s):return zs(s.transform),tuple(s.rect),tuple(s.uv)
def snapshot(s,c,f,cur,latch,group,area,count):
 return dict(sprites=tuple(sv(p) for p in s.sprites),pointer=tuple(s.pointer),motion=tuple(s.motion),reveal=tuple(s.reveal),scroll=s.scroll,**{k:getattr(s,k) for k,_ in INTS},voice=s.voice_active,music=s.music_mode,loaded=s.loaded,common=(c.curtain.alpha,c.curtain.speed,c.curtain.stage,bytes(c.wait),c.gate,c.action,c.blocked),flow=bytes(f),cursor=(ss(cur.sprite),bytes(cur.idle),cur.wanted),latch=latch,group=group,area=area,count=count)
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.names=[];self.events=[];self.draw_meta=[];self.point=[0,0];self.relative=[0,0];self.buttons=0;self.now=0;self.playing=0;self.record=False
  self.wi(0x53f10c,0x300e000)
  self.music=0x41f0000;self.voice=0x41f0200
  for obj,vt in [(self.music,0x41f0100),(self.voice,0x41f0300)]:self.wi(obj,vt)
  self.wi(0x41f013c,0x300e100)
  for off,fn in [(0x3c,0x300e200),(0x24,0x300e300),(0x48,0x300e400)]:self.wi(0x41f0300+off,fn)
  self.wi(0x728dd0,self.music)
  for i in range(8):self.wi(0xbeee10+i*0x120,100+i)
  for a in [0x4ad8ec,0x466805,0x466814,0x43e583,0x5039df,0x4b75aa,0x4b757e,0x4b768d,0x4b76c2,0x46435e,0x4e77bf,0x505b87,0x300e000,0x300e100,0x300e200,0x300e300,0x300e400]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.u.mem_write(0x5767c8,b'\1')
 def event(self,*v):
  if self.record:self.events.append((*v,snapshot(*self.read())))
 def hook(self,u,a,size,data):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',u.mem_read(sp+4,20));pop=4;result=0
  if a in [0x443bb8,0x443be5,0x43e96a]:
   if a==0x43e96a:
    i=(args[0]-0x4000000)//0x400;raw=u.mem_read(self.verts(i),192)
    self.draw_meta.append((i,(*struct.unpack_from('<2f',raw,0),*struct.unpack_from('<2f',raw,64)),(*struct.unpack_from('<2f',raw,24),*struct.unpack_from('<2f',raw,88)),self.rf(ADDR[i]+0x12c)))
   return super().hook(u,a,size,data)
  if a==0x5039df:u.reg_write(UC_X86_REG_EIP,self.stop);return
  elif a==0x505b87:self.event('replace',self.ri(0xbf9b94));u.reg_write(UC_X86_REG_EIP,0x50638e);return
  elif a==0x4ad8ec:u.mem_write(args[0],b'assets\0')
  elif a==0x43e583:
   i=ADDR.index(args[0]-0x100);self.blank(i);self.wi(args[0],self.handle(i));self.names.append((i,self.text(args[1]).split('\\')[-1]))
  elif a==0x4b75aa:
   for p,v in zip(args,self.point):self.wf(p,v)
  elif a==0x4b757e:
   for p,v in zip(args,self.relative):self.wf(p,v)
  elif a==0x4b768d:self.point=list(struct.unpack('<2f',u.mem_read(sp+4,8)));self.event('warp',*self.point)
  elif a==0x4b76c2:assert args[0] in KEYS;result=bool(self.buttons&KEYS[args[0]])
  elif a==0x46435e:
   assert args[1]==0
   if args[0]==self.voice:self.event('voice_play')
   else:self.event('sound',args[0]-100)
  elif a==0x4e77bf:
   self.event('release',args[0]&255)
   for p in ADDR[:48]:self.wi(p+0x100,0)
  elif a==0x300e000:result=self.now
  elif a==0x300e100:self.event('gain',C.c_int32(args[1]).value);pop=12
  elif a==0x300e200:self.event('voice_gain',C.c_int32(args[1]).value);pop=12
  elif a==0x300e300:self.event('status');self.wi(args[1],self.playing);pop=12
  elif a==0x300e400:self.event('voice_stop');pop=8
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,self.ri(sp));u.reg_write(UC_X86_REG_ESP,sp+pop)
 def install(self,s,c,f,cur,latch,group,area,count,photos,inp):
  for i,p in enumerate(s.sprites):
   self.install_zoom(ADDR[i],i,p.transform,p.rect,1);self.u.mem_write(ADDR[i]+0x149,b'\1')
   for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):self.u.mem_write(self.verts(i)+32*j+24,struct.pack('<2f',p.uv[x],p.uv[y]))
   if not s.loaded&(1<<i):self.wi(ADDR[i]+0x100,0)
  for i,p in [(48,cur.sprite),(49,Flat(c.curtain,(C.c_float*4)(0,0,1280*inp.scale,960*inp.scale)))]:
   self.install_zoom(ADDR[i],i,Zoom(p.fade,(C.c_float*2)(1,1),(C.c_float*2)(0,0)),p.rect,1);self.u.mem_write(ADDR[i]+0x149,b'\1')
  for name,a in INTS:self.wi(a,getattr(s,name)&0xffffffff)
  for a,v in zip(PROGRESS,s.reveal):self.wf(a,v)
  self.u.mem_write(0xbf40d8,bytes(s.pointer));self.u.mem_write(0xbf40cc,bytes(s.motion));self.wf(0xbf40d4,s.scroll)
  for a,v in [(0xbfbb9d,s.voice_active),(0x728dd8,s.music_mode),(0xbeeb7c,c.gate),(0xbeeb7e,c.action),(0xbeeb7f,c.blocked),(0xbeeb84,f.current),(0x721ad4,f.previous),(0xbfbbb9,f.target),(0xbfbb9c,f.mode),(0xb5394f,cur.wanted),(0xbef178,latch),(0xbef778,inp.special)]:self.u.mem_write(a,bytes([v]))
  for a,v in [(0x7219a8,group),(0x7219ac,area),(0x734050,count),(0xbe9a0c,inp.music_master),(0xbe9a08,inp.voice_master)]:self.wi(a,v&0xffffffff)
  self.u.mem_write(0x721b14,bytes(photos));self.u.mem_write(0xbeeb54,bytes(c.wait));self.u.mem_write(0xb53924,bytes(cur.idle));self.wi(0x73b218,self.voice if inp.voice_present else 0)
  self.wf(0x733700,inp.seconds);self.wf(0x721ad0,inp.scale);self.buttons=inp.buttons;self.now=inp.now;self.draw_meta=[];self.events=[]
 def read(self):
  s=State()
  for i,a in enumerate(ADDR[:48]):
   raw=self.u.mem_read(self.verts(i),192)
   s.sprites[i]=Sprite(self.read_zoom(a),(C.c_float*4)(*[self.rf(a+o) for o in [0x114,0x118,0x10c,0x110]]),(C.c_float*4)(*struct.unpack_from('<2f',raw,24),*struct.unpack_from('<2f',raw,88)))
   if self.ri(a+0x100):s.loaded|=1<<i
  for name,a in INTS:setattr(s,name,C.c_int32(self.ri(a)).value)
  s.pointer[:]=struct.unpack('<2f',self.u.mem_read(0xbf40d8,8));s.motion[:]=struct.unpack('<2f',self.u.mem_read(0xbf40cc,8));s.reveal[:]=[self.rf(a) for a in PROGRESS];s.scroll=self.rf(0xbf40d4);s.voice_active=self.u.mem_read(0xbfbb9d,1)[0];s.music_mode=self.u.mem_read(0x728dd8,1)[0]
  c=Common();c.curtain=self.read_zoom(ADDR[49]).fade;c.wait=Timer.from_buffer_copy(self.u.mem_read(0xbeeb54,C.sizeof(Timer)));c.gate,c.action,c.blocked=[self.u.mem_read(a,1)[0] for a in [0xbeeb7c,0xbeeb7e,0xbeeb7f]]
  f=Flow(*[self.u.mem_read(a,1)[0] for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]])
  a=ADDR[48];cur=Cursor(Flat(self.read_zoom(a).fade,(C.c_float*4)(*[self.rf(a+o) for o in [0x114,0x118,0x10c,0x110]])),Timer.from_buffer_copy(self.u.mem_read(0xb53924,C.sizeof(Timer))),self.u.mem_read(0xb5394f,1)[0])
  return s,c,f,cur,self.u.mem_read(0xbef178,1)[0],*[C.c_int32(self.ri(a)).value for a in [0x7219a8,0x7219ac,0x734050]]

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256)
 lib.bk_selection_ui_initialize.argtypes=[C.POINTER(State),C.c_uint,C.c_uint8,C.c_int32,C.c_void_p]
 lib.bk_selection_ui_view.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
 lib.bk_selection_ui_control.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.c_void_p]
 lib.bk_selection_image.argtypes=[C.c_uint,C.c_uint8];lib.bk_selection_image.restype=C.c_char_p
 lib.bk_menu_cursor_initialize.argtypes=[C.POINTER(Cursor),C.c_uint,C.c_uint]
 rng=random.Random(0x504802);constructors=frames=draws=events=0;photos=(C.c_int32*5)(2,9,15,4,30)
 def init(width,special):
  s=State();assert lib.bk_selection_ui_initialize(C.byref(s),width,special,-900,e),e.value;return s
 for width in [320,640,1001,1280,1920]:
  for special in [0,1,2,255]:
   s=init(width,0);c=Common();f=Flow(0x38,1,0,0);cur=Cursor();lib.bk_menu_cursor_initialize(C.byref(cur),width,width*3//4)
   n.install(s,c,f,cur,0,2,3,4,photos,Input(0,0,0,width/1280,-900,-777,special,1));n.names=[];n.record=False;n.call(0x502480,b'');want=n.read()[0]
   assert lib.bk_selection_ui_initialize(C.byref(s),width,special,-900,e)
   for i,p in enumerate(s.sprites):assert sv(p)==sv(want.sprites[i]),('constructor',width,special,i,sv(p),sv(want.sprites[i]))
   assert s.loaded==want.loaded
   assert n.names==[(i,lib.bk_selection_image(i,special).decode()) for i in range(48) if lib.bk_selection_image(i,special)]
   constructors+=1
 def check(s,c,f,cur,la,inp,point,relative,playing):
  nonlocal frames,draws,events
  group=C.c_int32(3);area=C.c_int32(6);count=C.c_int32(41);latch=C.c_uint8(la)
  n.install(s,c,f,cur,la,group.value,area.value,count.value,photos,inp);n.record=True;n.point=list(point);n.relative=list(relative);n.playing=playing
  args=struct.pack('<3I',0xbfbb9d,0xbf9b94,0xbfbba4);n.call(0x504335,args);wantview=snapshot(*n.read());wd=n.draw_meta.copy();n.call(0x504802,args);want=snapshot(*n.read());wt=n.events.copy()
  trace=[];current=list(point)
  def snap():return snapshot(s,c,f,cur,latch.value,group.value,area.value,count.value)
  @Sound
  def sound(_,slot,err):trace.append(('sound',slot,snap()));return 1
  @Gain
  def gain(_,volume,err):trace.append(('gain',volume,snap()));return 1
  @Pointer
  def position(_,out,err):out[0],out[1]=current;return 1
  @Pointer
  def motion(_,out,err):out[0],out[1]=relative;return 1
  @Warp
  def warp(_,x,y,err):current[:]=[x,y];trace.append(('warp',x,y,snap()));return 1
  @Stop
  def stop(_,err):trace.append(('voice_stop',snap()));return 1
  @Gain
  def voice(_,volume,err):trace.extend([('voice_gain',volume,snap()),('voice_play',snap())]);return 1
  @Status
  def status(_,out,err):trace.append(('status',snap()));out[0]=playing;return 1
  @Sound
  def replace(_,group,err):trace.append(('replace',group,snap()));return 1
  @Release
  def release(_,flow,err):trace.append(('release',flow,snap()));return 1
  ops=Ops(None,sound,gain,position,motion,warp,stop,voice,status,replace,release);b=Bindings(C.pointer(c),C.pointer(f),C.pointer(cur),C.pointer(latch),photos,C.pointer(group),C.pointer(area),C.pointer(count));frame=Frame()
  def eq(actual,wanted,label):
   if actual!=wanted:
    for k,v in actual.items():
     if v!=wanted[k]:
      if k=='sprites':
       for j,(p,q) in enumerate(zip(v,wanted[k])):
        if p!=q:raise AssertionError((label,frames,k,j,p,q))
      raise AssertionError((label,frames,k,v,wanted[k]))
  assert lib.bk_selection_ui_view(C.byref(s),C.byref(b),C.byref(inp),C.byref(ops),C.byref(frame),e),e.value
  eq(snap(),wantview,'view');actual=[(d.slot,tuple(d.corners),tuple(d.uv),d.alpha) for d in frame.draws[:frame.count]];assert actual==wd,('draws',frames,actual,wd)
  assert lib.bk_selection_ui_control(C.byref(s),C.byref(b),C.byref(inp),C.byref(ops),e),(frames,e.value)
  eq(snap(),want,'control');assert len(trace)==len(wt),(frames,'events',len(trace),len(wt))
  for j,(x,y) in enumerate(zip(trace,wt)):
   assert x[:-1]==y[:-1],('event',frames,j,x[:-1],y[:-1]);eq(x[-1],y[-1],('event-state',j,x[:-1]))
  assert current==n.point
  frames+=1;draws+=len(actual);events+=len(trace);return latch.value,current
 for case in range(7000):
  width=[320,640,1001,1280,1920][case%5];special=[0,0,1,2,255][case%5];s=init(width,special)
  s.selected=rng.randrange(5);s.camera_mode=rng.choice([0,1,2]);s.info=rng.choice([1,1,100,0]);s.row=rng.randrange(6);s.column=rng.randrange(2 if special==1 else 4);s.voice_active=rng.randrange(2);s.music_volume=rng.randrange(-6000,1);s.music_mode=rng.choice([0,1,2]);s.scroll=rng.random()
  s.character_hover=rng.choice([7,9,11,13,15,99]);s.camera_hover=rng.choice([17,20,99]);s.action_hover=rng.choice([23,25,38,40,99]);s.reveal[:]=[rng.random() for _ in range(5)]
  for p in s.sprites:
   p.transform.fade=Fade(rng.random(),2,rng.randrange(6))
  for i in range(7,17):s.sprites[i].transform.scale[0]=s.reveal[(i-7)//2];s.sprites[i].uv[0]=1-s.reveal[(i-7)//2]
  c=Common();c.curtain=Fade(rng.random(),2,rng.randrange(6));c.blocked=rng.randrange(3);c.action=rng.choice([0,23,25,255]);f=Flow(0x38,1,0,0);cur=Cursor();lib.bk_menu_cursor_initialize(C.byref(cur),width,width*3//4);cur.idle=Timer(10000,rng.getrandbits(32),rng.randrange(2));cur.wanted=rng.randrange(2)
  slot=[1,27,29,31,33,35,17,20,23,25,38,40][case%12];p=s.sprites[slot];point=[p.rect[0]+p.rect[2]*.5,p.rect[1]+p.rect[3]*.5]
  if special==1 and slot not in [23,25]:point=[-100,-100]
  if case%17==0:point=[p.rect[0],p.rect[1]]
  inp=Input(rng.randrange(32),rng.getrandbits(32),rng.choice([0,.00001,1/60,.1,1,10]),width/1280,-800,-777,special,1)
  check(s,c,f,cur,rng.choice([0,7,25,255]),inp,point,[0,0] if case%3 else [2,-1],case%2)
 for special in [0,1]:
  for target in [23,25]:
   s=init(1280,special);c=Common();c.curtain=Fade(0,2,0);f=Flow(0x38,1,0,0);cur=Cursor();lib.bk_menu_cursor_initialize(C.byref(cur),1280,960);latch=0
   for tick in range(300):
    point=[830,910] if target==23 else [1100,910]
    latch,_=check(s,c,f,cur,latch,Input(int(tick==30),tick*17,1/60,1,-900,-777,special,1),point,[0,0],1)
    if f.current!=0x38:break
   assert f.current==0x50 and f.target==(2 if special else 8) if target==23 else f.target==1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,frames=frames,draws=draws,events=events,max_error=0,scope=__doc__)
 (ROOT/'local/original-selection-ui-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
