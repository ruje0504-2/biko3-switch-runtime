"""Resolve outer entry destination, mode2 controller reset and process globals.
Actual27 game CRT initializers execute without hooks. No new-game/menu/save or
resource/UI loader implementation is implied by the cold process baseline.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_entry_oracle import Native,Request
from original_prop_route_oracle import Native as Base
from original_player_view_oracle import State as View,FIELDS
from original_actor_entry_oracle import transfer
from original_aim_oracle import I
from model_binding import ROOT,library
class Progress(C.Structure):
 _fields_=[('cursor',(C.c_uint32*9)*5),('start',(C.c_uint32*9)*5)]
def populate(s,rng):
 if isinstance(s,C.Structure):
  for name,typ in s._fields_:
   if issubclass(typ,(C.Structure,C.Array)):populate(getattr(s,name),rng)
   else:setattr(s,name,rng.uniform(-30,30) if typ is C.c_float else rng.randrange(1,90))
 else:
  for i in range(len(s)):
   if isinstance(s[i],(C.Structure,C.Array)):populate(s[i],rng)
   else:s[i]=rng.uniform(-30,30) if s._type_ is C.c_float else rng.randrange(1,90)
def values(s):
 if isinstance(s,C.Structure):
  for name,_ in s._fields_:yield from values(getattr(s,name))
 elif isinstance(s,C.Array):
  for x in s:yield from values(x)
 else:yield s
def compare(a,b):
 worst=0
 for i,(x,y) in enumerate(zip(values(a),values(b))):
  d=abs(x-y);assert d<=1e-5,(i,x,y);worst=max(worst,d)
 return worst
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe,a.data);lib=library();u=n.uc;rng=random.Random(0x4e8e3b)
 lib.bk_game_entry_resolve.argtypes=[C.POINTER(Request),C.POINTER(C.c_uint32),C.POINTER(Progress),C.c_uint32,C.c_uint32,C.c_uint8]
 lib.bk_entry_player_view_reset.argtypes=[C.POINTER(View),C.POINTER(C.c_float)]
 progress=Progress();count=0;worst=0;rejects=0
 for g in range(5):
  for area in range(9):
   progress.cursor[g][area]=rng.randrange(1024);progress.start[g][area]=rng.randrange(1024)
   n.word(0xbf3ea8+g*108+area*12,progress.start[g][area])
 for g in range(5):
  for area in range(9):
   for flow in range(256):
    n.word(0x7219a8,g);n.word(0x7219ac,area);u.mem_write(0x721ad4,bytes([flow]));n.run(0x4e8e3b,0x4e8ed5)
    resolved=struct.unpack('<2I',u.mem_read(0x7219a8,8));expected=n.cursor(*resolved,flow,progress.cursor[resolved[0]][resolved[1]])[0]
    start=C.c_uint32(777);request=Request();assert lib.bk_game_entry_resolve(C.byref(request),C.byref(start),C.byref(progress),g,area,flow)
    assert (request.group,request.area)==resolved and request.route_cursor==expected and request.previous_flow==flow
    assert start.value==struct.unpack('<I',u.mem_read(0x3000000+0x834,4))[0];count+=1
 controller,frame,context=0x71af38,0x3005000,0x3006000
 for case in range(1024):
  state=View();populate(state,rng);state.pose.world[:]=I;expected=View.from_buffer_copy(state)
  u.mem_write(controller,bytes(0x600));transfer(u,controller,state,FIELDS,True)
  u.mem_write(controller+0x420,bytes(state.pose.position));n.word(0x645600,context);n.word(0x645604,frame)
  for node in [frame,context]:
   u.mem_write(node,bytes(0x400))
   for offset in [0x80,0xc0,0x100]:u.mem_write(node+offset,struct.pack('<16f',*I))
  n.word(n.stack+8,controller);n.run(0x4b7d50,0x4b7e38)
  origin=(C.c_float*3)(*[rng.uniform(-1000,1000) for _ in range(3)]);u.mem_write(0x729084,bytes(origin));n.run(0x4ec25d,0x4ec27f)
  transfer(u,controller,expected,FIELDS,False);expected.pose.world[:]=struct.unpack('<16f',u.mem_read(frame+0xc0,64));expected.pose.position[:]=struct.unpack('<3f',u.mem_read(controller+0x420,12))
  assert lib.bk_entry_player_view_reset(C.byref(state),origin);worst=max(worst,compare(state,expected))
 saved=bytes(state)
 for origin in [None,(C.c_float*3)(0,float('nan'),0),(C.c_float*3)(0,0,float('inf'))]:
  assert not lib.bk_entry_player_view_reset(C.byref(state),origin) and bytes(state)==saved;rejects+=1
 for g,area,flow in [(5,0,0),(0,9,0),(0,0,8)]:
  progress.cursor[0][0]=1024;before=(bytes(request),start.value);assert not lib.bk_game_entry_resolve(C.byref(request),C.byref(start),C.byref(progress),g,area,flow);assert before==(bytes(request),start.value);rejects+=1
 # Fresh mapped PE, original C++ constructors, all represented global regions.
 cold=Base(exe);constructors=[]
 for p in range(0x544004,0x544070,4):
  fn=struct.unpack('<I',cold.u.mem_read(p,4))[0];cold.call(fn,b'');constructors.append(hex(fn))
 regions=[(0x71b510,0x998,'player'),(0x728de8,0x998,'npc'),(0x71af38,0x600,'camera'),(0x7219a8,8,'profile'),(0x729780,1,'camera transition'),(0xbef784,4,'camera stage'),(0x708878,8,'voice envelope'),(0x708884,544,'player latches'),(0x709084,426,'shared latches'),(0x709898,1,'any wall'),(0x7099ec,4,'move latch'),(0xbf3c8c,108*5,'cursor grid'),(0xbf3ea8,108*5,'start grid'),(0xbf3c6c,4,'run countdown'),(0xbf4b54,8,'prop last-crossed/alpha'),(0x728de4,4,'prop selection'),(0xbf3bd8,13,'ambient timer/gate'),(0xbeecef,1,'notice visible'),(0xbeeccc,1,'notice timer armed'),(0xbe9460,4,'selected item'),(0xbe8f4c,4,'message cursor')]
 for address,size,name in regions:assert bytes(cold.u.mem_read(address,size))==bytes(size),name
 assert struct.unpack('<f',cold.u.mem_read(0x559a80,4))[0]==10
 # Original srand function called at4e6e02, low32 time supplied by platform.
 for seed in [0,1,0xffffffff,0x80000000,1790000000]:cold.call(0x534a2a,struct.pack('<I',seed));assert struct.unpack('<I',cold.u.mem_read(0x58edd8,4))[0]==seed
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),destination_cases=count,camera_resets=1024,max_error=worst,rejections=rejects,game_crt_constructors=constructors,zero_regions=[dict(address=hex(p),size=s,name=name) for p,s,name in regions],cold_lean=10,seed_cases=5,scope='Actual outer destination range then existing native cursor range; mode2 view fields and held sensors; native PE/game CRT process baseline. No menu/new-game/save/complete4e82b8 loader or51a190 claim.')
 (ROOT/'local/original-session-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',count,'destinations',1024,'camera resets',len(constructors),'CRT constructors max',worst)
if __name__=='__main__':main()
