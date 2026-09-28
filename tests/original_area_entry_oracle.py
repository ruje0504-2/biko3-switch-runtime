"""Native4e94e4 actor/controller stores across40 next-area profiles. Runs
4e952b..4e9706 (including original CKP loading and both4befc0 calls), mode2
controller stores4b7d50..4b7e38,4e9715..4e973c and full4ebfd0 with existing
UI/clip service hooks. Assets/scene-release ordering outside this CPU scope.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from original_entry_oracle import Native,Request,Selection
from original_actor_entry_oracle import Player,Npc,Actions,Metadata,PF,NF,GF,transfer,populate,compare
from original_session_entry_oracle import populate as populate_view,compare as compare_view
from original_player_view_oracle import State as View,FIELDS as VF
from original_camera_policy_oracle import State as Camera
from original_aim_oracle import I
from model_binding import ROOT,library

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe,a.data);u=n.uc;lib=library();rng=random.Random(0x4e94e4);e=C.create_string_buffer(256);cases=rejects=0;worst=0
 lib.bk_area_entry_reset.argtypes=[C.POINTER(Player),C.POINTER(Npc),C.POINTER(C.c_float),C.POINTER(View),C.POINTER(Camera),C.POINTER(Metadata),C.POINTER(Actions),C.c_void_p,C.POINTER(Request),C.c_void_p]
 lib.bk_game_entry_select.argtypes=[C.POINTER(Selection),C.POINTER(Request)]
 lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_route_decode.restype=C.c_void_p;lib.bk_route_destroy.argtypes=[C.c_void_p];lib.bk_route_count.argtypes=[C.c_void_p]
 player,actor,controller,frame,context=0x71b510,0x728de8,0x71af38,0x3005000,0x3006000
 for g in range(5):
  for area in range(1,9):
   req=Request(g,area,0,2);sel=Selection();assert lib.bk_game_entry_select(C.byref(sel),C.byref(req));raw=(a.data/sel.route_file.decode()).read_bytes();route=lib.bk_route_decode(raw,len(raw),e);assert route,e.value;count=lib.bk_route_count(route)
   try:
    for case in range(64):
     req.previous_flow=[2,0x20,8,0x38,0x48,255,0,1][case%8]
     p=Player();populate(p,rng);pn=Player.from_buffer_copy(p);s=Npc();populate(s,rng);sn=Npc.from_buffer_copy(s);v=View();populate_view(v,rng);v.pose.world[:]=I;vn=View.from_buffer_copy(v);cam=Camera(rng.randrange(256),rng.randrange(256),rng.randrange(-30,40));cn=Camera.from_buffer_copy(cam)
     meta=Metadata(rng.randrange(count+1),rng.randrange(1024));actions=Actions();populate(actions,rng);vertical=C.c_float(rng.uniform(-50,50))
     u.mem_write(controller,bytes(0x5d0));transfer(u,controller,v,VF,True);u.mem_write(controller+0x420,bytes(v.pose.position));n.word(0x645600,context);n.word(0x645604,frame)
     for node in [frame,context]:
      u.mem_write(node,bytes(0x400))
      for offset in [0x80,0xc0,0x100]:u.mem_write(node+offset,struct.pack('<16f',*I))
     transfer(u,player,p,PF,True);transfer(u,actor,s,NF,True);transfer(u,0,s,GF,True);u.mem_write(actor+0x834,bytes(meta));u.mem_write(actor+0x18,bytes(actions.walk));u.mem_write(actor+0x2a8,bytes(vertical));n.word(0x7219a8,g);n.word(0x7219ac,area-1);u.mem_write(0x721ad4,bytes([req.previous_flow]));u.mem_write(0x5767c8,b'\1');u.mem_write(0x71ba88,bytes([cam.phase]));u.mem_write(0x729780,bytes([cam.transition]));u.mem_write(0xbef784,struct.pack('<i',cam.stage))
     n.run(0x4e952b,0x4e9706)
     assert struct.unpack('<I',u.mem_read(0x7219ac,4))[0]==area
     n.word(n.stack+8,controller);n.run(0x4b7d50,0x4b7e38)
     n.run(0x4e9715,0x4e973c);n.call(0x4ebfd0,b'')
     transfer(u,player,pn,PF,False);pn.spatial.scene.wall.position[:]=pn.spatial.movement.position
     transfer(u,actor,sn,NF,False);transfer(u,0,sn,GF,False);transfer(u,controller,vn,VF,False);vn.pose.world[:]=struct.unpack('<16f',u.mem_read(frame+0xc0,64));vn.pose.position[:]=struct.unpack('<3f',u.mem_read(controller+0x420,12));pn.spatial.scene.wall.camera_distance=vn.target_distance
     expected_vertical=struct.unpack('<f',u.mem_read(actor+0x2a8,4))[0];cn.phase=u.mem_read(0x71ba88,1)[0];cn.transition=u.mem_read(0x729780,1)[0];cn.stage=struct.unpack('<i',u.mem_read(0xbef784,4))[0]
     before=(bytes(meta),bytes(actions));assert lib.bk_area_entry_reset(C.byref(p),C.byref(s),C.byref(vertical),C.byref(v),C.byref(cam),C.byref(meta),C.byref(actions),route,C.byref(req),e),e.value
     try:
      worst=max(worst,compare(p,pn),compare(s,sn),compare_view(v,vn));assert bytes(cam)==bytes(cn) and vertical.value==expected_vertical;assert before==(bytes(meta),bytes(actions))
     except AssertionError:
      print('case',g,area,case,req.previous_flow,flush=True);raise
     cases+=1
    meta.start=count+1;before=(bytes(p),bytes(s),bytes(vertical),bytes(v),bytes(cam));assert not lib.bk_area_entry_reset(C.byref(p),C.byref(s),C.byref(vertical),C.byref(v),C.byref(cam),C.byref(meta),C.byref(actions),route,C.byref(req),e);assert before==(bytes(p),bytes(s),bytes(vertical),bytes(v),bytes(cam));rejects+=1
   finally:lib.bk_route_destroy(route)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=40,cases=cases,rejections=rejects,max_error=worst,scope=__doc__)
 (ROOT/'local/original-area-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS area-entry',report)
if __name__=='__main__':main()
