"""Retail flow16 camera loader and six controllers with actual two tracks.
Full4b79e0/4be4f8 observes file/path/model/context boundaries, with decoded
authored frame trees supplied to the native loader. Native selection/root
rotation/name lookup/presets run unchanged. Separate track VMs execute actual
XAN/SRT/controllers/publication; the target is an explicit stationary fixture.
Text-X parsing has its own original447667 oracle; no ending body/UI implied.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_menu_track_oracle import Native as Track,bind_actor,State,values,Visit,Edit,I
from original_prop_route_oracle import Native as Base
from original_matrix_oracle import multiply
from playback_binding import library
from model_binding import ROOT,Model,Frame
from clip_binding import State as ClipState
from bk3_assets import Archive
from original_ending_preset_oracle import Transitions,Gate
F=C.c_float;Fp=C.POINTER(F);U=C.c_uint32;Ip=C.POINTER(U);F16=F*16
class Presets(C.Structure):_fields_=[('active',(F*3)*4),('authored',(F*3)*4)]
class Loader(Base):
 camera=0x3000000
 def __init__(self,exe):
  super().__init__(exe);self.u.mem_map(0x4000000,0x100000)
  for a in [0x4ad8ec,0x4ad97e,0x401074,0x42892a,0x428a70,0x428bb6,0x4a7109,0x428caf,0x428b69,0x428a23,0x42cf0e]:
   self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
 def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def read(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def vector(self,a,v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
 def floats(self,a,n):return struct.unpack('<'+'f'*n,self.u.mem_read(a,n*4))
 def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0] if a else b''
 def hook(self,u,a,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);args=struct.unpack('<3I',u.mem_read(sp+4,12));result=0
  if a==0x4ad8ec:
   dst,prefix,suffix=args;u.mem_write(dst,self.string(prefix)+self.string(suffix)+b'\0');result=dst
  elif a==0x4ad97e:
   directory,filename,src=args;folder,_,name=self.string(src).rpartition(b'\\');u.mem_write(directory,folder+b'\\\0');u.mem_write(filename,name+b'\0')
  elif a==0x401074:
   i=len(self.loads);self.loads.append(self.string(args[0]).decode());result=0x4000000+i*0x10000
  elif a==0x42cf0e:self.fov=self.floats(sp+4,1)[0]
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
 def run(self,g,v,degrees,state,models,xans,flow=0x10,seconds=0):
  self.loads=[];self.fov=state.fov;self.u.mem_write(self.camera,bytes(0x600))
  self.word(self.camera+0x41c,degrees);self.vector(self.camera+0x4a4,state.matrix);self.vector(self.camera+0x5c8,state.focus)
  self.word(0x7219a8,g);self.u.mem_write(0x721b3d,bytes([v]));self.u.mem_write(0x5767c8,b'\1')
  self.vector(0x733700,[seconds])
  self.roots=[];self.frames=[]
  for i,(m,xan) in enumerate(zip(models,xans)):
   clip=0x4000000+i*0x10000;model=clip+0x5000;frames=clip+0x6000;links=clip+0xe000
   self.u.mem_write(clip,bytes(0x10000));self.u.mem_write(clip,xan[512:]);self.word(clip+0x18c,0);self.word(clip+0x160,model)
   world={};children={j:[] for j in range(m.frame_count)}
   for j in range(m.frame_count):
    f=m.frames[j];p=f.parent_index
    if p==0xffffffff:root=j;world[j]=list(f.local)
    else:children[p].append(j);world[j]=list(multiply(self.u,f.local,world[p]))
   self.word(model+0x14,frames+root*0x400);self.roots.append(root);self.frames.append(frames)
   for j in range(m.frame_count):
    f=m.frames[j];p=frames+j*0x400;self.u.mem_write(p+8,f.name+b'\0');self.vector(p+0x80,f.local);self.vector(p+0xc0,world[j]);self.vector(p+0x100,I if f.parent_index==0xffffffff else world[f.parent_index])
    c=children[j];self.word(p+0x238,len(c))
    if c:self.word(p+0x230,links+c[0]*16)
    for k,n in enumerate(c):
     self.word(links+n*16,frames+n*0x400);self.word(links+n*16+8,links+c[k+1]*16 if k+1<len(c) else 0)
  self.call(0x4b79e0,struct.pack('<II',self.camera,flow))
  assert self.loads==['\\cam00_00.xan',f'\\cam{g+1:02}_50.xan' if flow==0x48 else '\\cam00_03.xan'],self.loads
  out=State.from_buffer_copy(state);out.pose.position[:]=self.floats(self.camera+0x420,3)
  out.yaw,out.pitch,out.radius,out.height=self.floats(self.camera+0x42c,4);out.fov=self.fov
  out.matrix[:]=self.floats(self.camera+0x4a4,16);out.focus[:]=self.floats(self.camera+0x5c8,3)
  return out,bytes(self.u.mem_read(self.camera+0x444,96))
def bindings(lib):
 bind_actor(lib)
 defs=[('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
 ('bk_ending_camera_assets_create',[C.c_void_p,C.c_uint,C.c_uint,C.c_int32,C.c_void_p],C.c_void_p),('bk_ending_camera_assets_destroy',[C.c_void_p],None),
 ('bk_ending_camera_assets_pose',[C.c_void_p,C.c_uint],C.c_void_p),('bk_ending_camera_assets_root',[C.c_void_p,C.c_uint],U),('bk_ending_camera_assets_node',[C.c_void_p,C.c_uint],U),
 ('bk_ending_camera_assets_attach',[C.c_void_p,C.c_void_p,Ip,C.POINTER(State),C.POINTER(Presets),C.c_void_p],C.c_int),
 ('bk_ending_camera_assets_step',[C.c_void_p,C.c_void_p,Ip,C.POINTER(State),C.c_int,Fp,Fp,C.c_uint,U,F,C.c_void_p],C.c_int),
 ('bk_ending_camera_assets_preset',[C.c_void_p,C.c_void_p,Ip,C.POINTER(State),C.POINTER(Transitions),C.POINTER(Presets),C.c_int,C.c_uint,Fp,C.POINTER(Gate),C.c_uint8,F,C.POINTER(C.c_int),C.c_void_p],C.c_int),
 ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model)),('bk_actor_pose_parent_world',[C.c_void_p,U],Fp),('bk_actor_pose_visibility',[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p],C.c_int),
 ('bk_actor_pose_create_loaded',[C.POINTER(Model),C.c_void_p,U,Fp,F,C.c_void_p],C.c_void_p),
 ('bk_actor_forest_create',[C.POINTER(C.c_void_p),U,C.c_void_p],C.c_void_p),('bk_actor_forest_destroy',[C.c_void_p],None),('bk_actor_forest_node',[C.c_void_p,U,U],U),
 ('bk_actor_forest_attach',[C.c_void_p,U,U,C.c_void_p],C.c_int),('bk_actor_forest_anchor',[C.c_void_p,U,Fp,U,C.c_void_p],C.c_int),
 ('bk_actor_forest_draw',[C.c_void_p,U,C.POINTER(C.POINTER(Visit)),Ip,C.c_void_p],C.c_int),
 ('bk_ending_camera_root_rotation',[Fp,Fp,C.c_int32],C.c_int),('bk_ending_camera_config',[C.POINTER(Presets),C.c_uint,C.c_uint],C.c_int)]
 for name,args,res in defs:f=getattr(lib,name);f.argtypes=args;f.restype=res
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path)
 ap.add_argument('--output',type=Path,default=ROOT/'local/original-ending-camera-six-oracle.json');a=ap.parse_args()
 exe=a.exe.read_bytes();lib=library();bindings(lib);e=C.create_string_buffer(256);loader=Loader(exe);arc=Archive(a.data/'bk3_04.pp');worst=0.;matrices=frames=0;indices=(U*2)(1,2)
 xans=[arc.read(next(t for t in arc.entries if t.name==name)) for name in ['cam00_00.xan','cam00_03.xan']]
 def equal(got,want,label):
  nonlocal worst
  for i,(g,w) in enumerate(zip(got,want)):
   d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,i,g,w,d)
 def compare(pose,model,vm,label):
  nonlocal matrices
  for j in range(model.frame_count):
   for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
    equal(getattr(lib,fn)(pose,j)[:16],vm.floats(vm.frames+j*0x400+off,16),(label,j,fn));matrices+=1
  st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),(label,'clock'))
 store=lib.bk_resources_create(e);assert store,e.value
 assert lib.bk_resources_mount(store,b'bk3_04',str(a.data/'bk3_04.pp').encode(),e)
 fixture=bytearray(0x5190);fixture[:9]=b'target.x\0';fixture[256:265]=b'target.x\0'
 clips=lib.bk_clip_set_decode(bytes(fixture),len(fixture),e);assert clips,e.value
 f=Frame();f.id=1;f.parent_index=f.mesh_index=0xffffffff;f.local[:]=I
 m=Model();m.frames=C.pointer(f);m.frame_count=1
 target=lib.bk_actor_pose_create_loaded(C.byref(m),clips,0,(F*3)(100,18,40),0,e);assert target,e.value
 try:
  for g in range(5):
   for v in range(10):
    degrees=27+g*61+v*9;s=State();s.pose.world[:]=s.matrix[:]=I;s.pose.world[12]=9;s.matrix[12]=21;s.focus[:]=[7,8,9];s.fov=.4
    assets=lib.bk_ending_camera_assets_create(store,g,v,degrees,e);assert assets,e.value;forest=None
    try:
     poses=[lib.bk_ending_camera_assets_pose(assets,i) for i in range(2)];models=[lib.bk_actor_pose_model(p).contents for p in poses]
     wanted,preset=loader.run(g,v,degrees,s,models,xans)
     forest=lib.bk_actor_forest_create((C.c_void_p*3)(target,*poses),3,e);assert forest,e.value
     t=lib.bk_actor_forest_node(forest,0,0);assert lib.bk_actor_forest_attach(forest,0,t,e)
     assert lib.bk_actor_forest_anchor(forest,1,s.pose.world,0,e)
     presets=Presets();assert lib.bk_ending_camera_assets_attach(assets,forest,indices,C.byref(s),C.byref(presets),e),e.value
     equal(values(s),values(wanted),(g,v,'load state'));assert bytes(presets)==preset,(g,v,'presets')
     vms=[]
     for i in range(2):
      model=models[i];vm=Track(exe);vm.bind(C.string_at(model.source,model.source_size),model);vm.bind_clip(xans[i],lib.bk_ending_camera_assets_node(assets,i));vm.connect()
      angle=C.c_float(degrees*.01745329238474369).value;vm.vector(vm.argument,[0,1,0]);vm.call(0x42363b,struct.pack('<IIIf',vm.frames+vm.root*0x400,1,vm.argument,angle))
      for j in range(model.frame_count):
       equal(lib.bk_actor_pose_local(poses[i],j)[:16],loader.floats(loader.frames[i]+j*0x400+0x80,16),(g,v,'loader local',i,j))
       equal(lib.bk_actor_pose_frame(poses[i],j)[:16],loader.floats(loader.frames[i]+j*0x400+0xc0,16),(g,v,'loader cached',i,j))
      compare(poses[i],model,vm,(g,v,'initial',i));vms.append(vm)
     vm=vms[0];vm.set_state(s);vm.uc.mem_write(0xbeeb84,b'\x10');vm.word(0x719448,vm.head);vm.word(0x721ef4,vm.head);vm.vector(vm.head+0xf0,[100,18,40])
     clocks=Transitions(.2,.75,1)
     vm.uc.mem_write(vm.controller+0x444,bytes(presets))
     for addr,value in zip([0x719c64,0x7099a8,0x7099e4],[clocks.preset_progress,clocks.zoom_progress,clocks.zoom_fov]):vm.vector(addr,[value])
     for step in range(120):
      kind=step%6;dt=C.c_float([0,.016,.033,.1,.25][step%5]).value;hidden=step%11==3
      edit=Edit(lib.bk_ending_camera_assets_root(assets,0),hidden);assert lib.bk_actor_pose_visibility(poses[0],C.byref(edit),1,e)
      vm.call(0x423a99,struct.pack('<II',vm.frames+vm.root*0x400,hidden))
      offset=(F*3)(1,2,3);motion=(F*2)(2,-1);buttons=step%3;vm.motion=list(motion);vm.buttons=buttons;vm.vector(0x733700,[dt])
      if kind<4:
       vm.call([0x4bb0a4,0x4e1711,0x4e1d16,0x4df411][kind],struct.pack('<I3f',vm.controller,*offset))
       assert lib.bk_ending_camera_assets_step(assets,forest,indices,C.byref(s),kind,offset,motion,buttons,t,dt,e),(g,v,step,e.value)
      else:
       choice=(step//6)%3;gate=Gate(0x18 if step%5 else 8,8 if step%7 else 1,0,4,0,0,0,0)
       vm.uc.mem_write(0x721ad4,bytes([gate.previous_flow]))
       for addr,name in zip([0x721e00,0x721ed8,0x721ee0,0x721ee4,0x721eec,0x721ef0,0x719b20],[f[0] for f in Gate._fields_[1:]]):vm.word(addr,getattr(gate,name))
       vm.call([0x4e0ecb,0x4bc444][kind-4],struct.pack('<II3fI',vm.controller,choice,*offset,0xffffffff))
       done=vm.uc.reg_read(UC_X86_REG_EAX)&255;completed=C.c_int(-1)
       assert lib.bk_ending_camera_assets_preset(assets,forest,indices,C.byref(s),C.byref(clocks),C.byref(presets),kind-4,choice,offset,C.byref(gate),0x10,dt,C.byref(completed),e),(g,v,step,e.value)
       assert completed.value==done
       equal([clocks.preset_progress,clocks.zoom_progress,clocks.zoom_fov],[vm.floats(a,1)[0] for a in [0x719c64,0x7099a8,0x7099e4]],(g,v,step,'transitions'))
      equal(values(s),values(vm.output_state()),(g,v,step,'camera'));compare(poses[0],models[0],vm,(g,v,step,'pre-draw'))
      visits=C.POINTER(Visit)();count=U();only_camera=step%7==3
      assert lib.bk_actor_forest_draw(forest,1 if only_camera else 0,C.byref(visits),C.byref(count),e),e.value
      vm.draw(vm.rendered if only_camera else vm.context)
      if not only_camera:vms[1].publish()
      compare(poses[0],models[0],vm,(g,v,step,'draw'));compare(poses[1],models[1],vms[1],(g,v,step,'secondary'));frames+=1
    finally:lib.bk_actor_forest_destroy(forest);lib.bk_ending_camera_assets_destroy(assets)
   print('group',g,'PASS',flush=True)
  # Independently execute42363b over nonidentity locals and wide integer yaw.
  rng=random.Random(0x4b88fb)
  for case in range(1000):
   base=F16(*[rng.uniform(-3,3) for _ in range(16)]);base[3]=base[7]=base[11]=0;base[15]=1
   degrees=rng.choice([-(1<<31),(1<<31)-1,-16777217,16777217,rng.randrange(-1000,1000)])
   ptr=0x3008000;loader.vector(ptr+0x80,base);loader.vector(ptr+0x100,I);loader.vector(ptr+0x400,[0,1,0])
   loader.call(0x42363b,struct.pack('<IIIf',ptr,1,ptr+0x400,C.c_float(degrees*.01745329238474369).value))
   out=F16();assert lib.bk_ending_camera_root_rotation(out,base,degrees)
   equal(out,loader.floats(ptr+0x80,16),('root',case));matrices+=1
 finally:lib.bk_actor_pose_destroy(target);lib.bk_clip_set_destroy(clips);lib.bk_resources_destroy(store)
 report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=50,native_loaders=50,frames=frames,matrices=matrices,integer_root_rotations=1000,max_relative_error=worst,scope=__doc__)
 a.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS',report,flush=True)
if __name__=='__main__':main()
