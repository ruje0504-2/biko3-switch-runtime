"""Retail51ac5d with actual four-object selection forest and native components.

Original caller/body, camera/XAN, stage/XAN and face control execute in separate
VMs. A fifth original D3DX/frame-tree VM publishes their shared caches at the
actual camera prepass and draw boundaries; its outputs are copied to the VMs.
No port matrices/timelines feed the reference after initialization. Movie and
voice-envelope are explicit recorded services, not playback implementations.
Special1/4bb9de and GPU rasterization are outside this retail special0 test.
"""
import argparse, ctypes as C, hashlib, json, math, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_selection_actor_oracle import Native as BodyNative, bind as bind_body, FIELDS
from original_menu_track_oracle import Native as CameraNative
from original_actor_forest_oracle import VM as TreeNative
from original_actor_phase_oracle import Native as ActorNative
from original_menu_camera_oracle import State as Camera, values
from original_frame_tree_oracle import Visit
from original_aim_oracle import I
from clip_binding import State as ClipState
from playback_binding import library
from model_binding import ROOT
from bk3_assets import Archive
NONE=0xffffffff
class Input(C.Structure):
    _fields_=[('selected',C.c_uint),('camera_mode',C.c_uint),('buttons',C.c_uint),('voice_active',C.c_uint8),('seconds',C.c_float),('motion',C.c_float*2),('timestamp',C.c_uint32),('face_clocks',C.c_uint32*3)]
Movie=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
Voice=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_float,C.POINTER(C.c_float),C.c_void_p)
class Ops(C.Structure):_fields_=[('context',C.c_void_p),('movie_step',Movie),('voice_level',Voice)]

def bind(lib):
    bind_body(lib);fp=C.POINTER(C.c_float);ip=C.POINTER(C.c_uint32)
    apis=[('create',[C.c_void_p,C.c_uint,C.c_uint8,C.c_float,C.POINTER(Camera),ip,ip,C.c_void_p],C.c_void_p),
          ('destroy',[C.c_void_p],None),('replace',[C.c_void_p,C.c_uint,C.c_uint8,ip,ip,C.POINTER(C.c_void_p),C.c_void_p],C.c_int),
          ('step',[C.c_void_p,C.POINTER(Input),C.POINTER(Ops),ip,C.c_void_p],C.c_int),
          ('pose',[C.c_void_p,C.c_uint],C.c_void_p),('body',[C.c_void_p],C.c_void_p),('forest',[C.c_void_p],C.c_void_p),
          ('root',[C.c_void_p,C.c_uint],C.c_uint32),('focus',[C.c_void_p],C.c_uint32)]
    for name,args,result in apis:
        fn=getattr(lib,'bk_selection_world_'+name);fn.argtypes=args;fn.restype=result
    lib.bk_actor_forest_draw.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.POINTER(Visit)),ip,C.c_void_p];lib.bk_actor_forest_draw.restype=C.c_int
    lib.bk_actor_forest_view.argtypes=[C.c_void_p];lib.bk_actor_forest_view.restype=fp

class Publication(TreeNative):
    def configure(self,vms,models,camera):
        self.vms,self.models,self.camera=vms,models,camera
        self.bindings=[None,None];self.roots=[];parents=[NONE,0]
        for vm,m in zip(vms,models):
            base=len(self.bindings);self.roots.append(base+vm.root)
            self.bindings.extend((vm,f) for f in range(m.frame_count))
            parents.extend(0 if m.frames[f].parent_index==NONE else base+m.frames[f].parent_index for f in range(m.frame_count))
        assert len(parents)<2048
        self.u.mem_write(self.frames,bytes(len(parents)*0x400))
        for n,p in enumerate(parents):
            f=self.frames+n*0x400;self.word(f+0x22c,0 if p==NONE else self.frames+p*0x400)
            children=[i for i,x in enumerate(parents) if x==n]
            self.word(f+0x238,len(children));self.word(f+0x230,self.links+children[0]*16 if children else 0)
            self.word(f+0x234,self.links+children[-1]*16 if children else 0)
            for j,c in enumerate(children):
                link=self.links+c*16;self.word(link,self.frames+c*0x400)
                self.word(link+4,self.links+children[j-1]*16 if j else 0);self.word(link+8,self.links+children[j+1]*16 if j+1<len(children) else 0)
        self.word(0x645600,self.frames);self.word(0x645604,self.frames+0x400)
        self.word(0x63f104,1);self.word(0x6455f4,0)
    def sync_in(self):
        for n,b in enumerate(self.bindings):
            f=self.frames+n*0x400
            if n<2:
                for off in [0x80,0xc0,0x100]:self.vector(f+off,I if n==0 or off==0x100 else self.camera.floats(self.camera.rendered+off,16))
            else:
                vm,node=b;src=vm.frames+node*0x400
                for off in [0x80,0xc0,0x100]:self.u.mem_write(f+off,bytes(vm.uc.mem_read(src+off,64)))
                self.u.mem_write(f+0x70,bytes(vm.uc.mem_read(src+0x70,4)))
    def sync_out(self):
        for n,b in enumerate(self.bindings[2:],2):
            vm,node=b
            for off in [0xc0,0x100]:vm.uc.mem_write(vm.frames+node*0x400+off,bytes(self.u.mem_read(self.frames+n*0x400+off,64)))
    def refresh(self):
        self.sync_in();self.call(0x423be2,b'');self.sync_out()
    def draw(self,object):
        self.sync_in();self.call(0x42261a,struct.pack('<I',self.frames+self.roots[object]*0x400));self.sync_out()

