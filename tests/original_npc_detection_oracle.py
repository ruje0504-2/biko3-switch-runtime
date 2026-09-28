"""Complete4f3c80 with no service hooks. Shared existing NPC spatial/AI and
contact outcome objects, including arbitrary untouched state and action aliases.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_prop_route_oracle import Native
from original_npc_spatial_oracle import State
from original_npc_ai_oracle import Shared
from model_binding import ROOT,library
class Input(C.Structure):_fields_=[('area',C.c_int32),('player_action',C.c_int32),('suppressed',C.c_int32*4),('forced',C.c_int32*2)]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);u=n.u;lib=library();rng=random.Random(0x4f3c80)
 lib.bk_npc_detection_resolve.argtypes=[C.POINTER(State),C.POINTER(Shared),C.POINTER(Input)]
 cleared=changed=forced=0
 for case in range(24000):
  s=State.from_buffer_copy(rng.randbytes(C.sizeof(State)));shared=Shared.from_buffer_copy(rng.randbytes(C.sizeof(Shared)))
  s.ai.point.motion.action=rng.randrange(-10,30);s.ai.point.motion.behavior=rng.randrange(-20,20)
  s.visible=rng.choice([0,1,1,2,255]);s.ai.stimulus=rng.choice([0,1,1,-1,2]);shared.outcome=rng.choice([0,0,0,1,2,3,255])
  inp=Input(rng.choice([-5,0,1,7,8,9,2147483647]),rng.randrange(-10,30),(C.c_int32*4)(*[rng.randrange(-10,30) for _ in range(4)]),(C.c_int32*2)(*[rng.randrange(-10,30) for _ in range(2)]))
  if case%3==0:inp.player_action=inp.suppressed[case%4]
  if case%4==0:s.ai.point.motion.action=inp.forced[case%2]
  before=State.from_buffer_copy(s);expected=State.from_buffer_copy(s);want_shared=Shared.from_buffer_copy(shared)
  for addr,value in [(0x7219ac,inp.area),(0x71b520,inp.player_action),(0x728df8,s.ai.point.motion.action),(0x729638,s.ai.point.motion.behavior)]:u.mem_write(addr,struct.pack('<i',value))
  u.mem_write(0x71b550,bytes(inp.suppressed));u.mem_write(0x728e50,bytes(inp.forced))
  u.mem_write(0x729100,bytes([s.visible,s.ai.stimulus&255]));u.mem_write(0x71bcd8,bytes([shared.outcome]))
  n.call(0x4f3c80,b'')
  expected.visible=u.mem_read(0x729100,1)[0];expected.ai.point.motion.behavior=struct.unpack('<i',u.mem_read(0x729638,4))[0];want_shared.outcome=u.mem_read(0x71bcd8,1)[0]
  oldoutcome=shared.outcome
  assert lib.bk_npc_detection_resolve(C.byref(s),C.byref(shared),C.byref(inp))
  assert bytes(s)==bytes(expected),(case,'npc')
  assert bytes(shared)==bytes(want_shared),(case,'outcome')
  cleared+=before.visible!=0 and s.visible==0;changed+=shared.outcome!=oldoutcome;forced+=oldoutcome==0 and shared.outcome==3
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=24000,detection_cleared=cleared,outcome_changes=changed,outcome3=forced,hooks=[],scope=__doc__)
 (ROOT/'local/original-npc-detection-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
