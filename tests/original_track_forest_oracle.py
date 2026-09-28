"""Real FOLLOW/TRACK XAN controllers and global publication in one native VM.

Executes original4bdc12/4ba2f2, animation,42261a, camera prepass and D3DX.
Synthetic actor/head inputs and no-obstacle services remain explicit. Only
matrix-stack allocation/release and GPU view upload are additionally hooked.
Selected camera-only draws and hidden tracks deliberately hold Cam_AUTO.
"""
import argparse, ctypes as C, hashlib, json, math, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_player_track_oracle import Native as TrackNative, View, Input, Effects
from original_follow_phase_oracle import bind_follow
from original_actor_phase_oracle import bind as bind_actor
from original_frame_tree_oracle import Visit
from original_aim_oracle import Pose, I
from playback_binding import library
from clip_binding import State
from model_binding import ROOT, decode
from bk3_assets import Archive
F16=C.c_float*16
class Edit(C.Structure):
    _fields_=[('frame',C.c_uint32),('hidden',C.c_uint32)]
class Native(TrackNative):
    extra=0x20180000
    def __init__(self, exe):
        super().__init__(exe)
        for a in [0x52440d,self.extra+0x100,self.extra+0x104]:
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=a,end=a)
    def read(self,a): return struct.unpack('<I',self.uc.mem_read(a,4))[0]
    def boundary(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp)
        if a==0x52440d:
            u.mem_write(self.matrix_stack,struct.pack('<5I',self.extra,1024,self.matrices,0,1))
            self.vector(self.matrices,I);self.word(self.read(sp+8),self.matrix_stack);extra=8
        else: extra=4 if a==self.extra+0x100 else 12
        u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
    def connect(self):
        self.uc.mem_write(self.extra,bytes(self.uc.mem_read(0x53fb10,0x48)))
        self.word(self.extra+8,self.extra+0x100)
        self.word(self.extra+0x200,self.extra+0x300)
        self.word(self.extra+0x32c,self.extra+0x104)
        self.word(0x6455a0,self.extra+0x200)
        root=self.frames+self.root*0x400
        self.word(self.context+0x230,self.extra+0x400)
        self.word(self.context+0x234,self.extra+0x410)
        self.word(self.context+0x238,2)
        for i,f in enumerate([self.rendered,root]):
            link=self.extra+0x400+i*16
            self.word(link,f);self.word(link+4,link-16 if i else 0)
            self.word(link+8,link+16 if not i else 0);self.word(f+0x22c,self.context)
        self.vector(self.controller+0x4a4,self.floats(self.rendered+0xc0,16))
    def draw(self,target):
        self.call(0x42261a,struct.pack('<I',target))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();lib=library();bind_follow(lib);bind_actor(lib)
    fp=C.POINTER(C.c_float);error=C.create_string_buffer(256)
    bindings=[('bk_follow_camera_bind_track',[C.c_void_p],C.c_void_p),
      ('bk_follow_camera_player_view',[C.c_void_p,C.POINTER(View),C.c_int,C.POINTER(Input),C.POINTER(Effects),C.c_void_p],C.c_int),
      ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],fp),
      ('bk_actor_pose_visibility',[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p],C.c_int),
      ('bk_actor_forest_create',[C.POINTER(C.c_void_p),C.c_uint32,C.c_void_p],C.c_void_p),
      ('bk_actor_forest_destroy',[C.c_void_p],None),
      ('bk_actor_forest_attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p],C.c_int),
      ('bk_actor_forest_node',[C.c_void_p,C.c_uint32,C.c_uint32],C.c_uint32),
      ('bk_actor_forest_anchor',[C.c_void_p,C.c_uint32,fp,C.c_uint32,C.c_void_p],C.c_int),
      ('bk_actor_forest_view',[C.c_void_p],fp),
      ('bk_actor_forest_draw',[C.c_void_p,C.c_uint32,C.POINTER(C.POINTER(Visit)),C.POINTER(C.c_uint32),C.c_void_p],C.c_int)]
    for name,types,result in bindings:
        f=getattr(lib,name);f.argtypes=types;f.restype=result
    arc=Archive(args.data/'bk3_04.pp');records=[];worst=0;matrices=0;held=0
    def equal(a,b,label):
        nonlocal worst
        for j,(a,b) in enumerate(zip(a,b)):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
            assert math.isfinite(d) and d<3e-5,(name,step,label,j,a,b,d)
    for name in ['cam00_01','cam00_02']:
        vm=Native(exe)
        raw=arc.read(next(e for e in arc.entries if e.name==name+'.x'))
        xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'))
        ok,model,message=decode(lib,raw);assert ok,message
        clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value
        camera=forest=None
        try:
            vm.bind(raw,model.contents);track_node=C.c_uint32()
            assert lib.bk_model_find_frame(model,b'Cam_AUTO',C.byref(track_node),error)
            vm.bind_clip(xan,track_node.value);vm.connect()
            initial=I.copy();initial[13]=20
            pose=Pose(F16(*initial),(C.c_float*3)(0,20,0))
            camera=lib.bk_follow_camera_create(model,clips,vm.root,track_node.value,C.byref(pose),error);assert camera,error.value
            track=lib.bk_follow_camera_bind_track(camera);assert track
            forest=lib.bk_actor_forest_create((C.c_void_p*1)(track),1,error);assert forest,error.value
            root=lib.bk_actor_forest_node(forest,0,vm.root)
            assert lib.bk_actor_forest_attach(forest,0,root,error),error.value
            view=View();view.pose=pose;view.matrix[:]=initial;view.lean=10
            visits=C.POINTER(Visit)();count=C.c_uint32()
            for step in range(480):
                hidden=step%17 in [4,5]
                assert lib.bk_actor_pose_visibility(track,C.byref(Edit(vm.root,hidden)),1,error)
                vm.call(0x423a99,struct.pack('<II',vm.frames+vm.root*0x400,hidden))
                seconds=C.c_float([0,.016,.033,.1,.25][step%5]).value
                origin=(C.c_float*3)(math.sin(step*.03)*30,math.sin(step*.02),math.cos(step*.04)*40)
                head=(C.c_float*3)(origin[0]+1,20+math.sin(step*.07)*2,origin[2]+1)
                cached=list(lib.bk_follow_camera_track(camera)[:16])
                if step%2:
                    inp=Input(position=origin,seconds=seconds);effects=Effects()
                    wanted,reset=vm.step_player(inp)
                    assert lib.bk_follow_camera_player_view(camera,C.byref(view),6,C.byref(inp),C.byref(effects),error),error.value
                    assert effects.reset_mode==reset
                else:
                    wanted=vm.step(origin,head,seconds)
                    assert lib.bk_follow_camera_step(camera,origin,head,None,seconds,error),error.value
                current=lib.bk_follow_camera_pose(camera).contents
                equal(list(current.world)+list(current.position),wanted,'controller')
                assert list(lib.bk_follow_camera_track(camera)[:16])==cached
                state=State();assert lib.bk_follow_camera_clip_state(camera,C.byref(state))
                equal([getattr(state,n) for n,_ in State._fields_],vm.state(),'timeline')
                assert lib.bk_actor_forest_anchor(forest,1,current.world,0,error),error.value
                target=[0,1,root,lib.bk_actor_forest_node(forest,0,track_node.value)][step%4]
                native_target=vm.context if target==0 else vm.rendered if target==1 else vm.frames+(target-2)*0x400
                vm.draw(native_target)
                assert lib.bk_actor_forest_draw(forest,target,C.byref(visits),C.byref(count),error),error.value
                for f in range(model.contents.frame_count):
                    for getter,offset in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                        equal(getattr(lib,getter)(track,f)[:16],vm.floats(vm.frames+f*0x400+offset,16),(f,offset));matrices+=1
                equal(lib.bk_actor_forest_view(forest)[:16],vm.floats(0x642fa8,16),'view');matrices+=1
                fresh=list(lib.bk_follow_camera_track(camera)[:16])
                equal(fresh,vm.floats(vm.node+0xc0,16),'shared Cam_AUTO')
                if fresh==cached:held+=1
            records.append(dict(model=name,frames=480,model_sha256=hashlib.sha256(raw).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest()))
            print(name,'PASS',flush=True)
        finally:
            lib.bk_actor_forest_destroy(forest);lib.bk_follow_camera_destroy(camera);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,frames=960,matrices=matrices,held_track_draws=held,max_relative_error=worst,scope=__doc__)
    (ROOT/'local/original-track-forest-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
