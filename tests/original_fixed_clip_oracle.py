"""40168c active-slot request +4021a1 fixed-call scheduler, real and synthetic.
Only ANIM submission observed; native selection, automatic chaining and clock
math run unmodified. Interleaving with4026fe checks shared state semantics.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from original_clip_oracle import Native,Archive
from original_clip_edits_oracle import Edit,bind as bind_edits
from clip_binding import library,State,Sample
from model_binding import ROOT

def bind(lib):
 bind_edits(lib)
 lib.bk_clip_player_create_loaded.argtypes=[C.c_void_p,C.c_void_p];lib.bk_clip_player_create_loaded.restype=C.c_void_p
 lib.bk_clip_request_active.argtypes=[C.c_void_p,C.c_uint,C.c_void_p]
 lib.bk_clip_advance_frame.argtypes=[C.c_void_p,C.POINTER(Sample),C.c_void_p]
 lib.bk_clip_frame_clock.argtypes=[C.c_void_p,C.c_uint,C.POINTER(C.c_int32),C.POINTER(C.c_int32)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);e=C.create_string_buffer(256);rng=random.Random(0x4021a1);frames=states=selections=chains=0;worst=0.;records=[]
 def equal(got,want,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(got,want)):
   if g==w:continue
   diff=abs(g-w)/max(1,abs(w));worst=max(worst,diff);assert math.isfinite(diff) and diff<1e-6,(label,i,g,w)
 def run(raw,label,steps,authored=True):
  nonlocal frames,states,selections,chains
  s=lib.bk_clip_set_decode(raw,len(raw),e);assert s,(label,e.value)
  p=lib.bk_clip_player_create_loaded(s,e) if authored else lib.bk_clip_player_create(s,e);copy=lib.bk_clip_player_create(s,e);assert p and copy,(label,e.value)
  try:
   n.bind(raw,authored);active=[i for i in range(128) if lib.bk_clip_definition(s,i).contents.active];assert active
   if not authored:
    n.select(active[0],3);assert lib.bk_clip_request_mode(p,active[0],1,e)
   for step in range(steps):
    before=n.state()[0]
    if step%13==0:
     slot=active[(step//13)%len(active)]
     n.call(0x40168c,struct.pack('<II',n.obj,slot));assert lib.bk_clip_request_active(p,slot,e),e.value;selections+=1
    if step%23==0:
     slot=n.state()[0];n.call(0x40168c,struct.pack('<II',n.obj,slot));assert lib.bk_clip_request_active(p,slot,e);selections+=1
    if step%19==0:
     # Repeated ordinary configured request can differ from active.
     slot=active[0];n.select(slot,3);assert lib.bk_clip_request_mode(p,slot,1,e)
    if step%31==0:
     current=n.state()[0];value=lib.bk_clip_definition(s,current).contents.start+3
     edit=Edit(current,4,0,0,value);assert lib.bk_clip_edit(p,C.byref(edit),1,e);n.uc.mem_write(n.obj+0x190+current*156+0x60,struct.pack('<f',value))
    if step%17==3:
     dt=C.c_float([0,.003,.01][step%3]).value;expected=n.step(dt);assert lib.bk_clip_advance(p,dt,C.byref(sample:=Sample()),e),(label,step,e.value)
    else:
     n.call(0x4021a1,struct.pack('<I',n.obj));assert len(n.calls)==1
     expected=n.calls[0];assert lib.bk_clip_advance_frame(p,C.byref(sample:=Sample()),e),(label,step,e.value)
    equal([getattr(sample,k) for k,_ in Sample._fields_],expected,(label,step,'sample'))
    st=State();assert lib.bk_clip_state(p,C.byref(st));equal([getattr(st,k) for k,_ in State._fields_],n.state(),(label,step,'state'))
    if st.slot!=before:chains+=1
    for slot in range(128) if step%17==0 else [before,st.slot]:
     interval=C.c_int32();counter=C.c_int32();assert lib.bk_clip_frame_clock(p,slot,C.byref(interval),C.byref(counter));off=n.obj+0x190+slot*156
     want=struct.unpack('<2i',n.uc.mem_read(off+0x68,8));assert (interval.value,counter.value)==want,(label,step,slot,interval.value,counter.value,want);states+=1
    if step%29==0:
     assert lib.bk_clip_player_copy(copy,p);ci=C.c_int32();cc=C.c_int32();pi=C.c_int32();pc=C.c_int32()
     assert lib.bk_clip_frame_clock(copy,st.slot,C.byref(ci),C.byref(cc)) and lib.bk_clip_frame_clock(p,st.slot,C.byref(pi),C.byref(pc));assert (ci.value,cc.value)==(pi.value,pc.value)
    frames+=1
  finally:lib.bk_clip_player_destroy(copy);lib.bk_clip_player_destroy(p);lib.bk_clip_set_destroy(s)
 for case in range(100):
  raw=bytearray(0x5190);raw[:6]=raw[256:262]=b'test.x'
  for slot in range(3):
   off=512+0x190+slot*156;start,end=(10,30) if case%3 else (30,10)
   struct.pack_into('<i',raw,off,(case+slot)%2);struct.pack_into('<i',raw,off+0x48,(case+slot)%3);struct.pack_into('<i',raw,off+0x50,4+case%7)
   struct.pack_into('<2f',raw,off+0x54,start,end);struct.pack_into('<3f',raw,off+0x5c,.4,start,0)
   interval=[0,1,2,5,-1,-2147483648,2147483647][(case+slot)%7];counter=[0,1,-1,2147483647,-2147483648][(case+slot)%5]
   struct.pack_into('<2i',raw,off+0x68,interval,counter)
   struct.pack_into('<3i',raw,off+0x70,case%2,(slot+1)%3,case%3);struct.pack_into('<f',raw,off+0x7c,[0,1e-7,2e-6,5][case%4])
  run(bytes(raw),('synthetic',case),240)
 print('PASS synthetic fixed/seconds scheduler',frames,states,flush=True)
 # Seven actual BOM auxiliary XANs and cameras/NPCs exercise authored scalars
 # and exactly the slot0 request used at the end of4a4dc3.
 names=[(8,f'h{i:02}_01.xan') for i in range(1,6)]+[(11,'h03_30.xan'),(11,'h03_31.xan'),(4,'cam00_02.xan'),(1,'h00_80.xan')]
 for pack,name in names:
  arc=Archive(a.data/f'bk3_{pack:02}.pp');raw=arc.read(next(x for x in arc.entries if x.name==name));run(raw,(pack,name),240);records.append(dict(pack=pack,name=name,sha256=hashlib.sha256(raw).hexdigest(),frames=240));print('PASS',pack,name,flush=True)
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,frame_counter_checks=states,active_requests=selections,active_changes=chains,max_normalized_error=worst,assets=records,scope=__doc__)
 (ROOT/'local/original-fixed-clip-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',frames,states,worst,flush=True)
if __name__=='__main__':main()
