"""Original4e01e4/4643e4 and packaged4cc582 audio stage4ce865..4cea73.
DirectSound and resource IO are observed boundaries; actual caller, gain math,
loop, pointer aliasing and latch execute unchanged. Rejected gain retains the
old buffer gain (documented DirectSound valid range), distinct from controller
completion. Emits native commands for real PCM replay, not Windows DSP.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
from bk3_assets import Archive
class Input(C.Structure):
 _fields_=[('direction',C.c_int32),('master',C.c_int32),('seconds',C.c_float),('present',C.c_uint8*2)]
Status=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
Volume=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int32),C.c_void_p)
Gain=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int32,C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('status',Status),('volume',Volume),('gain',Gain)]
class Native(Base):
 buffers,vtable,stub=0x3005000,0x3006000,0x3007000
 def __init__(self,exe,names):
  super().__init__(exe);self.names=names
  for off in [0x18,0x24,0x30,0x34,0x3c,0x48]:
   self.word(self.vtable+off,self.stub+off);self.u.hook_add(UC_HOOK_CODE,self.hook,begin=self.stub+off,end=self.stub+off)
  for i in range(48):self.word(self.buffers+i*32,self.vtable)
  for a in [0x4ad8ec,0x4a06df]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def integer(self,a):return struct.unpack('<i',self.u.mem_read(a,4))[0]
 def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0]
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,=struct.unpack('<I',u.mem_read(sp,4));pop=4;result=0
  def words(n):return struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4))
  if a==0x4ad8ec:
   dst,path,name=words(3);u.mem_write(dst,(self.string(path) if path else b'')+(self.string(name) if name else b'')+b'\0')
  elif a==0x4a06df:
   path,name=words(2);assert self.string(path)==b'\\bk3_02.pp'
   slot=47 if self.loads==0 else self.loads+1;self.loads+=1;raw=self.string(name);present=raw in self.names
   self.events.append(('load',slot,raw.hex(),present));result=self.buffers+slot*32 if present else 0
  else:
   ptr,=words(1);slot=(ptr-self.buffers)//32;assert 0<=slot<48;off=a-self.stub
   if off==0x18:
    _,out=words(2);value=self.values[self.reads];write=self.read_write[self.reads];result=self.read_hr[self.reads];self.reads+=1
    if write:self.word(out,value)
    self.events.append(('volume',value if write else 0,self.integer(0x719c5c)));pop=12
   elif off==0x24:
    _,out=words(2);self.word(out,self.flags[slot]);result=self.hr[slot];self.events.append(('status',slot,int(not result and self.flags[slot]&1),self.integer(0x719c5c)));pop=12
   elif off==0x3c:
    _,v=words(2);v=C.c_int32(v).value
    if self.mode=='duck':self.events.append(('gain',v,self.integer(0x719c5c)))
    else:self.events.append(('gain',slot,v))
    if -10000<=v<=0:self.gain=v
    else:result=0x80070057
    pop=12
   elif off==0x34:
    _,pos=words(2);assert pos==0;self.events.append(('rewind',slot));pop=12
   elif off==0x30:
    _,z1,z2,flags=words(4);assert z1==z2==0;self.events.append(('play',slot,flags));pop=20
   else:self.events.append(('pause',slot));pop=8
  u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
 def duck(self,in_,latch,values,flags=(1,1),hr=(0,0),read_write=(True,True),read_hr=(0,0)):
  self.mode='duck';self.events=[];self.reads=0;self.values=values;self.flags=flags;self.hr=hr;self.read_write=read_write;self.read_hr=read_hr;self.gain=values[0]
  self.word(0xbe9a08,in_.master);self.word(0x719c5c,latch);self.u.mem_write(0x733700,struct.pack('<f',in_.seconds))
  for i in range(2):self.word(0x722334+i*0x120,self.buffers+i*32 if in_.present[i] else 0)
  self.call(0x4e01e4,struct.pack('<i',in_.direction))
  return self.integer(0x719c5c),self.u.reg_read(UC_X86_REG_EAX),self.events.copy()
 def entry(self,g,v,volume):
  self.mode='entry';self.events=[];self.loads=0;self.flags=[0]*48;self.hr=[0]*48;self.word(0xbe9a0c,volume);self.word(0x721e04,v);self.u.mem_write(0x721b3c,bytes([g]));self.u.mem_write(0x5767c8,b'\1')
  for i in range(48):self.word(0x722214+i*0x120,0)
  bp=self.stack;self.u.reg_write(UC_X86_REG_EBP,bp);self.u.reg_write(UC_X86_REG_ESP,bp-0x1000);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
  self.u.emu_start(0x4ce865,0x4cea73,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x4cea73
  slots=[47,*range(47)]
  for i,slot in enumerate(slots):
   load=next((x for x in self.events if x[0]=='load' and x[1]==slot),None)
   assert self.integer(0x722214+i*0x120)==(self.buffers+slot*32 if load and load[3] else 0)
  return self.events.copy()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();arc=Archive(a.data/'bk3_02.pp');names={x.name.encode('cp932') for x in arc.entries};n=Native(exe,names);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4e01e4)
 lib.bk_ending_sound_duck.argtypes=[C.POINTER(C.c_int32),C.POINTER(Input),C.POINTER(Ops),C.POINTER(C.c_int32),C.c_void_p]
 lib.bk_ending_sound_music.argtypes=[C.c_uint,C.c_uint];lib.bk_ending_sound_music.restype=C.c_char_p
 lib.bk_ending_sound_effect.argtypes=[C.c_uint];lib.bk_ending_sound_effect.restype=C.c_char_p
 calls=reads=gains=0
 for case in range(12000):
  master=rng.choice([0,-500,-8000,-9000,-10000,rng.randrange(-10000,1)])
  sec=C.c_float(rng.choice([0,.00049,.0005,.00051,1/60,1/30,.25,1,5,1000000])).value
  in_=Input(rng.choice([-1,0,1,1,2,17]),master,sec,(C.c_uint8*2)(case%11!=0,case%7!=0));latch=rng.choice([0,0,1,7,-11]);vols=[rng.choice([master,master-2000,master-1999,master+1,master-1,-10000,-2147483648,2147483647,rng.randrange(-10000,1)]) for _ in range(2)]
  flags=(rng.choice([0,1,2,3]),rng.choice([0,1,2,3]));hr=(int(case%13==0),int(case%17==0));rw=(case%19!=0,case%23!=0);rh=(int(case%29==0),int(case%31==0))
  expected=n.duck(in_,latch,vols,flags,hr,rw,rh);actual=[];state=C.c_int32(latch);result=C.c_int32(-9);nr=[0]
  @Status
  def status(_,slot,out,err):out[0]=int(not hr[slot] and flags[slot]&1);actual.append(('status',slot,out[0],state.value));return 1
  @Volume
  def volume(_,out,err):i=nr[0];nr[0]+=1;out[0]=vols[i] if rw[i] else 0;actual.append(('volume',out[0],state.value));return 1
  @Gain
  def gain(_,v,err):actual.append(('gain',v,state.value));return 1
  ops=Ops(None,status,volume,gain)
  assert lib.bk_ending_sound_duck(C.byref(state),C.byref(in_),C.byref(ops),C.byref(result),e),e.value
  assert (state.value,result.value,actual)==expected,(case,(state.value,result.value,actual),expected)
  calls+=len(actual);reads+=nr[0];gains+=sum(x[0]=='gain' for x in actual)
 print('PASS native duck',12000,calls,flush=True)
 trace=[];entries=[];frames=0;rejected=0
 for g in range(5):
  for v in range(2):
   entry=n.entry(g,v,-1500-g*100);entries.append(entry);trace.append(f'ENTRY {g} {v} {-1500-g*100}')
   for kind,slot,raw,present in [x for x in entry if x[0]=='load']:
    wanted=lib.bk_ending_sound_music(g,v) if slot==47 else lib.bk_ending_sound_effect(slot-2)
    assert wanted==(bytes.fromhex(raw) if present else None),(g,v,slot,wanted,raw)
    trace.append(f'LOAD {slot} {bytes.fromhex(raw).decode() if present else "-"}')
   assert entry[-3:]==[('rewind',47),('gain',47,-1500-g*100),('play',47,1)],entry[-5:]
   latch=73;current=-500;present=[False,False];playing=[False,False]
   for t in range(360):
    # Both sides start from identical explicit commands; all expected gain
    # requests and latch/return values come from the unchanged native code.
    event=0
    if t%120==0:event=1;present[0]=playing[0]=True;current=-500
    elif t%120==20:event=2;present[1]=playing[1]=True
    elif t%120==70:event=3;playing[1]=False
    elif t%120==105:event=4;playing[0]=False
    direction=int(playing[1]) if t%17 else 1-int(playing[1])
    master=[-500,-9500,-10000][t//120];dt=C.c_float([1/60,1/30,0,.05,.0005][t%5]).value
    in_=Input(direction,master,dt,(C.c_uint8*2)(*present));old=latch
    latch,result,events=n.duck(in_,old,[current,current],tuple(map(int,playing)))
    writes=[x[1] for x in events if x[0]=='gain'];assert len(writes)<=1
    if writes:
     if -10000<=writes[0]<=0:current=writes[0]
     else:rejected+=1
    trace.append(f'FRAME {event} {direction} {master} {dt.hex()} {latch} {result} {int(bool(writes))} {writes[0] if writes else 0} {current}')
    frames+=1
  print('PASS entry/trace group',g,flush=True)
 (ROOT/'local/ending-sound-trace.txt').write_text('\n'.join(trace)+'\n')
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),duck_frames=12000,service_calls=calls,volume_reads=reads,gain_requests=gains,entry_profiles=10,loads=460,valid_effects=41,absent_effects=4,alias_effects_to_auxiliary=[0,1,2,3],entry_events=entries,replay_frames=frames,replay_rejected_gains=rejected,exact=True,scope=__doc__)
 (ROOT/'local/original-ending-sound-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS ending sound oracle',calls,frames,rejected,flush=True)
if __name__=='__main__':main()
