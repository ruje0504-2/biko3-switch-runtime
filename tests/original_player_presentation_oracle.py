"""Whole4bfea9 over actual player XAN/SRT/visibility plus original materials,
4c155d events and optional kage_01 in separate native component VMs.
Only audio I/O is supplied. Unknown-event-direction policy remains explicit.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_actor_phase_oracle import Native as ActorNative,bind as actor_bind
from original_npc_fade_oracle import Native as MaterialNative,bind as material_bind
from original_player_events_oracle import Native as EventNative,State,Input as EventInput,Output
from original_entry_oracle import Request
from model_binding import ROOT,Model,Material,decode
from playback_binding import library
from clip_binding import State as ClipState
from bk3_assets import Archive
class Input(C.Structure):
    _fields_=[('seconds',C.c_float),('action',C.c_int32),('actions',C.c_int32*21),('camera_mode',C.c_int8),('interface_mode',C.c_int8),('interaction_mode',C.c_int8),('hidden',C.c_uint8),('surface',C.c_char_p),('voice_present',C.c_int),('voice_playing',C.c_int)]
Submit=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Output),C.c_void_p)
def bind(lib):
    actor_bind(lib);material_bind(lib)
    for name,args,result in [
      ('bk_resources_create',[C.c_void_p],C.c_void_p),
      ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
      ('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
      ('bk_resources_destroy',[C.c_void_p],None),
      ('bk_entry_assets_create',[C.c_void_p,C.POINTER(Request),C.c_void_p],C.c_void_p),('bk_entry_assets_destroy',[C.c_void_p],None),
      ('bk_entry_assets_player',[C.c_void_p],C.c_void_p),('bk_entry_assets_player_materials',[C.c_void_p],C.c_void_p),
      ('bk_entry_assets_load_player_shadow',[C.c_void_p,C.c_void_p,C.c_void_p],C.c_int),('bk_entry_assets_player_shadow',[C.c_void_p],C.c_void_p),
      ('bk_npc_shadow_pose',[C.c_void_p],C.c_void_p),('bk_npc_shadow_publish',[C.c_void_p],None),
      ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model)),('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
      ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],C.POINTER(C.c_float)),('bk_player_actions_initialize',[C.POINTER(C.c_int32)],C.c_int),
      ('bk_entry_assets_step_player_presentation',[C.c_void_p,C.POINTER(State),C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(Input),Submit,C.c_void_p,C.c_void_p],C.c_int)]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result
class Native(ActorNative):
    auxiliary,aux_meta,aux_root=0x20170000,0x20171000,0x20172000
    def __init__(self,exe):
        super().__init__(exe);self.material_vm=MaterialNative(exe);self.events_vm=EventNative(exe);self.shadow_vm=ActorNative(exe);self.active=False
        for addr in [0x4b0de5,0x423a99,0x4c155d,0x401b0a,0x4018c8,0x4026fe]:self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=addr,end=addr)
    def boundary(self,u,addr,size,user):
        if not self.active:return
        sp=u.reg_read(UC_X86_REG_ESP);ret,p=struct.unpack('<II',u.mem_read(sp,8));skip=False
        if addr==0x4b0de5:
            self.order.append('alpha');alpha=struct.unpack('<I',u.mem_read(sp+8,4))[0];m=self.material_vm;m.call(addr,m.frames[self.root],alpha);skip=True
        elif addr==0x423a99:
            if p==self.aux_root:
                self.order.append('shadow-hidden');n=self.shadow_vm;n.call(addr,struct.pack('<II',n.frames+n.root*0x400,self.input.hidden));skip=True
            elif p==self.frames+self.root*0x400:self.order.append('body-hidden')
        elif addr==0x4c155d:
            self.order.append('events');inp=self.input;event=EventInput(self.group_index,self.area,inp.action,inp.actions,self.state()[7],inp.surface,inp.voice_present,inp.voice_playing)
            self.expected,self.expected_steps,self.expected_shared=self.events_vm.step(self.event_state,self.steps,self.shared,event,1);skip=True
        elif addr in [0x401b0a,0x4018c8] and p==self.clip:self.order.append('request10' if addr==0x401b0a else 'request')
        elif addr==0x4026fe:
            self.order.append('shadow' if p==self.auxiliary else 'body')
            if p==self.auxiliary:
                n=self.shadow_vm;n.call(addr,struct.pack('<I',n.clip)+bytes(u.mem_read(sp+8,4)));skip=True
        if skip:u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def frame(self,inp,state,steps,shared,has_shadow):
        self.input=inp;self.event_state=state;self.steps=steps;self.shared=shared;self.order=[]
        self.word(self.actor,self.clip);self.word(self.actor+4,self.auxiliary if has_shadow else 0);self.word(self.actor+0x10,inp.action);self.uc.mem_write(self.actor+0x14,bytes(inp.actions));self.uc.mem_write(self.actor+0x328,bytes([inp.hidden]));self.uc.mem_write(self.actor+0x579,bytes(C.c_int8(inp.interaction_mode)))
        self.uc.mem_write(0x729780,bytes(C.c_int8(inp.camera_mode)));self.uc.mem_write(0xbeeb84,bytes(C.c_int8(inp.interface_mode)));self.vector(0x733700,[inp.seconds]);self.word(0x7219a8,self.group_index);self.word(0x7219ac,self.area)
        self.word(self.auxiliary+0x160,self.aux_meta);self.word(self.aux_meta+0x14,self.aux_root)
        self.active=True
        try:
            self.uc.mem_write(self.stack,struct.pack('<II',self.stop,self.actor));self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
            self.uc.emu_start(0x4bfea9,self.stop,count=400000000)
            assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        except Exception:
            print('native fault',hex(self.uc.reg_read(UC_X86_REG_EIP)),self.order,flush=True);raise
        finally:self.active=False
        expected=['alpha','body-hidden']+(['shadow-hidden'] if has_shadow else [])+['events','request10' if inp.action in [inp.actions[0],inp.actions[1],inp.actions[3]] else 'request','body']+(['shadow'] if has_shadow else [])
        # Internal scheduler can request additional clips; top-level prefix and
        # relative stage order remain checked separately from clip state.
        assert self.order==expected,(self.order,expected)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();lib=library();bind(lib);error=C.create_string_buffer(256);n=Native(exe)
    store=lib.bk_resources_create(error);assert store
    for pack in ['bk3_01','bk3_04']:assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error)
    for pack in [b'routes',b'faces']:assert lib.bk_resources_mount_directory(store,pack,str(args.data).encode(),20480,error)
    arc=Archive(args.data/'bk3_01.pp');files={e.name:e for e in arc.entries};data=arc.read(files['h00_80.x']);xan=arc.read(files['h00_80.xan']);sd=arc.read(files['kage_01.x']);sx=arc.read(files['kage_01.xan'])
    ok,sm,msg=decode(lib,sd);assert ok,msg
    frames=matrices=materials=commands=defaults=rejects=0;worst=0
    def equal(a,b,label):
        nonlocal worst
        for a,b in zip(a,b):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,a,b,d)
    def compare(pose,vm,model):
        nonlocal matrices
        for i in range(model.frame_count):
            for name,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                equal(getattr(lib,name)(pose,i)[:16],vm.floats(vm.frames+i*0x400+off,16),(frame,i,name));matrices+=1
            hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(pose,i,C.byref(hidden));assert hidden.value==struct.unpack('<I',vm.uc.mem_read(vm.frames+i*0x400+0x70,4))[0]
        st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),'clip')
    try:
        for group,area in [(0,0),(0,7),(1,8),(2,6),(3,5),(4,5)]:
            request=Request(group,area,0,8);entry=lib.bk_entry_assets_create(store,C.byref(request),error);assert entry,error.value
            try:
                actor=lib.bk_entry_assets_player(entry);model=lib.bk_actor_pose_model(actor).contents;mp=lib.bk_entry_assets_player_materials(entry);n.group_index,n.area=group,area
                n.bind(data,model);n.bind_clip(xan,n.find('qqq21_atama'),0,True);p=lib.bk_actor_pose_placement(actor).contents;n.place_actor(p.position,p.yaw_degrees);n.material_vm.create(model,n.root,[])
                shadow=None
                if group%2==0:
                    assert lib.bk_entry_assets_load_player_shadow(entry,store,error);shadow=lib.bk_entry_assets_player_shadow(entry)
                    sn=n.shadow_vm;sn.bind(sd,sm.contents);sn.bind_clip(sx,1,0,True);sn.word(sn.model+0x148,0);sn.place_actor((0,0,0),0)
                state=State();steps=(C.c_uint8*544)();shared=(C.c_uint8*426)();inp=Input();lib.bk_player_actions_initialize(inp.actions)
                captures=[];callback_errors=[]
                @Submit
                def submit(ctx,out,err):
                    try:
                        assert lib.bk_actor_pose_head(actor)[:3]==cached
                        st=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(st));assert bytes(st)==bytes(before)
                        captures.extend((out.contents.commands[i].file,out.contents.commands[i].loop) for i in range(out.contents.count));return 1
                    except Exception as ex:callback_errors.append(repr(ex));return 0
                for frame in range(144):
                    inp.action=inp.actions[[0,1,2,3,4,5,6,8,9,11,12,13,14,16,17,18,19,20][(frame//8)%18]]
                    inp.seconds=[0,.016,.1,.5,1][frame%5];inp.camera_mode=[0,1,1,-1][frame%4];inp.interface_mode=[0,0x40,2,-1][(frame//4)%4];inp.interaction_mode=[0,1,2,5,7,-1][frame%6];inp.hidden=[0,0,1,255,0][frame%5]
                    inp.surface=[b'NULL',b'miss',b'Mesh_KusattaALL@M01_03.X'][frame%3];inp.voice_present=frame%2;inp.voice_playing=frame%3==0
                    before=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(before));cached=lib.bk_actor_pose_head(actor)[:3];captures.clear()
                    bad=Input.from_buffer_copy(inp);bad.seconds=float('nan');assert not lib.bk_entry_assets_step_player_presentation(entry,C.byref(state),steps,544,shared,426,C.byref(bad),submit,None,error);rejects+=1
                    n.frame(inp,state,steps,bytes(shared)[:409],bool(shadow))
                    assert lib.bk_entry_assets_step_player_presentation(entry,C.byref(state),steps,544,shared,426,C.byref(inp),submit,None,error),(error.value,callback_errors)
                    assert captures==n.events_vm.commands,(group,area,frame,captures,n.events_vm.commands)
                    assert bytes(state)==bytes(n.expected) and bytes(steps)==n.expected_steps and bytes(shared)[:409]==n.expected_shared
                    assert lib.bk_actor_pose_head(actor)[:3]==cached;compare(actor,n,model)
                    if shadow:compare(lib.bk_npc_shadow_pose(shadow),n.shadow_vm,sm.contents)
                    for i,wanted in enumerate(n.material_vm.values()):
                        m=lib.bk_material_pose_material(mp,i).contents;assert C.string_at(C.addressof(m)+Material.diffuse.offset,68)==wanted,(group,frame,i,'material');materials+=1
                    if frame%4!=2:
                        lib.bk_actor_pose_publish(actor);n.publish();compare(actor,n,model)
                        if shadow:lib.bk_npc_shadow_publish(shadow);n.shadow_vm.publish();compare(lib.bk_npc_shadow_pose(shadow),n.shadow_vm,sm.contents)
                    frames+=1;commands+=len(captures)
                print('PASS player presentation',group,area,flush=True)
            finally:lib.bk_entry_assets_destroy(entry)
    finally:lib.bk_model_destroy(sm);lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,matrices=matrices,materials=materials,commands=commands,rejected_inputs=rejects,max_relative_error=worst,scope=__doc__)
    (ROOT/'local/original-player-presentation-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
