"""Original 4fc36d composition, with separately emulated owned components.

Body visibility/XAN/SRT run in the caller VM. Face controller, static shadow
and recursive fade callees execute their original instructions in independent
VMs. Audio-envelope output and clocks are explicit service inputs. Native
face commands feed a separate C MORP instance (sampler independently covered
by original_morph_pose_oracle); no original GPU/audio backend is claimed.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_actor_phase_oracle import Native as ActorNative, bind as actor_bind
from original_face_controller_oracle import Native as FaceNative, State as FaceState, Commands, FIELDS
from original_face_assets_oracle import bind as face_bind
from original_npc_fade_oracle import Native as FadeNative, bind as material_bind
from original_npc_spatial_oracle import State
from original_entry_oracle import Request, Selection
from model_binding import ROOT, Model, Material, decode
from playback_binding import library
from clip_binding import State as ClipState
from bk3_assets import Archive

class Input(C.Structure):
    _fields_=[('seconds',C.c_float),('voice_level',C.c_float),('interface_mode',C.c_int8),('phase',C.c_int8),('timestamp_ms',C.c_uint32),('request_clock_ms',C.c_uint32),('mouth_clock_ms',C.c_uint32),('blink_clock_ms',C.c_uint32)]

def bind(lib):
    actor_bind(lib);face_bind(lib);material_bind(lib)
    for name,args,result in [
        ('bk_entry_assets_create',[C.c_void_p,C.POINTER(Request),C.c_void_p],C.c_void_p),
        ('bk_entry_assets_destroy',[C.c_void_p],None),
        ('bk_entry_assets_actor',[C.c_void_p],C.c_void_p),
        ('bk_entry_assets_face',[C.c_void_p],C.c_void_p),
        ('bk_entry_assets_selection',[C.c_void_p],C.POINTER(Selection)),
        ('bk_entry_assets_actor_material',[C.c_void_p,C.c_uint32],C.POINTER(Material)),
        ('bk_entry_assets_load_mesh_shadow',[C.c_void_p,C.c_void_p,C.c_void_p],C.c_int),
        ('bk_entry_assets_shadow',[C.c_void_p],C.c_void_p),
        ('bk_npc_shadow_pose',[C.c_void_p],C.c_void_p),
        ('bk_npc_shadow_publish',[C.c_void_p],None),
        ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model)),
        ('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
        ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],C.POINTER(C.c_float)),
        ('bk_entry_assets_step_npc_presentation',[C.c_void_p,C.POINTER(State),C.POINTER(FaceState),C.POINTER(C.c_uint32),C.POINTER(Input),C.c_void_p],C.c_int)]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result

class Native(ActorNative):
    auxiliary,aux_meta,aux_root,face_ptr,clock=0x20170000,0x20171000,0x20172000,0x20173000,0x300e000
    def __init__(self,exe):
        super().__init__(exe)
        self.face_vm=FaceNative(exe);self.fade_vm=FadeNative(exe);self.shadow_vm=ActorNative(exe)
        self.word(self.auxiliary+0x160,self.aux_meta);self.word(self.aux_meta+0x14,self.aux_root)
        self.word(0x53f358,self.clock)
        # Explicit audio service return: FLD real input level; RET.
        self.uc.mem_write(0x4af18f,b'\xd9\x05'+struct.pack('<I',0x300d000)+b'\xc3')
        for addr in [self.clock,0x423a99,0x410fd8,0x411985,0x4110ef,0x4026fe,0x4fc7a2,0x4af18f]:
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=addr,end=addr)
        self.active=False
    def boundary(self,u,address,size,user):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret,p=struct.unpack('<II',u.mem_read(sp,8));skip=False
        if address==self.clock:
            u.reg_write(UC_X86_REG_EAX,self.input.timestamp_ms);skip=True
        elif address==0x4af18f:self.events.append('voice')
        elif address==0x423a99 and p==self.aux_root:
            hidden=struct.unpack('<I',u.mem_read(sp+8,4))[0]
            n=self.shadow_vm;n.call(0x423a99,struct.pack('<II',n.frames+n.root*0x400,hidden));skip=True
        elif address in [0x410fd8,0x411985,0x4110ef]:
            assert p==self.face_ptr;n=self.face_vm
            stage={0x410fd8:('request',self.input.request_clock_ms,1),0x411985:('mouth',self.input.mouth_clock_ms,2),0x4110ef:('blink',self.input.blink_clock_ms,1)}[address]
            self.events.append(stage[0]);n.now=stage[1]
            args=struct.unpack('<'+'I'*stage[2],u.mem_read(sp+8,stage[2]*4))
            n.call(address,n.face,*args);self.commands+=n.commands;skip=True
        elif address==0x4026fe:
            self.events.append('shadow' if p==self.auxiliary else 'body')
            if p==self.auxiliary:
                n=self.shadow_vm;n.call(address,struct.pack('<I',n.clip)+bytes(u.mem_read(sp+8,4)));skip=True
        elif address==0x4fc7a2:
            self.events.append('fade');assert p==self.actor
            alpha=self.floats(self.actor+0x32c,1)[0];mode=u.mem_read(self.actor+0x331,1)[0]
            new=self.fade_vm.fade(alpha,mode,self.group_index,self.input.seconds)
            self.vector(self.actor+0x32c,[new]);skip=True
        if skip:u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def frame(self,state,inp,with_shadow):
        self.input=inp;self.commands=[];self.events=[]
        self.word(self.actor,self.clip);self.word(self.actor+4,self.auxiliary if with_shadow else 0)
        self.word(self.actor+0x10,state.ai.point.motion.action);self.word(self.actor+0x850,state.ai.point.motion.behavior)
        self.uc.mem_write(self.actor+0x328,bytes([state.ai.point.motion.hidden]));self.uc.mem_write(self.actor+0x331,bytes([state.ai.point.fade_out]))
        self.vector(self.actor+0x32c,[state.alpha]);self.vector(0x733700,[inp.seconds]);self.vector(0x300d000,[inp.voice_level])
        self.uc.mem_write(0xbeeb84,bytes([inp.interface_mode&255]));self.uc.mem_write(0x71ba88,bytes([inp.phase&255]))
        self.active=True
        try:
            self.uc.mem_write(self.stack,struct.pack('<II',self.stop,self.actor))
            self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x4fc36d,self.stop,count=400000000)
            assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        except Exception:
            print('native frame fault',hex(self.uc.reg_read(UC_X86_REG_EIP)),self.events,flush=True);raise
        finally:self.active=False
        assert self.events==['voice','body','request','mouth','blink']+(['shadow'] if with_shadow else [])+['fade'],self.events

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();lib=library();bind(lib);n=Native(exe);error=C.create_string_buffer(256)
    store=lib.bk_resources_create(error);assert store
    archive=Archive(args.data/'bk3_01.pp');files={e.name:e for e in archive.entries}
    for pack in ['bk3_01','bk3_04']:assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
    for pack in [b'routes',b'faces']:assert lib.bk_resources_mount_directory(store,pack,str(args.data).encode(),20480,error)
    shadow_data=archive.read(files['kage_01.x']);shadow_xan=archive.read(files['kage_01.xan'])
    ok,shadow_model,msg=decode(lib,shadow_data);assert ok,msg
    steps=matrices=materials=vertices=rejects=0;worst=0;records=[]
    def equal(actual,expected,label):
        nonlocal worst
        for a,b in zip(actual,expected):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,a,b,d)
    def compare_pose(pose,vm,model,label):
        nonlocal matrices
        for i in range(model.frame_count):
            for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                equal(getattr(lib,fn)(pose,i)[:16],vm.floats(vm.frames+i*0x400+off,16),(label,i,fn));matrices+=1
            hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(pose,i,C.byref(hidden))
            assert hidden.value==struct.unpack('<I',vm.uc.mem_read(vm.frames+i*0x400+0x70,4))[0],(label,i,'hidden')
        state=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(state))
        equal([getattr(state,key) for key,_ in ClipState._fields_],vm.state(),(label,'clock'))
    try:
        for group in range(5):
            # Real entry binds model/face/markers; areas0 and8 cover policy split.
            for area in [0,8]:
                request=Request(group,area,0,8);entry=lib.bk_entry_assets_create(store,C.byref(request),error);assert entry,error.value
                reference=lib.bk_entry_assets_create(store,C.byref(request),error);assert reference,error.value
                try:
                    actor=lib.bk_entry_assets_actor(entry);model=lib.bk_actor_pose_model(actor).contents
                    selected=lib.bk_entry_assets_selection(entry).contents;name=selected.actor_clip.decode();xan=archive.read(files[name]);data=archive.read(files[name[:-3]+'x'])
                    n.bind(data,model);head=n.find(['atama','kubiX','kubiX','atama','atama'][group]);assert head is not None
                    n.bind_clip(xan,head,1,False);placement=lib.bk_actor_pose_placement(actor).contents;n.place_actor(placement.position,placement.yaw_degrees)
                    marks=[n.find(x) for x in ['mark_02','mark_01','mark_00','mark_03']];assert None not in marks
                    for i,m in enumerate(marks):n.word(n.actor+0x860+i*4,n.frames+m*0x400)
                    n.fade_vm.create(model,n.root,marks);n.group_index=group;n.word(0x7219ac,area)
                    shadow=None
                    if area==0:
                        assert lib.bk_entry_assets_load_mesh_shadow(entry,store,error)
                        shadow=lib.bk_entry_assets_shadow(entry);sn=n.shadow_vm
                        sn.bind(shadow_data,shadow_model.contents);sn.bind_clip(shadow_xan,1,0,True);sn.word(sn.model+0x148,0)
                        sn.place_actor((0,0,0),0)
                    face=lib.bk_entry_assets_face(entry);ref_face=lib.bk_entry_assets_face(reference)
                    fs=FaceState();rs=FaceState();seed=C.c_uint32(123);ref_seed=C.c_uint32(123);clocks=(C.c_uint32*4)(1000,1001,1002,1003)
                    assert lib.bk_face_assets_initialize(face,C.byref(fs),clocks,C.byref(seed),error)
                    assert lib.bk_face_assets_initialize(ref_face,C.byref(rs),clocks,C.byref(ref_seed),error)
                    n.face_vm.seed(fs,seed.value);n.word(n.model+0x180,n.face_ptr)
                    state=State();state.alpha=1
                    for frame in range(36):
                        state.ai.point.motion.action=1 if frame%8<4 else 4;state.ai.point.motion.hidden=[0,0,1,255,0][frame%5]
                        state.ai.point.motion.behavior=frame%6;state.ai.point.fade_out=(frame//6)%3
                        now=(0xfffffff0+frame*137)&0xffffffff if frame>=24 else 1100+frame*137
                        inp=Input([0,1/60,.1,.5][frame%4],frame%10,2 if frame%3 else 0,frame%5,now,(now+1)&0xffffffff,(now+2)&0xffffffff,(now+3)&0xffffffff)
                        before=bytes(fs),bytes(state),seed.value
                        old=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(old));cached=lib.bk_actor_pose_head(actor)[:3]
                        bad=Input.from_buffer_copy(inp);bad.seconds=float('nan')
                        assert not lib.bk_entry_assets_step_npc_presentation(entry,C.byref(state),C.byref(fs),C.byref(seed),C.byref(bad),error)
                        assert before==(bytes(fs),bytes(state),seed.value);new=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(new)) and bytes(new)==bytes(old);rejects+=1
                        n.frame(state,inp,bool(shadow))
                        assert lib.bk_entry_assets_step_npc_presentation(entry,C.byref(state),C.byref(fs),C.byref(seed),C.byref(inp),error),error.value
                        assert lib.bk_actor_pose_head(actor)[:3]==cached
                        compare_pose(actor,n,model,(group,area,frame,'body'))
                        if shadow:compare_pose(lib.bk_npc_shadow_pose(shadow),n.shadow_vm,shadow_model.contents,(group,frame,'shadow'))
                        wanted=n.face_vm.state()
                        for key,typ,_ in FIELDS:
                            if typ==C.c_float:equal([getattr(fs,key)],[getattr(wanted,key)],('face',key))
                            else:assert getattr(fs,key)==getattr(wanted,key),(key,getattr(fs,key),getattr(wanted,key))
                        assert seed.value==struct.unpack('<I',n.face_vm.u.mem_read(0x58edd8,4))[0]
                        assert state.alpha==n.floats(n.actor+0x32c,1)[0]
                        commands=Commands();commands.count=len(n.commands)
                        for i,cmd in enumerate(n.commands):
                            c=commands.commands[i];c.group,c.index,c.blend,c.from_,c.to,c.weight=cmd
                        assert lib.bk_face_assets_apply(ref_face,C.byref(commands),error),error.value
                        for i in range(model.submesh_count):
                            actual=lib.bk_face_assets_mesh(face,i);expected=lib.bk_face_assets_mesh(ref_face,i)
                            assert bool(actual)==bool(expected)
                            if actual:
                                count=model.submeshes[i].vertex_count
                                assert C.string_at(lib.bk_morph_mesh_vertices(actual),count*60)==C.string_at(lib.bk_morph_mesh_vertices(expected),count*60),(group,frame,'face vertices')
                                vertices+=count
                        for i,wanted in enumerate(n.fade_vm.values()):
                            m=lib.bk_entry_assets_actor_material(entry,i).contents
                            assert C.string_at(C.addressof(m)+Material.diffuse.offset,68)==wanted,(group,frame,i,'material');materials+=1
                        if frame%4!=2:
                            n.publish();lib.bk_actor_pose_publish(actor)
                            compare_pose(actor,n,model,(group,area,frame,'published'))
                            if shadow:
                                n.shadow_vm.publish();lib.bk_npc_shadow_publish(shadow)
                                compare_pose(lib.bk_npc_shadow_pose(shadow),n.shadow_vm,shadow_model.contents,(group,frame,'shadow published'))
                        steps+=1
                    records.append(dict(group=group,area=area,model=name,steps=36,shadow=bool(shadow),sha256=hashlib.sha256(data).hexdigest()))
                    print('PASS combined NPC',group,area,name,flush=True)
                finally:lib.bk_entry_assets_destroy(entry);lib.bk_entry_assets_destroy(reference)
    finally:lib.bk_model_destroy(shadow_model);lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,matrices=matrices,materials=materials,face_vertices=vertices,rejected_inputs=rejects,max_normalized_error=worst,records=records,native_functions=['0x4fc36d','0x423a99','0x401b0a','0x4026fe','ANIM SRT','0x410fd8','0x411985','0x4110ef','0x4fc7a2','0x42273b'],boundaries=__doc__)
    (ROOT/'local/original-npc-frame-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,'frames;',matrices,'matrices;',materials,'materials;',vertices,'face vertices; error',worst)
if __name__=='__main__':main()
