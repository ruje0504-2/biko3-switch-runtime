"""Original51765d/517c8e metadata parser, actual resources and retained flags.
Only packed file read, allocation and release are service boundaries; all
scans, CRT byte comparisons/atoi/strncpy and cursor writes execute original.
"""
import argparse,ctypes as C,hashlib,json,random,re,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_prop_route_oracle import Native as Base
from original_message_oracle import Message
from model_binding import ROOT,library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class State(C.Structure):
 _fields_=[('cursor',C.c_uint32),('first_label',C.c_int32),('last_label',C.c_int32),('current_label',C.c_int32),('filename',C.c_char*256),('text',Message),('code_c',C.c_int32),('code_f',C.c_int32),('code_e',C.c_int32),('code_m',C.c_int32),('previous_sound',C.c_char*256),('sound',C.c_char*256),('image',C.c_char*256),('music',C.c_char*256),('sound_pending',C.c_uint8),('image_kind',C.c_uint8),('music_pending',C.c_uint8)]
FIELDS=[('cursor',4,'I'),('first_label',0x10c,'i'),('last_label',0x110,'i'),('current_label',0x114,'i'),('filename',12,'256s'),('code_c',0x518,'i'),('code_f',0x51c,'i'),('code_e',0x520,'i'),('code_m',0x524,'i'),('previous_sound',0x52c,'256s'),('sound',0x64c,'256s'),('image',0x74c,'256s'),('music',0x84d,'256s'),('sound_pending',0x644,'B'),('image_kind',0x84c,'B'),('music_pending',0x94d,'B')]
class Native(Base):
 source,dest=0x4000000,0x5000000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(self.source,0x100000);self.u.mem_map(self.dest,0x100000)
  for addr in [0x51ceee,0x52043e,0x520538]:self.u.hook_add(UC_HOOK_CODE,self.boundary,begin=addr,end=addr)
  self.u.mem_write(0x3002000,b'assets.pp\0')
 def boundary(self,u,addr,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0
  if addr==0x51ceee:
   u.mem_write(args[2],struct.pack('<I',self.source));u.mem_write(args[3],struct.pack('<I',len(self.raw)))
  elif addr==0x52043e:assert args[0]==len(self.raw);result=self.dest
  else:assert args[0]==self.source
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def call(self,address,args):
  u=self.u;u.mem_write(self.stack,struct.pack('<I',self.stop)+args);u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(address,self.stop,count=30000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
  return u.reg_read(UC_X86_REG_EAX)&255
 def write(self,s):
  u=self.u;p=self.actor;u.mem_write(p,bytes(0x960));u.mem_write(p,struct.pack('<I',self.dest))
  for name,off,fmt in FIELDS:u.mem_write(p+off,struct.pack('<'+fmt,getattr(s,name)))
  u.mem_write(p+0x118,bytes(s.text.bytes));u.mem_write(p+0x528,struct.pack('<I',s.text.carriage_returns))
 def read(self):
  s=State();u=self.u;p=self.actor
  for name,off,fmt in FIELDS:setattr(s,name,struct.unpack('<'+fmt,u.mem_read(p+off,struct.calcsize(fmt)))[0])
  raw=bytes(u.mem_read(p+0x118,1024));s.text.bytes[:]=raw;s.text.length=len(raw.split(b'\0')[0]);s.text.carriage_returns=struct.unpack('<I',u.mem_read(p+0x528,4))[0]
  return s
 def open(self,s,raw,filename):
  self.raw=raw;self.u.mem_write(self.source,raw+bytes(256));self.u.mem_write(self.dest,bytes(len(raw)+256));self.u.mem_write(0x3002200,filename+b'\0');self.write(s)
  self.call(0x51765d,struct.pack('<3I',0x3002000,0x3002200,self.actor));return self.read()
 def step(self,s):self.write(s);done=self.call(0x517c8e,struct.pack('<I',self.actor));return self.read(),done

def equal(a,b,label):
 for name,_,_ in FIELDS:assert getattr(a,name)==getattr(b,name),(label,name,getattr(a,name),getattr(b,name))
 assert bytes(a.text)==bytes(b.text),(label,'text',a.text.length,b.text.length,a.text.carriage_returns,b.text.carriage_returns)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x517c8e);error=C.create_string_buffer(256)
 lib.bk_dialogue_open.argtypes=[C.POINTER(State),C.c_void_p,C.c_size_t,C.c_char_p,C.c_void_p];lib.bk_dialogue_next.argtypes=[C.POINTER(State),C.c_void_p,C.c_size_t,C.POINTER(C.c_int),C.c_void_p]
 files=opens=steps=retained=rejects=0;rows=[]
 def seeded(first,last):
  s=State();s.first_label=first;s.last_label=last;s.current_label=-19;s.cursor=123;s.filename=b'old.txt';s.text.bytes[:]=b'z'*1024;s.text.length=1024;s.text.carriage_returns=31
  for name in ['previous_sound','sound','image','music']:setattr(s,name,('retained-'+name).encode())
  s.code_c,s.code_f,s.code_e,s.code_m=21,22,23,24;s.sound_pending,s.image_kind,s.music_pending=7,8,9
  return s
 def run(raw,filename,first,last,limit):
  nonlocal opens,steps,retained
  s=seeded(first,last);wanted=n.open(s,raw,filename);assert lib.bk_dialogue_open(C.byref(s),raw,len(raw),filename,error),(filename,error.value);equal(s,wanted,filename);opens+=1
  for j in range(limit):
   before=s.cursor;wanted,done=n.step(s);actual=C.c_int(-7);assert lib.bk_dialogue_next(C.byref(s),raw,len(raw),C.byref(actual),error),(filename,j,error.value);equal(s,wanted,(filename,j));assert actual.value==done
   steps+=1;retained+=s.cursor==before
   # A media consumer may acknowledge pending commands; parser must retain
   # this live state rather than recreate it from current metadata.
   if j%3==0:s.sound_pending=s.music_pending=s.image_kind=0
   if done:return j+1
  raise AssertionError(('did not finish',filename,first,last,limit))
 archive=Archive(a.data/'bk3_05.pp')
 for e in archive.entries:
  if not e.name.endswith('.txt'):continue
  raw=archive.read(e);ids=[int(x) for x in re.findall(rb'(?m)^#([0-9]{5})',raw)]
  if not ids:
   # i00_10 is an unkeyed area table, not a dialogue script. Native's
   # unbounded label scan would read past it; reject without executing OOB.
   s=seeded(1,2);saved=bytes(s);assert not lib.bk_dialogue_open(C.byref(s),raw,len(raw),e.name.encode(),error);assert bytes(s)==saved;rejects+=1;continue
  # Body parser's range completion includes the final label's text, and only
  # the following call returns1. No fall-through beyond the #end sentinel.
  frames=run(raw,e.name.encode(),ids[0],ids[-1],len(ids)+3);rows.append(dict(file=e.name,labels=len(ids),frames=frames));files+=1
  # Seek later labels while retaining preamble media state.
  for i in sorted(set([len(ids)//2,len(ids)-1])):
   run(raw,e.name.encode(),ids[i],ids[-1],len(ids)-i+3)
  s=seeded(-987,987);wanted=n.open(s,raw,e.name.encode());assert lib.bk_dialogue_open(C.byref(s),raw,len(raw),e.name.encode(),error);equal(s,wanted,'missing label');assert s.cursor==0;opens+=1
 # Synthetic directives in authored order, all3 quote pairs, fixed widths,
 # embedded NUL, nonordered tags, retained endpoint and repeated labels.
 for case in range(96):
  opening,closing=[(0x75,0x76),(0x69,0x6a),(0x67,0x68)][case%3]
  body=b'prefix\x81'+bytes([opening])+b'A\r\nB'+(b'\0tail' if case%4==0 else b'')+b'\x81'+bytes([closing])
  tags=[b'#C01#F02#E03#M04#PT01000#se007',b'#M-2#E+3#F 4#C09',b'#F00',b''][case%4]
  raw=b'#SG01_00\r\n#bg003\r\n#00001 '+tags+b'\r\n'+body+b'\r\n#bg_off\r\n#g00_12\r\n#00002 #C02\r\nplain\r\n#00003 #E08\r\nlast\r\n#end\r\n'
  run(raw,b'synthetic.txt',1,3,8)
  s=seeded(1,3);s.current_label=3;wanted=n.open(s,raw,b'synthetic.txt');assert lib.bk_dialogue_open(C.byref(s),raw,len(raw),b'synthetic.txt',error);equal(s,wanted,'retained current');wanted,done=n.step(s);actual=C.c_int(-1);assert lib.bk_dialogue_next(C.byref(s),None,0,C.byref(actual),error);equal(s,wanted,'endpoint');assert done==actual.value==1;opens+=1;steps+=1
 for raw in [b'x',b'#SG',b'#bg001',b'#00001\n'+b'a'*1025+b'#end',b'#00001 #C',b'#00001\nno terminator']:
  s=seeded(1,2);saved=bytes(s);done=C.c_int(99)
  if not lib.bk_dialogue_open(C.byref(s),raw,len(raw),b'x',error):assert bytes(s)==saved
  else:
   saved=bytes(s);assert not lib.bk_dialogue_next(C.byref(s),raw,len(raw),C.byref(done),error);assert bytes(s)==saved and done.value==99
  rejects+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=files,opens=opens,steps=steps,unchanged_cursors=retained,rejections=rejects,resources=rows,scope='Actual51765d packed metadata/label scan and full517c8e; only packed read/allocation/free hooked. Raw text, flags, all directives and cursor writes. Native unbounded/malformed paths rejected port-only; no media/UI consumer or51a190 claim.')
 (ROOT/'local/original-dialogue-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS dialogue',files,'files',opens,'opens',steps,'steps',rejects,'rejections')
if __name__=='__main__':main()