class Caller(BodyNative):
    stage_token=0x300c000
    def __init__(self,exe):
        super().__init__(exe)
        for a in [0x401b0a,0x4bb612,0x4bac5b]:self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=a,end=a)
    def boundary(self,u,a,size,_):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret,first=struct.unpack('<II',u.mem_read(sp,8))
        if a in [0x4bb612,0x4bac5b]:
            self.events.append('camera');self.camera.call(a,struct.pack('<I',self.camera.controller))
            for off in [0x80,0xc0,0x100]:u.mem_write(self.rendered+off,bytes(self.camera.uc.mem_read(self.camera.rendered+off,64)))
        elif a in [0x401b0a,0x4026fe] and first==self.stage_token:
            arg=struct.unpack('<I',u.mem_read(sp+8,4))[0]
            self.stage.call(a,struct.pack('<II',self.stage.clip,arg))
            if a==0x4026fe:self.events.append('stage')
        elif a==0x401b0a:return
        else:return super().boundary(u,a,size,_)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,inp,alt,mouth):
        self.events=[];self.commands=[];self.now=inp.timestamp;self.clocks=list(inp.face_clocks);self.texture=None
        self.word(0xbfbba4,inp.camera_mode);self.word(0xbef77c,self.clip);self.word(0xbef780,self.stage_token);self.word(0xbf9b94,inp.selected)
        self.uc.mem_write(0xbef778,b'\0');self.uc.mem_write(0xbfbb9d,bytes([inp.voice_active]));self.uc.mem_write(0xbf4100,bytes([alt]))
        self.vector(0x733700,[inp.seconds]);self.vector(0x300d000,[mouth])
        self.camera.motion=list(inp.motion);self.camera.buttons=inp.buttons;self.camera.vector(0x733700,[inp.seconds])
        self.active=True
        try:
            self.uc.mem_write(self.stack,struct.pack('<I',self.stop));self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x51ac5d,self.stop,count=400000000);assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        finally:self.active=False
        wanted=['camera']+(['movie'] if alt else [])+(['envelope'] if inp.voice_active else [])+['body','gaze','texture','range','request','mouth','blink','stage']
        assert self.events==wanted,(self.events,wanted)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store
    archives={}
    for pack in ['bk3_01','bk3_03','bk3_04','bk3_06']:
        assert lib.bk_resources_mount(store,pack.encode(),str(a.data/(pack+'.pp')).encode(),error),error.value
        if pack!='bk3_06':archives[pack]=Archive(a.data/(pack+'.pp'))
    assert lib.bk_resources_mount_directory(store,b'faces',str(a.data).encode(),20480,error)
    def raw(pack,name):return archives[pack].read(next(x for x in archives[pack].entries if x.name==name))
    frames=matrices=replacements=0;worst=0
    def equal(got,want,label):
        nonlocal worst
        assert len(got)==len(want)
        for j,(x,y) in enumerate(zip(got,want)):
            d=abs(x-y)/max(1,abs(y));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,j,x,y,d)
    observed=[];mouth=0.
    @Movie
    def movie(_,err):observed.append('movie');return 1
    @Voice
    def voice(_,seconds,out,err):observed.append('envelope');out[0]=mouth;return 1
    ops=Ops(None,movie,voice)
    try:
      for retained in range(5):
        camera=Camera();camera.pose.world[:]=I;camera.matrix[:]=I;camera.pose.world[12]=9
        clocks=(C.c_uint32*4)(1000,1001,1002,1003);rng=C.c_uint32(12345+retained)
        load=C.c_float([0,.016,.1,1,4][retained]).value
        world=lib.bk_selection_world_create(store,retained,0,load,C.byref(camera),clocks,C.byref(rng),error);assert world,error.value
        pub=Publication(exe);cam=CameraNative(exe);second=ActorNative(exe);stage=ActorNative(exe);body=Caller(exe)
        vms=[cam,second,stage,body];poses=[lib.bk_selection_world_pose(world,i) for i in range(4)]
        models=[lib.bk_actor_pose_model(p).contents for p in poses]
        for i,(vm,m) in enumerate(zip(vms,models)):
            pack='bk3_04' if i<2 else 'bk3_03' if i==2 else 'bk3_01'
            name=['cam00_00.xan',f'cam{retained+1:02}_50.xan','m60_00.xan','h01_60.xan'][i]
            vm.bind(C.string_at(m.source,m.source_size),m)
            node=next((j for j in range(m.frame_count) if m.frames[j].name==b'Cam_AUTO'),vm.root)
            vm.bind_clip(raw(pack,name),node)
        second.call(0x4026fe,struct.pack('<If',second.clip,load));cam.connect();cam.set_state(camera)
        body.camera=cam;body.stage=stage
        pub.configure(vms,models,cam);pub.refresh()
        # Camera's native423be2 is delegated to the shared original forest VM,
        # including current body/stage locals and the primary's newly sampled SRT.
        def refresh(u,addr,size,_):
            pub.refresh()
            focus=lib.bk_selection_actor_assets_focus(lib.bk_selection_world_body(world))
            cam.vector(0x300b000+0xf0,body.floats(body.frames+focus*0x400+0xf0,3))
            sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
            u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
        cam.uc.hook_add(UC_HOOK_CODE,refresh,begin=0x423be2,end=0x423be2)
        cam.word(cam.controller+0x10,0x300b000)
        def check(label):
            nonlocal matrices
            equal(values(camera),values(cam.output_state()),(label,'camera'))
            for idx,(pose,m,vm) in enumerate(zip(poses,models,vms)):
                for f in range(m.frame_count):
                    for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                        equal(getattr(lib,fn)(pose,f)[:16],vm.floats(vm.frames+f*0x400+off,16),(label,idx,f,fn));matrices+=1
                st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st))
                equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),(label,idx,'clip'))
        try:
          for variant in range(10):
            group,alt=divmod(variant,2)
            retired=C.c_void_p()
            assert lib.bk_selection_world_replace(world,group,alt,clocks,C.byref(rng),C.byref(retired),error),error.value
            lib.bk_selection_actor_assets_destroy(retired);replacements+=1
            owner=lib.bk_selection_world_body(world);poses[3]=lib.bk_selection_world_pose(world,3);models[3]=lib.bk_actor_pose_model(poses[3]).contents
            m=models[3];body.bind(C.string_at(m.source,m.source_size),m);body.bind_clip(raw('bk3_01',f'h{group+1:02}_{61 if alt else 60}.xan'),lib.bk_selection_actor_assets_root(owner))
            body.word(body.model+0x180,body.face_ptr);body.face_vm.seed(lib.bk_selection_actor_assets_face_state(owner).contents,rng.value)
            eyes=lib.bk_eye_assets_binding(lib.bk_selection_actor_assets_eyes(owner)).contents
            body.uc.mem_write(0xbf3b10,bytes(0x2c));body.word(0xbf3b38,eyes.mode)
            for j,f in enumerate(eyes.frames):body.word(0xbf3b28+j*4,0 if f==NONE else body.frames+f*0x400)
            pub.configure(vms,models,cam);pub.refresh();check(('replace',retained,variant))
            for step in range(12):
                inp=Input();inp.selected=group;inp.camera_mode=(step//3)%2;inp.buttons=step%4;inp.voice_active=step%3!=0
                inp.seconds=C.c_float([0,.016,.033,.1,.22,.5][step%6]).value;inp.motion[:]=[math.sin(step*.13)*12,math.cos(step*.07)*3]
                inp.timestamp=1100+(variant*12+step)*137;inp.face_clocks[:]=[inp.timestamp+j+1 for j in range(3)]
                mouth=C.c_float(step%11*.9).value
                body.run(inp,alt,mouth);observed.clear()
                assert lib.bk_selection_world_step(world,C.byref(inp),C.byref(ops),C.byref(rng),error),(retained,variant,step,error.value)
                assert observed==(['movie'] if alt else [])+(['envelope'] if inp.voice_active else [])
                check(('step',retained,variant,step))
                face=lib.bk_selection_actor_assets_face_state(owner).contents;wanted=body.face_vm.state()
                for k,typ,_ in FIELDS:
                    if typ==C.c_float:equal([getattr(face,k)],[getattr(wanted,k)],(k,retained,variant,step))
                    else:assert getattr(face,k)==getattr(wanted,k),(k,retained,variant,step)
                assert rng.value==struct.unpack('<I',body.face_vm.u.mem_read(0x58edd8,4))[0]
                forest=lib.bk_selection_world_forest(world)
                for target in [2,3]:
                    visits=C.POINTER(Visit)();count=C.c_uint32()
                    assert lib.bk_actor_forest_draw(forest,lib.bk_selection_world_root(world,target),C.byref(visits),C.byref(count),error),error.value
                    pub.draw(target);check(('draw',retained,variant,step,target))
                    equal(lib.bk_actor_forest_view(forest)[:16],pub.floats(0x642fa8),'view')
                frames+=1
            print('PASS',retained,variant,flush=True)
        finally:lib.bk_selection_world_destroy(world)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),retained_groups=5,replacements=replacements,frames=frames,matrices=matrices,max_relative_error=worst,scope=__doc__)
    (ROOT/'local/original-selection-world-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
