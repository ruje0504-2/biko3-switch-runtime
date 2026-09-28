"""Actual4e0956/4ad2bf with complete real bk3_06 name existence at loader IO.
Verifies the finite known-absence policy: stopped old buffers are released,
missing native buffers stay null and Play is not invoked. Existing table
assets must remain required. This is null-buffer control, not decoded PCM.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_ending_audio_oracle import Native as Base
from model_binding import ROOT,library
from bk3_assets import Archive
class Native(Base):
 def hook(self,u,a,size,_):
  if a!=0x4a06df:return super().hook(u,a,size,_)
  sp=u.reg_read(UC_X86_REG_ESP);path,name=struct.unpack('<2I',u.mem_read(sp+4,8));assert self.string(path)=='\\bk3_06.pp'
  name=self.string(name);present=name.lower() in self.names;slot=self.slot
  self.commands.append((1,slot,'bk3_06',name,int(present),0));self.present[slot]=present;self.playing[slot]=False
  u.reg_write(UC_X86_REG_EAX,self.buffers+slot*32 if present else 0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0])
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);archive=a.data/'bk3_06.pp';n.names={x.name.lower() for x in Archive(archive).entries};lib=library();lib.bk_ending_sound_absent_speech.argtypes=[C.c_char_p]
 candidates=set()
 for g in range(1,6):
  for category,nums in [(2,[4,5,15,16,18,23,24,26,31,32,34,39,40,42,47,48,50,55,56,58,6,-1,17,25,27,33,35,41,43,49,57,59]),(0,[2,4,6])]:
   candidates.update(f'PH{g}{category:02d}{v:02d}.wav' for v in nums)
  for v in range(2):
   for selection in range(1,4):candidates.update(f'PH{g}{2+3*v}{selection}{cue:02d}.wav' for cue in [14,15,16,20,21,22,23])
 missing=[];loads=plays=0
 for name in sorted(candidates):
  present=name.lower() in n.names;assert lib.bk_ending_sound_absent_speech(name.encode())==int(not present),name
  if not present:missing.append(name)
  for slot in [0,1]:
   n.slot=slot;n.commands=[];n.present[slot]=True;n.playing[slot]=True;n.volumes[slot]=-700;n.word(0x722334+slot*0x120,n.buffers+slot*32)
   n.u.mem_write(0x300e000,name.encode()+b'\0');n.call(0x4e0956,struct.pack('<Ii',0x300e000,slot))
   ptr=struct.unpack('<I',n.u.mem_read(0x722334+slot*0x120,4))[0];assert bool(ptr)==present
   n.call(0x4ad2bf,struct.pack('<Iii',ptr,0,-700))
   assert n.commands[:3]==[(3,slot,'-','-',0,0),(0,slot,'-','-',0,0),(1,slot,'bk3_06',name,int(present),0)],n.commands
   assert n.commands[3:]==([(2,slot,'-','-',0,-700)] if present else []),n.commands
   loads+=1;plays+=present
 for name in [b'',b'PH10219.wav',b'missing.wav',b'PH60218.wav']:assert not lib.bk_ending_sound_absent_speech(name)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),candidates=len(candidates),missing=missing,loads=loads,plays=plays,exact=True,scope=__doc__)
 (ROOT/'local/original-ending-speech-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS speech bank',len(candidates),len(missing),loads,plays)
if __name__=='__main__':main()
