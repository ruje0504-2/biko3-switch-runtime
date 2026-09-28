"""Full4f175d/4f18cd and initial4f0ce7..4f0d9c packed-media control.

Real50db23 and46435e execute. Resource decode and DirectSound operations
are explicit service boundaries. Compares ordered commands and retained
state; does not claim the full dialogue/UI or original audio device output.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class Music(C.Structure):_fields_=[('volume',C.c_int32),('wanted',C.c_uint8)]
class Command(C.Structure):_fields_=[('kind',C.c_int),('pack',C.c_char_p),('name',C.c_char_p),('volume',C.c_int32),('loop',C.c_int)]
Callback=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Command),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('command',Callback)]
class Native(Base):
 music,speech,vt=0x3001000,0x3001100,0x3001200
 def __init__(self,exe):
  super().__init__(exe)
  for a in [self.music,self.speech]:self.word(a,self.vt)
  for off,addr in [(0x3c,0x300f100),(0x48,0x300f110),(0x24,0x300f120),(0x34,0x300f130),(0x30,0x300f140)]:self.word(self.vt+off,addr)
  for a in [0x50da5f,0x50d6b3,0x50d4fa,0x4f0d9c,0x300f100,0x300f110,0x300f120,0x300f130,0x300f140]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  self.u.mem_write(0x5767c8,b'\1')
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0]
 def hook(self,u,a,size,_):
  if a==0x4f0d9c:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<6I',u.mem_read(sp,24));pop=0
  if a==0x50da5f:
   assert args[0]==0xbefccc;self.word(0xbefddc,0);self.events.append((0,None,None,0,0))
  elif a in [0x50d6b3,0x50d4fa]:
   kind=1 if a==0x50d6b3 else 5
   assert args[0]==(0xbefccc if kind==1 else 0x728cd0)
   pack=self.string(args[1]);assert pack.endswith(b'.pp')
   volume=C.c_int32(args[3]).value;loop=args[4]&255
   self.events.append((kind,pack[:-3],self.string(args[2]),volume,loop))
   if kind==1:self.word(0xbefddc,self.speech)
   else:self.word(0x728dd0,self.music);self.word(0x728dd4,volume);u.mem_write(0x728dd8,b'\1')
  elif a==0x300f100:
   assert args[0]==self.music;self.events.append((3,None,None,C.c_int32(args[1]).value,0));pop=8
  elif a==0x300f110:
   assert args[0]==self.music;self.events.append((4,None,None,0,0));pop=4
  elif a==0x300f120:
   assert args[0]==self.speech;self.word(args[1],0);pop=8
  elif a==0x300f130:
   assert args[:2]==[self.speech,0];pop=8
  elif a==0x300f140:
   assert args[:4]==[self.speech,0,0,0];self.events.append((2,None,None,0,0));pop=16
  u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,mode,s,present,pending,name,dt,master):
  self.events=[];u=self.u
  self.word(0x728dd0,self.music if present else 0);self.word(0x728dd4,s.volume);u.mem_write(0x728dd8,bytes([s.wanted]));u.mem_write(0xbf00ed,bytes([pending]));u.mem_write(0xbeffed,name+bytes(256-len(name)))
  u.mem_write(0xbefde4,bytes([pending]));u.mem_write(0xbefdec,name+bytes(256-len(name)));self.word(0xbefddc,self.speech)
  u.mem_write(0x733700,struct.pack('<f',dt));self.word(0xbe9a08,master);self.word(0xbe9a0c,master)
  self.call([0x4f175d,0x4f18cd,0x4f0ce7][mode],b'')
  return (struct.unpack('<i',u.mem_read(0x728dd4,4))[0],u.mem_read(0x728dd8,1)[0],u.mem_read(0xbefde4 if mode==0 else 0xbf00ed,1)[0])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();err=C.create_string_buffer(256)
 lib.bk_dialogue_speech_step.argtypes=[C.POINTER(C.c_uint8),C.c_char_p,C.c_int32,C.POINTER(Ops),C.c_void_p]
 lib.bk_dialogue_music_open.argtypes=[C.POINTER(Music),C.POINTER(C.c_uint8),C.c_char_p,C.POINTER(Ops),C.c_void_p]
 lib.bk_dialogue_music_step.argtypes=[C.POINTER(Music),C.c_int,C.POINTER(C.c_uint8),C.c_char_p,C.c_float,C.c_int32,C.POINTER(Ops),C.c_void_p]
 rng=random.Random(0x4f175d);counts=[0,0,0];command_count=0
 names=[b'se001.wav',b'SE001.wav',b'se',b's',b'p01_001.wav',b'bg001.wav',b'']
 def check(mode,s,present,pending,name,dt,master):
  nonlocal command_count
  expected=n.run(mode,s,present,pending.value,name,dt,master);events=[]
  @Callback
  def service(_,c,e):
   v=c.contents;events.append((v.kind,v.pack,v.name,v.volume,v.loop));return 1
  ops=Ops(None,service)
  if mode==0:ok=lib.bk_dialogue_speech_step(C.byref(pending),name,master,C.byref(ops),err)
  elif mode==1:ok=lib.bk_dialogue_music_step(C.byref(s),present,C.byref(pending),name,dt,master,C.byref(ops),err)
  else:ok=lib.bk_dialogue_music_open(C.byref(s),C.byref(pending),name,C.byref(ops),err)
  assert ok,err.value
  assert (s.volume,s.wanted,pending.value)==expected,(mode,counts,(s.volume,s.wanted,pending.value),expected)
  assert events==n.events,(mode,counts,events,n.events)
  counts[mode]+=1;command_count+=len(events)
 for mode,total in [(0,6000),(1,16000),(2,2000)]:
  for i in range(total):
   s=Music(rng.choice([-10000,-6001,-6000,-5999,-20,0,rng.randrange(-10000,1)]),rng.choice([0,1,2,127,128,255]));present=i%2;p=C.c_uint8(rng.choice([0,1,1,1,2,127,128,255]));name=names[i%len(names)]
   dt=C.c_float(rng.choice([0,1e-8,.00124999,.00125,.00125001,1/60,1/53,.22,10])).value
   check(mode,s,present,p,name,dt,rng.choice([0,-100,-4000,-6000,-8000,-10000]))
 # Natural fade-switch and stop sequences, including zero-dt minimum1 gain.
 sequences=0
 for hz in [15,30,53,60]:
  s=Music(-6000,1);p=C.c_uint8(0)
  for i in range(hz*9):check(1,s,1,p,b'bg001.wav',C.c_float(1/hz).value,-700)
  for name in [b'bg002.wav',b'',b'bg001.wav']:
   p.value=1
   for i in range(hz*10):
    check(1,s,1,p,name,C.c_float(1/hz).value,-700)
    if p.value==0:sequences+=1;break
   else:raise AssertionError('music request did not finish')
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),speech_calls=counts[0],music_frames=counts[1],initializations=counts[2],commands=command_count,sequences=sequences,max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-media-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
