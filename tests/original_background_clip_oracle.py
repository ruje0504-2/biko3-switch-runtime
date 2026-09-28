"""Actual background/door/snow XAN timelines, including static/zero-duration
clips and door's retained -Inf rate with original unordered x87 clamping.
Original selection, scheduler, chaining and animation sample commands run x86.
"""
import argparse,ctypes as C,hashlib,json,math
from pathlib import Path
from original_clip_oracle import Native,Archive
from clip_binding import library,State,Sample
from model_binding import ROOT

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256)
 lib.bk_clip_player_create_loaded.argtypes=[C.c_void_p,C.c_void_p];lib.bk_clip_player_create_loaded.restype=C.c_void_p
 records=[];states=samples=zero_duration=0
 for pack in ['bk3_03','bk3_17','bk3_20']:
  arc=Archive(args.data/(pack+'.pp'))
  for entry in arc.entries:
   if not entry.name.endswith('.xan'):continue
   raw=arc.read(entry);s=lib.bk_clip_set_decode(raw,len(raw),error);assert s,(entry.name,error.value);p=lib.bk_clip_player_create_loaded(s,error);assert p,(entry.name,error.value);n.bind(raw,True)
   try:
    def check(label):
     nonlocal states
     st=State();assert lib.bk_clip_state(p,C.byref(st));actual=[getattr(st,k) for k,_ in st._fields_];wanted=n.state();assert actual==wanted,(pack,entry.name,label,actual,wanted);states+=1
    check('loaded')
    door='door' in entry.name
    if not door and entry.name!='yuki.xan':n.select(0,1);assert lib.bk_clip_select(p,0,1,error);check('initial')
    if door:
     # Same requested4 preserves -Inf. Native unordered comparisons clamp
     # the intermediate NaN from loop_start0 * -Inf back to the end tick.
     held=State();assert lib.bk_clip_state(p,C.byref(held));assert not math.isfinite(held.rate)
     n.select(4,2);assert lib.bk_clip_request(p,4,error);out=Sample();want=n.step(C.c_float(.01).value);assert lib.bk_clip_advance(p,.01,C.byref(out),error);assert [getattr(out,k) for k,_ in out._fields_]==list(want);check('clamped nonfinite first step')
     n.select(2,2);assert lib.bk_clip_request(p,2,error)
    slots=[i for i in range(128) if lib.bk_clip_definition(s,i).contents.active]
    zero_duration+=sum(lib.bk_clip_definition(s,i).contents.duration==0 for i in slots)
    for frame in range(600):
     if door:target=[2,4,2,4,3,1][(frame//100)%6]
     else:target=slots[(frame//73)%len(slots)] if slots else 0
     n.select(target,2);assert lib.bk_clip_request(p,target,error),(entry.name,target,error.value)
     dt=C.c_float([0,1/120,1/60,.1,.25,1][frame%6]).value;wanted=n.step(dt);out=Sample();assert lib.bk_clip_advance(p,dt,C.byref(out),error),(entry.name,frame,error.value);actual=[getattr(out,k) for k,_ in out._fields_];assert actual==list(wanted),(entry.name,frame,actual,wanted);samples+=1;check(frame)
    records.append(dict(pack=pack,file=entry.name,sha256=hashlib.sha256(raw).hexdigest()))
    print(pack,entry.name,'PASS',flush=True)
   finally:lib.bk_clip_player_destroy(p);lib.bk_clip_set_destroy(s)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=len(records),states=states,samples=samples,active_zero_duration_clips=zero_duration,records=records,scope=__doc__)
 (ROOT/'local/original-background-clip-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
