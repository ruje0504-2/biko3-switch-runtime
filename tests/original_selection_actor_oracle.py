"""Native5073de/507193 and51ac5d body tail with actual ten menu bodies.

XAN/SRT, visibility and gaze run in the original caller VM; original face
controller runs in an independent VM and its commands drive a second MORP
instance. Camera mode2 holds the supplied camera; movie/envelope/eye-texture
selection are explicit observed services. Stage is absent here. No complete
selection scene, video playback or native GPU/audio claim.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_actor_phase_oracle import Native as ActorNative, bind as actor_bind
from original_face_controller_oracle import Native as FaceNative, State as FaceState, Commands, FIELDS
from original_eye_assets_oracle import bind as eye_bind
from original_prop_route_oracle import Native as PlainNative
from model_binding import ROOT, Model
from playback_binding import library
from clip_binding import State as ClipState
from bk3_assets import Archive

class Rules(C.Structure):
    _fields_=[('eye_max',C.c_float),('pitch_limit',C.c_float),('yaw_limit',C.c_float),('expression',C.c_int32),('gaze',C.c_int8),('texture',C.c_int8)]

def bind(lib):
    actor_bind(lib);eye_bind(lib)
    fp=C.POINTER(C.c_float);u32p=C.POINTER(C.c_uint32)
    api=[('bk_selection_actor_rules',[C.POINTER(Rules),C.c_uint,C.c_uint8,C.c_float],C.c_int),
         ('bk_selection_actor_variant',[C.POINTER(C.c_uint8),u32p,C.POINTER(C.c_uint8)],C.c_int),
         ('bk_selection_actor_assets_create',[C.c_void_p,C.c_uint,C.c_uint8,u32p,u32p,C.c_void_p],C.c_void_p),
         ('bk_selection_actor_assets_destroy',[C.c_void_p],None),
         ('bk_selection_actor_assets_step',[C.c_void_p,C.c_uint,C.c_float,C.c_float,fp,C.c_uint32,u32p,u32p,C.c_void_p],C.c_int),
         ('bk_selection_actor_assets_face_state',[C.c_void_p],C.POINTER(FaceState)),
         ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model)),
         ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],fp),
         ('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,u32p],C.c_int)]
    for name in ['pose','face','eyes','voice']:
        api.append(('bk_selection_actor_assets_'+name,[C.c_void_p],C.c_void_p))
    for name in ['root','focus','needs_movie']:
        api.append(('bk_selection_actor_assets_'+name,[C.c_void_p],C.c_uint32))
    for name,args,result in api:
        f=getattr(lib,name);f.argtypes=args;f.restype=result

class Native(ActorNative):
    face_ptr,clock=0x20173000,0x300e000
    def __init__(self,exe):
        super().__init__(exe);self.face_vm=FaceNative(exe);self.active=False
        self.word(0x53f358,self.clock)
        self.uc.mem_write(0x4af18f,b'\xd9\x05'+struct.pack('<I',0x300d000)+b'\xc3')
        for addr in [self.clock,0x4e6138,0x4af18f,0x4f3c49,0x411de1,0x410fd8,0x411985,0x4110ef,0x4026fe,0x4f3bb9]:
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=addr,end=addr)
    def boundary(self,u,addr,size,_):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];skip=False
        if addr==self.clock:u.reg_write(UC_X86_REG_EAX,self.now);skip=True
        elif addr==0x4e6138:self.events.append('movie');skip=True
        elif addr==0x4af18f:self.events.append('envelope')
        elif addr==0x4026fe:self.events.append('body')
        elif addr==0x4f3bb9:self.events.append('gaze')
        elif addr==0x4f3c49:
            self.texture=u.mem_read(sp+4,1)[0];self.events.append('texture');skip=True
        elif addr in [0x411de1,0x410fd8,0x411985,0x4110ef]:
            params={0x411de1:('range',self.now,2),0x410fd8:('request',self.clocks[0],1),0x411985:('mouth',self.clocks[1],2),0x4110ef:('blink',self.clocks[2],1)}[addr]
            assert struct.unpack('<I',u.mem_read(sp+4,4))[0]==self.face_ptr
            self.face_vm.now=params[1];self.events.append(params[0])
            args=struct.unpack('<'+'I'*params[2],u.mem_read(sp+8,params[2]*4))
            self.face_vm.call(addr,self.face_vm.face,*args);self.commands+=self.face_vm.commands;skip=True
        if skip:u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def frame(self,group,alternate,voice,dt,mouth,camera,now,clocks):
        self.events=[];self.commands=[];self.now=now;self.clocks=clocks;self.texture=None
        self.word(0xbfbba4,2);self.word(0xbef77c,self.clip);self.word(0xbef780,0);self.word(0xbf9b94,group)
        self.uc.mem_write(0xbfbb9d,bytes([voice]));self.uc.mem_write(0xbf4100,bytes([alternate]))
        self.vector(0x733700,[dt]);self.vector(0x300d000,[mouth]);self.vector(self.rendered+0xc0,camera)
        self.active=True
        try:
            self.uc.mem_write(self.stack,struct.pack('<I',self.stop))
            self.uc.reg_write(UC_X86_REG_ESP,self.stack)
            self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x51ac5d,self.stop,count=400000000)
            assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop,(hex(self.uc.reg_read(UC_X86_REG_EIP)),self.events)
        finally:self.active=False
        assert self.events==(['movie'] if alternate else [])+(['envelope'] if voice else [])+['body','gaze','texture','range','request','mouth','blink'],self.events


def policies(exe,lib):
    n=PlainNative(exe);rng=random.Random(0x507193);variants=rules=0
    def word(p,v):n.u.mem_write(p,struct.pack('<I',v))
    for group in range(5):
        for mask in range(256):
            for seed in [0,1,rng.randrange(2**32)]:
                unlock=bytes(rng.choice([1,2,128,255]) if mask>>i&1 else 0 for i in range(8))
                n.u.mem_write(0xb54738+group*8,unlock);word(0x58edd8,seed)
                n.call(0x5073de,struct.pack('<I',group))
                state=C.c_uint32(seed);alternate=C.c_uint8(99)
                assert lib.bk_selection_actor_variant((C.c_uint8*8).from_buffer_copy(unlock),C.byref(state),C.byref(alternate))
                assert alternate.value==n.u.mem_read(0xbf4100,1)[0]
                assert state.value==struct.unpack('<I',n.u.mem_read(0x58edd8,4))[0];variants+=1
    endpoints=[240,275,290,310,717,730,750,800]
    cases=[C.c_float(x+d).value for x in endpoints for d in [-.0001,0,.0001]]+[C.c_float(rng.uniform(-100,1000)).value for _ in range(1000)]
    for group in range(5):
        for alt in [0,1]:
            n.u.mem_write(0xbf4100,bytes([alt]))
            for source in cases:
                out=Rules();base=0x300a000
                n.u.mem_write(base,b'\xdd'*12)
                n.call(0x507193,struct.pack('<IfIII',group,source,base,base+4,base+8))
                eye,expr,gaze=struct.unpack('<fii',n.u.mem_read(base,12))
                assert lib.bk_selection_actor_rules(C.byref(out),group,alt,source)
                assert (out.eye_max,out.expression,out.gaze,out.texture)==(eye,expr,gaze,gaze);rules+=1
    for group,alt,source in [(5,0,0),(0,2,0),(0,0,float('nan'))]:
        assert not lib.bk_selection_actor_rules(C.byref(Rules()),group,alt,source)
    return dict(variants=variants,rules=rules,max_error=0)


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();assert hashlib.sha256(exe).hexdigest()=='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    lib=library();bind(lib);policy=policies(exe,lib);print('PASS policies',policy,flush=True)
    n=Native(exe);error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store
    for pack in ['bk3_01','bk3_06']:
        assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
    assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,error)
    ar=Archive(args.data/'bk3_01.pp');files={e.name:e for e in ar.entries}
    records=[];steps=matrices=vertices=0;worst=0
    def equal(actual,wanted,label):
        nonlocal worst
        for x,y in zip(actual,wanted):
            diff=abs(x-y)/max(1,abs(y));worst=max(worst,diff)
            assert math.isfinite(diff) and diff<3e-6,(label,x,y,diff)
    try:
        for group in range(5):
            for alt in [0,1]:
                clocks=(C.c_uint32*4)(1000,1001,1002,1003);seed=C.c_uint32(98765);ref_seed=C.c_uint32(seed.value)
                a=lib.bk_selection_actor_assets_create(store,group,alt,clocks,C.byref(seed),error);assert a,(group,alt,error.value)
                ref=lib.bk_selection_actor_assets_create(store,group,alt,clocks,C.byref(ref_seed),error);assert ref,error.value
                try:
                    actor=lib.bk_selection_actor_assets_pose(a);model=lib.bk_actor_pose_model(actor).contents
                    face=lib.bk_selection_actor_assets_face(a);ref_face=lib.bk_selection_actor_assets_face(ref);eyes=lib.bk_selection_actor_assets_eyes(a)
                    name=f'h{group+1:02d}_{61 if alt else 60}'
                    xan=ar.read(files[name+'.xan']);filename=xan[:256].split(b'\0')[0].decode();data=ar.read(files[filename])
                    n.bind(data,model);n.bind_clip(xan,lib.bk_selection_actor_assets_root(a),0,True)
                    n.word(n.model+0x180,n.face_ptr);n.face_vm.seed(lib.bk_selection_actor_assets_face_state(a).contents,seed.value)
                    binding=lib.bk_eye_assets_binding(eyes).contents
                    n.uc.mem_write(0xbf3b10,bytes(0x2c));n.word(0xbf3b38,binding.mode)
                    for i,f in enumerate(binding.frames):n.word(0xbf3b28+4*i,0 if f==0xffffffff else n.frames+f*0x400)
                    # Verify lookup through the native frame search, not a filename guess.
                    table=0x588750 if alt else 0x588250
                    focus_name=bytes(n.uc.mem_read(table+group*256,256)).split(b'\0')[0].decode()
                    focus=lib.bk_selection_actor_assets_focus(a)
                    assert focus==(n.find(focus_name) if n.find(focus_name) is not None else 0xffffffff),(name,focus_name,focus)
                    def compare(label):
                        nonlocal matrices
                        for f in range(model.frame_count):
                            for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                                equal(getattr(lib,fn)(actor,f)[:16],n.floats(n.frames+f*0x400+off,16),(name,label,f,fn));matrices+=1
                        state=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(state))
                        equal([getattr(state,k) for k,_ in ClipState._fields_],n.state(),(name,label,'clip'))
                    compare('created')
                    for frame in range(96):
                        dt=C.c_float([0,1/60,.1,.25,.5,2][frame%6]).value
                        now=(1100+frame*137)&0xffffffff if frame<72 else (0xfffffff0+(frame-72)*137)&0xffffffff
                        clocks3=(C.c_uint32*3)(*[(now+i+1)&0xffffffff for i in range(3)])
                        camera=(C.c_float*16)(1,0,0,0,0,1,0,0,0,0,1,0,-40+frame,35+frame%9,60,1)
                        voice=frame%3!=0;mouth=C.c_float(frame%11*.9).value
                        n.frame(group,alt,voice,dt,mouth,list(camera),now,list(clocks3))
                        assert lib.bk_selection_actor_assets_step(a,group,dt,mouth if voice else 0,camera,now,clocks3,C.byref(seed),error),(name,frame,error.value)
                        compare(frame)
                        actual=lib.bk_selection_actor_assets_face_state(a).contents;wanted=n.face_vm.state()
                        for key,typ,_ in FIELDS:
                            if typ==C.c_float:equal([getattr(actual,key)],[getattr(wanted,key)],(name,frame,key))
                            else:assert getattr(actual,key)==getattr(wanted,key),(name,frame,key,getattr(actual,key),getattr(wanted,key))
                        assert seed.value==struct.unpack('<I',n.face_vm.u.mem_read(0x58edd8,4))[0]
                        assert n.texture==alt
                        commands=Commands();commands.count=len(n.commands)
                        for i,cmd in enumerate(n.commands):
                            c=commands.commands[i];c.group,c.index,c.blend,c.from_,c.to,c.weight=cmd
                        assert lib.bk_face_assets_apply(ref_face,C.byref(commands),error),error.value
                        for i in range(model.submesh_count):
                            x=lib.bk_face_assets_mesh(face,i);y=lib.bk_face_assets_mesh(ref_face,i);assert bool(x)==bool(y)
                            if x:
                                count=model.submeshes[i].vertex_count
                                assert C.string_at(lib.bk_morph_mesh_vertices(x),count*60)==C.string_at(lib.bk_morph_mesh_vertices(y),count*60),(name,frame,i)
                                vertices+=count
                        if frame%4!=2:n.publish();lib.bk_actor_pose_publish(actor);compare((frame,'published'))
                        steps+=1
                    records.append(dict(model=name,alternate=alt,focus=focus_name,frames=model.frame_count,sha256=hashlib.sha256(data).hexdigest()))
                    print('PASS',name,flush=True)
                finally:lib.bk_selection_actor_assets_destroy(a);lib.bk_selection_actor_assets_destroy(ref)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),policy=policy,steps=steps,matrices=matrices,face_vertices=vertices,max_error=worst,records=records,scope=__doc__)
    (ROOT/'local/original-selection-actor-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,matrices,vertices,worst)
if __name__=='__main__':main()
