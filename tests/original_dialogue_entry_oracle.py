"""Original retail dialogue selector and initial page with real parser.

Executes4efdfb..4f0955,51765d,4f0a8e..4f0c25 and4f0c9b..4f0ce7.
Packed reads/allocation, font construction/binding and sprite construction
are explicit service boundaries. Does not claim complete flow8 loading.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_dialogue_oracle import Native as Base,State as Dialogue,equal,Archive
from original_dialogue_backdrop_oracle import State as Backdrop
from original_item_notice_oracle import Fade
from original_text_flow_oracle import Flow
from model_binding import ROOT,library
class Entry(C.Structure):_fields_=[('file',C.c_char*16),('font',C.c_char*16),('first',C.c_int32),('last',C.c_int32)]
class Blob(C.Structure):_fields_=[('data',C.c_void_p),('size',C.c_size_t)]
class Assets(C.Structure):_fields_=[('raw',Blob),('state',Dialogue)]
class Native(Base):
 def __init__(self,exe):
  super().__init__(exe);self.actor=0xbef7a0
  for a in [0x4f0955,0x4f0c25,0x4f0ce7,0x4aa32e,0x4aa9f4]:self.u.hook_add(UC_HOOK_CODE,self.extra,begin=a,end=a)
 def extra(self,u,a,size,_):
  if a in [0x4f0955,0x4f0c25,0x4f0ce7]:u.reg_write(UC_X86_REG_EIP,self.stop);return
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def select(self,prev,g,area,response):
  u=self.u;bp=self.stack-0x4000;u.mem_write(bp-0xc00,b'\x91'*0xc00);u.reg_write(UC_X86_REG_EBP,bp)
  u.mem_write(0x721ad4,bytes([prev]));u.mem_write(0x7219a8,struct.pack('<ii',g,area));u.mem_write(0x71bcda,bytes([response]));self.call(0x4efdfb,b'')
  return Entry(bytes(u.mem_read(bp-0x760,256)).split(b'\0')[0],bytes(u.mem_read(bp-0xb80,256)).split(b'\0')[0],struct.unpack('<i',u.mem_read(bp-0xa80,4))[0],struct.unpack('<i',u.mem_read(bp-0x660,4))[0])
 def initialize(self,s,b,text,entry,raw):
  s=Dialogue.from_buffer_copy(s);s.first_label=entry.first;s.last_label=entry.last;s=self.open(s,raw,entry.file);self.write(s);u=self.u
  u.mem_write(0x6a13d0,bytes(text));u.reg_write(UC_X86_REG_EBP,self.stack-0x4000);self.call(0x4f0a8e,b'')
  sprite=bytearray(0x16c);struct.pack_into('<f',sprite,0x12c,1);struct.pack_into('<f',sprite,0x138,2);sprite[0x148:0x14b]=bytes([1,1,0]);u.mem_write(0x73449c,bytes(sprite));u.mem_write(0xbf00f0,struct.pack('<iBBB',b.saved_expression,b.cycle,b.kind,0));u.mem_write(0x734603,bytes([b.wanted]));self.call(0x4f0c9b,b'')
  bo=Backdrop.from_buffer_copy(b);bo.image=Fade(struct.unpack('<f',u.mem_read(0x7345c8,4))[0],struct.unpack('<f',u.mem_read(0x7345d4,4))[0],u.mem_read(0x7345d0,1)[0]);bo.kind=u.mem_read(0xbf00f5,1)[0];bo.wanted=u.mem_read(0x734603,1)[0]
  return self.read(),Flow.from_buffer_copy(u.mem_read(0x6a13d0,C.sizeof(Flow))),bo

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();err=C.create_string_buffer(256);rng=random.Random(0x4efdfb)
 lib.bk_dialogue_entry_select.argtypes=[C.POINTER(Entry),C.c_uint8,C.c_int32,C.c_int32,C.c_uint8,C.c_void_p]
 lib.bk_dialogue_entry_open.argtypes=[C.POINTER(Assets),C.POINTER(Flow),C.POINTER(Backdrop),C.c_void_p,C.POINTER(Entry),C.c_void_p]
 lib.bk_resources_create.argtypes=[C.c_void_p];lib.bk_resources_create.restype=C.c_void_p
 lib.bk_resources_mount.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p];lib.bk_resources_destroy.argtypes=[C.c_void_p];lib.bk_dialogue_assets_close.argtypes=[C.POINTER(Assets)]
 selects=rejects=0
 for prev in range(256):
  for g in [-2147483648,-1,0,1,2,3,4,5,2147483647]:
   for area,response in [(-2147483648,0),(0,255),(7,2),(8,3),(8,4),(8,0),(9,255),(2147483647,4)]:
    want=n.select(prev,g,area,response);out=Entry.from_buffer_copy(b'\x5a'*C.sizeof(Entry));old=bytes(out);ok=lib.bk_dialogue_entry_select(C.byref(out),prev,g,area,response,err)
    if not want.file:
     assert not ok and bytes(out)==old,(prev,g,area,response);rejects+=1
    else:
     assert ok and bytes(out)==bytes(want),(prev,g,area,response,bytes(out),bytes(want),err.value);selects+=1
 store=lib.bk_resources_create(err);assert store;assert lib.bk_resources_mount(store,b'bk3_05',str(a.data/'bk3_05.pp').encode(),err),err.value;archive=Archive(a.data/'bk3_05.pp');assets=Assets();count=0;profiles=[]
 try:
  for prev in [0x38,2,0x10,0]:
   for g in range(5):
    for area,response in ([(0,0)] if prev in [0x38,0] else [(0,0),(8,4),(8,3)]):
     entry=n.select(prev,g,area,response);raw=archive.read(next(x for x in archive.entries if x.name==entry.file.decode()));profiles.append(dict(previous=prev,group=g,area=area,response=response,file=entry.file.decode(),first=entry.first,last=entry.last))
     for case in range(3):
      d=Dialogue();d.current_label=[-19,entry.last,entry.first][case];d.code_c,d.code_f,d.code_e,d.code_m=21,22,23,24;d.sound_pending,d.image_kind,d.music_pending=7,8,9
      for name in ['previous_sound','sound','image','music']:setattr(d,name,('retained-'+name).encode())
      b=Backdrop(Fade(.3,4,5),-71,3,17,255);text=Flow(3,4,5,6,7);want,ft,bt=n.initialize(d,b,text,entry,raw);assets.state=d
      assert lib.bk_dialogue_entry_open(C.byref(assets),C.byref(text),C.byref(b),store,C.byref(entry),err),err.value;equal(assets.state,want,(count,'page'));assert bytes(text)==bytes(ft),(count,'flow',bytes(text),bytes(ft));assert bytes(b)==bytes(bt),(count,'backdrop',bytes(b),bytes(bt));assert C.string_at(assets.raw.data,assets.raw.size)==raw;count+=1
 finally:lib.bk_dialogue_assets_close(C.byref(assets));lib.bk_resources_destroy(store)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),selectors=selects,native_uninitialized_rejected=rejects,initial_pages=count,profiles=profiles,max_error=0,scope=__doc__)
 (ROOT/'local/original-dialogue-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS dialogue entry',selects,'selectors',rejects,'undefined branches',count,'real initial pages')
if __name__=='__main__':main()
