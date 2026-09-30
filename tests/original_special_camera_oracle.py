"""Original flow48 camera loader and four controllers with actual Japanese assets.

Native primary, secondary, body XANs and an independent native frame-tree VM
share their original caches; no portable matrices feed the reference after
initialization. Full4b79e0/4be4f8 observes paths/presets/selection and one loading
advance; that advance executes separately in the secondary VM. GPU/UI/audio,
face control and the complete4e29b0 loader are not covered here.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_ending_camera_assets_oracle import Loader,Presets,bindings as base_bind
from original_menu_track_oracle import Native as CameraNative,State,values,Visit,Edit,I
from original_actor_phase_oracle import Native as ActorNative
from original_selection_world_oracle import Publication
from original_ending_preset_oracle import Transitions
from clip_binding import State as ClipState
from playback_binding import library
from model_binding import ROOT,Model,decode
from bk3_assets import Archive
F,U,P=C.c_float,C.c_uint32,C.c_void_p
FP,UP=C.POINTER(F),C.POINTER(U)
NONE=0xffffffff

class Body(ActorNative):
    def call(self,addr,args):
        # h03_55's first full SRT preprocessing exceeds the camera VM budget.
        self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(addr,self.stop,count=400000000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop,(hex(addr),hex(self.uc.reg_read(UC_X86_REG_EIP)))
        return self.uc.reg_read(UC_X86_REG_EAX)

class Camera(CameraNative):
    secondary_token,secondary_model,secondary_node,focus_token=0x3007000,0x3007400,0x3007800,0x300b000
    def __init__(self,exe):
        super().__init__(exe);self.active=False
        for addr in [0x401b0a,0x4026fe,0x423be2]:self.uc.hook_add(UC_HOOK_CODE,self.delegate,begin=addr,end=addr)
    def delegate(self,u,addr,size,ctx):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret,first,arg=struct.unpack('<III',u.mem_read(sp,12))
        if addr==0x423be2:
            self.publication.refresh()
            self.vector(self.secondary_node+0xc0,self.secondary.floats(self.secondary.frames+self.secondary_frame*0x400+0xc0,16))
            self.vector(self.focus_token+0xc0,self.body.floats(self.body.frames+self.focus_frame*0x400+0xc0,16))
        elif first==self.secondary_token:self.secondary.call(addr,struct.pack('<II',self.secondary.clip,arg))
        else:return
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def configure(self,second,body,pub,secondary_frame,focus_frame):
        self.secondary,self.body,self.publication=second,body,pub
        self.secondary_frame,self.focus_frame=secondary_frame,focus_frame
        self.word(self.controller+4,self.secondary_token);self.word(self.controller+12,self.secondary_node)
        self.word(self.controller+16,self.focus_token);self.word(self.secondary_token+0x160,self.secondary_model)
        self.word(self.secondary_model+0x14,self.secondary_node)
        self.uc.mem_write(0xbeeb84,b'\x48');self.active=True
    def run(self,kind,choice,offset,motion,buttons,dt):
        self.motion=list(motion);self.buttons=buttons;self.vector(0x733700,[dt])
        if kind==0:
            # Deliberately NOT dt:4bb82e ignores its third argument entirely.
            self.call(0x4bb82e,struct.pack('<IIf',self.controller,choice,0))
        elif kind==1:self.call(0x4bc444,struct.pack('<II3f',self.controller,choice,*offset))
        elif kind==2:self.call(0x4bb0a4,struct.pack('<I3f',self.controller,*offset))
        else:self.call(0x4bb612,struct.pack('<I',self.controller))
        return self.uc.reg_read(UC_X86_REG_EAX)&255

class SpecialLoader(Loader):
    def __init__(self,exe):
        super().__init__(exe)
        self.u.hook_add(UC_HOOK_CODE,self.advance,begin=0x4026fe,end=0x4026fe)
    def advance(self,u,addr,size,ctx):
        sp=u.reg_read(UC_X86_REG_ESP);ret,actor=struct.unpack('<II',u.mem_read(sp,8))
        assert actor==0x4010000;self.advances.append(struct.unpack('<f',u.mem_read(sp+8,4))[0])
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)

def bind(lib):
    base_bind(lib)
    apis=[('create',[P,U,P],P),('destroy',[P],None),('pose',[P,U],P),('root',[P,U],U),('node',[P,U],U),
        ('attach',[P,P,UP,F,C.POINTER(State),C.POINTER(Presets),P],C.c_int),
        ('step',[P,P,UP,C.POINTER(State),C.POINTER(Transitions),C.POINTER(Presets),C.c_int,C.c_int,FP,FP,U,U,F,C.POINTER(C.c_uint8),P],C.c_int),
        ('place',[P,P,UP,U,FP,F,P],C.c_int)]
    for name,args,result in apis:
        fn=getattr(lib,'bk_special_camera_assets_'+name);fn.argtypes=args;fn.restype=result
    for name,args,result in [('bk_special_event_camera_config',[C.POINTER(Presets),U],C.c_int),
        ('bk_actor_pose_select',[P,U,C.c_int,P],C.c_int),('bk_actor_pose_advance',[P,C.c_int,F,P],C.c_int),
        ('bk_actor_pose_root_local',[P,FP,P],C.c_int),('bk_actor_forest_refresh',[P,P],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path)
    ap.add_argument('--frames',type=int,default=240);ap.add_argument('--output',type=Path,default=ROOT/'local/original-special-camera.json');a=ap.parse_args()
    if a.frames<1:ap.error('--frames must be positive')
    exe=a.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256)
    store=lib.bk_resources_create(error);assert store,error.value
    packs={p:Archive(a.data/(p+'.pp')) for p in ['bk3_04','bk3_14']}
    for p in packs:assert lib.bk_resources_mount(store,p.encode(),str(a.data/(p+'.pp')).encode(),error),error.value
    def raw(pack,name):return packs[pack].read(next(e for e in packs[pack].entries if e.name==name))
    worst=0;matrices=frames=loaders=placements=rejections=0;digest=hashlib.sha256();rows=[];modes=[0]*4
    def equal(got,want,label):
        nonlocal worst
        assert len(got)==len(want)
        for index,(x,y) in enumerate(zip(got,want)):
            d=abs(x-y)/max(1,abs(y));worst=max(worst,d)
            assert math.isfinite(d) and d<3e-6,(label,index,x,y,d)
    loader=SpecialLoader(exe)
    try:
      for group in range(5):
        assets=lib.bk_special_camera_assets_create(store,group,error);assert assets,error.value
        body_model=C.POINTER(Model)();body_clips=body=forest=None
        try:
            name=f'h{group+1:02}_55.xan';xan=raw('bk3_14',name);model_name=xan[:256].split(b'\0')[0].decode()
            data=raw('bk3_14',model_name);rc,body_model,msg=decode(lib,data);assert rc==1,msg
            body_clips=lib.bk_clip_set_decode(xan,len(xan),error);assert body_clips,error.value
            root=next(i for i in range(body_model.contents.frame_count) if body_model.contents.frames[i].parent_index==NONE)
            body=lib.bk_actor_pose_create_loaded(body_model,body_clips,root,(F*3)(0,0,0),0,error);assert body,error.value
            assert lib.bk_actor_pose_root_local(body,body_model.contents.frames[root].local,error)
            assert lib.bk_actor_pose_select(body,0,1,error)
            focus_name=['A_kao','A_Kao','kubiX','mune','atama'][group]
            focus=U();assert lib.bk_model_find_frame(body_model,focus_name.encode(),C.byref(focus),error),(group,error.value)
            poses=[lib.bk_special_camera_assets_pose(assets,i) for i in range(2)]+[body]
            models=[lib.bk_actor_pose_model(p).contents for p in poses]
            roots=[lib.bk_special_camera_assets_root(assets,i) for i in range(2)]+[root]
            nodes=[lib.bk_special_camera_assets_node(assets,i) for i in range(2)]+[focus.value]
            xans=[raw('bk3_04','cam00_00.xan'),raw('bk3_04',f'cam{group+1:02}_50.xan'),xan]
            vms=[Camera(exe),ActorNative(exe),Body(exe)];cam,second,actor=vms
            for i,(vm,m) in enumerate(zip(vms,models)):
                vm.bind(C.string_at(m.source,m.source_size),m);vm.bind_clip(xans[i],nodes[i])
            assert actor.find(focus_name)==focus.value
            camera=State();camera.pose.world[:]=I;camera.pose.world[12:15]=[9,22,-13];camera.pose.position[:]=[7,11,-12]
            camera.matrix[:]=I;camera.matrix[12]=21;camera.focus[:]=[3,-5,8];camera.yaw=63;camera.fov=.7
            before=State.from_buffer_copy(camera);presets=Presets();clocks=Transitions(.25,.125,.73);indices=(U*2)(0,1)
            load=F([0,.016,.1,1,4][group]).value
            loader.advances=[];wanted,bank=loader.run(group,0,0,before,models[:2],xans[:2],flow=0x48,seconds=load)
            assert loader.advances==[load];loaders+=1
            forest=lib.bk_actor_forest_create((P*3)(*poses),3,error);assert forest,error.value
            assert lib.bk_actor_forest_anchor(forest,1,camera.pose.world,0,error)
            body_root=lib.bk_actor_forest_node(forest,2,root);assert lib.bk_actor_forest_attach(forest,0,body_root,error)
            assert lib.bk_special_camera_assets_attach(assets,forest,indices,load,C.byref(camera),C.byref(presets),error),error.value
            equal(values(camera),values(wanted),('loader',group));assert bytes(presets)==bank
            # Attach refresh precedes the secondary's single loading advance.
            cam.connect();cam.set_state(camera);pub=Publication(exe);pub.configure(vms,models,cam);pub.refresh()
            second.call(0x4026fe,struct.pack('<If',second.clip,load))
            cam.configure(second,actor,pub,nodes[1],focus.value)
            cam.vector(cam.controller+0x444,list(presets.active[0])+list(presets.active[1])+list(presets.active[2])+list(presets.active[3]))
            for addr,value in zip([0x719c64,0x7099a8,0x7099e4],[clocks.preset_progress,clocks.zoom_progress,clocks.zoom_fov]):cam.vector(addr,[value])
            target=lib.bk_actor_forest_node(forest,2,focus.value)
            def check(label):
                nonlocal matrices
                equal(values(camera),values(cam.output_state()),(label,'camera'))
                equal([clocks.preset_progress,clocks.zoom_progress,clocks.zoom_fov],[cam.floats(x,1)[0] for x in [0x719c64,0x7099a8,0x7099e4]],(label,'transitions'))
                for i,(pose,m,vm) in enumerate(zip(poses,models,vms)):
                    for j in range(m.frame_count):
                        for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                            equal(getattr(lib,fn)(pose,j)[:16],vm.floats(vm.frames+j*0x400+off,16),(label,i,j,fn));matrices+=1
                    st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),(label,i,'clip'))
                digest.update(bytes(camera));digest.update(bytes(clocks))
            check(('load',group))
            snapshot=bytes(camera)
            assert not lib.bk_special_camera_assets_attach(assets,forest,indices,load,C.byref(camera),C.byref(presets),error)
            assert bytes(camera)==snapshot;rejections+=1
            for frame in range(a.frames):
                dt=F([0,1/60,.033,.1,.25,1][frame%6]).value;kind=0 if frame<30 else (frame//10)%4;modes[kind]+=1
                # Body local changes before camera; old caches remain until native refresh/draw.
                assert lib.bk_actor_pose_advance(body,-1,F(dt*.5),error),error.value
                actor.call(0x4026fe,struct.pack('<If',actor.clip,F(dt*.5).value))
                offset=(F*3)(-20 if group==3 else 0,0,-6.5 if group==0 else -6.7 if group==3 else 0)
                motion=(F*2)(math.sin(frame*.13)*12,math.cos(frame*.07)*3);buttons=frame%4;choice=frame%3 if kind==1 else 0
                fallback=kind==3 and frame%7==0;cam.word(cam.controller+16,0 if fallback else cam.focus_token)
                done=cam.run(kind,choice,offset,motion,buttons,dt);completed=C.c_uint8(177)
                assert lib.bk_special_camera_assets_step(assets,forest,indices,C.byref(camera),C.byref(clocks),C.byref(presets),kind,choice,offset,motion,buttons,NONE if fallback else target,dt,C.byref(completed),error),(group,frame,error.value)
                assert completed.value==(done if kind==1 else 177)
                check(('step',group,frame))
                #4e620a immediately follows camera; raw160 is radians, not degrees.
                position=(F*3)(-20 if group==3 else 0,0,40 if group==3 else 0)
                axis=(F*3)(0,1,0);angle=160 if group==2 else 0
                for i,vm in enumerate(vms[:2]):
                    if i:
                        vm.vector(0x300b800,axis);vm.call(0x4241e3,struct.pack('<IIIf',vm.frames+vm.root*0x400,vm.context,0x300b800,angle))
                    else:vm.call(0x422c49,struct.pack('<II3f',vm.frames+vm.root*0x400,vm.context,*position))
                    assert lib.bk_special_camera_assets_place(assets,forest,indices,i,axis if i else position,angle if i else 0,error),error.value;placements+=1
                check(('place',group,frame))
                if frame%7!=2:
                    target_draw=1 if frame%9==0 else body_root
                    visits=C.POINTER(Visit)();count=U();assert lib.bk_actor_forest_draw(forest,target_draw,C.byref(visits),C.byref(count),error),error.value
                    pub.sync_in();pub.call(0x42261a,struct.pack('<I',pub.frames+(1 if target_draw==1 else pub.roots[2])*0x400));pub.sync_out()
                    check(('draw',group,frame))
                frames+=1
            before=bytes(camera);done=C.c_uint8(177)
            assert not lib.bk_special_camera_assets_step(assets,forest,indices,C.byref(camera),C.byref(clocks),C.byref(presets),0,0,offset,motion,0,NONE,0,C.byref(done),error)
            assert bytes(camera)==before;rejections+=1
            rows.append(dict(group=group,focus=focus_name,body=model_name,frames=models[2].frame_count,
                camera_xan_sha256=[hashlib.sha256(x).hexdigest() for x in xans[:2]],body_sha256=hashlib.sha256(data).hexdigest()))
            print('PASS special camera group',group,flush=True)
        finally:
            lib.bk_actor_forest_destroy(forest);lib.bk_actor_pose_destroy(body);lib.bk_clip_set_destroy(body_clips)
            lib.bk_model_destroy(body_model);lib.bk_special_camera_assets_destroy(assets)
      for g in [5,NONE]:assert not lib.bk_special_camera_assets_create(store,g,error);rejections+=1
      empty=lib.bk_resources_create(error)
      try:assert not lib.bk_special_camera_assets_create(empty,0,error);rejections+=1
      finally:lib.bk_resources_destroy(empty)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),scope=__doc__,groups=5,native_loaders=loaders,
        frames=frames,controller_frames=modes,matrices=matrices,placements=placements,rejections=rejections,max_relative_error=worst,state_sha256=digest.hexdigest(),assets=rows)
    a.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS special camera:',json.dumps(report,sort_keys=True),flush=True)
if __name__=='__main__':main()
