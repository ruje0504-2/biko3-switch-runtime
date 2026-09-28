"""Per-instance XAN chain/next/source writes then original4018c8/4026fe.
Fifteen real bk3_08 XANs, immutable sibling/copy checks; original scheduler
and automatic chain run unchanged, only pose-submission observed here.
These are explicit edit sequences, not recovered full495d92 gameplay.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from original_clip_oracle import Native,Archive
from clip_binding import library,State,Sample
from model_binding import ROOT
class Edit(C.Structure):
 _fields_=[('slot',C.c_uint),('fields',C.c_uint),('chain',C.c_int32),('next',C.c_int32),('source',C.c_float)]
def bind(lib):
 lib.bk_clip_edit.argtypes=[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p]
 lib.bk_clip_link.argtypes=[C.c_void_p,C.c_uint,C.POINTER(C.c_int32),C.POINTER(C.c_int32)]
 lib.bk_clip_player_copy.argtypes=[C.c_void_p,C.c_void_p]
 lib.bk_clip_request_mode.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x495d92);e=C.create_string_buffer(256);arc=Archive(a.data/'bk3_08.pp');records=[];worst=0.;frames=edits=chains=copies=0
 def equal(got,want,label):
  nonlocal worst
  for i,(x,y) in enumerate(zip(got,want)):
   d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<1e-6,(label,i,x,y)
 for entry in arc.entries:
  if not entry.name.endswith('.xan'):continue
  raw=arc.read(entry);s=lib.bk_clip_set_decode(raw,len(raw),e);assert s,e.value
  p=lib.bk_clip_player_create(s,e);q=lib.bk_clip_player_create(s,e);copy=lib.bk_clip_player_create(s,e);assert p and q and copy,e.value
  try:
   definitions=[lib.bk_clip_definition(s,i).contents for i in range(128)];unchanged=[bytes(d) for d in definitions];active=[i for i,d in enumerate(definitions) if d.active];slot=active[0]
   n.bind(raw);n.select(slot,3);assert lib.bk_clip_request_mode(p,slot,1,e)
   for frame in range(800):
    if frame%7==0:
     current=n.state()[0];other=rng.choice(active);reset=rng.choice(active)
     batch=(Edit*4)(Edit(current,1,frame%3!=0,0,0),Edit(current,2,0,other,0),Edit(reset,4,0,0,definitions[reset].start),Edit(current,1,frame%5!=0,0,0))
     assert lib.bk_clip_edit(p,batch,4,e),e.value
     for edit in batch:
      ptr=n.obj+0x190+edit.slot*156
      if edit.fields&1:n.word(ptr+0x70,edit.chain)
      if edit.fields&2:n.word(ptr+0x74,edit.next)
      if edit.fields&4:n.uc.mem_write(ptr+0x60,struct.pack('<f',edit.source))
     edits+=4
    if frame%31==0:
     slot=rng.choice(active);n.select(slot,3);assert lib.bk_clip_request_mode(p,slot,1,e)
    if frame%29==0:
     assert lib.bk_clip_player_copy(copy,p);cs=State();ps=State();assert lib.bk_clip_state(copy,C.byref(cs)) and lib.bk_clip_state(p,C.byref(ps));assert bytes(cs)==bytes(ps)
     for i in active:
      a1=C.c_int32();a2=C.c_int32();b1=C.c_int32();b2=C.c_int32();assert lib.bk_clip_link(copy,i,C.byref(a1),C.byref(a2));assert lib.bk_clip_link(p,i,C.byref(b1),C.byref(b2));assert (a1.value,a2.value)==(b1.value,b2.value)
     copies+=1
    dt=C.c_float([0,.001,.016,.1,.5,2][frame%6]).value;previous=n.state()[0];wanted=n.step(dt);sample=Sample();assert lib.bk_clip_advance(p,dt,C.byref(sample),e),e.value
    equal([getattr(sample,k) for k,_ in Sample._fields_],wanted,(entry.name,frame,'sample'))
    state=State();assert lib.bk_clip_state(p,C.byref(state));equal([getattr(state,k) for k,_ in State._fields_],n.state(),(entry.name,frame,'state'))
    chains+=state.slot!=previous;frames+=1
    for i in range(128):
     ch=C.c_int32();to=C.c_int32();assert lib.bk_clip_link(p,i,C.byref(ch),C.byref(to))
     expected=struct.unpack('<2i',n.uc.mem_read(n.obj+0x190+i*156+0x70,8));assert (ch.value,to.value)==expected
     assert lib.bk_clip_link(q,i,C.byref(ch),C.byref(to));assert (ch.value,to.value)==(definitions[i].chain,definitions[i].next)
     assert bytes(lib.bk_clip_definition(s,i).contents)==unchanged[i]
   records.append(entry.name)
  finally:lib.bk_clip_player_destroy(p);lib.bk_clip_player_destroy(q);lib.bk_clip_player_destroy(copy);lib.bk_clip_set_destroy(s)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),assets=records,frames=frames,edits=edits,automatic_chains=chains,instance_copies=copies,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-clip-edits-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
