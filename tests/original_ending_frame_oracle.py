"""Original4d74c9..4d7a95 event-stage dispatch, flags, end-wait and camera.
Input capture/conversion/static constructor precede this kernel. Each missing
child controller is an explicit observing/mutating service, not implemented
animation/media. Captured11 words are explicit; no original stack-garbage
meaning or complete flow16 behavior is claimed.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class State(C.Structure):
 _fields_=[(x,C.c_int32) for x in ['phase','camera_mode','camera_clip','auxiliary_mode','state_721ee0','state_721ee4']]+[('camera_values',C.c_uint32*3),('camera_table',(C.c_uint32*4)*5)]+[(x,C.c_int32) for x in ['camera_request','camera_cached','camera_event']]+[(x,C.c_uint32) for x in ['clock_sample','previous_clock','finish_elapsed']]+[(x,C.c_uint8) for x in ['group','finish_fade_stage','finish_blocked','transition_action','curtain_wanted','camera_manual']]
class Input(C.Structure):_fields_=[('words',C.c_uint32*11)]
class Call(C.Structure):_fields_=[('operation',C.c_int),('with_input',C.c_int),('input',Input),('count',C.c_uint),('args',C.c_uint32*6)]
Invoke=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Call),C.POINTER(C.c_uint32),C.c_void_p)
Clock=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_uint32),C.c_void_p)
Key=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('invoke',Invoke),('clock',Clock),('key',Key)]
# address, captured input, remaining word count, pointer word positions
SPECS=[(0x4d7ac4,1,0,()),(0x4db608,1,0,()),(0x4df6c0,1,0,()),
 (0x47a5d0,1,1,(0,)),(0x47d3cb,1,3,(0,1,2)),(0x48d8e9,1,1,(0,)),
 (0x494015,1,3,(0,1,2)),(0x476720,1,1,(0,)),(0x479137,1,3,(0,1,2)),
 (0x47dc79,1,1,(0,)),(0x48181f,1,3,(0,1,2)),(0x48302b,1,0,()),
 (0x48bcbb,0,3,(0,1,2)),(0x4e2223,1,0,()),(0x4965b9,0,0,()),
 (0x4bb0a4,0,4,(0,)),(0x4e1d16,0,4,(0,)),(0x4e1711,0,1,(0,)),
 (0x4e0ecb,0,6,(0,)),(0x4bc444,0,5,(0,)),(0x4df411,0,4,(0,))]
POINTERS={0x71af38:1,0x70d370:2,0x709ef8:3,0x719b44:4}
FIELDS=list(zip(['phase','camera_mode','camera_clip','auxiliary_mode','state_721ee0','state_721ee4'],[0x721e00,0x721e0c,0x721e08,0x721ec8,0x721ee0,0x721ee4]))+list(zip(['camera_request','camera_cached','camera_event','clock_sample','previous_clock','finish_elapsed'],[0x722110,0x721ed0,0x7220e0,0x70d368,0x709fc4,0x719c50]))
BYTES=list(zip(['group','finish_fade_stage','finish_blocked','transition_action','curtain_wanted','camera_manual'],[0x721b3c,0x73aac4,0xbeeb4c,0xbeeb7e,0xbeeb7f,0x719b9c]))
def snap(s):return bytes(s)
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.lookup={row[0]:i for i,row in enumerate(SPECS)}
  self.u.mem_write(0x53f358,struct.pack('<I',0x300e000))
  for a in [*self.lookup,0x4b76c2,0x300e000]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def write(self,s):
  for name,a in FIELDS:self.word(a,getattr(s,name))
  for name,a in BYTES:self.u.mem_write(a,bytes([getattr(s,name)]))
  self.u.mem_write(0x721e14,bytes(s.camera_values));self.u.mem_write(0x709fcc,bytes(s.camera_table))
 def read(self):
  s=State()
  for name,a in FIELDS:setattr(s,name,struct.unpack('<I',self.u.mem_read(a,4))[0])
  for name,a in BYTES:setattr(s,name,self.u.mem_read(a,1)[0])
  C.memmove(C.addressof(s)+State.camera_values.offset,bytes(self.u.mem_read(0x721e14,12)),12)
  C.memmove(C.addressof(s)+State.camera_table.offset,bytes(self.u.mem_read(0x709fcc,80)),80)
  return s
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];result=0
  if a==0x300e000:
   result=self.clocks[self.clock_index];self.clock_index+=1;self.trace.append(('clock',result))
  elif a==0x4b76c2:
   key,mode,extra=struct.unpack('<3I',u.mem_read(sp+4,12));assert (mode,extra)==(1,0)
   self.trace.append(('key',key));result=self.pressed
  else:
   op=self.lookup[a];_,has_input,count,pointers=SPECS[op]
   prefix=tuple(struct.unpack('<11I',u.mem_read(sp+4,44))) if has_input else ()
   args=list(struct.unpack('<'+str(count)+'I',u.mem_read(sp+4+44*has_input,count*4))) if count else []
   for i in pointers:args[i]=POINTERS[args[i]]
   self.trace.append((op,prefix,tuple(args),snap(self.read()),bytes(u.mem_read(0x721dc6,40))))
   if op==self.mutate:
    s=self.read()
    for name,value in self.changes.items():setattr(s,name,value)
    self.write(s)
   result=self.result
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,s,working,inp,clocks,pressed,result,mutate,changes):
  self.write(s);u=self.u;u.mem_write(0x721dc6,working)
  self.trace=[];self.clocks=clocks;self.clock_index=0;self.pressed=pressed;self.result=result;self.mutate=mutate;self.changes=changes
  bp=self.stack;u.mem_write(bp,struct.pack('<2I',0,self.stop));u.mem_write(bp-0x2c,bytes(inp));u.reg_write(UC_X86_REG_EBP,bp);u.reg_write(UC_X86_REG_ESP,bp-0x4c)
  u.emu_start(0x4d74c9,self.stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
  return self.trace,snap(self.read()),bytes(u.mem_read(0x721dc6,40))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4d7436)
 lib.bk_ending_frame_step.argtypes=[C.POINTER(State),C.c_void_p,C.POINTER(Input),C.POINTER(Ops),C.c_void_p]
 total=flag_writes=0;operations=set();transitions=0
 for case in range(20000):
  s=State();s.phase=rng.choice([-1,0,1,2,3,4,5,6,7,7,8,9,10,0x7fffffff]);s.group=case%5
  s.camera_mode=rng.randrange(-1,6);s.camera_clip=rng.randrange(4);s.auxiliary_mode=rng.randrange(-1,5)
  s.state_721ee0=rng.randrange(-1,6);s.state_721ee4=rng.randrange(-1,6)
  for i in range(3):s.camera_values[i]=rng.getrandbits(32)
  for i in range(5):
   for j in range(4):s.camera_table[i][j]=rng.getrandbits(32)
  s.camera_request,s.camera_cached,s.camera_event=-4,88,97
  s.clock_sample=rng.getrandbits(32);s.previous_clock=rng.choice([0,1,0xfffffffc,30000]);s.finish_elapsed=rng.choice([0,1,29999,30000,30001,0xffffffff])
  s.finish_fade_stage=rng.choice([0,3,3,3,5]);s.finish_blocked=rng.choice([0,0,1,255]);s.transition_action=rng.choice([0,1,7,255]);s.curtain_wanted=0;s.camera_manual=rng.choice([0,1,2,255])
  inp=Input((C.c_uint32*11)(*[rng.getrandbits(32) for _ in range(11)]));working=rng.randbytes(40);work=(C.c_uint8*40).from_buffer_copy(working)
  clocks=[rng.getrandbits(32),rng.getrandbits(32)];pressed=case%2;result=rng.choice([0,1,255,256,0x80000000,0xffffffff]);mutate=rng.randrange(-1,len(SPECS))
  changes=dict(phase=rng.randrange(10),camera_mode=rng.randrange(5),camera_clip=rng.randrange(4),auxiliary_mode=rng.randrange(5),group=rng.randrange(5))
  expected=n.run(s,working,inp,clocks,pressed,result,mutate,changes);trace=[];clock_index=0
  @Invoke
  def invoke(_,call,ret,err):
   c=call.contents;trace.append((c.operation,tuple(c.input.words) if c.with_input else (),tuple(c.args[:c.count]),snap(s),bytes(work)))
   if c.operation==mutate:
    for name,value in changes.items():setattr(s,name,value)
   ret[0]=result;return 1
  @Clock
  def clock(_,out,err):
   nonlocal clock_index
   out[0]=clocks[clock_index];clock_index+=1;trace.append(('clock',out[0]));return 1
  @Key
  def key(_,code,out,err):trace.append(('key',code));out[0]=pressed;return 1
  ops=Ops(None,invoke,clock,key)
  assert lib.bk_ending_frame_step(C.byref(s),work,C.byref(inp),C.byref(ops),e),e.value
  assert (trace,snap(s),bytes(work))==expected,(case,trace,expected)
  total+=len(trace);flag_writes+=sum(a!=b for a,b in zip(working,bytes(work)));transitions+=s.transition_action==7 and s.curtain_wanted==1
  operations.update(x[0] for x in trace if isinstance(x[0],int))
 assert operations==set(range(len(SPECS))),operations
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=20000,observed_calls=total,changed_flag_bytes=flag_writes,finish_requests=transitions,operations=sorted(operations),max_error=0,scope=__doc__)
 (ROOT/'local/original-ending-frame-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS ending frame',report,flush=True)
if __name__=='__main__':main()
