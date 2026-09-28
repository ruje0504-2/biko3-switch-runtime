"""51a190 branches0/2/3 through51a3bc: real4ec528/6c0/777,517c8e/517ba9,
panel fade and idle11 prompt. Input, click, font, item reload and camera
select are service boundaries; rain skipped, common HUD excluded.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_dialogue_oracle import Native as DialogueNative,State as Dialogue,equal
from original_item_notice_oracle import Fade,State as Notice,Frame
from original_opening_prompt_oracle import Pulse
from original_text_flow_oracle import Flow
from model_binding import ROOT,library
class Camera(C.Structure):_fields_=[('phase',C.c_uint8),('transition',C.c_uint8),('stage',C.c_int32)]
class Bindings(C.Structure):_fields_=[('camera',C.POINTER(Camera)),('notice',C.POINTER(C.c_uint8)),('npc_hidden',C.POINTER(C.c_uint8)),('player_hidden',C.POINTER(C.c_uint8)),('behavior',C.POINTER(C.c_int32)),('panel',C.POINTER(Fade)),('flow',C.POINTER(Flow))]
Op=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Next=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('click',Op),('next',Next),('bind',Op),('clear',Op),('close',Op),('items',Op),('select',Op)]
class Native(DialogueNative):
 def __init__(self,exe):
  super().__init__(exe);self.actor=0xb54760
  for addr in [0x4b76c2,0x46435e,0x4aa9f4,0x4aaa29,0x517ba9,0x4ec87c,0x401d24,0x517c8e,0x4fac10,0x4aaa17,0x51a3bc]:self.u.hook_add(UC_HOOK_CODE,self.service,begin=addr,end=addr)
 def boundary(self,u,addr,size,_):
  if addr!=0x520538:return super().boundary(u,addr,size,_)
  sp=u.reg_read(UC_X86_REG_ESP);ret,ptr=struct.unpack('<2I',u.mem_read(sp,8));assert ptr in [self.source,self.dest]
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def service(self,u,addr,size,_):
  if addr==0x51a3bc:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<3I',u.mem_read(sp+4,12));value=0
  names={0x46435e:'click',0x517c8e:'next',0x4aa9f4:'bind',0x4aaa29:'clear',0x517ba9:'close',0x4ec87c:'items',0x401d24:'select',0x4aaa17:'draw'}
  if addr==0x4b76c2:
   assert args[1:]==(1,0);self.inputs.append(args[0]);value=int(args[0]==self.key)
  elif addr in names:self.trace.append(names[addr])
  if addr in [0x517c8e,0x517ba9]:return
  if addr==0x4aa9f4:assert args[0]==0xb54878
  if addr==0x46435e:assert args[:2]==(0x123456,0)
  if addr==0x401d24:assert args[:2]==(0x3004000,0)
  if addr==0x4ec87c:u.mem_write(0x6a13dc,struct.pack('<2i',1,0))
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,c,notice,npc,player,behavior,panel,flow,d,raw,key,source,end,prompt,dt):
  u=self.u;self.raw=raw;u.mem_write(self.dest,raw+bytes(64));self.write(d);self.trace=[];self.inputs=[];self.key=key
  for addr,data in [(0x71ba88,bytes([c.phase])),(0xbef784,struct.pack('<i',c.stage)),(0xbeecef,bytes([notice.value])),(0x729110,bytes([npc.value])),(0x71b838,bytes([player.value])),(0x729638,bytes(behavior)),(0x6a13d0,bytes(flow)),(0xbef290,struct.pack('<I',0x123456)),(0x71af38,struct.pack('<I',0x3004000)),(0x30041f0,struct.pack('<f',source)),(0x30041e8,struct.pack('<f',end)),(0xbeecbc,bytes([panel.stage]))]:u.mem_write(addr,data)
  for addr,fade,direction,idle in [(0xbeeb88,panel,0,0),(0xbef608,prompt.fade,prompt.direction,11)]:
   data=bytearray(0x16c);struct.pack_into('<f',data,0x12c,fade.alpha);data[0x134]=fade.stage;struct.pack_into('<f',data,0x138,fade.speed);data[0x148:0x14b]=bytes([1,1,idle]);data[0x160]=direction;u.mem_write(addr,bytes(data))
  u.mem_write(0xbeecef,bytes([notice.value]))
  u.mem_write(0x733700,struct.pack('<f',dt))
  self.call(0x51a190,b'')
  out=(u.mem_read(0x71ba88,1)[0],struct.unpack('<i',u.mem_read(0xbef784,4))[0],*[u.mem_read(x,1)[0] for x in [0xbeecef,0x729110,0x71b838]],struct.unpack('<i',u.mem_read(0x729638,4))[0],u.mem_read(0xbeecbc,1)[0])
  return out,bytes(u.mem_read(0x6a13d0,C.sizeof(Flow))),self.read(),list(self.trace),(struct.unpack('<f',u.mem_read(0xbeecb4,4))[0],struct.unpack('<f',u.mem_read(0xbef734,4))[0],u.mem_read(0xbef73c,1)[0],u.mem_read(0xbef768,1)[0])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256);rng=random.Random(0x4ec528)
 lib.bk_opening_phase_step.argtypes=[C.POINTER(Bindings),C.POINTER(Ops),C.c_int,C.c_float,C.c_float,C.c_void_p]
 lib.bk_opening_notice_step.argtypes=[C.POINTER(Notice),C.POINTER(Pulse),C.c_uint8,C.c_uint8,C.c_float,C.POINTER(Frame)]
 lib.bk_dialogue_next.argtypes=[C.POINTER(Dialogue),C.c_void_p,C.c_size_t,C.POINTER(C.c_int),C.c_void_p];lib.bk_dialogue_close.argtypes=[C.POINTER(Dialogue)]
 raw=b'#00001 #C02#F03#E04#M05\r\nfirst\r\n#00002 #C06\r\nsecond\r\n#end\r\n';steps=closes=0;seen=set()
 for case in range(7200):
  phase=[0,2,3][case%3];c=Camera(phase,rng.randrange(256),rng.choice([-1,0,0,1,2,2,3,99]));notice=C.c_uint8(rng.choice([0,1,1,2,255]));npc=C.c_uint8(rng.randrange(256));player=C.c_uint8(rng.randrange(256));behavior=C.c_int32(rng.randrange(-9,20));panel=Fade(rng.random(),2,rng.randrange(6));flow=Flow(rng.random()*50,rng.random()*10,rng.random()*100,rng.randrange(-2,3),rng.randrange(-2,3));d=Dialogue();d.first_label=1;d.last_label=2;d.current_label=rng.choice([0,1,2]);d.text.carriage_returns=23;d.code_c,d.code_f,d.code_e,d.code_m=7,8,9,10;d.previous_sound=b'old';d.sound=b'pending';d.image=b'image';d.music=b'music';d.sound_pending,d.image_kind,d.music_pending=7,8,9
  key=rng.choice([-1,0,0x5a,0x33450,0x33451,0x33452]);source=rng.choice([0,10,20]);end=10
  prompt=Pulse(Fade(rng.random(),2,rng.randrange(6)),rng.choice([0,1,2,255]));dt=C.c_float(rng.choice([0,1/60,.25,1,10])).value
  wanted,wflow,wd,wtrace,wsprites=n.run(c,notice,npc,player,behavior,panel,flow,d,raw,key,source,end,prompt,dt);trace=[]
  def op(name):
   def run(_,e):
    trace.append(name)
    if name=='close':lib.bk_dialogue_close(C.byref(d))
    if name=='items':flow.enabled=1;flow.started=0
    return 1
   return Op(run)
  @Next
  def next_(_,done,e):
   trace.append('next');return lib.bk_dialogue_next(C.byref(d),raw,len(raw),done,e)
  ops=Ops(None,op('click'),next_,op('bind'),op('clear'),op('close'),op('items'),op('select'))
  b=Bindings(C.pointer(c),C.pointer(notice),C.pointer(npc),C.pointer(player),C.pointer(behavior),C.pointer(panel),C.pointer(flow))
  assert lib.bk_opening_phase_step(C.byref(b),C.byref(ops),key!=-1,source,end,error),error.value
  ns=Notice();ns.panel=panel;frame=Frame()
  assert lib.bk_opening_notice_step(C.byref(ns),C.byref(prompt),phase,notice.value,dt,C.byref(frame))
  panel=ns.panel
  if frame.draw_text:trace.append('draw')
  assert (panel.alpha,prompt.fade.alpha,prompt.fade.stage,prompt.direction)==wsprites,(case,'sprites')
  actual=(c.phase,c.stage,notice.value,npc.value,player.value,behavior.value,panel.stage)
  assert actual==wanted,(case,actual,wanted);assert bytes(flow)==wflow,(case,'flow');equal(d,wd,case);assert trace==wtrace,(case,trace,wtrace)
  if phase==0:
   keys=[0,0x5a,0x33450,0x33451,0x33452];assert n.inputs==keys[:keys.index(key)+1] if key!=-1 else n.inputs==keys
  else:assert not n.inputs
  closes+='close' in trace;steps+=1;seen.update(trace)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,closes=closes,services=sorted(seen),scope=__doc__)
 (ROOT/'local/original-opening-phase-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS opening phase',steps,'steps',closes,'real closes')
if __name__=='__main__':main()
