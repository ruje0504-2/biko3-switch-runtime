"""Original FOG loader and42d295..42d46b vs portable device-state replay.
All capability combinations, retained modes/range, enable/reset and disabled
records. Original D3D SetRenderState is captured; no rasterization claim.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_lighting_oracle import Oracle
from environment_binding import Fog
from model_binding import library,ROOT
class State(C.Structure):
 _fields_=[(n,C.c_uint32) for n in ['enabled','vertex_mode','table_mode','range_based','color']]+[(n,C.c_float) for n in ['start','end','density']]
def word(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Oracle(exe);lib=library();err=C.create_string_buffer(256)
 lib.bk_fog_enable.argtypes=[C.POINTER(State),C.c_int,C.c_int,C.c_int,C.c_int]
 lib.bk_fog_load.argtypes=[C.POINTER(State),C.POINTER(Fog),C.c_int,C.c_int,C.c_void_p]
 lib.bk_fog_resolve.argtypes=[C.POINTER(State),C.POINTER(Fog),C.c_void_p]
 rng=random.Random(0x42d295);checks=retained_pixel=0
 for i in range(12000):
  initial=State(rng.randrange(2),rng.randrange(4),rng.randrange(4),rng.randrange(2),rng.getrandbits(32),rng.random(),10+rng.random()*100,rng.random())
  table=(i//2)%2;range_cap=i%2;mode=i%4
  record=Fog(i%5!=0,mode,(i//4)%2,(i//8)%2,rng.getrandbits(32),rng.uniform(-10,10),rng.uniform(20,2000),rng.random())
  n.states={28:initial.enabled,140:initial.vertex_mode,35:initial.table_mode,48:initial.range_based,34:initial.color,36:word(initial.start),37:word(initial.end),38:word(initial.density)}
  s=State.from_buffer_copy(initial);n.write(0x645638,0);n.write(0x6452ec,(0x100 if table else 0)|(0x10000 if range_cap else 0))
  if i%3==0:
   n.call(0x42d295,0,0);assert lib.bk_fog_enable(C.byref(s),0,0,table,range_cap)
  bp=0x2008000;source=0x3004000;n.uc.mem_write(source,bytes(record));n.write(bp-0x164,source)
  n.uc.reg_write(UC_X86_REG_EBP,bp);n.uc.reg_write(UC_X86_REG_ESP,bp-0xb00);n.uc.emu_start(0x41ae69,0x41af06,count=20000);assert n.uc.reg_read(UC_X86_REG_EIP)==0x41af06
  assert lib.bk_fog_load(C.byref(s),C.byref(record),table,range_cap,err),err.value
  expected=[n.states[k] for k in [28,140,35,48,34,36,37,38]]
  actual=list(struct.unpack('<8I',bytes(s)));assert actual==expected,(i,actual,expected)
  resolved=Fog();assert lib.bk_fog_resolve(C.byref(s),C.byref(resolved),err),err.value
  assert resolved.table==bool(s.table_mode) and resolved.mode==(s.table_mode or s.vertex_mode)
  retained_pixel+=bool(record.enabled and not record.table and table and resolved.table)
  before=bytes(s);bad=Fog.from_buffer_copy(record);bad.enabled=1;bad.density=float('nan');assert not lib.bk_fog_load(C.byref(s),C.byref(bad),table,range_cap,err);assert bytes(s)==before;checks+=1
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),states=checks,retained_pixel_mode_cases=retained_pixel,invalid_atomic_rejections=checks,max_error=0,scope=__doc__)
 (ROOT/'local/original-fog-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
