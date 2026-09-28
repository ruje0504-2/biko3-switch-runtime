"""Original packaged BOM4a5870: real metadata and all8 binding fields.
Only packed file bytes/free and Windows string calls observed. No topology,
node binding, deformation or physics is claimed by this parser oracle.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_face_config_oracle import Native as Base,Archive
from model_binding import ROOT,library
NAMES=['parent','reference','child','primary_aux','secondary_aux','target_mesh','source_mesh','selection']
OFFSETS=[0x108,0x1968,0x928,0x1148,0x2188,0x29a8,0x31c8,0x39e8]
class Binding(C.Structure):_fields_=[(n,C.c_char*260) for n in NAMES]
class Config(C.Structure):_fields_=[('mode',C.c_uint32),('count',C.c_uint32),('primary',C.c_char*260),('secondary',C.c_char*260),('bindings',Binding*4)]
class Native(Base):
 config=0x3000000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x100000)
  for a in [0x51ceee,0x520538]:self.u.hook_add(UC_HOOK_CODE,self.packed,begin=a,end=a)
 def packed(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp)
  if a==0x51ceee:
   args=struct.unpack('<4I',u.mem_read(sp+4,16));u.mem_write(0x4000000,self.data);self.word(args[2],0x4000000);self.word(args[3],len(self.data))
  u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def decode(self,data):
  self.data=data;self.u.mem_write(self.config,bytes(0x420c));self.u.mem_write(self.stack,struct.pack('<4I',self.stop,self.config,0x300d000,0x300d000));self.u.reg_write(UC_X86_REG_ESP,self.stack)
  self.u.emu_start(0x4a5870,self.stop,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop and self.u.reg_read(UC_X86_REG_EAX)==1

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);lib.bk_bom_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Config),C.c_void_p]
 arc=Archive(a.data/'fambom.pp');files=[(e.name,arc.read(e)) for e in arc.entries if e.name.lower().endswith('.bom')];real=len(files);records=[]
 for count in range(5):
  rows=[b'primary.xan',b'secondary.xan']+[b'field'*51+b'12']*32
  if count<4:rows[2+count*8]=b''
  raw=struct.pack('<I',0x12345678+count)+b''.join(struct.pack('<I',len(s)+3)+s+b'\0XY' for s in rows);files.append((f'synthetic{count}',raw))
 for name,raw in files:
  c=Config();assert lib.bk_bom_decode(raw,len(raw),C.byref(c),e),(name,e.value);n.decode(raw)
  assert c.mode==n.read(n.config+0x4208) and c.count==n.read(n.config),(name,c.count)
  for i in range(4):
   for field,offset in zip(NAMES,OFFSETS):assert getattr(c.bindings[i],field)==n.string(n.config+offset+i*260),(name,i,field)
  records.append(dict(name=name,mode=c.mode,count=c.count,sha256=hashlib.sha256(raw).hexdigest()))
 rejected=0;rng=random.Random(0x4a5870);raw=files[0][1]
 malformed=[raw[:i] for i in range(len(raw))]
 for _ in range(4096):
  b=bytearray(raw)
  for _ in range(rng.randrange(1,6)):b[rng.randrange(len(b))]=rng.randrange(256)
  malformed.append(bytes(b))
 for b in malformed:
  out=Config();C.memset(C.byref(out),0xa5,C.sizeof(out));before=bytes(out)
  if not lib.bk_bom_decode(b,len(b),C.byref(out),e):assert bytes(out)==before;rejected+=1
  else:assert out.count<=4
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=real,synthetic=5,records=records,malformed_cases=len(malformed),rejected=rejected,scope=__doc__)
 (ROOT/'local/original-bom-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',real,'BOM assets,5 synthetic,',len(malformed),'mutations/truncations; native fields exact',flush=True)
if __name__=='__main__':main()
