"""Actual UI portions of4d00fa/4d1025/4d2320/4d39e6 and final-image load.
Original constructors,setters,50e6ba and geometry run unchanged; only device
IO is substituted. Shared cursor8 and all13 stage slots retain scalar caches
across selective release/reload. This is not full4d499b or a whole loader.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
import original_ending_ui_oracle as m
from original_effect_sprite_oracle import snapshot
from model_binding import ROOT,library
m.COUNT=75
SEGMENTS={1:(0x4d0103,0x4d0490),2:(0x4d104e,0x4d1503),3:(0x4d2329,0x4d26a0),4:(0x4d39ef,0x4d3b41),5:(0x4d93f8,0x4d94ae)}
RELEASE={1:(0x4d0fa8,0x4d1003),2:(0x4d2289,0x4d22fe),3:(0x4d3976,0x4d39c4),4:(0x4d45cf,0x4d45e9)}
class Stage(C.Structure):_fields_=[('sprites',m.UiSprite*12),('images',C.c_char_p*13),('loaded',C.c_uint16)]
def sprite(base,stage,slot):return base.sprites[slot] if slot<63 else stage.sprites[slot-63]
def bind(lib):
 m.bind(lib)
 lib.bk_ending_stage_ui_initialize.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.c_int,C.c_uint,C.c_int32,C.c_uint,C.c_void_p]
 lib.bk_ending_stage_ui_release.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.c_int,C.c_void_p]
 lib.bk_ending_stage_ui_image.argtypes=[C.POINTER(Stage),C.c_uint];lib.bk_ending_stage_ui_image.restype=C.c_char_p
 lib.bk_ending_stage_ui_sprite_step.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.c_uint,C.c_float,C.POINTER(m.Draw),C.c_void_p]
class Native(m.Native):
 def __init__(self,exe):
  super().__init__(exe);self.freed=[];self.u.hook_add(UC_HOOK_CODE,self.free,begin=0x43e91b,end=0x43e91b)
 def free(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);self.freed.append((self.ri(sp+4)-0x4000000)//0x400);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.ri(sp))
 def segment(self,begin,end):
  self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x2000);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
  self.u.emu_start(begin,end,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==end
 def load_stage(self,kind,g,v,width):
  self.width=width;self.wf(0x721ad0,width/1280);self.wi(0x721e04,v);self.u.mem_write(0x5767c8,b'\1');self.u.mem_write(0x721b3c,bytes([g]));self.created=[];self.loaded=[];self.paths=[];self.segment(*SEGMENTS[kind])

def install(n,base,stage):
 for slot in range(75):
  p=sprite(base,stage,slot);a=m.BASE+slot*0x16c;n.install(a,slot,p.transform,p.rect);n.u.mem_write(a+0x13c,bytes(p.timer))
  for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
   n.wi(n.verts(slot)+32*j+16,(int(p.transform.fade.alpha*255)<<24)|p.rgb);n.wf(n.verts(slot)+32*j+24,p.uv[x]);n.wf(n.verts(slot)+32*j+28,p.uv[y])
  if not ((base.loaded>>slot)&1 if slot<63 else (stage.loaded>>(slot-62))&1):n.wi(a+0x100,0)
def same(n,base,stage,label):
 for slot in range(75):
  p=sprite(base,stage,slot);old=n.sprite(slot);want=n.read(m.BASE+slot*0x16c,slot)
  assert snapshot(p.transform)==snapshot(want),(label,slot,snapshot(p.transform),snapshot(want))
  assert tuple(p.rect)==(old.x,old.y,old.width,old.height),(label,slot,'rect',tuple(p.rect),old.x,old.y,old.width,old.height)
  assert tuple(p.uv)==tuple(old.uv) and p.rgb==old.rgb,(label,slot,'UV')
  assert (p.timer.duration,p.timer.deadline,p.timer.armed)==(old.timer.duration,old.timer.deadline,old.timer.armed)
  loaded=(base.loaded>>slot)&1 if slot<63 else (stage.loaded>>(slot-62))&1
  assert bool(loaded)==bool(n.ri(m.BASE+slot*0x16c+0x100)),(label,slot,'handle')
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256)
 base=m.Ui();stage=Stage()
 for slot in range(75):
  p=sprite(base,stage,slot);p.rect[:]=[10+slot,20+slot,48,32];assert lib.bk_effect_sprite_initialize(C.byref(p.transform),p.rect,1,1,0)
  p.transform.direction=[0,1,2,255][slot%4];p.transform.motion[:]=[slot*.007,-slot*.003];p.transform.degrees=slot*1.3;p.transform.radians=slot*.002;p.timer=m.Timer(777,0xfffffffa,slot%2);p.uv[:]=[.1,.2,.8,.9];p.rgb=0x123456
 loads=steps=vertices=releases=0;names=set()
 # Uneven widths verify per-argument float scaling and truncated enter2 centers.
 for width in [1,320,640,961,1001,1280,1920,16384]:
  for kind in SEGMENTS:
   for g in range(5):
    for v in range(2 if kind==2 else 1):
     install(n,base,stage);n.load_stage(kind,g,v,width)
     assert lib.bk_ending_stage_ui_initialize(C.byref(base),C.byref(stage),kind,g,v,width,e),e.value
     same(n,base,stage,('load',width,kind,g,v));loads+=1
     for slot,name in n.loaded:assert lib.bk_ending_stage_ui_image(C.byref(stage),slot)==name.encode();names.add(name)
     if width in [640,1001]:
      for t in range(90):
       dt=C.c_float([0,1/60,.1,.5][t%4]).value;n.wf(0x733700,dt)
       for slot,_ in n.loaded:
        p=sprite(base,stage,slot);wanted={1:1,30:0,40:1,70:0,80:1}.get(t)
        if wanted is not None:n.call(0x50e633,struct.pack('<II',m.BASE+slot*0x16c,wanted));assert lib.bk_fade_sprite_request(C.byref(p.transform.fade),wanted)
        n.draws=[];n.call(0x50e6ba,struct.pack('<II',m.BASE+slot*0x16c,0));d=m.Draw()
        assert lib.bk_ending_stage_ui_sprite_step(C.byref(base),C.byref(stage),slot,dt,C.byref(d),e),e.value
        for j,k in enumerate([0,1,2,3,0,2]):assert tuple(d.xy[2*k:2*k+2])==struct.unpack_from('<2f',n.draws[0][2],32*j),(kind,g,v,t,slot,j);vertices+=1
        steps+=1
       same(n,base,stage,('frame',kind,g,v,t))
     n.freed=[]
     if kind==5:n.call(0x50e7c1,struct.pack('<I',0x73a990))
     else:n.segment(*RELEASE[kind])
     assert set(n.freed)=={s for s,_ in n.loaded}
     assert lib.bk_ending_stage_ui_release(C.byref(base),C.byref(stage),kind,e)
     same(n,base,stage,('release',kind,g,v));releases+=len(n.freed)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),loads=loads,images=len(names),steps=steps,vertices=vertices,releases=releases,max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-stage-ui-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
