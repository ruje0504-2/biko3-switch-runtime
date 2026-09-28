"""Original4e9210 five-sprite construction and51a7a2 checkpoint/save prompt.
Real fades/cursor timer/flow scheduler/4cc320 HUD reserve reset. Hooks only
resource-release services, image/GPU/audio/input/clock and release20 handover;
the actual area handover body is outside this UI component's scope.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn.x86_const import *
from unicorn import UC_HOOK_CODE
from original_pause_oracle import (Native as PauseNative, State as PauseState,Sprite,Cursor,Bindings,Ops,Input,Frame,Sound,Warp,Pointer,Release,Remove,ADDR,KEYS,ss,fs)
from original_item_notice_oracle import Fade
from original_common_hud_oracle import State as Common,Flow,Timer
from model_binding import ROOT,library
class State(C.Structure):_fields_=[('sprites',Sprite*5),('selected',C.c_int32),('loaded',C.c_uint8)]
def snapshot(s,c,f,cursor,overlay,latch,phase,reserve):
 return (tuple(ss(p) for p in s.sprites),s.selected,s.loaded,phase,reserve,fs(c.curtain),c.blocked,c.action,bytes(f),ss(cursor.sprite),(cursor.idle.duration,cursor.idle.deadline,cursor.idle.armed),cursor.wanted,overlay,latch)
class Native(PauseNative):
 def __init__(self,exe):
  super().__init__(exe)
  for addr in [0x4ec504,0x4ef018,0x512073,0x4f7185,0x4b8a01]: self.u.hook_add(UC_HOOK_CODE,self.release_asset,begin=addr,end=addr)
 def release_asset(self,u,a,z,data):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.ri(sp))
 def read(self):
  _,c,f,cursor,overlay,latch=super().read();s=State();s.sprites[:]=[self.read_sprite(i) for i in range(5)];s.selected=C.c_int32(self.ri(0xbfbbbc)).value;s.loaded=bool(self.ri(ADDR[0]+0x100));c.action=self.u.mem_read(0xbeeb7e,1)[0]
  return s,c,f,cursor,overlay,latch,self.u.mem_read(0x71ba88,1)[0],self.rf(0x709c64)
 def trace_event(self,*event):
  if self.record:self.trace.append((*event,snapshot(*self.read())))
 def hook(self,u,a,size,data):
  if a==0x4e77bf:
   sp=u.reg_read(UC_X86_REG_ESP);flow=self.ri(sp+4)&255;self.trace_event('release',flow)
   if flow==0x20:
    for p in ADDR[:5]:self.wi(p+0x100,0)
   u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.ri(sp));return
  super().hook(u,a,size,data)
 def install(self,s,c,f,cursor,overlay,latch,inp,phase=0,reserve=1):
  tmp=PauseState();tmp.loaded=s.loaded;tmp.sprites[:5]=s.sprites
  super().install(tmp,c,f,cursor,overlay,latch,inp)
  self.wi(0xbfbbbc,s.selected);self.u.mem_write(0xbeeb7e,bytes([c.action]));self.u.mem_write(0x71ba88,bytes([phase]));self.wf(0x709c64,reserve)
 def run(self,s,c,f,cursor,overlay,latch,inp,point,motion,phase,reserve):
  self.install(s,c,f,cursor,overlay,latch,inp,phase,reserve);self.point=list(point);self.motion=list(motion);self.call(0x51a7a2,b'');return snapshot(*self.read()),self.draws,self.trace

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();KEYS.update({0x33451:1,0x33452:1});n=Native(exe);lib=library();rng=random.Random(0x51a7a2);e=C.create_string_buffer(256);steps=draws=events=constructors=natural=0
 lib.bk_checkpoint_prompt_initialize.argtypes=[C.POINTER(State),C.c_uint,C.POINTER(Ops),C.c_void_p];lib.bk_checkpoint_prompt_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(C.c_uint8),C.POINTER(C.c_float),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p];lib.bk_menu_cursor_initialize.argtypes=[C.POINTER(Cursor),C.c_uint,C.c_uint];lib.bk_checkpoint_prompt_image.argtypes=[C.c_uint];lib.bk_checkpoint_prompt_image.restype=C.c_char_p
 @Warp
 def nopwarp(_,x,y,e):return 1
 dummy=Ops();dummy.warp=nopwarp
 def initialized(width):
  s=State();assert lib.bk_checkpoint_prompt_initialize(C.byref(s),width,C.byref(dummy),e);return s
 for width in [320,640,1001,1280,1920]:
  for selection in [0,1,2,3,255]:
   s=initialized(width);s.selected=selection;c=Common();c.action=4;cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);f=Flow(0x20,2,0,2);inp=Input(0,100,0,width/1280,0)
   n.install(s,c,f,cursor,0,1,inp);n.record=False;n.width=width;n.height=width*3//4;n.names=[];n.call(0x4e9210,b'');native=n.read()[0]
   assert lib.bk_checkpoint_prompt_initialize(C.byref(s),width,C.byref(dummy),e)
   assert [ss(p) for p in s.sprites]==[ss(p) for p in native.sprites],(width,ss(s.sprites[0]),ss(native.sprites[0]))
   assert s.selected==native.selected
   assert n.point==[C.c_float(496*C.c_float(width/1280).value).value,C.c_float(548*C.c_float(width/1280).value).value]
   assert len(n.names)==5
   for i,name in n.names:assert name.endswith(lib.bk_checkpoint_prompt_image(i).decode()),(i,name)
   constructors+=1
 def check(s,c,f,cursor,overlay,latch,inp,point,motion,phase,reserve):
  nonlocal steps,draws,events
  want,wd,wt=n.run(s,c,f,cursor,overlay,latch,inp,point,motion,phase,reserve);trace=[];current=list(point);ov=C.c_uint8(overlay);la=C.c_uint8(latch);ph=C.c_uint8(phase);re=C.c_float(reserve)
  def snap():return snapshot(s,c,f,cursor,ov.value,la.value,ph.value,re.value)
  @Sound
  def sound(_,slot,e):trace.append(('sound',slot,snap()));return 1
  @Warp
  def warp(_,x,y,e):current[:]=[x,y];trace.append(('warp',x,y,snap()));return 1
  @Pointer
  def pointer(_,p,m,e):p[0],p[1]=current;m[0],m[1]=motion;return 1
  @Release
  def release(_,flow,e):trace.append(('release',flow,snap()));return 1
  ops=Ops(None,sound,warp,pointer,release,Remove());b=Bindings(C.pointer(c),C.pointer(f),C.pointer(cursor),C.pointer(ov),C.pointer(la));frame=Frame()
  assert lib.bk_checkpoint_prompt_step(C.byref(s),C.byref(b),C.byref(ph),C.byref(re),C.byref(inp),C.byref(ops),C.byref(frame),e),e.value
  got=snap()
  assert got==want,(steps,'state',[(i,a,b) for i,(a,b) in enumerate(zip(got,want)) if a!=b])
  assert trace==wt,(steps,'trace',trace,wt)
  gotdraw=[(d.slot,tuple(d.rect),d.alpha) for d in frame.draws[:frame.count]];assert gotdraw==wd,(steps,'draws',gotdraw,wd)
  steps+=1;draws+=len(wd);events+=len(wt)
  return ov.value,la.value,current
 def rf():return Fade(rng.choice([0,.25,.5,.75,1]),rng.choice([0,.1,2,10]),rng.randrange(5))
 for case in range(10000):
  width=rng.choice([320,640,1001,1280]);s=initialized(width)
  for p in s.sprites:p.fade=rf()
  s.selected=rng.choice([0,1,2]);s.loaded=case%9!=0
  c=Common();c.curtain=rf();c.blocked=rng.choice([0,0,1,1,2,255]);c.action=rng.choice([0,1,3,99]);f=Flow(0x20,rng.randrange(256),rng.randrange(256),rng.randrange(256));cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);cursor.sprite.fade=rf();cursor.idle=Timer(rng.choice([0,10000,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,255]));cursor.wanted=rng.choice([0,1,2,255])
  inp=Input(rng.randrange(32),rng.getrandbits(32),rng.choice([0,1/60,.25,1,10]),width/1280,0)
  r=s.sprites[rng.choice([1,3])].rect;point=[C.c_float(r[0]+rng.choice([0,r[2]/2,r[2],r[2]+1])).value,C.c_float(r[1]+rng.choice([0,r[3]/2,r[3],r[3]+1])).value];motion=rng.choice([[0,0],[1,0],[0,1],[-1,-1]])
  check(s,c,f,cursor,rng.choice([0,1,255]),rng.choice([0,1,255]),inp,point,motion,rng.randrange(256),rng.choice([0,.1,.5,.9,1,2]))
 for choice in [0,1]:
  for width in [320,640,1001,1280,1920]:
   s=initialized(width);c=Common();c.curtain=Fade(1,2,3);f=Flow(0x20,2,0x20,2);cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);point=[C.c_float(496*width/1280).value,C.c_float(548*width/1280).value];ov=la=0
   for tick in range(100):
    buttons=1 if tick==24 else 16 if tick==20 and choice else 0
    ov,la,point=check(s,c,f,cursor,ov,la,Input(buttons,tick*250,.25,width/1280,0),point,[0,0],2,.4);natural+=1
    if f.current!=0x20:break
   assert f.current==0x50 and f.target==(2 if choice else 0x28) and f.mode==(3 if choice else 0),(choice,f.current,f.target,f.mode)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,steps=steps,draws=draws,events=events,natural_frames=natural,scope=__doc__)
 (ROOT/'local/original-checkpoint-prompt-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS checkpoint-prompt',report)
if __name__=='__main__':main()
