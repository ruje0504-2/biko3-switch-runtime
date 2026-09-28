"""Actual31 item instances from45 profiles: authored XAN/model loading,
root placement and complete4ef095 visibility/request/SRT vs scene owner.
Independent native VMs execute each non-null item loop with no math/animation
hooks. Explicit logical hidden and dt inputs; pickup logic/sound excluded.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from original_actor_phase_oracle import bind
from original_prop_presentation_oracle import VM
from original_item_loader_oracle import State,Config
from model_binding import ROOT,Model
from clip_binding import State as ClipState
from playback_binding import library
from bk3_assets import Archive

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
 exe=args.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256)
 for name,types,result in [
 ('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),
 ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
 ('bk_item_config',[C.POINTER(C.POINTER(Config)),C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32],C.c_int),
 ('bk_item_assets_create',[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(C.c_uint8),C.POINTER(State),C.c_void_p],C.c_void_p),
 ('bk_item_assets_destroy',[C.c_void_p],None),('bk_item_assets_count',[C.c_void_p],C.c_uint32),
 ('bk_item_assets_pose',[C.c_void_p,C.c_uint32],C.c_void_p),('bk_item_assets_state',[C.c_void_p,C.c_uint32],C.POINTER(State)),
 ('bk_item_assets_hidden',[C.c_void_p,C.c_uint32,C.c_uint8],C.c_int),('bk_item_assets_step',[C.c_void_p,C.c_float,C.c_void_p],C.c_int),
 ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model))]:
  f=getattr(lib,name);f.argtypes=types;f.restype=result
 store=lib.bk_resources_create(error);assert store,error.value
 arc=Archive(args.data/'bk3_16.pp');vms=[VM(exe),VM(exe)];matrices=frames=instances=0;worst=0;records=[]
 def read(name):return arc.read(next(e for e in arc.entries if e.name==name))
 try:
  assert lib.bk_resources_mount(store,b'bk3_16',str(args.data/'bk3_16.pp').encode(),error),error.value
  for g in range(5):
   for area in range(9):
    retained=(State*16)();retained[0].yaw=79.32;retained[1].yaw=-139.12
    retained[0].hidden=127;retained[1].hidden=255
    collected=(C.c_uint8*5)(*[(area+i)%3 for i in range(5)])
    owner=lib.bk_item_assets_create(store,g,area,collected,retained,error);assert owner,(g,area,error.value)
    try:
     config=C.POINTER(Config)();count=C.c_uint32();assert lib.bk_item_config(C.byref(config),C.byref(count),g,area)
     assert lib.bk_item_assets_count(owner)==count.value;objects=[]
     for i in range(count.value):
      pose=lib.bk_item_assets_pose(owner,i);model=lib.bk_actor_pose_model(pose).contents
      xan=read(config[i].clip.decode());name=xan[:256].split(b'\0')[0].decode();raw=read(name)
      n=vms[i];n.bind(raw,model);n.uc.mem_write(n.rendered,bytes(0x19000));n.uc.mem_write(n.clip,xan[512:])
      n.word(n.clip+0x18c,0);n.word(n.clip+0x160,n.model);n.word(n.model+0x14,n.frames+n.root*0x400)
      n.word(n.model+0x148,n.group);n.vector(n.group+0x74,[0]);n.word(n.group+0x78,len(n.tracks));n.word(n.group+0x7c,n.track_array)
      for j,(obj,_,f,_) in enumerate(n.tracks):n.word(n.track_array+j*4,obj);n.word(obj+0x74,n.frames+f*0x400)
      n.publish();state=lib.bk_item_assets_state(owner,i).contents;n.place_actor(state.position,state.yaw)
      n.word(0xbef78c,1);n.word(0x726650,n.clip)
      objects.append((i,pose,model,n));instances+=1
      records.append(dict(group=g,area=area,clip=config[i].clip.decode(),model_sha256=hashlib.sha256(raw).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest()))
     def equal(a,b,label):
      nonlocal worst
      for x,y in zip(a,b):
       if x==y:continue
       d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(g,area,label,x,y,d)
     def check():
      nonlocal matrices
      for i,pose,model,n in objects:
       st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st))
       equal([getattr(st,k) for k,_ in st._fields_],n.state(),('timeline',i))
       for f in range(model.frame_count):
        equal(lib.bk_actor_pose_local(pose,f)[:16],n.floats(n.frames+f*0x400+0x80,16),('local',i,f))
        equal(lib.bk_actor_pose_frame(pose,f)[:16],n.floats(n.frames+f*0x400+0xc0,16),('world',i,f));matrices+=2
     check()
     for frame in range(90):
      dt=C.c_float([0,.016,.1,.25,1][frame%5]).value
      for i,pose,model,n in objects:
       hidden=[0,0,1,128,255,0,0][(frame+i)%7]
       assert lib.bk_item_assets_hidden(owner,i,hidden)
       n.uc.mem_write(0x72685c,bytes([hidden]));n.vector(0x733700,[dt]);n.call(0x4ef095,b'')
      assert lib.bk_item_assets_step(owner,dt,error),error.value;check()
      if frame%5!=2:
       for _,pose,_,n in objects:n.publish();lib.bk_actor_pose_publish(pose)
       check()
      frames+=1
     print(g,area,'PASS',count.value,flush=True)
    finally:lib.bk_item_assets_destroy(owner)
 finally:lib.bk_resources_destroy(store)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=45,frames=frames,instances=instances,matrices=matrices,max_relative_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-item-assets-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
