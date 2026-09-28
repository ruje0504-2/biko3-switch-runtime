"""Whole511940 load/initial placement and5121ce spatial loop over45 entries.
File/model/audio services are boundaries; original routes, state dispatch,
movement, zero-static geometry ground, facing and root setters execute x86.
Actual scene instances use original OBJM/XAN. Audio commands and animation
are checked separately, not claimed by this empty-static collision fixture.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_route_oracle import Native as RouteNative,Point
from original_prop_motion_oracle import State,Input,Shared,Effects
from original_npc_scene_oracle import Input as GroundInput
from original_actor_phase_oracle import bind as bind_actor
from original_collision_oracle import bind as bind_collision
from model_binding import ROOT,Model,library,decode
from bk3_assets import Archive
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class Config(C.Structure):_fields_=[('kind',C.c_int32),('cursor',C.c_int32),('clip',C.c_char_p),('route_file',C.c_char_p),('anchor',C.c_char_p),('point_count',C.c_uint32),('points',Point*4)]
class Native(RouteNative):
 def __init__(self,exe,directory,arc):
  super().__init__(exe,directory);self.arc=arc;self.uc.mem_map(0x5000000,0x200000)
  for a in [0x4a7109,0x401074,0x512853,0x512c0e,0x4b0de5,0x4b124d,0x501276,0x425904]:self.uc.hook_add(UC_HOOK_CODE,self.service,begin=a,end=a)
 def alloc(self,size):
  p=self.cursor;self.cursor+=(size+15)&~15;self.uc.mem_write(p,bytes(size));assert self.cursor<0x5200000;return p
 def boundary(self,u,a,size,user):
  if a!=0x4ad8ec:return super().boundary(u,a,size,user)
  sp=u.reg_read(UC_X86_REG_ESP);ret,dest,prefix,suffix=struct.unpack('<4I',u.mem_read(sp,16));s=self.string(prefix)+(self.string(suffix) if suffix else '');u.mem_write(dest,s.encode()+b'\0');u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def service(self,u,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0
  if a==0x401074:
   name=self.string(args[0]).split('\\')[-1];xan=self.arc.read(next(e for e in self.arc.entries if e.name==name));obj=self.alloc(0x4f90);group=self.alloc(0x180);root=self.alloc(0x280);u.mem_write(obj,xan[512:]);self.word(obj+0x160,group);self.word(group+0x14,root)
   for off in [0x80,0xc0,0x100]:u.mem_write(root+off,struct.pack('<16f',*I))
   self.loaded.append((name,obj,root));result=obj
  elif a==0x501276:
   dest,path,limit=args[:3];name=self.string(path).split('\\')[-1];raw=(self.directory/name).read_bytes()[:limit];u.mem_write(dest,raw);self.files[(dest-0xbf4b60)//1280]=name
  elif a==0x425904:
   root,name,out=args[:3];name=self.string(name);result=self.anchors.get(name,0);self.word(out,result)
  elif a in [0x4b0de5,0x4b124d]:
   root,alpha,policy=args[:3];index=next(i for i,x in enumerate(self.loaded) if x[2]==root);self.commands[index]=(-1 if a==0x4b0de5 else policy,struct.unpack('<f',struct.pack('<I',alpha))[0])
  elif a==0x512c0e:self.audio_calls+=1
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def setup(self,g,a,background,world):
  u=self.uc;self.cursor=0x5000000;self.loaded=[];self.files={};self.anchors={};self.commands={};self.audio_calls=0
  u.mem_write(0x729d80,bytes(16*0x998));u.mem_write(0x726640,bytes(12));self.word(0x7219a8,g);self.word(0x7219ac,a);u.mem_write(0x5767c8,b'\0');self.word(0xbf4b58,0)
  bg=self.alloc(0x180);group=self.alloc(0x180);root=self.alloc(0x280);self.word(0x725718,bg);self.bg=bg;self.word(bg+0x160,group);self.word(group+0x14,root)
  for f in range(background.frame_count):
   name=bytes(background.frames[f].name).decode()
   if name not in ['qqq184_null_kick_kan_01','qqq183_null_fall_danb','qqq185_null_kick_kan_02']:continue
   node=self.alloc(0x280);u.mem_write(node+0xc0,struct.pack('<16f',*world[f*16:f*16+16]));self.anchors[name]=node
  self.call(0x511940,struct.pack('<2I',g,a));assert len(self.loaded)==struct.unpack('<I',u.mem_read(0xbf4b50,4))[0]
 def step(self,inp,sh):
  u=self.uc;self.commands={};self.word(0x71b520,inp.player_action);u.mem_write(0x71b524,bytes(inp.player_actions));u.mem_write(0x71ba89,bytes([inp.player_mode&255]));self.word(self.bg+0x140,inp.background_clip);self.word(0xbf3c74,inp.npc_last_crossed)
  for i in range(2):u.mem_write(0xbe9a28+(inp.npc_last_crossed-1-i)*20,bytes([inp.npc_previous_flags[i]&255]))
  u.mem_write(0x733700,struct.pack('<f',inp.seconds));u.mem_write(0xbf4b58,struct.pack('<f',sh.alpha));self.word(0xbf4b54,sh.last_crossed)
  self.call(0x5121ce,b'')
 def read(self,i):
  p=0x729d80+i*0x998;u=self.uc
  vals=lambda o,n,fmt:struct.unpack('<'+fmt*n,u.mem_read(p+o,struct.calcsize(fmt)*n))
  return dict(kind=vals(0xc,1,'i')[0],action=vals(0x10,1,'i')[0],actions=vals(0x14,4,'i'),position=vals(0x29c,3,'f'),yaw=vals(0x2ac,1,'f')[0],velocity=vals(0x2b4,3,'f'),indices=vals(0x830,3,'i'),target=vals(0x804,1,'f')[0],hidden=vals(0x328,1,'B')[0],background_wait=vals(0x330,1,'b')[0],route_flag=vals(0x84c,1,'b')[0],timer=vals(0x840,2,'I')+vals(0x848,1,'B'),world=struct.unpack('<16f',u.mem_read(self.loaded[i][2]+0xc0,64)))
def bind(lib):
 bind_actor(lib);bind_collision(lib)
 for name,args,result in [
  ('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
  ('bk_prop_assets_create',[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(Model),C.POINTER(C.c_float),C.c_size_t,C.c_void_p],C.c_void_p),('bk_prop_assets_destroy',[C.c_void_p],None),('bk_prop_assets_count',[C.c_void_p],C.c_uint32),('bk_prop_assets_state',[C.c_void_p,C.c_uint32],C.POINTER(State)),('bk_prop_assets_pose',[C.c_void_p,C.c_uint32],C.c_void_p),('bk_prop_assets_route',[C.c_void_p,C.c_uint32],C.c_void_p),
  ('bk_prop_config',[C.POINTER(C.POINTER(Config)),C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32],C.c_int),('bk_route_count',[C.c_void_p],C.c_uint32),('bk_route_point',[C.c_void_p,C.c_uint32],C.POINTER(Point)),
  ('bk_prop_assets_step_spatial',[C.c_void_p,C.POINTER(Shared),C.POINTER(Input),C.c_void_p,C.POINTER(GroundInput),C.c_size_t,C.POINTER(Effects),C.c_void_p],C.c_int)]:
  f=getattr(lib,name);f.argtypes=args;f.restype=result

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();bind(lib);err=C.create_string_buffer(256);arc=Archive(args.data/'bk3_07.pp');n=Native(exe,args.data,arc);store=lib.bk_resources_create(err);assert store,err.value
 bg_arc=Archive(args.data/'bk3_03.pp');raw=bg_arc.read(next(e for e in bg_arc.entries if e.name=='m01_01.x'));ok,m,msg=decode(lib,raw);assert ok,msg;world=(C.c_float*(m.contents.frame_count*16))();assert lib.bk_model_world_matrices(m,world,len(world),err);empty=bytes(4293124);collision=lib.bk_collision_create(m,world,len(world),b'm01_01.x',empty,len(empty),err);assert collision,err.value;steps=initial=routes=0;worst=0
 def equal(a,b,label):
  nonlocal worst
  for x,y in zip(a,b):
   d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(g,a_idx,step,label,x,y,d)
 try:
  assert lib.bk_resources_mount(store,b'bk3_07',str(args.data/'bk3_07.pp').encode(),err),err.value
  assert lib.bk_resources_mount_directory(store,b'routes',str(args.data).encode(),20480,err),err.value
  for g in range(5):
   for a_idx in range(9):
    n.setup(g,a_idx,m.contents,world);assets=lib.bk_prop_assets_create(store,g,a_idx,m,world,len(world),err);assert assets,(g,a_idx,err.value);count=lib.bk_prop_assets_count(assets);assert count==len(n.loaded);shared=Shared();config=C.POINTER(Config)();size=C.c_uint32();assert lib.bk_prop_config(C.byref(config),C.byref(size),g,a_idx)
    try:
     for i in range(count):
      assert config[i].clip.decode()==n.loaded[i][0];assert (config[i].route_file.decode() if config[i].route_file else None)==n.files.get(i)
      route=lib.bk_prop_assets_route(assets,i)
      if route:
       for j in range(lib.bk_route_count(route)+1):
        point=lib.bk_route_point(route,j).contents;wanted=struct.unpack('<4fB3x',n.uc.mem_read(0xbf4b60+i*1280+j*20,20));assert (*point.position,point.parameter,point.flags)==wanted;routes+=1
     for step in range(-1,120):
      if step>=0:
       inp=Input();inp.seconds=[0,1/60,.1,.25][step%4];inp.player_actions[:]=list(range(100,121));inp.player_action=inp.player_actions[[0,11,12,13,14][(step//8)%5]];inp.player_mode=step%3;inp.background_clip=(step//17)%3;inp.npc_last_crossed=6;inp.npc_previous_flags[:]=[3 if step%7==0 else 0,0]
       ground=(GroundInput*16)()
       for gi in ground:
        gi.excluded_surface=b'';gi.cone.player_action=inp.player_action;gi.cone.short_range_action=inp.player_actions[8];gi.suppressed_actions[:]=[inp.player_actions[x] for x in [11,12,13,14,16,17]]
       n.step(inp,shared);effects=(Effects*16)();assert lib.bk_prop_assets_step_spatial(assets,C.byref(shared),C.byref(inp),collision,ground,count,effects,err),(g,a_idx,step,err.value)
       assert shared.alpha==struct.unpack('<f',n.uc.mem_read(0xbf4b58,4))[0]
       assert shared.last_crossed==struct.unpack('<i',n.uc.mem_read(0xbf4b54,4))[0]
       for i in range(count):assert (effects[i].material,effects[i].alpha)==n.commands.get(i,(-2,0))
      for i in range(count):
       actual=lib.bk_prop_assets_state(assets,i).contents;wanted=n.read(i)
       for field in ['kind','action','hidden','background_wait','route_flag']:assert getattr(actual,field)==wanted[field],(g,a_idx,step,field,getattr(actual,field),wanted[field])
       assert tuple(actual.actions)==wanted['actions'];assert (actual.path.cursor,actual.path.first,actual.path.last)==wanted['indices'];assert (actual.wait.duration,actual.wait.deadline,actual.wait.armed)==wanted['timer']
       equal([*actual.path.position,actual.path.yaw,*actual.path.velocity,actual.target_yaw],[*wanted['position'],wanted['yaw'],*wanted['velocity'],wanted['target']],('state',i));pose=lib.bk_prop_assets_pose(assets,i);equal(lib.bk_actor_pose_placement(pose).contents.world,wanted['world'],('root',i))
       if step==-1:initial+=1
       else:steps+=1
    finally:lib.bk_prop_assets_destroy(assets)
    print(g,a_idx,'PASS',count,flush=True)
 finally:lib.bk_collision_destroy(collision);lib.bk_model_destroy(m);lib.bk_resources_destroy(store)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=45,initializations=initial,route_points=routes,instance_steps=steps,max_relative_error=worst,scope=__doc__)
 (ROOT/'local/original-prop-assets-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
