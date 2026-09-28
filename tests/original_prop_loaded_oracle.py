"""Prop XAN loading: preserve authored/empty slots through first requests.
Original native scheduler and subsequent requests run unchanged; animation
submission is recorded at the same independently verified service boundary.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from original_clip_oracle import Native,Archive
from clip_binding import library,State,Sample
from model_binding import ROOT

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256)
 lib.bk_clip_player_create_loaded.argtypes=[C.c_void_p,C.c_void_p];lib.bk_clip_player_create_loaded.restype=C.c_void_p
 a=Archive(args.data/'bk3_07.pp');names=['h90_00','h91_00','h92_00','h93_00','h93_10','h93_11','h93_12','h93_13','h93_14','h94_00','h95_00','h95_01','h96_00','h98_00','h98_10','h98_20','h98_30'];states=samples=0;rows=[]
 for name in names:
  data=a.read(next(x for x in a.entries if x.name==name+'.xan'));s=lib.bk_clip_set_decode(data,len(data),e);assert s,(name,e.value);p=lib.bk_clip_player_create_loaded(s,e);assert p,(name,e.value);n.bind(data,True);slots=[i for i in range(128) if lib.bk_clip_definition(s,i).contents.active];before=states
  try:
   for i in range(361):
    st=State();assert lib.bk_clip_state(p,C.byref(st));actual=[getattr(st,k) for k,_ in st._fields_];wanted=n.state();assert actual==wanted,(name,i,actual,wanted);states+=1
    if i==360:break
    target=0 if i<80 or not slots else slots[(i//29)%len(slots)]
    # Empty slot0 can only be repeated at its original requested/active pair.
    if i>=80 and not slots:target=0
    n.select(target,2);assert lib.bk_clip_request(p,target,e),(name,i,e.value)
    dt=C.c_float([0,1/120,.02,.1,.5,2][i%6]).value;want=n.step(dt);out=Sample();assert lib.bk_clip_advance(p,dt,C.byref(out),e),e.value;assert [getattr(out,k) for k,_ in out._fields_]==list(want),(name,i,list(want));samples+=1
   rows.append(dict(name=name,sha256=hashlib.sha256(data).hexdigest(),states=states-before,active_slots=slots))
  finally:lib.bk_clip_player_destroy(p);lib.bk_clip_set_destroy(s)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),states=states,samples=samples,rows=rows,scope=__doc__)
 (ROOT/'local/original-prop-loaded-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
