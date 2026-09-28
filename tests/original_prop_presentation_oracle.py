"""Actual17 prop XAN/OBJM instances: original512102 request/visibility/SRT,
root setter and cached hierarchy traversal. Separate original4b124d material
rule traversal checks real prop materials. No animation/math/material hooks.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn.x86_const import *
from original_actor_phase_oracle import Native,bind
from original_npc_fade_oracle import Native as MaterialNative,bind as bind_material
from original_prop_motion_oracle import Effects
from original_placed_pose_oracle import I
from model_binding import ROOT,Model,Material,decode
from playback_binding import library
from clip_binding import State
from bk3_assets import Archive

class VM(Native):
 def call(self,address,args):
  u=self.uc;u.mem_write(self.stack,struct.pack('<I',self.stop)+args);u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f);u.emu_start(address,self.stop,count=400000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop,hex(u.reg_read(UC_X86_REG_EIP));return u.reg_read(UC_X86_REG_EAX)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();bind(lib);bind_material(lib);n=VM(exe);mn=MaterialNative(exe);err=C.create_string_buffer(256);arc=Archive(args.data/'bk3_07.pp')
 lib.bk_actor_pose_create_loaded.argtypes=[C.POINTER(Model),C.c_void_p,C.c_uint32,C.POINTER(C.c_float),C.c_float,C.c_void_p];lib.bk_actor_pose_create_loaded.restype=C.c_void_p
 lib.bk_actor_pose_advance.argtypes=[C.c_void_p,C.c_int,C.c_float,C.c_void_p]
 lib.bk_prop_material_apply.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Effects),C.c_void_p]
 class Edit(C.Structure):_fields_=[('frame',C.c_uint32),('hidden',C.c_uint32)]
 lib.bk_actor_pose_visibility.argtypes=[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p]
 names=['h90_00','h91_00','h92_00','h93_00','h93_10','h93_11','h93_12','h93_13','h93_14','h94_00','h95_00','h95_01','h96_00','h98_00','h98_10','h98_20','h98_30'];steps=matrices=materials=0;worst=0;records=[]
 for name in names:
  xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'));filename=xan[:256].split(b'\0')[0].decode();raw=arc.read(next(e for e in arc.entries if e.name==filename));ok,m,msg=decode(lib,raw);assert ok,msg;s=lib.bk_clip_set_decode(xan,len(xan),err);assert s,err.value;n.bind(raw,m.contents)
  n.uc.mem_write(n.rendered,bytes(0x19000));n.uc.mem_write(n.clip,xan[512:]);n.word(n.clip+0x18c,0);n.word(n.clip+0x160,n.model);n.word(n.model+0x14,n.frames+n.root*0x400);n.word(n.model+0x148,n.group);n.vector(n.group+0x74,[0]);n.word(n.group+0x78,len(n.tracks));n.word(n.group+0x7c,n.track_array)
  for i,(obj,_,frame,_) in enumerate(n.tracks):n.word(n.track_array+i*4,obj);n.word(obj+0x74,n.frames+frame*0x400)
  n.publish();n.word(0xbf4b50,1);n.word(0x729d80,n.clip)
  pos=(C.c_float*3)(11,-2,33);yaw=C.c_float(179.32).value;n.place_actor(pos,yaw);pose=lib.bk_actor_pose_create_loaded(m,s,n.root,pos,yaw,err);assert pose,(name,err.value)
  mat=lib.bk_material_pose_create(m,err);assert mat,err.value;mn.create(m.contents,n.root,[])
  def equal(a,b,label):
   nonlocal worst
   for x,y in zip(a,b):
    d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(name,label,x,y,d)
  def check():
   nonlocal matrices
   st=State();assert lib.bk_actor_pose_state(pose,C.byref(st));equal([getattr(st,k) for k,_ in st._fields_],n.state(),'timeline')
   for f in range(m.contents.frame_count):
    equal(lib.bk_actor_pose_frame(pose,f)[:16],n.floats(n.frames+f*0x400+0xc0,16),('world',f));equal(lib.bk_actor_pose_local(pose,f)[:16],n.floats(n.frames+f*0x400+0x80,16),('local',f));matrices+=2
  try:
   check();slots=[i for i in range(128) if lib.bk_clip_definition(s,i).contents.active]
   for frame in range(120):
    pos=(C.c_float*3)(11+frame*.3,-2+(frame%9)*.05,33-frame*.2);yaw=C.c_float(179.32+frame*.23).value;n.place_actor(pos,yaw);assert lib.bk_actor_pose_place(pose,pos,yaw,err),err.value
    action=0 if frame<20 or not slots else slots[(frame//19)%len(slots)];hidden=frame%11 in [2,3];dt=C.c_float([0,1/60,.1,.25][frame%4]).value;n.word(0x729d90,action);n.uc.mem_write(0x72a0a8,bytes([hidden]));n.vector(0x733700,[dt]);n.call(0x512102,b'');edit=Edit(n.root,hidden);assert lib.bk_actor_pose_visibility(pose,C.byref(edit),1,err);assert lib.bk_actor_pose_advance(pose,action,C.c_float(dt*.5).value,err),err.value;check()
    if frame%7!=3:n.publish();lib.bk_actor_pose_publish(pose);check()
    policy=[-2,-1,0,1][frame%4];alpha=C.c_float([0,.2,.6,1,-1,2][frame%6]).value;out=Effects(0,policy,alpha)
    if policy!=-2:
     bits=struct.unpack('<I',struct.pack('<f',alpha))[0]
     if policy==-1:mn.call(0x4b0de5,mn.frames[n.root],bits)
     else:mn.call(0x4b124d,mn.frames[n.root],bits,policy)
    assert lib.bk_prop_material_apply(mat,n.root,C.byref(out),err),err.value
    for i,want in enumerate(mn.values()):
     actual=lib.bk_material_pose_material(mat,i).contents;assert C.string_at(C.addressof(actual)+Material.diffuse.offset,68)==want,(name,frame,i,actual.name);materials+=1
    steps+=1
   records.append(dict(name=name,model=filename,frames=m.contents.frame_count,sha256=hashlib.sha256(raw).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest()))
   print(name,'PASS',flush=True)
  finally:lib.bk_material_pose_destroy(mat);lib.bk_actor_pose_destroy(pose);lib.bk_clip_set_destroy(s);lib.bk_model_destroy(m)
 r=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,matrices=matrices,material_snapshots=materials,max_relative_error=worst,records=records,scope=__doc__)
 (ROOT/'local/original-prop-presentation-oracle.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
if __name__=='__main__':main()
