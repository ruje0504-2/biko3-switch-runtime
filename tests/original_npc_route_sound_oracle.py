"""Complete4f6226 route selection and original50d2a0 volume/pan, plus exact
resource-initialization segments for retained NPC AI and shared system cues.
Only loading/DirectSound setters/play are replaced. No Windows device timing.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_area_frame_oracle import Native, Gain
from original_prop_route_oracle import Native as Base
from model_binding import ROOT, library
class Cue(C.Structure):
 _fields_=[('file',C.c_char_p),('gain',Gain)]
class Init(Base):
 def __init__(self,exe):
  super().__init__(exe); self.loads=[]
  self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x50d858,end=0x50d858)
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret,obj,pack,file,vol,loop=struct.unpack('<6I',u.mem_read(sp,24))
  string=lambda p: bytes(u.mem_read(p,260)).split(b'\0')[0].decode('ascii')
  self.loads.append(dict(object=hex(obj),handle=hex(obj+0x110),pack=string(pack),file=string(file),volume=struct.unpack('<i',struct.pack('<I',vol))[0],loop=loop))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,1)
 def segment(self,start,end,volume):
  u=self.u;self.loads=[];u.mem_write(0x5767c8,b'\1');u.mem_write(0xbe9a10,struct.pack('<i',volume))
  u.reg_write(UC_X86_REG_EBP,0x200f000);u.reg_write(UC_X86_REG_ESP,0x200a000)
  u.emu_start(start,end,count=100000);assert u.reg_read(UC_X86_REG_EIP)==end
  return list(self.loads)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();init=Init(exe);loaded=[]
 for volume in [-10000,-6000,-2317,0]:
  system=init.segment(0x4e72fe,0x4e755e,volume);npc=init.segment(0x4fbe4a,0x4fc019,volume)
  assert [r['file'] for r in system]==['se000.wav','se001.wav','se002.wav','se003.wav','se004.wav','se005.wav','se006.wav','se099.wav']
  assert [r['file'] for r in npc]==['se113.wav','se114.wav','se115.wav','se115.wav']
  assert system[2]['handle']=='0xbef050' and npc[1]['handle']=='0x7299c0' and npc[2]['handle']=='0x729ae0'
  assert all(r['volume']==volume and r['loop']==0 and r['pack'].lower().lstrip('\\')=='bk3_02.pp' for r in system+npc)
  loaded+=system+npc
 n=Native(exe);u=n.u;lib=library();vec=C.c_float*3
 lib.bk_npc_route_sound.argtypes=[C.POINTER(Cue),C.c_int32,C.c_int32,C.c_uint32,C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_float,C.c_int32]
 rng=random.Random(0x4f6226);frames=sounds=0
 triggers=[(0,1,59),(1,1,83),(1,5,75),(2,6,155),(2,8,171),(4,3,27)]
 cases=[(g,a,c) for g in range(-1,6) for a in range(-1,10) for c in [0,26,27,28,58,59,60,74,75,76,82,83,84,154,155,156,170,171,172,0xffffffff]]
 cases+= [triggers[i%6] for i in range(3600)]
 for g,a,cursor in cases:
  src=vec(*[rng.uniform(-3000,3000) for _ in range(3)]);listener=vec(*[rng.uniform(-3000,3000) for _ in range(3)])
  if frames%4==0:listener=vec(*src)
  yaw=C.c_float(rng.uniform(-720,720)).value;volume=rng.randrange(-10000,1)
  u.mem_write(0x5767c8,b'\1');u.mem_write(0x729084,bytes(src));u.mem_write(0x729094,struct.pack('<f',33))
  u.mem_write(0x71b7ac,bytes(listener));u.mem_write(0x71b7bc,struct.pack('<f',yaw));n.word(0xbe9a10,volume)
  n.commands=[];n.trace=[];n.call(0x4f6226,struct.pack('<iiI',g,a,cursor))
  out=Cue();assert lib.bk_npc_route_sound(C.byref(out),g,a,cursor,src,listener,yaw,volume)
  actual=[] if not out.file else [(0,out.file,out.gain.volume,out.gain.pan)]
  assert actual==n.commands,(g,a,cursor,actual,n.commands)
  assert n.trace==(['npc-load','volume','pan','npc-play'] if out.file else [])
  frames+=1;sounds+=bool(out.file)
 # No-op is branch-sensitive; invalid matched geometry does not commit.
 out=Cue();assert lib.bk_npc_route_sound(C.byref(out),3,1,59,None,None,float('nan'),-9)
 out.file=b'unchanged';out.gain=Gain(17,23);before=bytes(out)
 assert not lib.bk_npc_route_sound(C.byref(out),0,1,59,vec(float('nan'),0,0),vec(),0,-100)
 assert bytes(out)==before
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,sounds=sounds,resource_loads=loaded,scope=__doc__)
 (ROOT/'local/original-npc-route-sound-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS',frames,'route cues;',sounds,'sounds;',len(loaded),'initialization loads',flush=True)
if __name__=='__main__':main()
