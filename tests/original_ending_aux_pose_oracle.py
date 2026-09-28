"""Full495d92 ->4018c8/4026fe ->original SRT and hierarchy publication.
Five actual primary models from bk3_08. Audio is an observing boundary and
optional eye target is absent; actual PCM/eye bindings have separate probes.
Explicit action4 fixture at each run start; not full ending entry/stage logic.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from original_actor_clip_edits_oracle import Native,Timing
from original_actor_phase_oracle import bind
from original_ending_auxiliary_oracle import Native as Aux,State as AuxState,Frame,Ops,Active,Write,Request,Audio,Eyes
from original_ending_aux_clips_oracle import Native as AuxGlobals
from original_clip_edits_oracle import Edit
from playback_binding import library
from model_binding import ROOT,decode
from clip_binding import State
from bk3_assets import Archive

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();lib=library();bind(lib);arc=Archive(a.data/'bk3_08.pp');e=C.create_string_buffer(256);worst=0.;matrices=frames=changes=0
 lib.bk_actor_pose_edit_clips.argtypes=[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p]
 lib.bk_actor_pose_request_mode.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p]
 lib.bk_actor_pose_advance.argtypes=[C.c_void_p,C.c_int,C.c_float,C.c_void_p]
 lib.bk_actor_pose_timing.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Timing)]
 lib.bk_ending_auxiliary_change.argtypes=[C.POINTER(AuxState),C.POINTER(Frame),C.c_int32,C.c_int32,C.c_int32,C.POINTER(Ops),C.POINTER(C.c_int32),C.c_void_p]
 def equal(got,want,label):
  nonlocal worst
  for j,(g,w) in enumerate(zip(got,want)):
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-5,(label,j,g,w)
 for group in range(1,6):
  name=f'h{group:02}_00';raw=arc.read(next(x for x in arc.entries if x.name==name+'.x'));xan=arc.read(next(x for x in arc.entries if x.name==name+'.xan'))
  rc,model,msg=decode(lib,raw);assert rc==1,msg;clips=lib.bk_clip_set_decode(xan,len(xan),e);assert clips,e.value;actor=None
  try:
   vm=Native(exe);vm.bind(raw,model.contents);vm.bind_clip(xan,vm.root,4,False);origin=(C.c_float*3)(0,0,0);vm.place_actor(origin,0)
   observer=Aux.__new__(Aux);observer.u=vm.uc;observer.actor=vm.clip
   for addr in [0x4946b4,0x49490a,0x4a07a9,0x4ad2bf]:vm.uc.hook_add(UC_HOOK_CODE,observer.hook,begin=addr,end=addr)
   actor=lib.bk_actor_pose_create(model,clips,vm.root,None,origin,0,4,0,e);assert actor,e.value
   vm.publish();lib.bk_actor_pose_publish(actor)
   s=AuxState(1,0,0,10,0,0,0,.7,0,0);f=Frame();f.group=group-1;f.phase=6
   @Active
   def active(_,out,err):clock=State();ok=lib.bk_actor_pose_state(actor,C.byref(clock));out[0]=clock.slot;return ok
   @Write
   def write(_,i,k,v,err):
    timing=Timing();assert lib.bk_actor_pose_timing(actor,i,C.byref(timing));edit=Edit(i,[1,2,4][k],v if k==0 else 0,v if k==1 else 0,timing.start if k==2 else 0)
    return lib.bk_actor_pose_edit_clips(actor,C.byref(edit),1,err)
   @Request
   def request(_,i,err):return lib.bk_actor_pose_request_mode(actor,i,1,err)
   @Audio
   def audio(_,c,out,err):out[0]=0;return 1
   @Eyes
   def eyes(_,i,err):return 1
   ops=Ops(None,active,write,request,audio,eyes)
   def check(label):
    nonlocal matrices
    for frame in range(model.contents.frame_count):
     for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0)]:
      equal(getattr(lib,fn)(actor,frame)[:16],vm.floats(vm.frames+frame*0x400+off,16),(name,label,frame,fn));matrices+=1
    state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));equal([getattr(state,k) for k,_ in State._fields_],vm.state(),(name,label,'clock'))
   check('initial')
   for step in range(120):
    if step%40==0:
     vm.call(0x4018c8,struct.pack('<II',vm.clip,4));assert lib.bk_actor_pose_request_mode(actor,4,1,e),e.value
    if step%40<5 or step%7==0:
     proposed=[0,1,3,2,3][step%40 if step%40<5 else (step//7)%5];s.variant=(step//40)%2;s.selection=step//40;s.progress=C.c_float((step%11)/10).value
     AuxGlobals.globals(observer,s,f);observer.word(0x721b28,vm.clip)
     expected=vm.call(0x495d92,struct.pack('<i',proposed));snap=observer.read()[:2];result=C.c_int32()
     assert lib.bk_ending_auxiliary_change(C.byref(s),C.byref(f),proposed,-800,-500,C.byref(ops),C.byref(result),e),(name,step,e.value)
     assert result.value==expected and (bytes(s),bytes(f))==snap,(name,step,'state');changes+=1
    check((step,'before advance'))
    dt=C.c_float(.016 if step%40<5 else [0,.016,.1,.5,2][step%5]).value
    assert lib.bk_actor_pose_advance(actor,-1,dt,e),e.value
    vm.call(0x4026fe,struct.pack('<If',vm.clip,dt));check((step,'held world'))
    vm.publish();lib.bk_actor_pose_publish(actor);check((step,'published'));frames+=1
   print(name,'PASS',flush=True)
  finally:lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),actors=5,frames=frames,changes=changes,matrices=matrices,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-ending-aux-pose-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
