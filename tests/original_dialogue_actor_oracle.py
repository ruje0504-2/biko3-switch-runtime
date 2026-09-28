"""Original4f1b53,4bd641 and complete4f11f2 with five actual hNN_70 bodies.

Original XAN, SRT, visibility, eyes and traversal execute in the caller VM.
A separate original face-controller VM emits commands for a reference MORP
instance. OS clocks/audio envelope/eye texture are explicit service inputs;
this does not claim dialogue audio, UI, GPU or complete flow8 integration.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct, re
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_actor_phase_oracle import Native as ActorNative
from original_selection_actor_oracle import bind as selection_bind
from original_face_controller_oracle import Native as FaceNative, State as FaceState, Commands, FIELDS
from original_prop_route_oracle import Native as PlainNative
from original_menu_camera_oracle import Native as CameraNative, State as CameraState, values as camera_values
from original_dialogue_oracle import State as Dialogue
from original_npc_point_oracle import Timer
from model_binding import ROOT, Model
from playback_binding import library
from clip_binding import State as ClipState
from bk3_assets import Archive

class Rules(C.Structure):
    _fields_=[(x,C.c_int32) for x in ['expression','gaze','texture','clip','once']]
class State(C.Structure):
    _fields_=[('speaker',C.c_int32),('rules',Rules),('mouth',C.c_float),('visibility',C.c_float)]
class Input(C.Structure):
    _fields_=[('seconds',C.c_float),('mouth',C.c_float),('now',C.c_uint32),('timer_now',C.c_uint32),('clocks',C.c_uint32*3),('camera',C.c_float*16)]

def bind(lib):
    selection_bind(lib)
    api=[('bk_dialogue_actor_rules',[C.POINTER(Rules),C.c_uint8,C.c_int32,C.c_int32],C.c_int),
         ('bk_dialogue_actor_initialize',[C.POINTER(State),C.c_uint],C.c_int),
         ('bk_menu_camera_dialogue',[C.POINTER(CameraState)],C.c_int),
         ('bk_dialogue_actor_assets_create',[C.c_void_p,C.c_uint,C.c_void_p,C.c_void_p,C.c_void_p],C.c_void_p),
         ('bk_dialogue_actor_assets_destroy',[C.c_void_p],None),
         ('bk_dialogue_actor_assets_step',[C.c_void_p,C.POINTER(State),C.POINTER(Dialogue),C.POINTER(C.c_uint8),C.POINTER(Timer),C.POINTER(Input),C.c_void_p,C.c_void_p],C.c_int),
         ('bk_dialogue_actor_assets_face_state',[C.c_void_p],C.POINTER(FaceState)),
         ('bk_actor_pose_loops',[C.c_void_p,C.c_uint,C.POINTER(C.c_int32)],C.c_int)]
    api += [('bk_dialogue_world_create',[C.c_void_p,C.c_uint,C.POINTER(CameraState),C.c_void_p,C.c_void_p,C.c_void_p],C.c_void_p),
            ('bk_dialogue_world_destroy',[C.c_void_p],None),
            ('bk_dialogue_world_body',[C.c_void_p],C.c_void_p),
            ('bk_dialogue_world_forest',[C.c_void_p],C.c_void_p),
            ('bk_dialogue_world_root',[C.c_void_p],C.c_uint32),
            ('bk_dialogue_world_step',[C.c_void_p,C.POINTER(State),C.POINTER(Dialogue),C.POINTER(C.c_uint8),C.POINTER(Timer),C.POINTER(Input),C.c_void_p,C.c_void_p],C.c_int),
            ('bk_actor_forest_draw',[C.c_void_p,C.c_uint32,C.c_void_p,C.c_void_p,C.c_void_p],C.c_int)]
    for name in ['pose','face','eyes']:
        api.append(('bk_dialogue_actor_assets_'+name,[C.c_void_p],C.c_void_p))
    api.append(('bk_dialogue_actor_assets_root',[C.c_void_p],C.c_uint32))
    for name,args,result in api:
        f=getattr(lib,name);f.argtypes=args;f.restype=result

class FixedCamera(CameraNative):
    def call(self,address,args):
        return super().call(0x4bd641 if address==0x4bac5b else address,args)

def policies(exe,lib):
    n=PlainNative(exe);rng=random.Random(0x4f1b53);count=0
    fs=list(range(-3,64))+[-2**31,2**31-1]
    ms=list(range(-2,19))+[-2**31,2**31-1]
    for phase in [0,1,2,3,4,5,10,11,127,128,255]:
        for f in fs:
            for m in ms:
                r=Rules(*(rng.randint(-1000,1000) for _ in range(5)))
                n.u.mem_write(0x734034,bytes(r));n.u.mem_write(0xbef798,bytes([phase]))
                n.u.mem_write(0xbefcbc,struct.pack('<iii',f,99,m))
                n.call(0x4f1b53,b'');assert lib.bk_dialogue_actor_rules(C.byref(r),phase,f,m)
                assert bytes(r)==bytes(n.u.mem_read(0x734034,20)),(phase,f,m)
                count+=1
    cam=FixedCamera(exe);worst=0
    for case in range(1200):
        s=CameraState();vals=[rng.uniform(-500,500) for _ in range(C.sizeof(s)//4)]
        C.memmove(C.byref(s),struct.pack('<'+'f'*len(vals),*vals),C.sizeof(s))
        # Valid original frame world, with unrelated held menu fields nonzero.
        s.pose.world[:]=[1,0,0,0,0,1,0,0,0,0,1,0,*vals[:3],1]
        wanted=cam.run(s,0,[0,0],0,0,[0,0,0],None,8)
        assert lib.bk_menu_camera_dialogue(C.byref(s))
        for got,want in zip(camera_values(s),camera_values(wanted)):
            error=abs(got-want);worst=max(worst,error);assert error==0,(case,got,want)
    return dict(rule_cases=count,camera_cases=1200,max_error=worst)

class Native(ActorNative):
    face_ptr,clock,timer_clock=0x20173000,0x300e000,0x300e100
    def __init__(self,exe):
        super().__init__(exe);self.face_vm=FaceNative(exe);self.active=False
        self.word(0x53f358,self.clock);self.word(0x53f10c,self.timer_clock)
        self.uc.mem_write(0x4af18f,b'\xd9\x05'+struct.pack('<I',0x300d000)+b'\xc3')
        for addr in [self.clock,self.timer_clock,0x4bd641,0x4af18f,0x4adbb9,0x401b0a,0x4f3bb9,0x4f3c49,0x411de1,0x410fd8,0x411985,0x4110ef,0x423a99,0x4026fe]:
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=addr,end=addr)
    def boundary(self,u,addr,size,_):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];skip=False
        if addr==self.clock:u.reg_write(UC_X86_REG_EAX,self.inp.now);skip=True
        elif addr==self.timer_clock:u.reg_write(UC_X86_REG_EAX,self.inp.timer_now);skip=True
        elif addr==0x4bd641:
            # Fixed camera has its independent full native math proof above.
            self.events.append('camera');self.vector(self.rendered+0xc0,self.inp.camera);skip=True
        elif addr==0x4af18f:self.events.append('envelope')
        elif addr==0x4adbb9:self.events.append('timer')
        elif addr==0x401b0a:self.events.append('request')
        elif addr==0x4f3bb9:self.events.append('gaze')
        elif addr==0x423a99:
            if struct.unpack('<I',u.mem_read(sp+4,4))[0]==self.frames+self.root*0x400:self.events.append('visibility')
        elif addr==0x4026fe:self.events.append('body')
        elif addr==0x4f3c49:
            self.texture=u.mem_read(sp+4,1)[0];self.events.append('texture');skip=True
        else:
            params={0x411de1:('range',self.inp.now,2),0x410fd8:('request_face',self.inp.clocks[0],1),0x411985:('mouth',self.inp.clocks[1],2),0x4110ef:('blink',self.inp.clocks[2],1)}[addr]
            assert struct.unpack('<I',u.mem_read(sp+4,4))[0]==self.face_ptr
            self.face_vm.now=params[1];self.events.append(params[0])
            args=struct.unpack('<'+'I'*params[2],u.mem_read(sp+8,params[2]*4))
            self.face_vm.call(addr,self.face_vm.face,*args);self.commands+=self.face_vm.commands;skip=True
        if skip:u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def frame(self,group,s,d,phase,timer,inp,present=True):
        self.events=[];self.commands=[];self.texture=None;self.inp=inp
        self.word(0x733e28,self.clip if present else 0);self.word(0x7219a8,group)
        self.uc.mem_write(0x734030,bytes(s));self.uc.mem_write(0xbef798,bytes([phase]))
        self.uc.mem_write(0xbefcb8,struct.pack('<4i',d.code_c,d.code_f,d.code_e,d.code_m))
        self.uc.mem_write(0xbf00f8,bytes(timer));self.vector(0x733700,[inp.seconds]);self.vector(0x300d000,[inp.mouth])
        self.active=True
        try:
            # Full actor sampling has the same large real track sets as the
            # selection caller. The generic math helper's20M limit is too
            # small for h02_70's initial blend; retain a bounded400M caller.
            self.uc.mem_write(self.stack,struct.pack('<I',self.stop))
            self.uc.reg_write(UC_X86_REG_ESP,self.stack)
            self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x4f11f2,self.stop,count=400000000)
            assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop,(hex(self.uc.reg_read(UC_X86_REG_EIP)),self.events)
        finally:self.active=False
        out=State.from_buffer_copy(self.uc.mem_read(0x734030,C.sizeof(State)))
        p=self.uc.mem_read(0xbef798,1)[0];m=struct.unpack('<i',self.uc.mem_read(0xbefcc4,4))[0]
        t=Timer.from_buffer_copy(self.uc.mem_read(0xbf00f8,C.sizeof(Timer)))
        expected=['camera']
        if present:
            if s.speaker==d.code_c and phase==0:expected+=['envelope']
            expected+=['timer']
            if out.rules.expression!=-1:
                if out.rules.once in [0,1]:expected+=['request']
                expected+=['gaze','texture','range','request_face','mouth','blink']
            expected+=['visibility','body']
        assert self.events==expected,self.events
        return out,p,m,t

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--world',action='store_true');args=ap.parse_args()
    exe=args.exe.read_bytes();assert hashlib.sha256(exe).hexdigest()=='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    dialogue_archive=Archive(args.data/'bk3_05.pp');eye_inventory=[];authored_e=set()
    for asset in dialogue_archive.entries:
        if not asset.name.endswith('.txt'):continue
        raw=dialogue_archive.read(asset);codes=re.findall(rb'#E(.{2})',raw)
        assert all(re.fullmatch(rb'[0-9]{2}',c) for c in codes),(asset.name,codes)
        values=[int(c) for c in codes];authored_e.update(values)
        eye_inventory.append(dict(name=asset.name,sha256=hashlib.sha256(raw).hexdigest(),count=len(values),values=sorted(set(values))))
    assert authored_e=={0,1,4,5,6,7,8,9},authored_e
    lib=library();bind(lib);policy=policies(exe,lib);print('PASS policies',policy,flush=True)
    n=Native(exe);err=C.create_string_buffer(256);store=lib.bk_resources_create(err);assert store
    assert lib.bk_resources_mount(store,b'bk3_01',str(args.data/'bk3_01.pp').encode(),err),err.value
    assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,err),err.value
    arc=Archive(args.data/'bk3_01.pp');files={e.name:e for e in arc.entries}
    records=[];steps=matrices=vertices=loops_checks=hidden_frames=resets=0;worst=0
    def equal(got,want,label):
        nonlocal worst
        for a,b in zip(got,want):
            delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta)
            assert math.isfinite(delta) and delta<3e-6,(label,a,b,delta)
    try:
        for group in range(5):
            clocks=(C.c_uint32*4)(1000,1001,1002,1003);seed=C.c_uint32(98765);ref_seed=C.c_uint32(seed.value)
            world=None
            if args.world:
                camera=CameraState()
                world=lib.bk_dialogue_world_create(store,group,C.byref(camera),clocks,C.byref(seed),err);assert world,err.value
                a=lib.bk_dialogue_world_body(world)
            else:
                a=lib.bk_dialogue_actor_assets_create(store,group,clocks,C.byref(seed),err);assert a,err.value
            ref=lib.bk_dialogue_actor_assets_create(store,group,clocks,C.byref(ref_seed),err);assert ref,err.value
            try:
                actor=lib.bk_dialogue_actor_assets_pose(a);model=lib.bk_actor_pose_model(actor).contents
                face=lib.bk_dialogue_actor_assets_face(a);ref_face=lib.bk_dialogue_actor_assets_face(ref);eyes=lib.bk_dialogue_actor_assets_eyes(a)
                name=f'h{group+1:02}_70';xan=arc.read(files[name+'.xan']);filename=xan[:256].split(b'\0')[0].decode();data=arc.read(files[filename])
                n.bind(data,model);root=lib.bk_dialogue_actor_assets_root(a);n.bind_clip(xan,root,0,True)
                position=[(0,0,55),(2,0,55),(0,0,55),(1,2,55),(0,0,55)][group];n.place_actor(position,0)
                n.word(n.model+0x180,n.face_ptr);n.face_vm.seed(lib.bk_dialogue_actor_assets_face_state(a).contents,seed.value)
                binding=lib.bk_eye_assets_binding(eyes).contents
                n.uc.mem_write(0xbf3b10,bytes(0x2c));n.word(0xbf3b38,binding.mode)
                for i,f in enumerate(binding.frames):n.word(0xbf3b28+4*i,0 if f==0xffffffff else n.frames+f*0x400)
                s=State();assert lib.bk_dialogue_actor_initialize(C.byref(s),group)
                d=Dialogue();d.code_e=9;timer=Timer(11,0,1);phase=C.c_uint8(0)
                if not world:camera=CameraState()
                assert lib.bk_menu_camera_dialogue(C.byref(camera))
                def compare(label):
                    nonlocal matrices,loops_checks
                    for f in range(model.frame_count):
                        for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                            equal(getattr(lib,fn)(actor,f)[:16],n.floats(n.frames+f*0x400+off,16),(name,label,f,fn));matrices+=1
                    st=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(st))
                    equal([getattr(st,k) for k,_ in ClipState._fields_],n.state(),(name,label,'clip'))
                    for slot in range(128):
                        v=C.c_int32();assert lib.bk_actor_pose_loops(actor,slot,C.byref(v))
                        assert v.value==struct.unpack('<i',n.uc.mem_read(n.clip+0x1f4+slot*156,4))[0];loops_checks+=1
                compare('created')
                for frame in range(288):
                    dt=C.c_float([0,1/60,.25,.5,2][frame%5]).value
                    now=(1100+frame*137)&0xffffffff if frame<240 else (0xfffffff0+(frame-240)*137)&0xffffffff
                    inp=Input(dt,frame%11*.9,now,(now+1)&0xffffffff,(C.c_uint32*3)(*[(now+i+2)&0xffffffff for i in range(3)]),camera.pose.world)
                    if frame%24==0:d.code_f=(frame//24%6)*10+frame//24%7;d.code_m=9+frame//24%7;phase.value=0
                    if frame%24==16:s.rules.expression=-1;phase.value=1
                    if frame%24==21:s.rules.expression=2;phase.value=5
                    d.code_c=group+1 if frame%3 else 0;d.code_e=[0,1,4,5,6,7,8,9,17,0,9,1][frame//24]
                    old_m=d.code_m
                    try: wanted,p,m,t=n.frame(group,s,d,phase.value,timer,inp)
                    except Exception:
                        print('Native frame stopped',name,frame,hex(n.uc.reg_read(UC_X86_REG_EIP)),n.events,flush=True)
                        raise
                    step=lib.bk_dialogue_world_step if world else lib.bk_dialogue_actor_assets_step
                    assert step(world or a,C.byref(s),C.byref(d),C.byref(phase),C.byref(timer),C.byref(inp),C.byref(seed),err),(name,frame,err.value)
                    assert bytes(s)==bytes(wanted),(name,frame,'state',list(bytes(s)),list(bytes(wanted)))
                    assert (phase.value,d.code_m)==(p,m);assert bytes(timer)==bytes(t)
                    resets+=old_m!=0 and m==0;hidden_frames+=s.visibility==0
                    compare(frame)
                    actual=lib.bk_dialogue_actor_assets_face_state(a).contents;wanted_face=n.face_vm.state()
                    for key,typ,_ in FIELDS:
                        if typ==C.c_float:equal([getattr(actual,key)],[getattr(wanted_face,key)],(name,frame,key))
                        else:assert getattr(actual,key)==getattr(wanted_face,key),(name,frame,key)
                    assert seed.value==struct.unpack('<I',n.face_vm.u.mem_read(0x58edd8,4))[0]
                    if s.rules.expression!=-1:assert n.texture==s.rules.texture
                    cmds=Commands();cmds.count=len(n.commands)
                    for i,cmd in enumerate(n.commands):
                        c=cmds.commands[i];c.group,c.index,c.blend,c.from_,c.to,c.weight=cmd
                    assert lib.bk_face_assets_apply(ref_face,C.byref(cmds),err),err.value
                    for i in range(model.submesh_count):
                        x=lib.bk_face_assets_mesh(face,i);y=lib.bk_face_assets_mesh(ref_face,i);assert bool(x)==bool(y)
                        if x:
                            count=model.submeshes[i].vertex_count
                            assert C.string_at(lib.bk_morph_mesh_vertices(x),count*60)==C.string_at(lib.bk_morph_mesh_vertices(y),count*60),(name,frame,i)
                            vertices+=count
                    if frame%4!=2:
                        n.publish()
                        if world:
                            walk=C.c_void_p();count=C.c_uint32()
                            assert lib.bk_actor_forest_draw(lib.bk_dialogue_world_forest(world),lib.bk_dialogue_world_root(world),C.byref(walk),C.byref(count),err),err.value
                        else:lib.bk_actor_pose_publish(actor)
                        compare((frame,'published'))
                    steps+=1
                records.append(dict(model=name,frames=model.frame_count,sha256=hashlib.sha256(data).hexdigest()))
                print('PASS',name,flush=True)
            finally:
                if world:lib.bk_dialogue_world_destroy(world)
                else:lib.bk_dialogue_actor_assets_destroy(a)
                lib.bk_dialogue_actor_assets_destroy(ref)
        # Missing-body branch still resolves rules but preserves every other shared scalar.
        for case in range(256):
            s=State(4,Rules(2,1,1,3,1),.75,.33);d=Dialogue();d.code_c=4;d.code_f=case-3;d.code_m=case-8;phase=C.c_uint8(case)
            timer=Timer(999,1234,1);inp=Input();inp.now=5000;inp.timer_now=9000
            wanted,p,m,t=n.frame(0,s,d,case,timer,inp,False)
            assert lib.bk_dialogue_actor_assets_step(None,C.byref(s),C.byref(d),C.byref(phase),C.byref(timer),C.byref(inp),C.byref(seed),err)
            assert bytes(s)==bytes(wanted) and bytes(timer)==bytes(t) and (phase.value,d.code_m)==(p,m)
    finally:lib.bk_resources_destroy(store)
    assert hidden_frames and resets
    report=dict(passed=True,world_adapter=args.world,eye_code_inventory=eye_inventory,unsupported_scope="Negative eye range can request negative MORP time; existing fail-closed behavior retained. All original #E values 0,1,4,5,6,7,8,9 covered, plus17 positive held-end fixture.",exe_sha256=hashlib.sha256(exe).hexdigest(),policy=policy,steps=steps,matrices=matrices,face_vertices=vertices,slot_loop_checks=loops_checks,hidden_frames=hidden_frames,one_shot_resets=resets,missing_body_cases=256,max_error=worst,records=records,scope=__doc__)
    (ROOT/('local/original-dialogue-world-oracle.json' if args.world else 'local/original-dialogue-actor-oracle.json')).write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,matrices,vertices,worst,flush=True)
if __name__=='__main__':main()
