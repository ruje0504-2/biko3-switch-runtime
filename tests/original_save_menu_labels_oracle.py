"""Original50a2ae ten save rows,with only its509fd2 disk refresh intercepted.
Original string copies, fullwidth GBK digit/punctuation selection and fixed
stamp[2..15] traversal run x86. The port's explicit Japanese localization is
independently checked with Python GBK->Unicode, 场景号->エリア, then CP932.
This does not claim unpacked Japanese executable literal or screenshot parity.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT,library
class Label(C.Structure):_fields_=[('area',C.c_uint32),('stamp',C.c_char*32)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe)
  def read(u,a,z,d):
   sp=u.reg_read(UC_X86_REG_ESP);self.group=struct.unpack('<I',u.mem_read(sp+4,4))[0];ret=struct.unpack('<I',u.mem_read(sp,4))[0];u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
  self.u.hook_add(UC_HOOK_CODE,read,begin=0x509fd2,end=0x509fd2)
 def run(self,group,rows):
  for i,r in enumerate(rows):
   base=0xb53c40+group*560+i*56;self.u.mem_write(base+4,struct.pack('<I',r.area));self.u.mem_write(base+24,C.string_at(C.addressof(r)+4,32))
  self.u.mem_write(0x300a000,b'\xa5'*512);self.call(0x50a2ae,struct.pack('<II',group,0x300a000));assert self.group==group
  return bytes(self.u.mem_read(0x300a000,512)).split(b'\0')[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x50a2ae);e=C.create_string_buffer(256);total=0
 lib.bk_save_menu_labels.argtypes=[C.c_void_p,C.c_void_p,C.POINTER(C.c_size_t),C.c_void_p]
 for case in range(1500):
  rows=(Label*10)()
  for row in rows:
   row.area=rng.randrange(9)
   if case%3==0:row.stamp=rng.choice([b'',b'2026/09/28-01:02:03'])
   elif case%3==1:row.stamp=b'2000/02/29-23:59:59'
   else:row.stamp=bytes(rng.choice(b'0123456789/-:XYZ ?') for _ in range(31))
  expected=n.run(case%5,rows).decode('gbk').replace('场景号','エリア').encode('cp932');out=C.create_string_buffer(512);size=C.c_size_t()
  assert lib.bk_save_menu_labels(rows,out,C.byref(size),e),e.value
  assert out.raw[:size.value]==expected,(case,size.value,len(expected));assert out.raw[size.value]==0;total+=size.value
 # Invalid occupied area rejects atomically; empty slots never inspect area.
 rows[0].area=9;rows[0].stamp=b'2026/09/28-01:02:03';before=out.raw;old=size.value
 assert not lib.bk_save_menu_labels(rows,out,C.byref(size),e);assert before==out.raw and old==size.value
 rows[0].stamp=b'';assert lib.bk_save_menu_labels(rows,out,C.byref(size),e)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),banks=1500,rows=15000,bytes_compared=total,scope=__doc__)
 (ROOT/'local/original-save-menu-labels-jp-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS save-menu-labels-jp',report)
if __name__=='__main__':main()
