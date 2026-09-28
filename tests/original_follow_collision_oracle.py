"""Full original4bdc12 with real static collision4b61e5, XAN/SRT and publication.
Only FOV and original collision construction allocation/lookup are boundaries.
Checks shortened distance and retained probe at the actual intra-frame point.
"""
import argparse,ctypes as C,hashlib,json,math,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_placed_pose_oracle import Native as PlacedNative,I
from original_aim_oracle import Pose
from playback_binding import library
from animation_binding import RootTransform
from clip_binding import State
from model_binding import ROOT,decode
from original_collision_oracle import Native as CollisionNative, bind as bind_collision
from original_follow_obstacle_oracle import State as Obstacle
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def bind_follow(lib):
    fp=C.POINTER(C.c_float)
    from model_binding import Model
    for name,args,result in [
        ('bk_follow_camera_create',[C.POINTER(Model),C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(Pose),C.c_void_p],C.c_void_p),
        ('bk_follow_camera_destroy',[C.c_void_p],None),
        ('bk_follow_camera_step',[C.c_void_p,fp,fp,fp,C.c_float,C.c_void_p],C.c_int),
        ('bk_follow_camera_publish',[C.c_void_p],None),
        ('bk_follow_camera_pose',[C.c_void_p],C.POINTER(Pose)),
        ('bk_follow_camera_track',[C.c_void_p],fp),
        ('bk_follow_camera_clip_state',[C.c_void_p,C.POINTER(State)],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result

class Native(PlacedNative):
    rendered=0x20150000;context=0x20151000;group=0x20152000;track_array=0x20153000
    clip=0x20160000;model=0x20165000;controller=0x20166000;actor=0x20167000;head=0x20168000
    def __init__(self,exe):
        super().__init__(exe)
        for address in [0x42cf0e]:
            self.uc.hook_add(UC_HOOK_CODE,self.service,begin=address,end=address)
    def service(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def bind_clip(self,xan,node,initial_slot=0,instant=True):
        self.node=self.frames+node*0x400
        self.uc.mem_write(self.rendered,bytes(0x19000))
        self.uc.mem_write(self.clip,xan[512:])
        self.word(self.clip+0x18c,0);self.word(self.clip+0x160,self.model)
        self.word(self.model+0x14,self.frames+self.root*0x400);self.word(self.model+0x148,self.group)
        self.vector(self.group+0x74,[0]);self.word(self.group+0x78,len(self.tracks));self.word(self.group+0x7c,self.track_array)
        for index,(obj,_,frame,_) in enumerate(self.tracks):
            self.word(self.track_array+index*4,obj);self.word(obj+0x74,self.frames+frame*0x400)
        for frame in [self.rendered,self.context]:
            for offset in [0x80,0xc0,0x100]:self.vector(frame+offset,I)
        initial=I.copy();initial[13]=20
        self.vector(self.rendered+0x80,initial);self.vector(self.rendered+0xc0,initial)
        self.word(self.controller,self.clip);self.word(self.controller+8,self.node)
        self.vector(self.controller+0x420,[0,20,0]);self.word(self.actor+8,self.head)
        self.word(0x645600,self.context);self.word(0x645604,self.rendered)
        self.word(0x7219a8,0);self.word(0x7219ac,8);self.word(0x726644,0)
        self.call(0x401d24 if instant else 0x401b0a,struct.pack('<2I',self.clip,initial_slot))
        self.publish()
    def floats(self,address,count):return list(struct.unpack('<'+'f'*count,self.uc.mem_read(address,count*4)))
    def publish(self):
        self.word(self.matrix_stack+12,0);self.vector(self.matrices,I)
        self.call(0x42273b,struct.pack('<I',self.frames+self.root*0x400))
    def step(self,origin,head,seconds):
        self.vector(self.actor+0x29c,origin);self.vector(self.head+0xf0,head);self.vector(0x733700,[seconds])
        cached=self.floats(self.node+0xc0,16)
        self.call(0x4bdc12,struct.pack('<2I',self.controller,self.actor))
        # The scheduler writes local animation matrices, but does NOT publish
        # the Cam_AUTO world pose while the controller is reading it.
        assert self.floats(self.node+0xc0,16)==cached
        rendered=self.floats(self.rendered+0xc0,16)
        assert rendered==self.floats(self.controller+0x4a4,16)
        return rendered+self.floats(self.controller+0x420,3)
    def state(self):
        b=self.uc.mem_read(self.clip,0x4f90)
        def i(off):return struct.unpack_from('<i',b,off)[0]
        def f(off):return struct.unpack_from('<f',b,off)[0]
        slot=i(0x140);c=0x190+slot*156
        return [slot,i(0x148),i(0x184),i(0x188),i(0x164),i(c+0x64),f(c+0x44),f(c+0x60),f(c+0x5c),f(0x168),f(0x16c),f(0x170)]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();arc=Archive(args.data/'bk3_04.pp')
    xan=arc.read(next(e for e in arc.entries if e.name=='cam00_02.xan'));data=arc.read(next(e for e in arc.entries if e.name=='cam00_02.x'))
    rc,model,message=decode(lib,data);assert rc==1,message
    err=C.create_string_buffer(256);clips=lib.bk_clip_set_decode(xan,len(xan),err);assert clips,err.value
    bind_follow(lib);camera=None
    lib.bk_follow_camera_step_distance.argtypes=lib.bk_follow_camera_step.argtypes[:-1]+[C.POINTER(C.c_float),C.c_void_p]
    fp=C.POINTER(C.c_float);lib.bk_camera_follow_pose.argtypes=[C.POINTER(Pose),fp,fp,fp,C.c_float];lib.bk_camera_follow_pose.restype=C.c_int
    bind_collision(lib)
    bg_arc=Archive(args.data/'bk3_03.pp');bg_data=bg_arc.read(next(e for e in bg_arc.entries if e.name=='m01_01.x'))
    ok,bg,message=decode(lib,bg_data);assert ok,message
    bg_world=(C.c_float*(bg.contents.frame_count*16))();assert lib.bk_model_world_matrices(bg,bg_world,len(bg_world),err)
    atr=(args.data/'m01_01.atr').read_bytes()
    collision=lib.bk_collision_create(bg,bg_world,len(bg_world),b'M01_01.X',atr,len(atr),err);assert collision,err.value
    constructor=CollisionNative(exe);constructor.create(bg.contents,bg_world,b'M01_01.X',atr)
    lib.bk_follow_camera_step_collision.argtypes=[C.c_void_p,fp,fp,C.c_void_p,C.c_int,C.c_float,C.POINTER(Obstacle),C.c_void_p]
    try:
        native.bind(data,model.contents);node=C.c_uint32();assert lib.bk_model_find_frame(model,b'Cam_AUTO',C.byref(node),err)
        native.bind_clip(xan,node.value)
        assert constructor.cursor<0x4800000
        native.uc.mem_write(0x4500000,bytes(constructor.u.mem_read(0x4500000,constructor.cursor-0x4500000)))
        native.uc.mem_write(0x726640,struct.pack('<3I',constructor.count,constructor.count,constructor.meshes))
        initial=I.copy();initial[13]=20;pose=Pose((C.c_float*16)(*initial),(C.c_float*3)(0,20,0));worst=0;checks=0;divergent=0
        camera=lib.bk_follow_camera_create(model,clips,native.root,node.value,C.byref(pose),err);assert camera,err.value
        obstacle=Obstacle();shortened=0
        def equal(actual,expected):
            nonlocal worst
            for j,(a,b) in enumerate(zip(actual,expected)):
                e=abs(a-b)/max(1,abs(b));worst=max(worst,e);assert e<=3e-5,(checks,j,a,b,e)
        for case in range(720):
            seconds=C.c_float([1/60,1/60,1/60,.1,.25,0][case%6]).value
            origin=(C.c_float*3)(13.271+math.sin(case*.03)*240,0,31.173+math.cos(case*.04)*480)
            head=(C.c_float*3)(origin[0]+math.sin(case*.05),20+math.sin(case*.07)*2,origin[2]+1)
            previous=list(lib.bk_follow_camera_track(camera)[:16]);before=bytes(lib.bk_follow_camera_pose(camera).contents)
            assert not lib.bk_follow_camera_step(camera,origin,head,None,-1,err)
            assert before==bytes(lib.bk_follow_camera_pose(camera).contents)
            old_obstacle=bytes(obstacle)
            assert not lib.bk_follow_camera_step_collision(camera,origin,head,collision,1,-1,C.byref(obstacle),err)
            assert bytes(obstacle)==old_obstacle
            assert lib.bk_follow_camera_step_collision(camera,origin,head,collision,1,seconds,C.byref(obstacle),err),err.value
            assert not obstacle.singular,case
            initial_distance=C.c_float(math.hypot(origin[0]-previous[12],origin[2]-previous[14])).value
            shortened+=obstacle.distance<initial_distance
            assert list(lib.bk_follow_camera_track(camera)[:16])==previous
            wanted=native.step(origin,head,seconds);current=lib.bk_follow_camera_pose(camera).contents
            equal(list(current.world)+list(current.position),wanted)
            equal([obstacle.distance],native.floats(native.controller+0x43c,1))
            equal(obstacle.point,native.floats(native.controller+0x524,3))
            state=State();assert lib.bk_follow_camera_clip_state(camera,C.byref(state));equal([getattr(state,n) for n,_ in State._fields_],native.state())
            # Consecutive local updates sometimes happen without publication.
            # Both original and runtime must keep using the old world pose.
            if case%11!=3:
                native.publish();lib.bk_follow_camera_publish(camera)
                fresh=list(lib.bk_follow_camera_track(camera)[:16]);equal(fresh,native.floats(native.node+0xc0,16))
                wrong=Pose.from_buffer_copy(before)
                assert lib.bk_camera_follow_pose(C.byref(wrong),(C.c_float*3)(*fresh[12:15]),head,None,seconds)
                if list(wrong.position)!=list(current.position):divergent+=1
            checks+=1
        assert divergent>500 and shortened>0,(divergent,shortened)
        report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=checks,shortened_distance_frames=shortened,premature_pose_publication_diverges=divergent,max_normalized_error=worst,
                    native_functions=['0x4bdc12','0x4026fe','0x4097d6','0x409a94','0x406c6b','0x408927','0x42273b','0x425196','0x422c49'],
                    hooks=['0x42cf0e FOV boundary','separate constructor allocation/lookup'],
                    distance_output='native controller+43c and+524 probe with live collision; no publication before reads',scope=__doc__)
        (ROOT/'local/original-follow-collision-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    finally:lib.bk_follow_camera_destroy(camera);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model);lib.bk_collision_destroy(collision);lib.bk_model_destroy(bg)
if __name__=='__main__':main()
