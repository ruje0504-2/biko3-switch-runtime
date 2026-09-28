"""Export original media commands for two-mixer real PCM integration checks."""
import argparse,ctypes as C,json,hashlib
from pathlib import Path
from original_dialogue_media_oracle import Native,Music

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);rows=[];commands=0
 names=[b'PH10101.wav',b'PH20001.wav',b'PH30001.wav',b'PH40001.wav',b'PH50001.wav']
 def record(group,tick,dt,master,sp,mp,pause,speech,music,after,events):
  nonlocal commands
  rows.append(' '.join(map(str,[group,tick,format(dt,'.17g'),master,sp,mp,pause,speech.decode() or '-',music.decode() or '-',*after,len(events)])))
  for kind,pack,name,volume,loop in events:rows.append(f'{kind} {(pack or b"-").decode()} {(name or b"-").decode()} {volume} {loop}')
  commands+=len(events)
 for group in range(5):
  s=Music(-1234,255);sp=mp=1;speech=names[group];music=b'bg001.wav'
  volume,wanted,mp=n.run(2,s,0,mp,music,0,-900);s=Music(volume,wanted)
  record(group,-1,0,-900,sp,1,0,speech,music,(s.volume,s.wanted,sp,mp),n.events)
  for tick in range(360):
   dt=C.c_float([0,1/60,1/53,.1,.22][tick%5]).value;master=[-900,-2000,-8000][tick//120]
   if tick in [1,32,100,180,270]:sp=1;speech=b'se001.wav' if tick==100 else names[group]
   if tick in [55,150,220,330]:mp=1;music=b'' if tick==150 else b'bg002.wav' if tick==55 else b'bg001.wav'
   before=(sp,mp);pause=int(tick in [40,200]);_,_,sp=n.run(0,s,1,sp,speech,dt,-700-group*200);events=list(n.events)
   volume,wanted,mp=n.run(1,s,1,mp,music,dt,master);events+=n.events;s=Music(volume,wanted)
   record(group,tick,dt,master,*before,pause,speech,music,(volume,wanted,sp,mp),events)
 a.output.write_text('\n'.join(rows)+'\n')
 result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=1800,initializations=5,commands=commands,trace_sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),scope=__doc__)
 a.output.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
