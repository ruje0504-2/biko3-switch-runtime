"""Complete5158a0 construction,51A76D update+UI,51A78E backdrop and original
sprite fades, timer and51c47e. Hooks are GPU resources/submission, key/pointer,
clock, audio restart, scene release and output DeleteFileA. No policy hook.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_player_hud_oracle import Native as HudNative
from original_item_notice_oracle import Fade
from original_common_hud_oracle import State as Common,Flow,Timer
from model_binding import ROOT,library
class Sprite(C.Structure):_fields_=[('fade',Fade),('rect',C.c_float*4)]
class State(C.Structure):_fields_=[('sprites',Sprite*20),('cursor',C.c_float*2),('motion',C.c_float*2)]+[(x,C.c_int32) for x in ['row','hover','column','confirm_hover','action','page']]+[('background_show',C.c_uint8),('loaded',C.c_uint8)]
class Cursor(C.Structure):_fields_=[('sprite',Sprite),('idle',Timer),('wanted',C.c_uint8)]
class Bindings(C.Structure):_fields_=[('common',C.POINTER(Common)),('flow',C.POINTER(Flow)),('cursor',C.POINTER(Cursor)),('overlay',C.POINTER(C.c_uint8)),('latch',C.POINTER(C.c_uint8))]
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Warp=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_float,C.c_float,C.c_void_p)
Pointer=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_void_p)
Release=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_void_p)
Remove=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('sound',Sound),('warp',Warp),('pointer',Pointer),('release',Release),('remove',Remove)]
class Input(C.Structure):_fields_=[('buttons',C.c_uint32),('now',C.c_uint32),('seconds',C.c_float),('scale',C.c_float),('special',C.c_uint8)]
class Draw(C.Structure):_fields_=[('slot',C.c_uint),('rect',C.c_float*4),('alpha',C.c_float)]
class Frame(C.Structure):_fields_=[('count',C.c_uint),('draws',Draw*20)]
ADDR=[0x734058+i*0x16c for i in range(20)]+[0xb537e8,0xbeea18]
KEYS={0:1,0x5a:1,0x33450:1,0x26:2,0x30d40:2,0x28:4,0x30d41:4,0x25:8,0x30d42:8,0x27:16,0x30d43:16}
def fs(f):return (f.alpha,f.speed,f.stage)
def ss(s):return (fs(s.fade),tuple(s.rect))
def snapshot(s,c,f,cursor,overlay,latch):
 return (tuple(ss(p) for p in s.sprites),tuple(s.cursor),tuple(s.motion),tuple(getattr(s,x) for x in ['row','hover','column','confirm_hover','action','page','background_show','loaded']),fs(c.curtain),c.blocked,bytes(f),ss(cursor.sprite),(cursor.idle.duration,cursor.idle.deadline,cursor.idle.armed),cursor.wanted,overlay,latch)
class Native(HudNative):
 def __init__(self,exe):
  Base.__init__(self,exe);u=self.u;u.mem_map(0x4000000,0x200000);self.wi(0x53f10c,0x300e000);self.wi(0x53f108,0x300e100);u.mem_write(0x5767c8,b'\x01')
  for slot in range(8):self.wi(0xbeee10+slot*0x120,100+slot)
  for a in [0x428bb6,0x4af970,0x4af97a,0x4ad8ec,0x466805,0x466814,0x43e583,0x43ed45,0x43e96a,0x443bb8,0x443be5,0x4b768d,0x4b76c2,0x46435e,0x4e77bf,0x4b75aa,0x4b757e,0x300e000,0x300e100]:u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  def bad(u,access,a,size,value,_):
   print('VM invalid',hex(u.reg_read(UC_X86_REG_EIP)),hex(a),'events',[t[0] for t in self.trace[-4:]],flush=True);return False
  u.hook_add(UC_HOOK_MEM_INVALID,bad)
  self.width=640;self.height=480;self.draws=[];self.trace=[];self.point=[0.,0.];self.motion=[0.,0.];self.now=0;self.buttons=0;self.record=False
 def read_sprite(self,i):
  a=ADDR[i];return Sprite(Fade(self.rf(a+0x12c),self.rf(a+0x138),self.u.mem_read(a+0x134,1)[0]),(C.c_float*4)(*[self.rf(a+o) for o in [0x114,0x118,0x10c,0x110]]))
 def read(self):
  s=State();s.sprites[:]=[self.read_sprite(i) for i in range(20)];s.cursor[:]=[self.rf(0xbf9b64),self.rf(0xbf9b68)];s.motion[:]=[self.rf(0xbf9b84),self.rf(0xbf9b88)]
  for i,name in enumerate(['row','hover','column','confirm_hover','action','page']):setattr(s,name,C.c_int32(self.ri(0xbf9b6c+i*4)).value)
  s.background_show=self.u.mem_read(0x7341bf,1)[0];s.loaded=bool(self.ri(ADDR[0]+0x100));c=Common();c.curtain=self.read_sprite(21).fade;c.blocked=self.u.mem_read(0xbeeb7f,1)[0]
  f=Flow(*(self.u.mem_read(a,1)[0] for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]));cursor=Cursor(self.read_sprite(20),Timer.from_buffer_copy(self.u.mem_read(0xb53924,C.sizeof(Timer))),self.u.mem_read(0xb5394f,1)[0]);overlay=self.u.mem_read(0x725d38,1)[0];latch=self.u.mem_read(0xbef178,1)[0]
  return s,c,f,cursor,overlay,latch
 def trace_event(self,*event):
  if self.record:self.trace.append((*event,snapshot(*self.read())))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.ri(sp);args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0;pop=4
  if a==0x428bb6:result=1
  elif a==0x4af970:result=self.width
  elif a==0x4af97a:result=self.height
  elif a==0x4ad8ec:u.mem_write(args[0],('port'+self.text(args[1])).encode()+b'\0')
  elif a==0x43e583:
   i=ADDR.index(args[0]-0x100);self.blank(i);self.wi(args[0],self.handle(i));self.names.append((i,self.text(args[1]).split('\\')[-1]))
  elif a==0x443bb8:
   i=(args[0]-0x4000200)//0x400;assert 0<=i<22;self.wi(args[2],self.verts(i))
  elif a==0x43e96a:
   i=(args[0]-0x4000000)//0x400;p=self.read_sprite(i);self.draws.append((i,tuple(p.rect),p.fade.alpha))
  elif a==0x4b768d:
   self.point=list(struct.unpack('<2f',u.mem_read(sp+4,8)));self.trace_event('warp',*self.point)
  elif a==0x4b76c2:
   assert args[1:3]==(1,0) and args[0] in KEYS;result=bool(self.buttons&KEYS[args[0]])
  elif a==0x46435e:
   assert args[1]==0;self.trace_event('sound',args[0]-100)
  elif a==0x4e77bf:
   flow=args[0]&255;self.trace_event('release',flow)
   if flow==4:
    for p in ADDR[:20]:self.wi(p+0x100,0)
  elif a==0x4b75aa:
   for p,v in zip(args,self.point):self.wf(p,v)
  elif a==0x4b757e:
   for p,v in zip(args,self.motion):self.wf(p,v)
  elif a==0x300e000:result=self.now
  elif a==0x300e100:
   assert self.text(args[0])=='port\\sy_99.bmp';self.trace_event('remove');result=1;pop=8
  u.reg_write(UC_X86_REG_EAX,int(result));u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def install_sprite(self,i,p,loaded):
  a=ADDR[i];self.blank(i);self.u.mem_write(a,bytes(0x16c));self.wi(a+0x100,self.handle(i) if loaded else 0)
  for off,v in [(0x12c,p.fade.alpha),(0x138,p.fade.speed),(0x114,p.rect[0]),(0x118,p.rect[1]),(0x10c,p.rect[2]),(0x110,p.rect[3])]:self.wf(a+off,v)
  self.u.mem_write(a+0x134,bytes([p.fade.stage]));self.u.mem_write(a+0x148,bytes([0,0,0] if i==19 else [1,1,0]))
 def install(self,s,c,f,cursor,overlay,latch,inp):
  self.draws=[];self.trace=[];self.now=inp.now;self.buttons=inp.buttons;self.wf(0x721ad0,inp.scale);self.wf(0x733700,inp.seconds);self.u.mem_write(0xbef778,bytes([inp.special]))
  for i,p in enumerate(s.sprites):self.install_sprite(i,p,s.loaded)
  self.install_sprite(20,cursor.sprite,1);self.install_sprite(21,Sprite(c.curtain,(C.c_float*4)(0,0,C.c_float(1280*inp.scale).value,C.c_float(960*inp.scale).value)),1)
  for i,name in enumerate(['row','hover','column','confirm_hover','action','page']):self.wi(0xbf9b6c+i*4,getattr(s,name))
  for a,v in [(0xbeeb84,f.current),(0x721ad4,f.previous),(0xbfbbb9,f.target),(0xbfbb9c,f.mode),(0xbeeb7f,c.blocked),(0x725d38,overlay),(0xbef178,latch),(0x7341bf,s.background_show),(0xb5394f,cursor.wanted)]:self.u.mem_write(a,bytes([v]))
  for a,v in zip([0xbf9b64,0xbf9b68,0xbf9b84,0xbf9b88],[*s.cursor,*s.motion]):self.wf(a,v)
  self.u.mem_write(0xb53924,bytes(cursor.idle));self.record=True
 def run(self,s,c,f,cursor,overlay,latch,inp,point,motion):
  self.install(s,c,f,cursor,overlay,latch,inp);self.point=list(point);self.motion=list(motion);self.call(0x51a76d,b'');return snapshot(*self.read()),self.draws,self.trace

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x5162d9);e=C.create_string_buffer(256);steps=draws=events=constructors=backdrops=natural=0
 lib.bk_pause_initialize.argtypes=[C.POINTER(State),C.c_uint,C.POINTER(Ops),C.c_void_p];lib.bk_pause_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p];lib.bk_menu_cursor_initialize.argtypes=[C.POINTER(Cursor),C.c_uint,C.c_uint];lib.bk_pause_backdrop.argtypes=[C.POINTER(State),C.POINTER(Frame),C.c_void_p];lib.bk_pause_image.argtypes=[C.c_uint];lib.bk_pause_image.restype=C.c_char_p
 @Warp
 def nopwarp(_,x,y,e):assert (x,y)==(320,240);return 1
 dummy=Ops();dummy.warp=nopwarp
 def initialized(width):
  s=State();assert lib.bk_pause_initialize(C.byref(s),width,C.byref(dummy),e);return s
 for width in [320,640,1001,1280,1920]:
  for page in range(4):
   s=initialized(width);s.page=page;s.action=3;s.cursor[:]=[40,50];s.row=4;s.column=1
   c=Common();cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);f=Flow(4,2,0,0);inp=Input(0,100,0,width/1280,0)
   n.install(s,c,f,cursor,0,1,inp);n.record=False;n.width=width;n.height=width*3//4;n.names=[];n.call(0x5158a0,b'');native=n.read()[0]
   assert lib.bk_pause_initialize(C.byref(s),width,C.byref(dummy),e)
   assert [(ss(p)) for p in s.sprites]==[ss(p) for p in native.sprites]
   assert snapshot(s,c,f,cursor,0,1)[1:4]==snapshot(native,c,f,cursor,0,1)[1:4]
   for i,name in n.names:assert name.endswith(lib.bk_pause_image(i).decode()),(i,name)
   assert len(n.names)==20;constructors+=1
 def check(s,c,f,cursor,overlay,latch,inp,point,motion):
  nonlocal steps,draws,events
  want,wd,wt=n.run(s,c,f,cursor,overlay,latch,inp,point,motion);trace=[];current=list(point);ov=C.c_uint8(overlay);la=C.c_uint8(latch)
  def snap():return snapshot(s,c,f,cursor,ov.value,la.value)
  @Sound
  def sound(_,slot,e):trace.append(('sound',slot,snap()));return 1
  @Warp
  def warp(_,x,y,e):current[:]=[x,y];trace.append(('warp',x,y,snap()));return 1
  @Pointer
  def pointer(_,p,m,e):p[0],p[1]=current;m[0],m[1]=motion;return 1
  @Release
  def release(_,flow,e):trace.append(('release',flow,snap()));return 1
  @Remove
  def remove(_,e):trace.append(('remove',snap()));return 1
  ops=Ops(None,sound,warp,pointer,release,remove);b=Bindings(C.pointer(c),C.pointer(f),C.pointer(cursor),C.pointer(ov),C.pointer(la));frame=Frame()
  assert lib.bk_pause_step(C.byref(s),C.byref(b),C.byref(inp),C.byref(ops),C.byref(frame),e),e.value
  assert snap()==want,(steps,'state',snap(),want)
  assert trace==wt,(steps,'trace',trace,wt)
  got=[(d.slot,tuple(d.rect),d.alpha) for d in frame.draws[:frame.count]];assert got==wd,(steps,'draws',got,wd)
  steps+=1;draws+=len(wd);events+=len(wt)
  return ov.value,la.value,current
 def rf():return Fade(rng.choice([0,.25,.5,.75,1]),rng.choice([0,.1,2,10]),rng.randrange(6))
 for case in range(14000):
  width=rng.choice([320,640,1001,1280]);s=initialized(width)
  for p in s.sprites:p.fade=rf()
  s.page=rng.randrange(4);s.row=rng.randrange(5);s.column=rng.randrange(2);s.hover=rng.choice([2,3,4,5,6,99]);s.confirm_hover=rng.choice([14,16,99]);s.action=rng.choice([0,0,0,0,2,3,4,14,99]);s.background_show=rng.choice([0,1,2,255]);s.loaded=case%9!=0
  selected=rng.choice([2,3,4,5,6,14,16]);r=s.sprites[selected].rect;s.cursor[:]=[r[0]+rng.choice([0,r[2]/2,r[2],r[2]+1]),r[1]+r[3]/2];s.motion[:]=[7,8]
  c=Common();c.curtain=rf();c.blocked=rng.choice([0,0,0,1,2,255]);f=Flow(4,rng.randrange(256),rng.randrange(256),rng.randrange(256));cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);cursor.sprite.fade=rf();cursor.idle=Timer(rng.choice([0,10000,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,255]));cursor.wanted=rng.choice([0,1,2,255])
  inp=Input(rng.randrange(32),rng.getrandbits(32),rng.choice([0,1/60,.25,1,10]),width/1280,rng.choice([0,1,2,255]));point=[C.c_float(rng.uniform(0,width)).value,C.c_float(rng.uniform(0,width*.75)).value];motion=rng.choice([[0,0],[1,0],[0,1],[-1,-1]])
  check(s,c,f,cursor,rng.choice([0,1,2,255]),rng.choice([0,1,2,255]),inp,point,motion)
 # Separate original51a78e immediate screenshot sprite. Native instant alpha
 # setters dereference a null GPU handle; unloaded CPU suppression is a
 # deliberate defined policy tested separately, not an equivalence claim.
 for case in range(600):
  s=initialized(640);s.sprites[19].fade=rf();s.loaded=1;c=Common();cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),640,480);f=Flow(4,2,0,0);inp=Input(0,0,0,.5,0)
  n.install(s,c,f,cursor,0,0,inp);n.call(0x51a78e,b'');wanted=n.read()[0]
  frame=Frame();assert lib.bk_pause_backdrop(C.byref(s),C.byref(frame),e)
  assert ss(s.sprites[19])==ss(wanted.sprites[19]);assert [(d.slot,tuple(d.rect),d.alpha) for d in frame.draws[:frame.count]]==n.draws;backdrops+=1
 # Natural opening -> selection -> fade -> release; no phase/stage forcing.
 for choice in range(2,7):
  for special in [0,1,2]:
   s=initialized(640);c=Common();c.curtain=Fade(0,2,0);f=Flow(4,2,0,0);cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),640,480)
   r=s.sprites[choice].rect;point=[r[0]+r[2]/2,r[1]+r[3]/2];ov=la=0
   for tick in range(80):
    buttons=1 if tick==8 else 16 if tick==16 and choice>=5 else 1 if tick==18 and choice>=5 else 0
    ov,la,point=check(s,c,f,cursor,ov,la,Input(buttons,tick*250,.25,.5,special),point,[0,0]);natural+=1
    if f.current!=4:break
   assert f.current==(2 if choice==4 else 0x50),(choice,special,f.current)
   if choice!=4:assert f.target=={2:0x28,3:0x30,5:1,6:0x58}[choice]
 report=dict(backdrops=backdrops,natural_frames=natural,passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,steps=steps,draws=draws,events=events,scope=__doc__)
 (ROOT/'local/original-pause-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS pause',report)
if __name__=='__main__':main()
