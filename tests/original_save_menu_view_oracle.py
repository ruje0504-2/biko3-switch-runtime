"""Original4e97fd/50ace3 geometry and5095d5+50c371 sprite UI draw policy.
Only GPU/font/time/input/resource I/O are boundaries. Real56 sprite requests,
instant/fading transitions, detail selection, cursor timer and curtain run x86.
Text glyphs and save-slot label bytes are separate pending integration work.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
import original_pause_oracle as pause
from original_pause_oracle import Sprite,Cursor,Bindings,Input,Draw,ss,fs
from original_common_hud_oracle import State as Common,Flow,Timer
from original_item_notice_oracle import Fade
from original_save_menu_control_oracle import State as Control,FIELDS
from model_binding import ROOT,library
pause.ADDR[:]=[0x734058+i*0x16c for i in range(56)]+[0xb537e8,0xbeea18]
class View(C.Structure):_fields_=[('sprites',Sprite*56),('loaded',C.c_uint64),('detail_slot',C.c_int32)]
class Record(C.Structure):_fields_=[('area',C.c_uint32),('occupied',C.c_uint8),('inventory',C.c_uint8*5)]
class Frame(C.Structure):_fields_=[('count',C.c_uint),('text_after',C.c_uint),('draws',Draw*48)]
Text=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_float,C.c_void_p)
Motion=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_float),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('text',Text),('motion',Motion)]
def snapshot(v,s,c,cursor):return (tuple(ss(p) for p in v.sprites),v.loaded,v.detail_slot,s.skip_draw,ss(cursor.sprite),cursor.idle.duration,cursor.idle.deadline,cursor.idle.armed,cursor.wanted,fs(c.curtain))
class Native(pause.Native):
 def __init__(self,exe):
  super().__init__(exe)
  for a in [0x50a2ae,0x4aa32e,0x4ec9b1,0x4aaa01,0x4aa9f4,0x4aaa17]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.text_after=0xffffffff
 def hook(self,u,a,z,d):
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<4I',u.mem_read(sp+4,16))
  if a in [0x50a2ae,0x4aa32e,0x4ec9b1,0x4aaa01,0x4aa9f4,0x4aaa17,0x443bb8,0x43e96a]:
   if a==0x4aaa17:self.text_after=len(self.draws)
   elif a==0x443bb8:self.wi(args[2],self.verts((args[0]-0x4000200)//0x400))
   elif a==0x43e96a:
    i=(args[0]-0x4000000)//0x400;p=self.read_sprite(i);self.draws.append((i,tuple(p.rect),p.fade.alpha))
   u.reg_write(UC_X86_REG_EIP,self.ri(sp));u.reg_write(UC_X86_REG_ESP,sp+4);return
  super().hook(u,a,z,d)
 def read_view(self):
  v=View();v.sprites[:]=[self.read_sprite(i) for i in range(56)];v.loaded=sum(int(bool(self.ri(a+0x100)))<<i for i,a in enumerate(pause.ADDR[:56]));v.detail_slot=C.c_int32(self.ri(0xbf4b40)).value
  s=Control();s.skip_draw=C.c_int32(self.ri(0xbf410c)).value;c=Common();c.curtain=self.read_sprite(57).fade
  cursor=Cursor(self.read_sprite(56),Timer.from_buffer_copy(self.u.mem_read(0xb53924,C.sizeof(Timer))),self.u.mem_read(0xb5394f,1)[0])
  return v,s,c,cursor
 def install(self,v,s,c,cursor,inp,records,mode):
  self.draws=[];self.names=[];self.text_after=0xffffffff;self.now=inp.now;self.record=False
  for i,p in enumerate([*v.sprites,cursor.sprite,Sprite(c.curtain,(C.c_float*4)(0,0,1280*inp.scale,960*inp.scale))]):
   self.install_sprite(i,p,bool(v.loaded&(1<<i)) if i<56 else True)
   self.u.mem_write(pause.ADDR[i]+0x148,bytes([0,0,0] if i<=8 or i==19 or 40<=i<56 else [1,1,0]))
  for name,a in FIELDS.items():self.wi(a,getattr(s,name)&0xffffffff)
  self.wi(0xbf4b40,v.detail_slot);self.wf(0xbf4b20,s.cursor[0]);self.wf(0xbf4b24,s.cursor[1]);self.wf(0x721ad0,inp.scale);self.wf(0x733700,inp.seconds)
  self.u.mem_write(0xbeeb7f,bytes([c.blocked]));self.u.mem_write(0xb5394f,bytes([cursor.wanted]));self.u.mem_write(0xb53924,bytes(cursor.idle));self.u.mem_write(0x721ad4,bytes([0x20 if mode else 1]))
  for g in range(5):
   for k in range(10):
    a=0xb53c40+g*560+k*56;r=records[g][k];self.wi(a+4,r.area);self.u.mem_write(a+8,bytes(r.inventory));self.u.mem_write(a+24,bytes([r.occupied]))

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x5095d5);e=C.create_string_buffer(256)
 lib.bk_save_menu_view_initialize.argtypes=[C.POINTER(View),C.c_uint,C.c_uint,C.c_uint];lib.bk_save_menu_view_details.argtypes=[C.POINTER(View),C.c_uint,C.c_uint];lib.bk_save_menu_image.argtypes=[C.c_uint,C.c_uint,C.c_uint];lib.bk_save_menu_image.restype=C.c_char_p
 lib.bk_save_menu_view_step.argtypes=[C.POINTER(View),C.POINTER(Control),C.POINTER(Bindings),C.POINTER(Input),C.c_uint,C.c_void_p,C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
 lib.bk_menu_cursor_initialize.argtypes=[C.POINTER(Cursor),C.c_uint,C.c_uint]
 constructors=frames=draws=0
 records=((Record*10)*5)();s=Control();c=Common();cursor=Cursor();inp=Input(0,0,0,1,0)
 for width in [320,640,1001,1280,1920]:
  for mode in [0,1]:
   for group in range(5):
    v=View();v.detail_slot=7
    for p in v.sprites:p.fade=Fade(.25,2,3);p.rect[:]=[11,22,33,44]
    assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);inp.scale=width/1280;s.tab=group+3;n.width=width;n.height=width*3//4
    n.install(v,s,c,cursor,inp,records,mode);n.call(0x4e97fd,b'');want=n.read_view()[0]
    assert lib.bk_save_menu_view_initialize(C.byref(v),mode,group,width)
    assert tuple(ss(p) for p in v.sprites)==tuple(ss(p) for p in want.sprites),(width,mode,group,[(i,ss(a),ss(b)) for i,(a,b) in enumerate(zip(v.sprites,want.sprites)) if ss(a)!=ss(b)])
    assert v.loaded==want.loaded and v.detail_slot==want.detail_slot
    for i,name in n.names:assert name==lib.bk_save_menu_image(i,group,mode).decode(),(i,name)
    constructors+=1
 def fade():return Fade(rng.choice([0,.1,.25,.5,.75,1]),rng.choice([0,.25,2,10]),rng.randrange(6))
 for case in range(6000):
  mode=case%2;width=rng.choice([320,640,1001,1280]);group=rng.randrange(5);v=View();assert lib.bk_save_menu_view_initialize(C.byref(v),mode,group,width)
  v.detail_slot=rng.randrange(10)
  if case%11==0:v.loaded=0
  for p in v.sprites:p.fade=fade()
  s=Control();s.tab=group+3;s.hover=rng.choice([1,21,22,25,30,99]);s.confirm_hover=rng.choice([10,12,99]);s.page=rng.randrange(2);s.skip_draw=rng.choice([0,0,0,1]);s.cursor[:]=[rng.uniform(0,width),rng.uniform(0,width*3/4)]
  if not v.loaded:s.skip_draw=1
  c=Common();c.curtain=fade();c.blocked=rng.choice([0,1,255]);cursor=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cursor),width,width*3//4);cursor.sprite.fade=fade();cursor.idle=Timer(rng.choice([0,10000,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,255]));cursor.wanted=rng.choice([0,1,255])
  inp=Input(0,rng.getrandbits(32),rng.choice([0,1/60,.25,1,10]),width/1280,0)
  for g in range(5):
   for k in range(10):records[g][k]=Record(rng.randrange(9),rng.choice([0,1,255]),(C.c_uint8*5)(*[rng.choice([0,1,255]) for _ in range(5)]))
  motion=rng.choice([[0,0],[1,0],[0,1],[-1,-1]])
  n.install(v,s,c,cursor,inp,records,mode);n.motion=motion;n.call(0x5095d5,struct.pack('<I',mode));want=snapshot(*n.read_view());wd=n.draws;wa=n.text_after
  text_calls=[]
  @Text
  def text(_,g,dt,e):text_calls.append(g);return 1
  @Motion
  def move(_,out,e):out[0],out[1]=motion;return 1
  ops=Ops(None,text,move);b=Bindings(C.pointer(c),None,C.pointer(cursor),None,None);f=Frame()
  assert lib.bk_save_menu_view_step(C.byref(v),C.byref(s),C.byref(b),C.byref(inp),mode,C.byref(records),C.byref(ops),C.byref(f),e),(case,e.value)
  got=snapshot(v,s,c,cursor)
  assert got==want,(case,'state',[(i,x,y) for i,(x,y) in enumerate(zip(got[0],want[0])) if x!=y],got[1:],want[1:])
  gd=[(d.slot,tuple(d.rect),d.alpha) for d in f.draws[:f.count]]
  assert gd==wd,(case,'draws',gd,wd)
  assert f.text_after==wa and text_calls==([] if wa==0xffffffff else [group]),(case,'text',f.text_after,wa,text_calls)
  frames+=1;draws+=len(wd)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,frames=frames,draws=draws,scope=__doc__)
 (ROOT/'local/original-save-menu-view-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS save-menu-view',report)
if __name__=='__main__':main()
