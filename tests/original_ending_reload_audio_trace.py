"""Native pause trace for the real PCM probe; resource phase0 is an explicit
no-resource fixture. Does not model full ending UI or destructor lifetime.
"""
import argparse,ctypes as C,hashlib,json
from pathlib import Path
from original_ending_reload_oracle import Native,State
from model_binding import ROOT

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);n.image_shapes=0;lines=[];queries=pauses=waits=0
 for g in range(5):
  for v in range(2):
   lines.append(f'ENTRY {g} {v}');flags=[0]*48;flags[47]=1;present=[1]*43+[0]*4+[1];s=State();s.frame.group=g;s.previous=8
   for t in range(240):
    pos=t%120;event={0:1,30:2,50:3,100:2,110:3}.get(pos,0);mode=2 if pos in [15,30,90,100] else 1 if pos==75 else 0
    if event==1 or event==3:flags=[int(x) for x in present]
    elif event==2:flags[0]=0
    if mode==2:s.frame.transition_action=63;s.frame.curtain_wanted=1;s.frame.finish_blocked=3
    mask=0
    if mode:
     trace,raw,early=n.run(s,mode,present,flags,[0]*48,1,0,[]);s=State.from_buffer_copy(raw);assert not early
     for x,_ in trace:
      if x[0]=='pause':mask|=1<<x[1];flags[x[1]]=0;pauses+=1
      elif x[0]=='status':queries+=1
      else:assert x[0] in ['present','light']
     waits+=mode==2 and s.frame.curtain_wanted==1
    lines.append(f'FRAME {event} {mode} {mask:x} {s.frame.transition_action} {s.frame.curtain_wanted}')
 (ROOT/'local/ending-reload-audio-trace.txt').write_text('\n'.join(lines)+'\n')
 r=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=10,frames=2400,status_queries=queries,pause_calls=pauses,voice_waits=waits,scope=__doc__)
 (ROOT/'local/original-ending-reload-audio-trace.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
if __name__=='__main__':main()
