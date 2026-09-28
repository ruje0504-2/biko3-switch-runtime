"""4f64ae prop bounds/train endpoints/shared ambient gate,4f5e24 route cue
and4f607a resource selection. Original CRT trig executes; no service hooks.
NPC rectangle is compared to4f3d34's actual4f3f76..4f3fd0 instruction segment.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX
from original_prop_route_oracle import Native
from original_prop_motion_oracle import State
from model_binding import ROOT,library
class Bounds(C.Structure):_fields_=[('left',C.c_int32),('top',C.c_int32),('right',C.c_int32),('bottom',C.c_int32)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);u=n.u;lib=library();rng=random.Random(0x4f64ae)
 lib.bk_area_prop_boundary.argtypes=[C.POINTER(State),C.POINTER(C.c_uint8),C.c_uint,C.c_uint,C.POINTER(Bounds)]
 lib.bk_area_npc_outside.argtypes=[C.POINTER(C.c_uint8),C.POINTER(C.c_float),C.POINTER(Bounds)]
 lib.bk_area_sound_trigger.argtypes=[C.c_int32,C.c_int32,C.c_int32,C.c_uint8]
 lib.bk_area_sound_file.argtypes=[C.c_int32,C.c_int32];lib.bk_area_sound_file.restype=C.c_char_p
 frames=outside=gateones=cues=triggers=files=0
 for g in range(5):
  for a in range(9):
   for case in range(384):
    bounds=Bounds(-200,200,200,-200)
    if case%3==0:bounds=Bounds(rng.randrange(-1000,1000),rng.randrange(-1000,1000),rng.randrange(-1000,1000),rng.randrange(-1000,1000))
    if case%29==0:bounds=Bounds(16777217,16777217,2147483647,-2147483648)
    s=State.from_buffer_copy(rng.randbytes(C.sizeof(State)));s.kind=rng.choice([0,1,1,2,7,18,19]);s.path.yaw=rng.choice([0,90,180,270,rng.uniform(-720,720)])
    s.path.position[:]=[rng.choice([bounds.left,bounds.right,-500,400,rng.uniform(-1500,1500)]),rng.uniform(-10,10),rng.choice([bounds.bottom,bounds.top,-300,300,rng.uniform(-1500,1500)])]
    gate=C.c_uint8(rng.choice([0,1,2,255]));before=State.from_buffer_copy(s)
    u.mem_write(n.actor,bytes(0x998));u.mem_write(n.actor+0xc,struct.pack('<i',s.kind));u.mem_write(n.actor+0x29c,bytes(s.path.position));u.mem_write(n.actor+0x2ac,struct.pack('<f',s.path.yaw));u.mem_write(0xbf3be4,bytes([gate.value]));u.mem_write(0x7219a8,struct.pack('<2i',g,a))
    n.call(0x4f64ae,struct.pack('<I',n.actor)+bytes(bounds));want=u.reg_read(UC_X86_REG_EAX)&255;wantgate=u.mem_read(0xbf3be4,1)[0]
    assert lib.bk_area_prop_boundary(C.byref(s),C.byref(gate),g,a,C.byref(bounds))
    before.hidden=want
    assert bytes(s)==bytes(before) and gate.value==wantgate,(g,a,case,s.hidden,want,gate.value,wantgate,list(s.path.position),s.path.yaw)
    outside+=want;gateones+=wantgate==1;frames+=1
    u.mem_write(0x725920,bytes(bounds));u.mem_write(0x729084,bytes(s.path.position));u.emu_start(0x4f3f76,0x4f3fd0,count=1000)
    result=C.c_uint8();assert lib.bk_area_npc_outside(C.byref(result),s.path.position,C.byref(bounds));assert result.value==u.mem_read(0x729110,1)[0]
 for g in range(-1,6):
  for a in range(-1,10):
   n.call(0x4f607a,struct.pack('<2i',g,a));index=u.reg_read(UC_X86_REG_EAX);want=[b'se304.wav',b'se155.wav',b'se156.wav'][index] if index!=99 else None
   assert lib.bk_area_sound_file(g,a)==want;files+=1
   points={-1,0,1,2147483647}
   for point in [234,55,132,26,165,280,230,276,241,329]:points.update([point-1,point,point+1])
   for point in points:
    for played in [0,1,2,255]:
     u.mem_write(n.actor+0xc,struct.pack('<i',g));u.mem_write(n.actor+0x830,struct.pack('<i',point));u.mem_write(n.actor+0x332,bytes([played]));n.call(0x4f5e24,struct.pack('<Ii',n.actor,a));want=u.reg_read(UC_X86_REG_EAX)&255
     assert lib.bk_area_sound_trigger(g,a,point,played)==want;cues+=1;triggers+=want
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),prop_queries=frames,npc_queries=frames,hidden_props=outside,ambient_gate_one=gateones,sound_queries=cues,triggered=triggers,file_queries=files,hooks=[],scope=__doc__)
 (ROOT/'local/original-area-boundary-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
