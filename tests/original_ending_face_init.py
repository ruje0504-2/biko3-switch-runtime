"""Execute real4f2dae post-construction setup/warm-up, with eye IO observed."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_face_controller_oracle import Native
class EndingController(Native):
 def __init__(self,exe):
  super().__init__(exe);self.sequence=None;self.u.hook_add(UC_HOOK_CODE,self.eye,begin=0x4a07da,end=0x4a07da)
 def hook(self,u,a,size,user):
  if a==self.clock_stub and self.sequence is not None:
   assert self.clock_calls<len(self.sequence);self.now=self.sequence[self.clock_calls]
  super().hook(u,a,size,user)
 def eye(self,u,a,size,user):
  self.eye_calls+=1;sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def initialize(self,state,random_state,clocks):
  # Initial expression0 is already selected, so its request reads no clock.
  self.seed(state,random_state);self.sequence=clocks[1:];self.commands=[];self.clock_calls=self.eye_calls=0
  actor,model,config=0x3004000,0x3005000,0x3006000;base=0x200e000
  self.word(actor+0x160,model);self.word(model+0x180,self.face);self.word(model+0x14,0)
  self.u.mem_write(config,bytes(0x3a02));self.word(base+8,0x300a000);self.word(base+0xc,config);self.word(base+0x10,actor);self.word(base-0x10,self.face)
  self.u.reg_write(UC_X86_REG_EBP,base);self.u.reg_write(UC_X86_REG_ESP,base-0x1000);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  self.u.emu_start(0x4f33ae,0x4f34cb,count=300000);assert self.u.reg_read(UC_X86_REG_EIP)==0x4f34cb
  assert self.eye_calls==1 and self.clock_calls==3,(self.eye_calls,self.clock_calls);self.sequence=None
  return list(self.commands)
