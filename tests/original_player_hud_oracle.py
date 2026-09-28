"""4c9bf0/4cbca4/4cb902/4cc32f, actual sprite constructors, transitions,
UV/color setters and43ed45 vertices. Device creation/stream lock/draw, clock,
projection and49d0eb capture are boundaries, not emulated game logic.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class Timer(C.Structure):_fields_=[('duration',C.c_uint32),('deadline',C.c_uint32),('armed',C.c_uint8)]
class Sprite(C.Structure):
 _fields_=[(x,C.c_float) for x in ['x','y','width','height','sx','sy','px','py']]+[('uv',C.c_float*4),('alpha',C.c_float),('rgb',C.c_uint32),('stage',C.c_uint8),('blink',C.c_uint8),('timer',Timer)]
class State(C.Structure):_fields_=[('sprites',Sprite*49)]+[(x,C.c_float) for x in ['indicator_x','scroll','depth','reserve','extent']]
class Input(C.Structure):
 _fields_=[('action',C.c_int32),('actions',C.c_int32*21),('trigger_kind',C.c_int32),('npc_behavior',C.c_int32),('counter',C.c_int32)]+[(x,C.c_uint8) for x in ['interface_mode','npc_in_view','menu_request','response','special_mode','prop_available','cover_available','npc_prompt']]+[('inventory',C.c_uint8*5)]
class Point(C.Structure):_fields_=[('position',C.c_int32*2),('depth',C.c_float)]
class Draw(C.Structure):_fields_=[('slot',C.c_uint),('sprite',Sprite)]
class Frame(C.Structure):_fields_=[('draws',Draw*40),('count',C.c_uint),('capture_after',C.c_uint),('capture',C.c_uint8)]
class Layout(C.Structure):_fields_=[('name',C.c_char_p)]+[(x,C.c_float) for x in ['x','y','width','height']]+[('requested',C.c_uint8)]
BASE=0x71bea8
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);u=self.u;u.mem_map(0x4000000,0x200000);self.draws=[];self.loaded=[];self.width=1280
  u.mem_write(0x53f10c,struct.pack('<I',0x300e000))
  # Return projected depth through x87, without changing the caller's stack.
  u.mem_write(0x300e300,b'\xd9\x05'+struct.pack('<I',0x300e310)+b'\xc3')
  for a in [0x4af970,0x4af97a,0x4ad8ec,0x466805,0x466814,0x43e583,0x443bb8,0x443be5,0x43e96a,0x49d0eb,0x42d56c,0x300e000]:u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def wi(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
 def wf(self,p,v):self.u.mem_write(p,struct.pack('<f',v))
 def rf(self,p):return struct.unpack('<f',self.u.mem_read(p,4))[0]
 def ri(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
 def text(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0].decode('cp932')
 def handle(self,i):return 0x4000000+i*0x400
 def verts(self,i):return 0x4100000+i*0x100
 def blank(self,i):
  h=self.handle(i);u=self.u;u.mem_write(h,bytes(0x400));self.wi(h+0x78,h+0x200);self.wi(h+0x27c,6)
  for a in [0x88,0x8c,0x90,0xfc,0x100,0x104]:self.wf(h+a,1)
  v=bytearray(192)
  for j,(x,y) in enumerate([(0,0),(1,0),(1,1),(0,1),(0,0),(1,1)]):struct.pack_into('<4fII2f',v,32*j,0,0,0,1,0xffffffff,0,x,y)
  u.mem_write(self.verts(i),bytes(v))
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.ri(sp);args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0
  if a==0x4af970:result=self.width
  elif a==0x4af97a:result=self.width*3//4
  elif a==0x4ad8ec:u.mem_write(args[0],b'assets\0')
  elif a==0x43e583:
   i=(args[0]-0x100-BASE)//0x16c;assert 0<=i<49;self.blank(i);self.wi(args[0],self.handle(i));self.loaded.append((i,self.text(args[1]).split('\\')[-1]))
  elif a==0x443bb8:
   i=(args[0]-0x4000200)//0x400;assert 0<=i<49;self.wi(args[2],self.verts(i))
  elif a==0x43e96a:
   i=(args[0]-0x4000000)//0x400;self.draws.append((i,self.sprite(i),bytes(u.mem_read(self.verts(i),192))))
  elif a==0x49d0eb:self.capture=len(self.draws)
  elif a==0x42d56c:
   u.mem_write(args[1],bytes(self.point.position));self.wf(0x300e310,self.point.depth);u.reg_write(UC_X86_REG_EIP,0x300e300);return
  elif a==0x300e000:result=self.now
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def sprite(self,i):
  b=BASE+i*0x16c;h=self.handle(i);v=self.verts(i);s=Sprite()
  for name,off in [('x',0x114),('y',0x118),('width',0x10c),('height',0x110),('sx',0x124),('sy',0x128),('px',0x11c),('py',0x120),('alpha',0x12c)]:setattr(s,name,self.rf(b+off))
  s.stage=self.u.mem_read(b+0x134,1)[0];s.blink=self.u.mem_read(b+0x160,1)[0];s.timer=Timer.from_buffer_copy(self.u.mem_read(b+0x13c,C.sizeof(Timer)))
  s.uv[:]=[self.rf(v+0x18),self.rf(v+0x1c),self.rf(v+0x58),self.rf(v+0x5c)];s.rgb=self.ri(v+0x10)&0xffffff
  return s
 def state(self):
  s=State()
  for i in range(49):
   if i!=2:s.sprites[i]=self.sprite(i)
  for name,off in [('indicator_x',0),('scroll',4),('depth',8),('reserve',12),('extent',16)]:setattr(s,name,self.rf(0x709c58+off))
  return s
 def initialize(self,g,special,width):
  self.loaded=[];self.width=width;self.wi(0x7219a8,g);self.u.mem_write(0xbef778,bytes([special]));self.wf(0x721ad0,width/1280);self.call(0x4c9bf0,b'');return self.state()
 def install(self,s):
  u=self.u
  for i in range(49):
   if i==2:continue
   p=s.sprites[i];b=BASE+i*0x16c;h=self.handle(i);self.blank(i);self.wi(b+0x100,h)
   for name,off in [('x',0x114),('y',0x118),('width',0x10c),('height',0x110),('sx',0x124),('sy',0x128),('px',0x11c),('py',0x120),('alpha',0x12c)]:self.wf(b+off,getattr(p,name))
   u.mem_write(b+0x134,bytes([p.stage]));u.mem_write(b+0x160,bytes([p.blink]));u.mem_write(b+0x148,bytes(3));u.mem_write(b+0x13c,bytes(p.timer))
   for off,v in [(0x7c,p.x),(0x80,p.y),(0xf4,p.width),(0xf8,p.height),(0x8c,p.sx),(0x90,p.sy),(0xdc,p.px),(0xe0,p.py),(0x88,p.alpha)]:self.wf(h+off,v)
   self.wi(h+0x74,1)
   for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
    v=self.verts(i)+j*32;self.wi(v+0x10,(int(p.alpha*255)<<24)|p.rgb);self.wf(v+0x18,p.uv[x]);self.wf(v+0x1c,p.uv[y])
  for name,off in [('indicator_x',0),('scroll',4),('depth',8),('reserve',12),('extent',16)]:self.wf(0x709c58+off,getattr(s,name))
 def run(self,s,inp,point,dt,now,width,outcome,update=True):
  self.install(s);u=self.u;self.now=now;self.point=point;self.draws=[];self.capture=None
  for addr,v in [(0x71b520,inp.action),(0x71bd10,inp.trigger_kind),(0x729638,inp.npc_behavior),(0x734050,inp.counter)]:self.wi(addr,v)
  u.mem_write(0x71b524,bytes(inp.actions));u.mem_write(0x71bcdc,bytes(inp.inventory))
  for addr,v in [(0x71ba89,inp.interface_mode),(0x71b828,inp.npc_in_view),(0x71bcd9,inp.menu_request),(0x71bcda,inp.response),(0xbef778,inp.special_mode),(0x71ba8b,inp.prop_available),(0x71ba8a,inp.cover_available),(0x71ba8c,inp.npc_prompt),(0x71bcd8,outcome)]:u.mem_write(addr,bytes([v]))
  self.wf(0x721ad0,width/1280);self.wf(0x733700,dt)
  if update:self.call(0x4cbca4,b'')
  middle=self.state();self.call(0x4cb902,b'')
  return middle,self.state(),u.mem_read(0x71bcd8,1)[0]

def sprite_values(s):return tuple(getattr(s,x) for x in ['x','y','width','height','sx','sy','px','py'])+tuple(s.uv)+(s.alpha,s.rgb,s.stage,s.blink,s.timer.duration,s.timer.deadline,s.timer.armed)
def same(a,b,label):
 for i in range(49):assert sprite_values(a.sprites[i])==sprite_values(b.sprites[i]),(label,'sprite',i,sprite_values(a.sprites[i]),sprite_values(b.sprites[i]))
 for x in ['indicator_x','scroll','depth','reserve','extent']:assert getattr(a,x)==getattr(b,x),(label,x,getattr(a,x),getattr(b,x))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--cases',type=int,default=6000);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4cbca4);e=C.create_string_buffer(256)
 lib.bk_player_hud_initialize.argtypes=[C.POINTER(State),C.c_uint,C.c_uint8,C.c_uint];lib.bk_player_hud_layout.argtypes=[C.POINTER(Layout),C.c_uint,C.c_uint8,C.c_uint]
 lib.bk_player_hud_update.argtypes=[C.POINTER(State),C.POINTER(Input),C.POINTER(C.c_uint8),C.POINTER(Point),C.c_float,C.c_uint32,C.c_uint,C.c_void_p]
 lib.bk_player_hud_draws.argtypes=[C.POINTER(State),C.POINTER(Input),C.c_uint,C.POINTER(Frame),C.c_void_p];lib.bk_player_hud_rect.argtypes=[C.POINTER(Sprite),C.POINTER(C.c_float)]
 s=State();loads=0
 for i in range(49):
  if i==2:continue
  s.sprites[i].timer=Timer(123,0xfffffffa,1);s.sprites[i].blink=255
 n.install(s)
 for g in range(5):
  for special in [0,1,2,255]:
   for width in [640,1001,1280]:
    want=n.initialize(g,special,width);assert lib.bk_player_hud_initialize(C.byref(s),g,special,width);same(s,want,('load',g,special,width));l=(Layout*49)();assert lib.bk_player_hud_layout(l,g,special,width)
    assert [(i,x.name.decode()) for i,x in enumerate(l) if x.name]==n.loaded;loads+=1
 samples=draws=vertices=0
 def check(s,inp,point,dt,now,width,update=True):
  nonlocal samples,draws,vertices
  outcome=C.c_uint8(rng.randrange(256));middle,wanted,wo=n.run(s,inp,point,dt,now,width,outcome.value,update)
  if update:assert lib.bk_player_hud_update(C.byref(s),C.byref(inp),C.byref(outcome),C.byref(point),dt,now,width,e),e.value
  same(s,middle,(samples,'update'));assert outcome.value==wo
  f=Frame();assert lib.bk_player_hud_draws(C.byref(s),C.byref(inp),width,C.byref(f),e),e.value;same(s,wanted,(samples,'draw'))
  assert f.count==len(n.draws);assert (f.capture_after if f.capture else None)==n.capture
  for j,(idx,p,v) in enumerate(n.draws):
   got=f.draws[j];assert got.slot==idx;assert sprite_values(got.sprite)==sprite_values(p),(samples,j,'snapshot')
   rect=(C.c_float*4)();assert lib.bk_player_hud_rect(C.byref(got.sprite),rect)
   for k,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
    wantxy=struct.unpack_from('<2f',v,32*k);assert (rect[x],rect[y])==wantxy,(samples,idx,k,tuple(rect),wantxy)
    argb=struct.unpack_from('<I',v,32*k+16)[0];assert argb==((int(p.alpha*255)<<24)|p.rgb),(samples,idx,'color',hex(argb),p.alpha,hex(p.rgb))
    vertices+=1
  draws+=f.count;samples+=1
 for case in range(a.cases):
  width=rng.choice([640,1001,1280]);s=State();lib.bk_player_hud_initialize(C.byref(s),case%5,0,width)
  for p in s.sprites:
   p.stage=rng.randrange(6);p.alpha=rng.choice([0,.5,1]);p.blink=rng.choice([0,1,2,255]);p.timer=Timer(rng.choice([0,500,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,255]));p.sx=rng.choice([.18,.37,1]);p.sy=rng.choice([.5,1]);p.px=rng.choice([0,.5]);p.py=rng.choice([0,.5])
  # Slot2 never constructed or reached in valid game counters.
  s.sprites[2]=Sprite();s.extent=rng.choice([0,.18,.985,.99,1]);s.reserve=rng.choice([0,.001,.5,.99,1]);s.scroll=rng.choice([0,.01,.5,.99,1]);s.indicator_x=rng.choice([118,250,394]);s.depth=rng.choice([-1,0,.5,1,2])
  inp=Input();inp.actions[:]=range(21) if case%7 else [rng.randrange(14) for _ in range(21)];inp.action=rng.randrange(24);inp.trigger_kind=rng.choice([0,10,11,18,19]);inp.npc_behavior=rng.choice([0,0,1,2]);inp.counter=rng.choice([0,1,9,10,11,99,100,101,999,-1,-9,-99])
  for name in ['interface_mode','npc_in_view','menu_request','response','special_mode','prop_available','cover_available','npc_prompt']:setattr(inp,name,rng.choice([0,0,1,1,2,255]))
  inp.inventory[:]=[rng.choice([0,1,2,255]) for _ in range(5)];point=Point((C.c_int32*2)(rng.randrange(-1000,2000),rng.randrange(-1000,2000)),rng.choice([-1,.5,1,2]));dt=C.c_float(rng.choice([0,1/60,.25,1,10])).value
  check(s,inp,point,dt,rng.getrandbits(32),width,case%4!=0)
 for g in range(5):
  s=State();lib.bk_player_hud_initialize(C.byref(s),g,0,1280);inp=Input();inp.actions[:]=range(21);inp.prop_available=inp.cover_available=inp.npc_prompt=1
  for tick in range(600):
   inp.npc_in_view=0 if tick<300 else 1;inp.counter=tick%140;inp.inventory[:]=[int(tick//80%2)]*5
   check(s,inp,Point((C.c_int32*2)(640,480),.5),C.c_float(1/30).value,tick*34,1280)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),loads=loads,frames=samples,draws=draws,vertices=vertices,scope=__doc__)
 (ROOT/'local/original-player-hud-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS player HUD',report)
if __name__=='__main__':main()
