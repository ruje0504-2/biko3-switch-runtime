"""Both original player-camera handovers with unhooked Euler/matrix math.

Only FOV service is replaced. Completion tests run the actual native branches.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_aim_oracle import Pose,I
from model_binding import ROOT,library
class Player(C.Structure):_fields_=[('origin',C.c_float*3),('head',C.c_float*3),('base_head_height',C.c_float),('yaw_degrees',C.c_float)]
class Native:
    stack=0x2008000;stop=0x300f000;controller=0x3000000;player=0x3001000;head=0x3002000;frame=0x3003000
    def __init__(self,exe):
        self.uc=machine(exe);self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=0x42cf0e,end=0x42cf0e)
    def hook(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def word(self,a,v):self.uc.mem_write(a,struct.pack('<I',v))
    def vector(self,a,v):self.uc.mem_write(a,struct.pack('<'+'f'*len(v),*v))
    def call(self,address,args):
        self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args);self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(address,self.stop,count=200000);assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        return self.uc.reg_read(UC_X86_REG_EAX)
    def transition(self,pose,player,mode,seconds):
        self.uc.mem_write(self.controller,bytes(0x4000))
        self.vector(self.controller+0x4a4,pose.world);self.vector(self.controller+0x420,pose.position)
        self.vector(self.player+0x29c,player.origin);self.vector(self.player+0x298,[player.base_head_height]);self.vector(self.player+0x2ac,[player.yaw_degrees,0])
        self.word(self.player+8,self.head);self.vector(self.head+0xf0,player.head)
        self.word(0x645604,self.frame);self.vector(self.frame+0x100,I);self.vector(self.frame+0x80,pose.world);self.vector(self.frame+0xc0,pose.world);self.vector(0x733700,[seconds])
        complete=self.call(0x4be290 if mode else 0x4bdfb0,struct.pack('<2I',self.controller,self.player))&255
        world=list(struct.unpack('<16f',self.uc.mem_read(self.frame+0xc0,64)));assert world==list(struct.unpack('<16f',self.uc.mem_read(self.controller+0x4a4,64)))
        return world+list(struct.unpack('<3f',self.uc.mem_read(self.controller+0x420,12))),complete

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();native=Native(exe);lib=library();fp=C.POINTER(C.c_float)
    lib.bk_player_camera_transition.argtypes=[C.POINTER(Pose),C.POINTER(Player),C.c_int,C.c_float,C.POINTER(C.c_int)];lib.bk_player_camera_transition.restype=C.c_int
    lib.bk_camera_aim.argtypes=[fp,fp,fp];lib.bk_camera_aim.restype=C.c_int
    rng=random.Random(362);worst=0;checks=0;done=0
    def check(pose,player,mode,seconds):
        nonlocal worst,checks,done
        expected,complete=native.transition(pose,player,mode,seconds);actual=Pose.from_buffer_copy(pose);ended=C.c_int(-7)
        assert lib.bk_player_camera_transition(C.byref(actual),C.byref(player),mode,seconds,C.byref(ended))
        assert ended.value==complete,(mode,seconds,ended.value,complete)
        for j,(a,b) in enumerate(zip(list(actual.world)+list(actual.position),expected)):
            e=abs(a-b)/max(1,abs(b));worst=max(worst,e);assert e<3e-5,(checks,j,a,b,e)
        checks+=1;done+=complete
        return actual
    for case in range(2048):
        position=(C.c_float*3)(*[rng.uniform(-500,500) for _ in range(3)]);world=(C.c_float*16)(*I);world[12:15]=position
        target=(C.c_float*3)(*[rng.uniform(-500,500) for _ in range(3)]);assert lib.bk_camera_aim(world,world,target)
        pose=Pose(world,position);player=Player((C.c_float*3)(*[rng.uniform(-500,500) for _ in range(3)]),(C.c_float*3)(*[rng.uniform(-500,500) for _ in range(3)]),rng.uniform(10,30),rng.uniform(-360,360))
        seconds=C.c_float([0,1/120,1/60,.1,.249999,.25,.49999,.5,1][case%9]).value
        for mode in [0,1]:check(pose,player,mode,seconds)
    # Inclusive arrival bounds and repeated handover recurrence; arrival is
    # positional only, even while orientation is still visibly turning.
    for delta in [-.200001,-.2,-.199999,0,.199999,.2,.200001]:
        player=Player((C.c_float*3)(0,0,0),(C.c_float*3)(0,0,0),20,170)
        pose=Pose((C.c_float*16)(*I),(C.c_float*3)(0,0,0));pose.world[12]=delta
        check(pose,player,1,0)
        for _ in range(120):pose=check(pose,player,1,C.c_float(1/60).value)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=checks,arrivals=done,max_normalized_error=worst,native_functions=['0x4bdfb0','0x4be290','0x4ade8f','0x4adcd9','0x42ee68','original D3DX rotations/multiply','0x42407a'],hooks=['0x42cf0e FOV service'],scope='Both player-camera handovers, original matrix-angle interpolation and inclusive completion bounds. Explicit player input; no game branch selection/collision/player movement.')
    (ROOT/'local/original-player-camera-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
