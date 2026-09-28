"""Real retail menu camera assets, original4bb612/4bac5b with live XAN/SRT
and423be2 global refresh in one VM for primary; second independent original
VM verifies per-group secondary selection and one loading advance. Geometry
and other menu actors are absent. Heap/stack GPU and input boundaries only.
"""
import argparse,ctypes as C,hashlib,json,math,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_track_forest_oracle import Native as ForestNative, Visit, Edit
from original_actor_phase_oracle import bind as bind_actor
from original_menu_camera_oracle import State, values
from original_aim_oracle import I
from playback_binding import library
from model_binding import ROOT,Model
from clip_binding import State as ClipState
from bk3_assets import Archive
class Native(ForestNative):
    def __init__(self,exe):
        self.fov=1;self.motion=[0,0];self.buttons=0
        super().__init__(exe)
        for a in [0x4b757e,0x4b76c2]:self.uc.hook_add(UC_HOOK_CODE,self.input,begin=a,end=a)
    def service(self,u,a,size,user):
        if a==0x42cf0e:
            sp=u.reg_read(UC_X86_REG_ESP);self.fov=struct.unpack('<f',u.mem_read(sp+4,4))[0]
        super().service(u,a,size,user)
    def input(self,u,a,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret,x,y,z=struct.unpack('<4I',u.mem_read(sp,16));v=0
        if a==0x4b757e:self.vector(x,self.motion[:1]);self.vector(y,self.motion[1:])
        else:
            assert y==2 and z==0 and x in [0,1];v=bool(self.buttons&(1<<x))
        u.reg_write(UC_X86_REG_EAX,int(v));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def set_state(self,s):
        self.vector(self.rendered+0x80,s.pose.world);self.vector(self.rendered+0xc0,s.pose.world)
        self.vector(self.controller+0x420,s.pose.position)
        self.vector(self.controller+0x42c,[s.yaw,s.pitch,s.radius,s.height]);self.vector(self.controller+0x4a4,s.matrix);self.vector(self.controller+0x5c8,s.focus)
        self.fov=s.fov;self.uc.mem_write(0xbeeb84,b'\x38')
    def output_state(self):
        s=State();s.pose.world[:]=self.floats(self.rendered+0xc0,16);s.pose.position[:]=self.floats(self.controller+0x420,3)
        s.yaw,s.pitch,s.radius,s.height=self.floats(self.controller+0x42c,4);s.matrix[:]=self.floats(self.controller+0x4a4,16);s.focus[:]=self.floats(self.controller+0x5c8,3);s.fov=self.fov
        return s

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();lib=library();bind_actor(lib);e=C.create_string_buffer(256);fp=C.POINTER(C.c_float);ip=C.POINTER(C.c_uint32)
    defs=[('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
    ('bk_menu_camera_assets_create',[C.c_void_p,C.c_uint,C.c_float,C.POINTER(State),C.c_void_p],C.c_void_p),('bk_menu_camera_assets_destroy',[C.c_void_p],None),
    ('bk_menu_camera_assets_pose',[C.c_void_p,C.c_uint],C.c_void_p),('bk_menu_camera_assets_root',[C.c_void_p,C.c_uint],C.c_uint32),('bk_menu_camera_assets_node',[C.c_void_p,C.c_uint],C.c_uint32),
    ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model)),('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],fp),('bk_actor_pose_visibility',[C.c_void_p,C.POINTER(Edit),C.c_size_t,C.c_void_p],C.c_int),
    ('bk_actor_forest_create',[C.POINTER(C.c_void_p),C.c_uint32,C.c_void_p],C.c_void_p),('bk_actor_forest_destroy',[C.c_void_p],None),('bk_actor_forest_node',[C.c_void_p,C.c_uint32,C.c_uint32],C.c_uint32),
    ('bk_actor_forest_attach',[C.c_void_p,C.c_uint32,C.c_uint32,C.c_void_p],C.c_int),('bk_actor_forest_anchor',[C.c_void_p,C.c_uint32,fp,C.c_uint32,C.c_void_p],C.c_int),
    ('bk_actor_forest_draw',[C.c_void_p,C.c_uint32,C.POINTER(C.POINTER(Visit)),ip,C.c_void_p],C.c_int),
    ('bk_menu_camera_assets_step',[C.c_void_p,C.c_void_p,ip,C.POINTER(State),C.c_uint,fp,C.c_uint,C.c_uint32,C.c_float,C.c_void_p],C.c_int)]
    for name,args,restype in defs:f=getattr(lib,name);f.argtypes=args;f.restype=restype
    store=lib.bk_resources_create(e);assert store,e.value
    assert lib.bk_resources_mount(store,b'bk3_04',str(a.data/'bk3_04.pp').encode(),e),e.value
    arc=Archive(a.data/'bk3_04.pp');worst=0.;matrices=frames=0
    def equal(got,want,label):
        nonlocal worst
        for i,(g,w) in enumerate(zip(got,want)):
            d=abs(g-w)/max(1,abs(w));worst=max(worst,d);assert math.isfinite(d) and d<3e-6,(label,i,g,w,d)
    def pose_equal(pose,model,vm,label):
        nonlocal matrices
        for i in range(model.frame_count):
            for fn,offset in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                equal(getattr(lib,fn)(pose,i)[:16],vm.floats(vm.frames+i*0x400+offset,16),(label,i,fn));matrices+=1
        st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st))
        equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),(label,'timeline'))
    try:
      for group in range(5):
        state=State();state.pose.world[:]=I;state.matrix[:]=I;state.pose.world[12]=9;state.matrix[12]=21;state.focus[:]=[7,4,-1]
        seconds=C.c_float([0,.016,.1,1,4][group]).value
        assets=lib.bk_menu_camera_assets_create(store,group,seconds,C.byref(state),e);assert assets,e.value;forest=None
        try:
            poses=[lib.bk_menu_camera_assets_pose(assets,i) for i in range(2)];models=[lib.bk_actor_pose_model(p).contents for p in poses]
            vms=[]
            for i in range(2):
                vm=Native(exe);model=models[i];raw=C.string_at(model.source,model.source_size)
                vm.bind(raw,model);name='cam00_00.xan' if i==0 else f'cam{group+1:02}_50.xan'
                xan=arc.read(next(t for t in arc.entries if t.name==name));vm.bind_clip(xan,lib.bk_menu_camera_assets_node(assets,i))
                if i:
                    vm.call(0x4026fe,struct.pack('<If',vm.clip,seconds))
                    # Native loader does not publish this new local until
                    # a subsequent attach/global refresh. Compare locals/time now.
                    for n in range(model.frame_count):equal(lib.bk_actor_pose_local(poses[i],n)[:16],vm.floats(vm.frames+n*0x400+0x80,16),'secondary local')
                    st=ClipState();assert lib.bk_actor_pose_state(poses[i],C.byref(st));equal([getattr(st,k) for k,_ in ClipState._fields_],vm.state(),'secondary clock')
                vms.append(vm)
            vm=vms[0];vm.connect();vm.set_state(state)
            forest=lib.bk_actor_forest_create((C.c_void_p*2)(*poses),2,e);assert forest,e.value
            assert lib.bk_actor_forest_anchor(forest,1,state.pose.world,0,e)
            roots=[]
            for i in range(2):
                root=lib.bk_actor_forest_node(forest,i,lib.bk_menu_camera_assets_root(assets,i));roots.append(root)
                assert lib.bk_actor_forest_attach(forest,0,root,e),e.value;vms[i].publish()
                pose_equal(poses[i],models[i],vms[i],('initial',i))
            for step in range(240):
                dt=C.c_float([0,.016,.033,.1,.25][step%5]).value;mode=(step//20)%2
                hidden=step%17==4
                edit=Edit(lib.bk_menu_camera_assets_root(assets,0),hidden)
                assert lib.bk_actor_pose_visibility(poses[0],C.byref(edit),1,e)
                vm.call(0x423a99,struct.pack('<2I',vm.frames+vm.root*0x400,hidden))
                motion=(C.c_float*2)(math.sin(step*.13)*12,math.cos(step*.07)*3);buttons=step%4
                vm.motion=list(motion);vm.buttons=buttons;vm.vector(0x733700,[dt])
                focus_node=lib.bk_menu_camera_assets_node(assets,0) if step%7==0 else None
                vm.word(vm.controller+0x10,vm.frames+focus_node*0x400 if focus_node is not None else 0)
                focus=lib.bk_actor_forest_node(forest,0,focus_node) if focus_node is not None else 0xffffffff
                vm.call(0x4bb612 if mode else 0x4bac5b,struct.pack('<I',vm.controller))
                assert lib.bk_menu_camera_assets_step(assets,forest,(C.c_uint32*2)(0,1),C.byref(state),mode,motion,buttons,focus,dt,e),(group,step,e.value)
                equal(values(state),values(vm.output_state()),(group,step,'camera'))
                pose_equal(poses[0],models[0],vm,(group,step,'before draw'))
                visits=C.POINTER(Visit)();count=C.c_uint32()
                assert lib.bk_actor_forest_draw(forest,0,C.byref(visits),C.byref(count),e),e.value
                vm.draw(vm.context);pose_equal(poses[0],models[0],vm,(group,step,'draw'))
                pose_equal(poses[1],models[1],vms[1],(group,step,'secondary held'))
                frames+=1
            print('group',group,'PASS',flush=True)
        finally:lib.bk_actor_forest_destroy(forest);lib.bk_menu_camera_assets_destroy(assets)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),groups=5,frames=frames,matrices=matrices,max_relative_error=worst,scope=__doc__)
    (ROOT/'local/original-menu-track-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
