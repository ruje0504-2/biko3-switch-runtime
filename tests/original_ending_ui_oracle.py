"""Original4ccabd..4ce7a9 constructors and full4d901c toolbar motion.
Textures/device creation and held-key reads are substituted. Actual setters,
transition functions, rotation and geometry execute original instructions.
"""
import json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
from original_player_hud_oracle import Native as CaptureBase,Sprite,Timer
from original_effect_sprite_oracle import Native as EffectNative, Effect, bind as effect_bind, snapshot as effect_snapshot
import ctypes as C, hashlib, random
from model_binding import ROOT,library
BASE=0x734058
COUNT=(0x739880-BASE)//0x16c+1
class Native(EffectNative):
 def __init__(self,exe):
  CaptureBase.__init__(self,exe);self.created=[];self.paths=[]
  self.u.hook_add(UC_HOOK_CODE,self.observe,begin=0x50de40,end=0x50de40)
 def observe(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<10I',u.mem_read(sp+4,40));slot=(args[0]-BASE)//0x16c
  self.created.append((slot,self.text(args[2]),list(struct.unpack('<4f',u.mem_read(sp+16,16))),list(args[7:])))
 def hook(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<4I',u.mem_read(sp+4,16));ret=self.ri(sp)
  if a==0x43e583:
   i=(args[0]-0x100-BASE)//0x16c;assert 0<=i<COUNT;self.blank(i);self.wi(args[0],self.handle(i));self.loaded.append((i,self.text(args[1]).split('\\')[-1]))
  elif a==0x443bb8:
   i=(args[0]-0x4000200)//0x400;assert 0<=i<COUNT;self.wi(args[2],self.verts(i))
  elif a==0x43e96a:
   i=(args[0]-0x4000000)//0x400;self.draws.append((i,self.sprite(i),bytes(u.mem_read(self.verts(i),192))))
  elif a==0x4ad8ec:
   self.paths.append((self.text(args[1]) if args[1] else None,self.text(args[2]) if args[2] else None));u.mem_write(args[0],b'assets\0')
  else:return CaptureBase.hook(self,u,a,size,user)
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def sprite(self,i):
  b=BASE+i*0x16c;v=self.verts(i);s=Sprite()
  for name,off in [('x',0x114),('y',0x118),('width',0x10c),('height',0x110),('sx',0x124),('sy',0x128),('px',0x11c),('py',0x120),('alpha',0x12c)]:setattr(s,name,self.rf(b+off))
  s.stage=self.u.mem_read(b+0x134,1)[0];s.blink=self.u.mem_read(b+0x160,1)[0];s.timer=Timer.from_buffer_copy(self.u.mem_read(b+0x13c,12));s.uv[:]=[self.rf(v+0x18),self.rf(v+0x1c),self.rf(v+0x58),self.rf(v+0x5c)];s.rgb=self.ri(v+0x10)&0xffffff;return s
 def initialize(self,width=1280):
  self.loaded=[];self.created=[];self.paths=[];self.width=width;self.wf(0x721ad0,width/1280);self.u.mem_write(0x5767c8,b'\1')
  self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x1000);self.u.reg_write(UC_X86_REG_FPCW,0x37f);self.u.emu_start(0x4ccabd,0x4ce7a9,count=3000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x4ce7a9
  return [(i,name,rect,modes,self.rf(BASE+i*0x16c+0x138),self.sprite(i).stage,list((self.sprite(i).px,self.sprite(i).py))) for i,name,rect,modes in self.created]

class UiSprite(C.Structure):
 _fields_=[('transform',Effect),('rect',C.c_float*4),('uv',C.c_float*4),('rgb',C.c_uint32),('timer',Timer)]
class Draw(C.Structure):
 _fields_=[('slot',C.c_uint),('xy',C.c_float*8),('uv',C.c_float*4),('alpha',C.c_float),('rgb',C.c_uint32)]
class Ui(C.Structure):
 _fields_=[('sprites',UiSprite*COUNT),('loaded',C.c_uint64)]
class Rect(C.Structure):
 _fields_=[(x,C.c_float) for x in ['x','y','width','height']]
KEY=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint,C.POINTER(C.c_uint32),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('key',KEY)]

def bind(lib):
 effect_bind(lib)
 lib.bk_ending_ui_sprite_step.argtypes=[C.POINTER(Ui),C.c_uint,C.c_float,C.POINTER(Draw),C.c_void_p]
 lib.bk_ending_ui_initialize.argtypes=[C.POINTER(Ui),C.c_uint,C.POINTER(C.c_uint8),C.POINTER(C.c_float),C.c_void_p]
 lib.bk_ending_ui_image.argtypes=[C.c_uint];lib.bk_ending_ui_image.restype=C.c_char_p
 lib.bk_ending_ui_hover.argtypes=[C.POINTER(Ui),C.POINTER(C.c_int32),C.POINTER(C.c_int32),C.POINTER(C.c_float),C.c_float,C.c_float,C.POINTER(Ops),C.POINTER(C.c_uint8),C.c_void_p]
 lib.bk_ending_ui_control_rects.argtypes=[C.POINTER(Ui),C.POINTER(Rect)]

def install(n,s):
 for i,p in enumerate(s.sprites):
  n.install(BASE+i*0x16c,i,p.transform,p.rect)
  n.u.mem_write(BASE+i*0x16c+0x13c,bytes(p.timer))
  for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
   n.wi(n.verts(i)+32*j+16,(int(p.transform.fade.alpha*255)<<24)|p.rgb)
   n.wf(n.verts(i)+32*j+24,p.uv[x]);n.wf(n.verts(i)+32*j+28,p.uv[y])

def same(n,s,label):
 for i,p in enumerate(s.sprites):
  a=BASE+i*0x16c;old=n.sprite(i);want=n.read(a,i)
  assert effect_snapshot(p.transform)==effect_snapshot(want),(label,i,effect_snapshot(p.transform),effect_snapshot(want))
  assert tuple(p.rect)==(old.x,old.y,old.width,old.height),(label,i,'rect',tuple(p.rect),old.x,old.y,old.width,old.height)
  assert tuple(p.uv)==tuple(old.uv) and p.rgb==old.rgb,(label,i,'uv/color')
  assert (p.timer.duration,p.timer.deadline,p.timer.armed)==(old.timer.duration,old.timer.deadline,old.timer.armed),(label,i,'timer')

def main():
 import argparse
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256);rng=random.Random(0x4ccabd)
 s=Ui();s.loaded=1<<8
 for i,p in enumerate(s.sprites):
  p.rect[:]=[10+i,20+i,64,32];assert lib.bk_effect_sprite_initialize(C.byref(p.transform),p.rect,1,1,0)
  p.transform.direction=[0,1,2,255][i%4];p.transform.motion[:]=[i*.007,-i*.003];p.transform.degrees=i*1.3;p.transform.radians=i*.002
  p.timer=Timer(333,0xfffffffa,i%2);p.uv[:]=[.1,.2,.8,.9];p.rgb=0x123456
 flags=(C.c_uint8*6)(*[255]*6);gauge=C.c_float(-99);loads=0
 for width in [1,320,640,961,1001,1280,1920,16384]*3:
  install(n,s)
  for i in range(57,63):n.u.mem_write(BASE+i*0x16c+0x167,b'\xff')
  n.initialize(width)
  assert lib.bk_ending_ui_initialize(C.byref(s),width,flags,C.byref(gauge),e),e.value
  same(n,s,('load',width));assert gauge.value==n.rf(0x721e24)
  assert list(flags)==[n.u.mem_read(BASE+i*0x16c+0x167,1)[0] for i in range(57,63)]==[0]*6
  assert s.loaded==(1<<63)-1
  assert [(i,lib.bk_ending_ui_image(i).decode()) for i in range(COUNT) if lib.bk_ending_ui_image(i)]==n.loaded
  assert len(n.created)==62 and n.paths==[('\\bk3_00.pp',None)]
  loads+=1
 # Continuous real layout animations, paired against original full50e6ba.
 frames=vertices=0
 for width in [640,1001,1280]:
  assert lib.bk_ending_ui_initialize(C.byref(s),width,flags,C.byref(gauge),e)
  install(n,s)
  for tick in range(240):
   dt=C.c_float([0,1/60,.1,.5][(tick//60)%4]).value;n.wf(0x733700,dt)
   for i,p in enumerate(s.sprites):
    if i==8:continue
    wanted={1:1,70:0,80:1,130:1,180:0,210:1}.get(tick)
    if wanted is not None:
     n.call(0x50e633,struct.pack('<II',BASE+i*0x16c,wanted));assert lib.bk_fade_sprite_request(C.byref(p.transform.fade),wanted)
    n.draws=[];n.call(0x50e6ba,struct.pack('<II',BASE+i*0x16c,0));d=Draw();assert lib.bk_ending_ui_sprite_step(C.byref(s),i,dt,C.byref(d),e),e.value
    q=d.xy;assert d.slot==i and d.alpha==p.transform.fade.alpha and d.rgb==p.rgb and tuple(d.uv)==tuple(p.uv)
    assert len(n.draws)==1
    for j,k in enumerate([0,1,2,3,0,2]):
     assert tuple(q[2*k:2*k+2])==struct.unpack_from('<2f',n.draws[0][2],32*j),(width,tick,i,j)
     vertices+=1
   same(n,s,('step',width,tick));frames+=1
 #4d901c: record independent native key call order/AL and compare live rects.
 n.keys=[0,0];n.trace=[]
 def key_hook(u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);code,mode,third=struct.unpack('<3I',u.mem_read(sp+4,12));n.trace.append((code,mode,third))
  assert mode==2 and third==0 and code in [0,1]
  u.reg_write(UC_X86_REG_EAX,n.keys[code]);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,n.ri(sp))
 n.u.hook_add(UC_HOOK_CODE,key_hook,begin=0x4b76c2,end=0x4b76c2)
 hover=keycalls=0
 for case in range(6000):
  scale=C.c_float(rng.choice([.5,.75078125,1,1.3333333])).value;dt=C.c_float(rng.choice([0,1/60,.1,.5,1,10])).value
  assert lib.bk_ending_ui_initialize(C.byref(s),1280,flags,C.byref(gauge),e)
  for i in range(12,50):s.sprites[i].rect[0]=rng.uniform(800,1500)*scale
  panel=s.sprites[49].rect;panel[1]=rng.uniform(-50,50);panel[3]=rng.uniform(0,1000)
  x=rng.choice([1096*scale,1280*scale,1200*scale,1095*scale,1281*scale]);y=rng.choice([panel[1],panel[1]+panel[3],panel[1]+panel[3]*.5,-9999])
  ptr=(C.c_float*2)(x,y);op=C.c_int32(rng.choice([0,1,2,-1]));request=C.c_int32(rng.choice([0,1,2,-1]));visible=C.c_uint8(99)
  n.keys=[rng.choice([0,1,255,256,257,0xffffffff]) for _ in range(2)];n.trace=[];install(n,s)
  n.wi(0x72210c,op.value);n.wi(0x722110,request.value);n.wf(0x721ad0,scale);n.wf(0x733700,dt)
  n.call(0x4d901c,struct.pack('<ffI',*ptr,0x3000100));trace=[]
  @KEY
  def key(_,code,mode,out,err):trace.append((code,mode,0));out[0]=n.keys[code];return 1
  assert lib.bk_ending_ui_hover(C.byref(s),C.byref(op),C.byref(request),ptr,scale,dt,C.byref(Ops(None,key)),C.byref(visible),e),e.value
  assert trace==n.trace and visible.value==n.u.mem_read(0x3000100,1)[0] and op.value==C.c_int32(n.ri(0x72210c)).value,(case,trace,n.trace,visible.value,op.value)
  same(n,s,('hover',case));rects=(Rect*13)();assert lib.bk_ending_ui_control_rects(C.byref(s),rects)
  from original_ending_control_oracle import RECT_ADDR
  for r,addr in zip(rects,RECT_ADDR):assert (r.width,r.height,r.x,r.y)==struct.unpack('<4f',n.u.mem_read(addr-8,16))
  hover+=1;keycalls+=len(trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),loads=loads,loaded_sprites=loads*62,frames=frames,vertices=vertices,hover_frames=hover,key_calls=keycalls,max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-ui-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
