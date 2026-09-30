"""Compose complete51b647 with real Japanese body/face/camera native instances.

Original caller, XAN/SRT, gaze, face control, camera and world traversal execute
in independent x86 VMs. Only key/audio/video/clock services are explicit fixtures.
Native face commands drive a second MORP owner; initialization uses the already
verified face warm-up boundary. No PCM/video/GPU or full4e29b0/UI claim.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
import original_special_event_oracle as event
from original_special_camera_oracle import Body,Camera,Publication as BasePublication,Presets,Transitions,State as CameraState,values,I as IDENTITY,bind as camera_bind
from original_face_controller_oracle import Native as FaceNative,State as FaceState,Commands,FIELDS
from original_eye_assets_oracle import Native as EyeNative,bind as eye_bind
from original_actor_phase_oracle import Native as ActorNative
from original_frame_tree_oracle import Visit
from playback_binding import library
from model_binding import ROOT
from clip_binding import State as ClipState
from bk3_assets import Archive
F,U,P=event.F,event.U,event.P
FP,UP=C.POINTER(F),C.POINTER(U)
class Services(C.Structure):
    _fields_=[('context',P),('key',event.Key),('present',event.Present),('audio',event.Audio),
              ('movie',event.Movie),('clock',event.Clock),('level',event.Level)]

class Publication(BasePublication):
    def configure(self,vms,models,camera):
        super().configure(vms,models,camera)
        #4e29b0 loads the body before4b79e0 creates both tracks.42261a stops
        # at the selected global child, so this order matters at draw time.
        children=[1,self.roots[2],self.roots[0],self.roots[1]]
        self.word(self.frames+0x230,self.links+children[0]*16)
        self.word(self.frames+0x234,self.links+children[-1]*16)
        for i,node in enumerate(children):
            link=self.links+node*16
            self.word(link+4,self.links+children[i-1]*16 if i else 0)
            self.word(link+8,self.links+children[i+1]*16 if i+1<len(children) else 0)
    def sync_in(self):
        super().sync_in()
        for node,(vm,frame) in enumerate(self.bindings[2:],2):
            self.u.mem_write(self.frames+node*0x400+0x240,
                            bytes(vm.uc.mem_read(vm.frames+frame*0x400+0x240,4)))

def bind(lib):
    camera_bind(lib);eye_bind(lib)
    api=[('create',[P,U,F,C.POINTER(CameraState),C.POINTER(Transitions),UP,UP,event.BP,C.c_size_t,P],P),
         ('destroy',[P],None),('pose',[P,U],P),('forest',[P],P),('face',[P],P),('eyes',[P],P),
         ('face_state',[P],C.POINTER(FaceState)),('presets',[P],C.POINTER(Presets)),
         ('root',[P,U],U),('back',[P],U),('focus',[P],U),('needs_movie',[P],C.c_int),
         ('step',[P,C.POINTER(event.Bindings),FP,U,C.POINTER(Services),P],C.c_int)]
    for name,args,result in api:
        fn=getattr(lib,'bk_special_world_'+name);fn.argtypes=args;fn.restype=result
    for name,args in [('bk_actor_pose_draw_disabled',[P,U,UP]),('bk_actor_pose_hidden',[P,U,UP])]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=C.c_int

class Face(FaceNative):
    def hook(self,u,address,size,user):
        if address==self.clock_stub:self.now=self.owner.clock_value(0)
        super().hook(u,address,size,user)

class Caller(event.Native):
    def __init__(self,exe):
        super().__init__(exe);self.face=Face(exe);self.face.owner=self
    def clock_value(self,timer):
        value=self.f.clock();self.trace.append(('clock',timer));self.f.clock_index.value+=1
        return value
    def emit(self,ev):
        self.trace.append(ev);self.f.effect(ev,len(self.trace));self.sync(False);return True
    def timing_sync(self):
        for i,vm in enumerate([self.body,self.second]):
            slot=vm.state()[0];self.f.timings[i][:]=[vm.floats(vm.clip+0x190+slot*156+0x60,1)[0],
                vm.floats(vm.clip+0x190+slot*156+0x58,1)[0]]
    def hook(self,u,addr,size,ctx):
        if addr==0x4afe00:return # real shared709084 latch, actual timing proxy
        custom={0x4bb82e,0x4bc444,0x4bb0a4,0x4bb612,0x422c49,0x4241e3,
            0x534a34,0x401b0a,0x4026fe,0x4f3bb9,0x4f3c49,0x411de1,0x410fd8,0x411985,0x4110ef,0x423b01}
        if addr not in custom:return super().hook(u,addr,size,ctx)
        self.sync(True);sp=u.reg_read(UC_X86_REG_ESP);ret=self.words(sp,1)[0];a=self.words(sp+4,5);result=0
        if addr in [0x4bb82e,0x4bc444,0x4bb0a4,0x4bb612]:
            kind={0x4bb82e:0,0x4bc444:1,0x4bb0a4:2,0x4bb612:3}[addr]
            offset=self.f32(sp+12,3) if kind==1 else self.f32(sp+8,3) if kind==2 else (0,0,0)
            result=self.camera.run(kind,a[1] if kind in [0,1] else 0,offset,self.motion,self.buttons,self.f.seconds.value)
        elif addr in [0x422c49,0x4241e3]:
            vm=self.camera if addr==0x422c49 else self.second
            if addr==0x422c49:args=struct.pack('<II3f',vm.frames+vm.root*0x400,vm.context,*self.f32(sp+12,3))
            else:
                vm.vector(0x300b800,self.f32(a[2],3));args=struct.pack('<IIIf',vm.frames+vm.root*0x400,vm.context,0x300b800,self.f32(sp+16)[0])
            vm.call(addr,args)
        elif addr==0x534a34:
            self.face.call(addr);result=self.face.u.reg_read(UC_X86_REG_EAX)
        elif addr in [0x401b0a,0x4026fe]:self.body.call(addr,struct.pack('<II',self.body.clip,a[1]))
        elif addr==0x4f3bb9:
            self.body.vector(self.body.rendered+0xc0,self.camera.floats(self.camera.rendered+0xc0,16))
            self.body.call(addr,struct.pack('<III',a[0],a[1],a[2]))
        elif addr==0x4f3c49:self.eye.call(0x4a07a9,self.eye.texture,a[0]&255)
        elif addr in [0x411de1,0x410fd8,0x411985,0x4110ef]:
            count={0x411de1:2,0x410fd8:1,0x411985:2,0x4110ef:1}[addr]
            self.face.call(addr,self.face.face,*a[1:1+count]);self.commands+=self.face.commands
        elif addr==0x423b01:self.body.call(addr,struct.pack('<II',self.body.frames+self.back*0x400,a[1]))
        self.timing_sync();self.sync(False)
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def run_frame(self):
        self.trace=[];self.commands=[];self.timing_sync();self.sync(False)
        self.word(self.stack,self.stop);self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x51b647,self.stop,count=200000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True)

def portable_services(f):
    trace=[]
    def emit(ev):trace.append(ev);f.effect(ev,len(trace));return 1
    @event.Key
    def key(_,code,out,e):out[0]=f.keys[[0,0x5a,0x33450].index(code)];return emit(('key',code))
    @event.Present
    def present(_,obj,out,e):out[0]=f.present[obj];return 1
    @event.Audio
    def audio(_,ptr,e):
        c=ptr.contents;return emit(event.audio_event(c.operation,c.slot,event.string(c.pack),event.string(c.name),c.volume,c.flags,c.amount,c.source,c.listener))
    @event.Movie
    def movie(_,e):return emit(('movie',))
    @event.Clock
    def clock(_,timer,out,e):out[0]=f.clock();return emit(('clock',timer))
    @event.Level
    def level(_,effect,seconds,out,e):out[0]=f.level_value.value;return emit(('level',effect,event.bits(seconds)))
    services=Services(None,key,present,audio,movie,clock,level)
    bindings=event.Bindings(C.pointer(f.state),C.pointer(f.group),C.pointer(f.phase),C.pointer(f.camera_clip),C.pointer(f.camera_mode),
        C.pointer(f.seconds),C.pointer(f.paused),C.pointer(f.music_wanted),C.pointer(f.packed),C.pointer(f.visibility),
        C.pointer(f.effect_loop),C.pointer(f.effect_volume),f.listener,(FP*4)(*[C.cast(x,FP) for x in f.positions]),C.pointer(f.envelope))
    return services,bindings,trace

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path)
    ap.add_argument('--frames',type=int,default=180);ap.add_argument('--output',type=Path,default=ROOT/'local/original-special-world.json');args=ap.parse_args()
    if args.frames<1:ap.error('--frames must be positive')
    exe=args.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store
    packs={p:Archive(args.data/(p+'.pp')) for p in ['bk3_01','bk3_04','bk3_14']}
    for pack in packs:assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
    assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,error),error.value
    def raw(pack,name):return packs[pack].read(next(e for e in packs[pack].entries if e.name==name))
    frames=matrices=vertices=calls=rejections=0;worst=0;digest=hashlib.sha256();rows=[]
    def equal(got,want,label):
        nonlocal worst
        assert len(got)==len(want)
        for i,(x,y) in enumerate(zip(got,want)):
            delta=abs(x-y)/max(1,abs(y));worst=max(worst,delta)
            assert math.isfinite(delta) and delta<3e-6,(label,i,x,y,delta)
    try:
      for group in range(5):
        camera=CameraState();camera.pose.world[:]=IDENTITY;camera.pose.world[13]=20;camera.matrix[:]=IDENTITY
        clocks=Transitions();init=(U*4)(1000,1001,1002,1003);rng=U(98765);latches=(event.B*426)()
        world=lib.bk_special_world_create(store,group,F(.1),C.byref(camera),C.byref(clocks),init,C.byref(rng),latches,len(latches),error);assert world,(group,error.value)
        rcamera=CameraState();rcamera.pose.world[:]=IDENTITY;rcamera.pose.world[13]=20;rclocks=Transitions();rrng=U(98765);rlatches=(event.B*426)()
        ref=lib.bk_special_world_create(store,group,F(.1),C.byref(rcamera),C.byref(rclocks),init,C.byref(rrng),rlatches,len(rlatches),error);assert ref,error.value
        try:
            # Missing late camera pack follows face warm-up but must not
            # commit borrowed camera/RNG/transitions/latches to the caller.
            incomplete=lib.bk_resources_create(error);assert incomplete
            try:
                for pack in ['bk3_01','bk3_14']:
                    assert lib.bk_resources_mount(incomplete,pack.encode(),str(args.data/(pack+'.pp')).encode(),error)
                assert lib.bk_resources_mount_directory(incomplete,b'faces',str(args.data).encode(),20480,error)
                before=tuple(bytes(x) for x in [rcamera,rclocks,rrng,rlatches])
                assert not lib.bk_special_world_create(incomplete,group,F(.1),C.byref(rcamera),C.byref(rclocks),init,C.byref(rrng),rlatches,len(rlatches),error)
                assert before==tuple(bytes(x) for x in [rcamera,rclocks,rrng,rlatches]);rejections+=1
            finally:lib.bk_resources_destroy(incomplete)
            for bad_group,seconds,count in [(5,.1,426),(group,-1,426),(group,float('nan'),426),(group,.1,120)]:
                before=tuple(bytes(x) for x in [rcamera,rclocks,rrng,rlatches])
                assert not lib.bk_special_world_create(store,bad_group,F(seconds),C.byref(rcamera),C.byref(rclocks),init,C.byref(rrng),rlatches,count,error)
                assert before==tuple(bytes(x) for x in [rcamera,rclocks,rrng,rlatches]);rejections+=1
            poses=[lib.bk_special_world_pose(world,i) for i in range(3)];models=[lib.bk_actor_pose_model(p).contents for p in poses]
            forest=lib.bk_special_world_forest(world);face=lib.bk_special_world_face(world);reference=lib.bk_special_world_face(ref)
            body_name=f'h{group+1:02}_55.xan';xans=[raw('bk3_04','cam00_00.xan'),raw('bk3_04',f'cam{group+1:02}_50.xan'),raw('bk3_14',body_name)]
            vms=[Camera(exe),ActorNative(exe),Body(exe)];cam,second,body=vms
            names=['Cam_AUTO','cam',['A_kao','A_Kao','kubiX','mune','atama'][group]];nodes=[]
            for i,(vm,m) in enumerate(zip(vms,models)):
                vm.bind(C.string_at(m.source,m.source_size),m);node=vm.find(names[i]);assert node is not None;nodes.append(node);vm.bind_clip(xans[i],node)
            cam.connect();cam.set_state(camera);pub=Publication(exe);pub.configure(vms,models,cam);pub.refresh();second.call(0x4026fe,struct.pack('<If',second.clip,F(.1).value));cam.configure(second,body,pub,nodes[1],nodes[2])
            presets=lib.bk_special_world_presets(world).contents
            cam.vector(cam.controller+0x444,list(presets.active[0])+list(presets.active[1])+list(presets.active[2])+list(presets.active[3]))
            for address in [0x719c64,0x7099a8,0x7099e4]:cam.vector(address,[0])
            n=Caller(exe);n.camera,n.second,n.body=cam,second,body;n.back=body.find('Back');assert n.back==lib.bk_special_world_back(world)
            n.face.seed(lib.bk_special_world_face_state(world).contents,rng.value)
            n.eye=EyeNative(exe);n.eye.decode((args.data/f'h{group+1:02}_55.fam').read_bytes());target_index,target=n.eye.seed(models[2],xans[2][:256].split(b'\0')[0])
            binding=lib.bk_eye_assets_binding(lib.bk_special_world_eyes(world)).contents
            body.uc.mem_write(0xbf3b10,bytes(0x2c));body.word(0xbf3b38,binding.mode)
            for i,frame in enumerate(binding.frames):body.word(0xbf3b28+i*4,0 if frame==0xffffffff else body.frames+frame*0x400)
            f=event.fixture(random.Random(48),1);f.state=event.State();f.group.value=group;f.phase.value=0;f.camera_mode.value=f.camera_clip.value=0
            f.mutate_at=0;f.keys[:]=[0,0,0];f.clock_step.value=1;f.packed.value=1;f.effect_loop.value=0
            f.present[:]=[1,1,1,int(group in [1,4]),int(group!=2),0,0,0];f.envelope=event.Envelope();f.visibility.value=0
            n.f=f.clone();n.u.mem_write(0x709084,bytes(latches));services,bindings,trace=portable_services(f)
            for frame in range(args.frames):
                trace.clear()
                for field,value in [('seconds',F([1/60,.1,.25,1,0][frame%5]).value),('now',10000+frame*1000),
                    ('paused',int(frame%13==0)),('music_wanted',frame%2),('visibility',[0,1,255][frame%3]),('level_value',(frame%10)*.9)]:
                    getattr(f,field).value=value;getattr(n.f,field).value=value
                if f.phase.value==2:
                    for item in [f,n.f]:item.camera_mode.value=(frame//30)%3;item.camera_clip.value=(frame//7)%3
                motion=(F*2)(math.sin(frame*.13)*8,math.cos(frame*.1)*2);buttons=frame%4;n.motion=list(motion);n.buttons=buttons
                n.run_frame();assert lib.bk_special_world_step(world,C.byref(bindings),motion,buttons,C.byref(services),error),(group,frame,error.value)
                assert trace==n.trace,(group,frame,'services',trace,n.trace);calls+=len(trace)
                for field,_ in event.SCALARS:
                    assert bytes(getattr(f,field))==bytes(getattr(n.f,field)),(group,frame,field)
                assert bytes(f.state)==bytes(n.f.state),(group,frame,'process')
                assert bytes(latches)==bytes(n.u.mem_read(0x709084,len(latches))),(group,frame,'latches')
                equal(values(camera),values(cam.output_state()),(group,frame,'camera'))
                equal([clocks.preset_progress,clocks.zoom_progress,clocks.zoom_fov],[cam.floats(x,1)[0] for x in [0x719c64,0x7099a8,0x7099e4]],'transitions')
                got=lib.bk_special_world_face_state(world).contents;want=n.face.state()
                for key,typ,_ in FIELDS:
                    if typ==F:equal([getattr(got,key)],[getattr(want,key)],(group,frame,key))
                    else:assert getattr(got,key)==getattr(want,key),(group,frame,key,getattr(got,key),getattr(want,key))
                assert rng.value==struct.unpack('<I',n.face.u.mem_read(0x58edd8,4))[0]
                selected=lib.bk_eye_assets_selected(lib.bk_special_world_eyes(world))
                if target:assert n.eye.read(target+0x98)==n.eye.read(n.eye.texture+selected*4)
                commands=Commands();commands.count=len(n.commands)
                for i,cmd in enumerate(n.commands):
                    c=commands.commands[i];c.group,c.index,c.blend,c.from_,c.to,c.weight=cmd
                assert lib.bk_face_assets_apply(reference,C.byref(commands),error),error.value
                for mesh in range(models[2].submesh_count):
                    left=lib.bk_face_assets_mesh(face,mesh);right=lib.bk_face_assets_mesh(reference,mesh);assert bool(left)==bool(right)
                    if left:
                        count=models[2].submeshes[mesh].vertex_count
                        assert C.string_at(lib.bk_morph_mesh_vertices(left),count*60)==C.string_at(lib.bk_morph_mesh_vertices(right),count*60),(group,frame,mesh)
                        vertices+=count
                for stage in range(2):
                    for object,(pose,m,vm) in enumerate(zip(poses,models,vms)):
                        for j in range(m.frame_count):
                            for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                                equal(getattr(lib,fn)(pose,j)[:16],vm.floats(vm.frames+j*0x400+off,16),(group,frame,stage,object,j,fn));matrices+=1
                            for fn,off in [('bk_actor_pose_hidden',0x70),('bk_actor_pose_draw_disabled',0x240)]:
                                value=U();assert getattr(lib,fn)(pose,j,C.byref(value));assert value.value==struct.unpack('<I',vm.uc.mem_read(vm.frames+j*0x400+off,4))[0],(group,frame,j,fn)
                        state=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(state));equal([getattr(state,k) for k,_ in ClipState._fields_],vm.state(),'clips')
                    if stage==0:
                        visits=C.POINTER(Visit)();count=U();assert lib.bk_actor_forest_draw(forest,lib.bk_special_world_root(world,2),C.byref(visits),C.byref(count),error),error.value;pub.draw(2)
                digest.update(bytes(camera));digest.update(bytes(got));digest.update(bytes(f.state));digest.update(bytes(rng));frames+=1
            rows.append(dict(group=group,body=body_name,back=n.back,focus=names[2],phase=f.phase.value,sequence=f.state.sequence))
            print('PASS special world group',group,flush=True)
        finally:lib.bk_special_world_destroy(ref);lib.bk_special_world_destroy(world)
    finally:lib.bk_resources_destroy(store)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,matrices=matrices,
        face_vertices=vertices,service_calls=calls,construction_rejections=rejections,max_relative_error=worst,state_sha256=digest.hexdigest(),groups=rows,scope=__doc__)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS special world:',json.dumps(result,sort_keys=True),flush=True)
if __name__=='__main__':main()
