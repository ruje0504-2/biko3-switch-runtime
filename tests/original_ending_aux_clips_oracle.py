"""Actual495d92 then4018c8/4026fe with five FAM-designated primary bk3_08 XANs.
No animation scheduler hook: only SRT submission, media and eye textures are
service boundaries. Dynamic slot links and all128 source clocks compared.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX
from original_ending_auxiliary_oracle import Native as Aux,State,Frame,Ops,Active,Write,Request,Audio,Eyes,ADDR,FIELDS,BYTES
from original_clip_oracle import Native as Clip,Archive
from original_clip_edits_oracle import Edit,bind
from clip_binding import library,State as Clock,Sample
from model_binding import ROOT
from original_face_config_oracle import Config,bind as bind_config
class Timing(C.Structure):
 _fields_=[("start",C.c_float),("end",C.c_float),("source",C.c_float)]
class Native(Aux):
 obj=Clip.obj;model=Clip.model;root=Clip.root
 def __init__(self,exe):
  super().__init__(exe);self.uc=self.u
  for addr in [0x4097d6,0x409a94]:self.u.hook_add(UC_HOOK_CODE,self.submit,begin=addr,end=addr)
 def hook(self,u,a,size,p):
  if a==0x4018c8:return
  super().hook(u,a,size,p)
 submit=Clip.submit;bind=Clip.bind;call=Clip.call;select=Clip.select;step=Clip.step;state=Clip.state
 def globals(self,s,f):
  for (name,_),addr in zip(State._fields_,ADDR):self.u.mem_write(addr,bytes(s)[getattr(State,name).offset:getattr(State,name).offset+4])
  for name,a in FIELDS:self.word(a,getattr(f,name))
  for name,a in BYTES:self.u.mem_write(a,bytes([getattr(f,name)]))
  self.voice=-800;self.word(0xbe9a08,-800);self.word(0xbe9a10,-500)
  self.present=[False]*6;self.playing=[False]*6;self.hr=[0]*6
  for i in range(6):self.word(0x722334+i*0x120,0)
  self.word(0x70d374,0);self.word(0x70d384,0);self.trace=[]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256);arc=Archive(a.data/'bk3_08.pp')
 lib.bk_ending_auxiliary_change.argtypes=[C.POINTER(State),C.POINTER(Frame),C.c_int32,C.c_int32,C.c_int32,C.POINTER(Ops),C.POINTER(C.c_int32),C.c_void_p]
 bind_config(lib);lib.bk_clip_timing.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Timing)]
 primary=set();fam_archive=Archive(a.data/"fambom.pp");configs=[]
 for ent in fam_archive.entries:
  if not ent.name.endswith("_00.fam"):continue
  raw=fam_archive.read(ent);config=Config();assert lib.bk_face_config_decode(raw,len(raw),C.byref(config),e),e.value
  primary.add(config.actor_clip.decode());configs.append(dict(fam=ent.name,primary=config.actor_clip.decode(),face_source=config.source_clip.decode()))
 assert len(primary)==5
 frames=changes=chains=0;worst=0.;files=[]
 def equal(a,b,label):
  nonlocal worst
  for x,y in zip(a,b):
   d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<1e-6,(label,x,y,d)
 for ent in arc.entries:
  if ent.name not in primary:continue
  raw=arc.read(ent);clips=lib.bk_clip_set_decode(raw,len(raw),e);assert clips,e.value;p=lib.bk_clip_player_create(clips,e);assert p,e.value
  try:
   n.bind(raw);n.select(4,3);assert lib.bk_clip_request_mode(p,4,1,e),e.value
   s=State(1,0,0,10,0,0,0,.7,0,0);f=Frame();f.group=int(ent.name[1:3])-1;f.phase=6
   @Active
   def active(_,out,err):clock=Clock();ok=lib.bk_clip_state(p,C.byref(clock));out[0]=clock.slot;return ok
   @Write
   def write(_,i,k,v,err):
    ed=Edit(i,[1,2,4][k],v if k==0 else 0,v if k==1 else 0,lib.bk_clip_definition(clips,i).contents.start if k==2 else 0)
    return lib.bk_clip_edit(p,C.byref(ed),1,err)
   @Request
   def request(_,i,err):return lib.bk_clip_request_mode(p,i,1,err)
   @Audio
   def audio(_,c,out,err):out[0]=0;return 1
   @Eyes
   def eyes(_,i,err):return 1
   ops=Ops(None,active,write,request,audio,eyes)
   for frame in range(300):
    if frame%7==0:
     s.variant=(frame//7)%2;s.selection=(frame//14)%3;s.progress=C.c_float((frame%11)/10).value
     proposed=[0,1,3,2,3][(frame//7)%5];n.globals(s,f);n.call(0x495d92,struct.pack('<i',proposed));want=n.read()[:2];wanted_result=n.u.reg_read(UC_X86_REG_EAX)
     result=C.c_int32();assert lib.bk_ending_auxiliary_change(C.byref(s),C.byref(f),proposed,-800,-500,C.byref(ops),C.byref(result),e),(ent.name,frame,e.value)
     assert result.value==wanted_result and (bytes(s),bytes(f))==want,(ent.name,frame,result.value,wanted_result,bytes(s),want[0])
     changes+=1
    dt=C.c_float([.001,.016,.1,.5,2][frame%5]).value;old=n.state()[0];want=n.step(dt);sample=Sample();assert lib.bk_clip_advance(p,dt,C.byref(sample),e),e.value
    equal([getattr(sample,k) for k,_ in Sample._fields_],want,('sample',ent.name,frame))
    state=Clock();assert lib.bk_clip_state(p,C.byref(state));equal([getattr(state,k) for k,_ in Clock._fields_],n.state(),('clock',ent.name,frame));chains+=state.slot!=old
    for i in range(128):
     ch=C.c_int32();to=C.c_int32();assert lib.bk_clip_link(p,i,C.byref(ch),C.byref(to));assert (ch.value,to.value)==struct.unpack('<2i',n.u.mem_read(n.obj+0x190+i*156+0x70,8))
     timing=Timing();assert lib.bk_clip_timing(p,i,C.byref(timing))
     want_source=struct.unpack("<f",n.u.mem_read(n.obj+0x190+i*156+0x60,4))[0];equal([timing.source],[want_source],("source",ent.name,frame,i))
    frames+=1
   files.append(ent.name)
  finally:lib.bk_clip_player_destroy(p);lib.bk_clip_set_destroy(clips)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=files,configs=configs,frames=frames,changes=changes,automatic_chains=chains,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-ending-aux-clips-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
