"""Raw message51876a vs original instructions, including CRT sprintf/strncmp/
strncpy. No hooks. Every numeric header in all27 bk3_05 text resources, plus
byte-level synthetic cases. Unsafe unbounded native cases are port-only tests.
"""
import argparse,ctypes as C,hashlib,json,random,re,struct,sys
from pathlib import Path
from original_prop_route_oracle import Native as Base
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from model_binding import ROOT,library
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Message(C.Structure):
 _fields_=[('bytes',C.c_uint8*1024),('length',C.c_size_t),('carriage_returns',C.c_uint32)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x40000)
 def run(self,raw,key,old):
  u=self.u;u.mem_write(0x4000000,raw+b'\0'*16);u.mem_write(0x3000000,bytes([0xa5])*0x600)
  u.mem_write(0x3000000,struct.pack('<I',0x4000000));u.mem_write(0x3000528,struct.pack('<I',old))
  u.mem_write(self.stack,struct.pack('<IiI',self.stop,key,0x3000000));u.reg_write(UC_X86_REG_ESP,self.stack)
  u.emu_start(0x51876a,self.stop,count=5000000)
  assert u.reg_read(UC_X86_REG_EIP)==self.stop,('native instruction bound',len(raw),key,hex(u.reg_read(UC_X86_REG_EIP)))
  return bytes(u.mem_read(0x3000118,1024)),struct.unpack('<I',u.mem_read(0x3000528,4))[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
 exe=a.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256);rng=random.Random(0x51876a)
 lib.bk_message_lookup.argtypes=[C.c_void_p,C.c_size_t,C.c_int32,C.POINTER(Message),C.c_void_p]
 count=total=0;resources=[]
 def check(raw,key,label):
  nonlocal count,total
  old=rng.randrange(2**32);out=Message();C.memset(C.byref(out),0xa5,C.sizeof(out));out.carriage_returns=old
  wanted,lines=n.run(raw,key,old)
  assert lib.bk_message_lookup(raw,len(raw),key,C.byref(out),error),(label,key,error.value)
  assert bytes(out.bytes)==wanted,(label,key,bytes(out.bytes[:100]),wanted[:100])
  assert out.carriage_returns==lines,(label,key,out.carriage_returns,lines)
  length=wanted.find(b'\0');length=1024 if length<0 else length
  assert out.length==length,(label,key,out.length,length)
  count+=1;total+=length
 archive=Archive(a.data/'bk3_05.pp')
 for e in sorted(archive.entries,key=lambda e:e.name):
  if not e.name.lower().endswith('.txt'):continue
  raw=archive.read(e);keys=[int(m[1:]) for m in re.findall(rb'#[0-9]{5}',raw)]
  # i00_10 is metadata without a numeric-message terminator; don't run OOB.
  if not keys:resources.append(dict(name=e.name,bytes=len(raw),keys=0,scope='no numeric messages'));continue
  for key in keys:check(raw,key,e.name)
  check(raw,99999,e.name)
  resources.append(dict(name=e.name,bytes=len(raw),keys=len(keys),sha256=hashlib.sha256(raw).hexdigest()))
  print(e.name,'PASS',len(keys)+1,flush=True)
 # No key alignment assumption, six-byte ID truncation, quote nesting and
 # exact CR count, case-sensitive sentinel, NUL padding, duplicate IDs.
 fixed=[(b'#00001#end',1),(b'#END\n#00001\nx#end',1),(b'ab#00001\nfirst#00001\nsecond#end',1),
        (b'#00001\nabc\0\rxyz\n#end',1),(b'#00001\n'+b'x'*1024+b'#end',1),
        (b'#00001\n'+b'x'*1022+b'\x81\x75#end',1),(b'#00001\n\x81\x75'+b'x'*1020+b'\x81\x76#end',1),
        (b'#00001\n\x81\x76tail#end',1),(b'#00001\n#end',1),(b'#end\n#00001\nx#end',1)]
 for key in [-2147483648,-999999,-12345,-1,0,1,99999,100000,2147483647]:
  fixed.append((f'#{key:05d}\r\nbody\r\n#end'.encode(),key))
 for i,(raw,key) in enumerate(fixed):check(raw,key,('fixed',i))
 atoms=[b'a',b'\0',b'\r',b'\n',b'\r\n',b'\x81',b'\x81\x75',b'\x81\x76',b'\x81\x69',b'\x81\x6a',b'\x81\x67',b'\x81\x68',b'#',b'\xff']
 for i in range(2400):
  key=rng.randrange(-100,120000);header=f'#{key:05d}'.encode()
  raw=b'prefix\0#END\r\n'+header+b' ignored\r\n'+b''.join(rng.choice(atoms) for _ in range(rng.randrange(1,90)))+b'#end'
  check(raw,key,('synthetic',i))
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),queries=count,output_bytes=total,resources=resources,hooks=[],scope=__doc__)
 (ROOT/'local/original-message-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
