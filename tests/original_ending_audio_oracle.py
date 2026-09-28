"""Original4946b4/49490a/4e0956/4ad2bf resource and buffer-command order.
Packaged-file loader, Win32 string helpers and DirectSound buffer vtable are
observed boundaries; naming, slot selection and all original control run.
Emits original commands for actual-PCM two-mixer replay (not Windows DSP).
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from original_ending_auxiliary_oracle import Call
from model_binding import ROOT,library
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.present=[False]*6;self.playing=[False]*6;self.volumes=[0]*6
  self.word(0x53f2f0,0x300d000);self.word(0x53f214,0x300d010);self.u.mem_write(0x5767c8,b'\1')
  self.vtable=0x300b000;self.buffers=0x300a000
  for off in [8,0x24,0x30,0x34,0x3c,0x48]:
   a=0x300d100+off;self.word(self.vtable+off,a);self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
  for i in range(6):self.word(self.buffers+i*32,self.vtable)
  for a in [0x300d000,0x300d010,0x4ad8ec,0x4a06df]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0].decode('ascii')
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,=struct.unpack('<I',u.mem_read(sp,4));pop=4;result=0
  def words(n):return struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4))
  if a==0x300d000:
   dst,fmt,g,bank,cue=words(5);value=self.string(fmt)%(g,bank,cue);u.mem_write(dst,value.encode()+b'\0');result=len(value)
  elif a==0x300d010:
   dst,src=words(2);u.mem_write(dst,self.string(src).encode()+b'\0');pop=12;result=dst
  elif a==0x4ad8ec:
   dst,path,_=words(3);u.mem_write(dst,self.string(path).encode()+b'\0')
  elif a==0x4a06df:
   path,name=words(2);pack=self.string(path).strip('\\').removesuffix('.pp');name=self.string(name)
   self.commands.append((1,self.slot,pack,name,0,0));self.present[self.slot]=True;self.playing[self.slot]=False;result=self.buffers+self.slot*32
  else:
   ptr,=words(1);slot=(ptr-self.buffers)//32;assert 0<=slot<6;off=a-0x300d100
   if off==8:
    self.commands.append((0,slot,'-','-',0,0));self.present[slot]=self.playing[slot]=False;pop=8
   elif off==0x24:
    _,out=words(2);self.word(out,int(self.playing[slot]));pop=12
   elif off==0x30:
    _,z1,z2,flags=words(4);assert z1==z2==0;self.commands.append((2,slot,'-','-',flags&1,self.volumes[slot]));self.playing[slot]=True;pop=20
   elif off==0x34:
    _,pos=words(2);assert pos==0;pop=12
   elif off==0x3c:
    _,volume=words(2);self.volumes[slot]=C.c_int32(volume).value;pop=12
   else:
    self.commands.append((3,slot,'-','-',0,0));self.playing[slot]=False;pop=8
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,g,v,selection,c):
  self.commands=[];self.slot=1 if c.operation==3 and g==0 and v and c.cue==30 else c.slot
  self.u.mem_write(0x721b3c,bytes([g]));self.word(0x721e04,v);self.word(0x7220f4,selection);self.word(0xbe9a08,c.volume)
  for i in range(6):self.word(0x722334+i*0x120,self.buffers+i*32 if self.present[i] else 0)
  ptr=self.buffers+c.slot*32 if self.present[c.slot] else 0
  if c.operation==3:self.call(0x4946b4,struct.pack('<3i',c.cue,c.slot,c.flags))
  elif c.operation==4:self.call(0x49490a,struct.pack('<4i',c.cue,c.bank,c.slot,c.flags))
  elif c.operation==2:self.call(0x4ad2bf,struct.pack('<Iii',ptr,c.flags,c.volume))
  elif c.operation==1:self.call(0x4ad34a,struct.pack('<I',ptr))
  return self.commands

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();lib=library();n=Native(exe);e=C.create_string_buffer(256);pack=C.create_string_buffer(16);name=C.create_string_buffer(32)
 lib.bk_ending_audio_resource.argtypes=[C.c_uint,C.c_int32,C.c_int32,C.POINTER(Call),C.c_void_p,C.c_void_p,C.c_void_p]
 total=loads=0;trace=[];cases=[]
 for g in range(5):
  for v in range(2):
   for selection in range(3):
    for t in range(48):
     slot=(t//4)%6;op=[3,1,2,4][t%4];cue=[5,7,9,10][(t//4)%4] if op==3 else 1+3*v+(t//4)%3
     cases.append((g,v,selection,Call(op,slot,cue,2,[0,1,3][t%3],-500-(t%5)*250)))
 # Special voice redirection to slot1, including supplied slot0/5.
 for slot in range(6):cases.append((0,1,2,Call(3,slot,30,0,1,-750)))
 for g,v,selection,c in cases:
  cmds=n.run(g,v,selection,c);trace.append(' '.join(map(str,[g,v,selection,*[getattr(c,k) for k,_ in Call._fields_],len(cmds)])))
  trace.extend(' '.join(map(str,x)) for x in cmds);total+=len(cmds)
  if c.operation in [3,4]:
   assert lib.bk_ending_audio_resource(g,v,selection,C.byref(c),pack,name,e),e.value
   load=next(x for x in cmds if x[0]==1);assert (pack.value.decode(),name.value.decode())==load[2:4]
   play=cmds[-1];assert play[0]==2 and play[1]==load[1] and play[4:]==(c.flags&1,c.volume)
   loads+=1
 (ROOT/'local/ending-audio-trace.txt').write_text('\n'.join(trace)+'\n')
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),calls=len(cases),loads=loads,commands=total,resource_identity_exact=True,scope=__doc__)
 (ROOT/'local/original-ending-audio-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
