"""Five real bk3_08 actors with original runtime clip writes,4018c8 and4026fe.
Original SRT and hierarchy publication run with actual authored tracks.
Explicit edit sequences and placements, not full ending event simulation.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW,UC_X86_REG_EAX
from original_actor_phase_oracle import Native as ActorNative,bind
from original_clip_edits_oracle import Edit
from playback_binding import library
from model_binding import ROOT,decode
from clip_binding import State
from bk3_assets import Archive
class Native(ActorNative):
 # Full actor blend, like the existing dialogue/selection caller oracle.
 # The generic single-matrix20M budget expires inside ordinary D3DX math.
 def call(self,addr,args):
  self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
  self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
  self.uc.emu_start(addr,self.stop,count=400000000)
  assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop,(hex(addr),hex(self.uc.reg_read(UC_X86_REG_EIP)))
  return self.uc.reg_read(UC_X86_REG_EAX)
class Timing(C.Structure):_fields_=[(n,C.c_float) for n in ['start','end','source']]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();lib=library();bind(lib);arc=Archive(a.data/'bk3_08.pp');e=C.create_string_buffer(256);worst=0.;matrices=frames=0
 lib.bk_actor_pose_edit_clips.argtypes=[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p]
 lib.bk_actor_pose_request_mode.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p]
 lib.bk_actor_pose_advance.argtypes=[C.c_void_p,C.c_int,C.c_float,C.c_void_p]
 lib.bk_actor_pose_timing.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Timing)]
 def equal(got,want,label):
  nonlocal worst
  for j,(g,w) in enumerate(zip(got,want)):
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(label,j,g,w)
 for group in range(1,6):
  name=f'h{group:02}_00';raw=arc.read(next(x for x in arc.entries if x.name==name+'.x'));xan=arc.read(next(x for x in arc.entries if x.name==name+'.xan'))
  rc,model,msg=decode(lib,raw);assert rc==1,msg;clips=lib.bk_clip_set_decode(xan,len(xan),e);assert clips,e.value;actor=None
  try:
   vm=Native(exe);vm.bind(raw,model.contents);vm.bind_clip(xan,vm.root,4,False);origin=(C.c_float*3)(0,0,0);vm.place_actor(origin,0)
   actor=lib.bk_actor_pose_create(model,clips,vm.root,None,origin,0,4,0,e);assert actor,e.value
   vm.publish();lib.bk_actor_pose_publish(actor)
   def check(label):
    nonlocal matrices
    for frame in range(model.contents.frame_count):
     for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0)]:
      equal(getattr(lib,fn)(actor,frame)[:16],vm.floats(vm.frames+frame*0x400+off,16),(name,label,frame,fn));matrices+=1
    state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));equal([getattr(state,k) for k,_ in State._fields_],vm.state(),(name,label,'clock'))
   check('initial')
   for step in range(180):
    slot=[4,5,9,13][(step//15)%4];target=[5,9,13,4][(step//15)%4];reset=[6,7,10,11][step%4]
    timing=Timing();assert lib.bk_actor_pose_timing(actor,reset,C.byref(timing))
    batch=(Edit*3)(Edit(slot,1,step%3!=0,0,0),Edit(slot,2,0,target,0),Edit(reset,4,0,0,timing.start))
    assert lib.bk_actor_pose_edit_clips(actor,batch,3,e),e.value
    for edit in batch:
     ptr=vm.clip+0x190+edit.slot*156
     if edit.fields&1:vm.word(ptr+0x70,edit.chain)
     if edit.fields&2:vm.word(ptr+0x74,edit.next)
     if edit.fields&4:vm.vector(ptr+0x60,[edit.source])
    assert lib.bk_actor_pose_request_mode(actor,slot,1,e),e.value
    vm.call(0x4018c8,struct.pack('<II',vm.clip,slot));check((step,'before advance'))
    dt=C.c_float([0,.016,.1,.5,2][step%5]).value
    assert lib.bk_actor_pose_advance(actor,-1,dt,e),e.value
    try:vm.call(0x4026fe,struct.pack('<If',vm.clip,dt))
    except AssertionError:
     print('Native stop',name,step,dt,hex(vm.uc.reg_read(UC_X86_REG_EIP)),vm.state(),flush=True);raise
    check((step,'held world'))
    vm.publish();lib.bk_actor_pose_publish(actor);check((step,'published'));frames+=1
   print(name,'PASS',flush=True)
  finally:lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),actors=5,frames=frames,matrices=matrices,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-actor-clip-edits-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
