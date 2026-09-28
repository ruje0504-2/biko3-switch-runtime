"""Complete4f0e44 dialogue UI with real parser, byte count and sprite fades.

Input/font/media/persistent unlock/schedule are explicit service boundaries.
Font-flow service executes original4758f7/474be3 in a separate VM. This does
not stand in for a full flow8 scene, persistent unlock IO or GPU integration.
"""
import argparse,ctypes as C,hashlib,json,random,re,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP
from original_dialogue_oracle import Native as Base,State as Dialogue,equal,Archive
from original_item_notice_oracle import Fade
from original_opening_prompt_oracle import Pulse
from original_text_flow_oracle import Flow,Update,Native as TextNative
from original_dialogue_backdrop_oracle import State as Backdrop
from original_message_oracle import Message
from model_binding import ROOT,library
class Ui(C.Structure):_fields_=[('panel',Fade),('prompt',Pulse),('columns',C.c_int32),('rows',C.c_int32),('step_y',C.c_int32),('parameter',C.c_int32)]
class Result(C.Structure):_fields_=[('group',C.c_int32),('kind',C.c_int32),('choice',C.c_int32)]
class Bindings(C.Structure):_fields_=[('dialogue',C.POINTER(Dialogue)),('text',C.POINTER(Flow)),('backdrop',C.POINTER(Backdrop)),('phase',C.POINTER(C.c_uint8)),('curtain',C.POINTER(Fade)),('wanted',C.POINTER(C.c_uint8)),('result',C.POINTER(Result))]
class Input(C.Structure):_fields_=[('seconds',C.c_float),('advance',C.c_int),('voice',C.c_int),('previous',C.c_uint8),('response',C.c_uint8),('group',C.c_uint32),('area',C.c_int32)]
Op=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Next=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int),C.c_void_p)
Text=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_float,C.c_void_p)
Unlock=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Schedule=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_uint8,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('pause',Op),('next',Next),('bind',Op),('text',Text),('speech',Op),('music',Op),('unlock',Unlock),('schedule',Schedule)]
class Frame(C.Structure):_fields_=[('panel',C.c_float),('prompt',C.c_float),('curtain',C.c_float)]
def fv(f):return (f.alpha,f.speed,f.stage)
def uv(s):return (fv(s.panel),fv(s.prompt.fade),s.prompt.direction,s.columns,s.rows,s.step_y,s.parameter)
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.actor=0xbef7a0;self.font=TextNative(exe)
  self.u.mem_write(0x3006000,struct.pack('<I',0x3006100));self.u.mem_write(0x3006148,struct.pack('<I',0x300e500))
  for a in [0x4b76c2,0x300e500,0x517c8e,0x4aa9f4,0x4aaa17,0x4f175d,0x4f18cd,0x50c6bc,0x51c47e,0x4f0c25,0x4f1039]:self.u.hook_add(UC_HOOK_CODE,self.service,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def fade(self,addr,f,direction=0,idle=0):
  raw=bytearray(0x16c);struct.pack_into('<f',raw,0x12c,f.alpha);struct.pack_into('<f',raw,0x138,f.speed);raw[0x134]=f.stage;raw[0x148:0x14b]=bytes([1,1,idle]);raw[0x160]=direction;self.u.mem_write(addr,bytes(raw))
 def read_fade(self,a):return Fade(struct.unpack('<f',self.u.mem_read(a+0x12c,4))[0],struct.unpack('<f',self.u.mem_read(a+0x138,4))[0],self.u.mem_read(a+0x134,1)[0])
 def service(self,u,a,size,_):
  if a in [0x4f0c25,0x4f1039] and self.layout_only:u.reg_write(UC_X86_REG_EIP,self.stop);return
  if a in [0x4f0c25,0x4f1039]:return
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<4I',u.mem_read(sp,16));value=0;pop=0
  if a==0x4b76c2:
   assert args[1:]==[1,0];self.keys.append(args[0]);value=int(args[0]==self.key)
  elif a==0x300e500:
   assert args[0]==0x3006000;self.events.append('pause');pop=4
  elif a==0x517c8e:self.events.append('next');return
  elif a==0x4aa9f4:assert args[0]==0xbef8b8;self.events.append('bind')
  elif a==0x4aaa17:
   self.events.append('text');f=Flow.from_buffer_copy(u.mem_read(0x6a13d0,C.sizeof(Flow)));out,_=self.font.run(f,self.seconds,self.changed);u.mem_write(0x6a13d0,bytes(out))
  elif a==0x4f175d:self.events.append('speech')
  elif a==0x4f18cd:self.events.append('music')
  elif a==0x50c6bc:
   assert args[1]==0x721dc6+args[0]*8;self.events.append(('unlock',args[0]))
  elif a==0x51c47e:self.events.append(('schedule',args[0]&255,args[1]&255))
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def target(self,m,cols,rows,step,initial):
  self.layout_only=True;u=self.u
  u.mem_write(0xbef8b8,bytes(m.bytes)+b'\0');self.word(0x6a13c0,cols);self.word(0x6a13c4,rows);self.word(0x6a13cc,step);u.reg_write(UC_X86_REG_EBP,self.stack)
  self.call(0x4f0b38 if initial else 0x4f0f7d,b'');return struct.unpack('<f',u.mem_read(0x6a13d8,4))[0]
 def run(self,s,d,f,b,p,c,w,r,inp,raw,key,changed):
  self.layout_only=False;self.raw=raw;u=self.u;u.mem_write(self.dest,raw+bytes(64));self.write(d);self.events=[];self.keys=[];self.key=key;self.seconds=inp.seconds;self.changed=changed
  self.fade(0x734058,s.panel);self.fade(0x7341c4,s.prompt.fade,s.prompt.direction,11);self.fade(0xbeea18,c)
  for a,v in [(0x6a13c0,s.columns),(0x6a13c4,s.rows),(0x6a13cc,s.step_y),(0xbf00f8,s.parameter),(0xbf00f0,b.saved_expression),(0xbefddc,0x3006000 if inp.voice else 0),(0x7219a8,inp.group),(0x7219ac,inp.area),(0x728de0,r.group),(0x729788,r.kind),(0x729784,r.choice)]:self.word(a,v)
  for a,v in [(0xbef798,p.value),(0xbeeb7f,w.value),(0xbf00f5,b.kind),(0x721ad4,inp.previous),(0x71bcda,inp.response)]:u.mem_write(a,bytes([v]))
  u.mem_write(0x6a13d0,bytes(f));u.mem_write(0x733700,struct.pack('<f',inp.seconds));self.call(0x4f0e44,b'')
  out=Ui.from_buffer_copy(s);out.panel=self.read_fade(0x734058);out.prompt.fade=self.read_fade(0x7341c4);out.prompt.direction=u.mem_read(0x734324,1)[0];out.parameter=struct.unpack('<i',u.mem_read(0xbf00f8,4))[0]
  bo=Backdrop.from_buffer_copy(b);bo.saved_expression=struct.unpack('<i',u.mem_read(0xbf00f0,4))[0];bo.kind=u.mem_read(0xbf00f5,1)[0]
  result=Result(*[struct.unpack('<i',u.mem_read(a,4))[0] for a in [0x728de0,0x729788,0x729784]])
  return out,self.read(),Flow.from_buffer_copy(u.mem_read(0x6a13d0,C.sizeof(Flow))),bo,u.mem_read(0xbef798,1)[0],self.read_fade(0xbeea18),u.mem_read(0xbeeb7f,1)[0],result,list(self.events)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();err=C.create_string_buffer(256);rng=random.Random(0x4f0e44)
 lib.bk_dialogue_ui_step.argtypes=[C.POINTER(Ui),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
 lib.bk_dialogue_text_target.argtypes=[C.POINTER(Message),C.c_uint,C.c_uint,C.c_uint,C.c_int,C.POINTER(C.c_float),C.c_void_p]
 lib.bk_dialogue_next.argtypes=[C.POINTER(Dialogue),C.c_void_p,C.c_size_t,C.POINTER(C.c_int),C.c_void_p]
 lib.bk_dialogue_open.argtypes=[C.POINTER(Dialogue),C.c_void_p,C.c_size_t,C.c_char_p,C.c_void_p]
 lib.bk_text_flow_step.argtypes=[C.POINTER(Flow),C.c_float,C.c_int,C.POINTER(Update)]
 targets=0
 def target(m,cols,rows,step):
  nonlocal targets
  for initial in [0,1]:
   wanted=n.target(m,cols,rows,step,initial);out=C.c_float(-1)
   assert lib.bk_dialogue_text_target(C.byref(m),cols,rows,step,initial,C.byref(out),err),err.value
   assert wanted==out.value,(targets,cols,rows,step,initial,wanted,out.value)
   targets+=1
 for i in range(1200):
  m=Message();raw=bytes(rng.choice([13,10,65,0x82,0xa0]) for j in range(i%1024));m.bytes[:len(raw)]=raw;m.length=len(raw);target(m,[1,7,27,32,640][i%5],[1,4,17,480][i%4],[1,16,32,480][i%4])
 actual_pages=0;files=0
 archive=Archive(a.data/'bk3_05.pp')
 for e in archive.entries:
  if not e.name.endswith('.txt'):continue
  raw=archive.read(e);ids=[int(x) for x in re.findall(rb'(?m)^#([0-9]{5})',raw)]
  if not ids:continue
  d=Dialogue();d.first_label=ids[0];d.last_label=ids[-1]
  assert lib.bk_dialogue_open(C.byref(d),raw,len(raw),e.name.encode(),err),err.value
  for _ in range(len(ids)+3):
   done=C.c_int();assert lib.bk_dialogue_next(C.byref(d),raw,len(raw),C.byref(done),err),err.value
   target(d.text,27,4,16);actual_pages+=1
   if done.value:break
  else:raise AssertionError(e.name)
  files+=1
 raw=b'#00001 #C02#F34#E09#M03\r\nfirst\r\nline\r\n#00002 #C06#F12\r\nsecond\r\n#end\r\n'
 frames=transitions=next_calls=unlocks=0
 for case in range(12000):
  s=Ui(Fade(rng.random(),2,rng.randrange(6)),Pulse(Fade(rng.random(),2,rng.randrange(6)),rng.choice([0,1,2,255])),27,4,16,-19)
  d=Dialogue();d.first_label=1;d.last_label=2;d.current_label=case%3;d.code_f=rng.randrange(-3,58);d.image_kind=rng.choice([0,1,2,3,255]);d.sound_pending=7;d.music_pending=8;d.text.bytes[:5]=b'old\r\n';d.text.length=5
  f=Flow(rng.random()*60,rng.random()*100,rng.random()*100,rng.randrange(-1,3),rng.randrange(-1,3))
  if case%2:f.scroll=f.target
  b=Backdrop();b.saved_expression=-7;b.kind=17;p=C.c_uint8(rng.choice([0,0,0,0,1,2,5,10,255]));c=Fade(rng.random(),rng.choice([0,.1,2,100]),rng.randrange(6));w=C.c_uint8(rng.choice([0,1,1,2,255]));r=Result(97,81,73)
  key=[-1,0,0x5a,0x33450,0x33451,0x33452][case%6];inp=Input(C.c_float(rng.choice([0,1/60,.1,1,10])).value,key!=-1,case%2,rng.choice([0x38,2,0x10,1,0,255]),rng.choice([0,3,4,255]),case%5,rng.choice([0,7,8,9]));changed=case%3!=0
  expected=n.run(s,d,f,b,p,c,w,r,inp,raw,key,changed);events=[]
  def op(name):
   def call(_,e):events.append(name);return 1
   return Op(call)
  @Next
  def next_(_,done,e):events.append('next');return lib.bk_dialogue_next(C.byref(d),raw,len(raw),done,e)
  @Text
  def text_(_,dt,e):events.append('text');return lib.bk_text_flow_step(C.byref(f),dt,changed,C.byref(Update()))
  @Unlock
  def unlock(_,g,e):events.append(('unlock',g));return 1
  @Schedule
  def schedule(_,t,m,e):events.append(('schedule',t,m));return 1
  ops=Ops(None,op('pause'),next_,op('bind'),text_,op('speech'),op('music'),unlock,schedule)
  bindings=Bindings(C.pointer(d),C.pointer(f),C.pointer(b),C.pointer(p),C.pointer(c),C.pointer(w),C.pointer(r));out=Frame()
  assert lib.bk_dialogue_ui_step(C.byref(s),C.byref(bindings),C.byref(inp),C.byref(ops),C.byref(out),err),err.value
  es,ed,ef,eb,ep,ec,ew,er,ev=expected
  assert uv(s)==uv(es),(case,'ui',uv(s),uv(es));equal(d,ed,case)
  assert bytes(f)==bytes(ef),(case,'text',bytes(f),bytes(ef));assert bytes(b)==bytes(eb),(case,'backdrop')
  assert (p.value,fv(c),w.value,bytes(r))==(ep,fv(ec),ew,bytes(er)),(case,'control')
  assert events==ev,(case,events,ev);assert (out.panel,out.prompt,out.curtain)==(s.panel.alpha,s.prompt.fade.alpha,c.alpha)
  keys=[0,0x5a,0x33450,0x33451,0x33452];assert n.keys==(keys[:keys.index(key)+1] if key!=-1 else keys)
  frames+=1;next_calls+='next' in events;transitions+=any(isinstance(x,tuple) and x[0]=='schedule' for x in events);unlocks+=any(isinstance(x,tuple) and x[0]=='unlock' for x in events)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,next_calls=next_calls,transitions=transitions,unlock_requests=unlocks,target_checks=targets,actual_text_pages=actual_pages,files=files,max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-ui-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
