"""Execute original mode-0 aim and controller pose arithmetic, with native
frame setters, normalization, matrix math and snapshot left unhooked.
Only resource/animation/obstacle/FOV boundaries are substituted.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_matrix_oracle import machine,ROOT
from model_binding import library
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def f(x):return C.c_float(x).value
class Pose(C.Structure):_fields_=[('world',C.c_float*16),('position',C.c_float*3)]
class Native:
    def __init__(self,exe):
        self.uc=machine(exe);self.uc.mem_map(0x4000000,0x100000)
        for addr in [0x42cf0e,0x4026fe,0x4bed86,0x4b61e5]:self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
    def word(self,a,x):self.uc.mem_write(a,struct.pack('<I',x))
    def vector(self,a,x):self.uc.mem_write(a,struct.pack('<'+'f'*len(x),*x))
    def floats(self,a,n):return struct.unpack('<'+'f'*n,self.uc.mem_read(a,n*4))
    def hook(self,u,addr,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        value=0
        if addr==0x4026fe:self.vector(0x4004000+0xf0,self.sample)
        elif addr==0x4bed86:value=self.obstacle
        elif addr==0x4b61e5:value=1
        u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def call(self,address,args):
        stack=0x2008000;stop=0x300f000
        self.uc.mem_write(stack,struct.pack('<I',stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(address,stop,count=500000);assert self.uc.reg_read(UC_X86_REG_EIP)==stop
    def aim(self,world,target):
        self.uc.mem_write(0x4000000,bytes(0x2000))
        self.vector(0x4000080,world);self.vector(0x40000c0,world);self.vector(0x4000100,I)
        self.vector(0x4001000,target)
        self.call(0x425196,struct.pack('<4I',0x4000000,0x4001000,0,0))
        return self.floats(0x40000c0,16)
    def follow(self,world,previous,sample,target,correction,dt):
        self.sample=sample;self.obstacle=int(correction is not None)
        self.uc.mem_write(0x4000000,bytes(0xa000))
        for a,x in [(0x4000000,0x4001000),(0x4000008,0x4004000),(0x4001160,0x4002000),
                    (0x4002014,0x4003000),(0x4005008,0x4006000),(0x645600,0x4008000),
                    (0x645604,0x4007000),(0x7219a8,0),(0x7219ac,8),(0x726644,1),(0x726648,0x4009000)]:self.word(a,x)
        for frame in [0x4003000,0x4007000,0x4008000]:
            for off in [0x80,0xc0,0x100]:self.vector(frame+off,I)
        self.vector(0x4007080,world);self.vector(0x40070c0,world)
        self.vector(0x4000000+0x420,previous)
        self.vector(0x4006000+0xf0,target)
        if correction:self.vector(0x4000000+0x524,correction)
        self.vector(0x733700,[dt]);self.vector(0x4001000+0x1f0,[9]);self.vector(0x4001000+0x1e8,[10])
        self.call(0x4bdc12,struct.pack('<2I',0x4000000,0x4005000))
        rendered=self.floats(0x40070c0,16);snapshot=self.floats(0x4000000+0x4a4,16)
        assert rendered==snapshot
        return rendered+self.floats(0x4000000+0x420,3)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args();exe=args.exe.read_bytes()
    native=Native(exe);lib=library();fp=C.POINTER(C.c_float)
    lib.bk_camera_aim.argtypes=[fp,fp,fp];lib.bk_camera_aim.restype=C.c_int
    lib.bk_camera_follow_pose.argtypes=[C.POINTER(Pose),fp,fp,fp,C.c_float];lib.bk_camera_follow_pose.restype=C.c_int
    rng=random.Random(3402);worst=0;aim_count=0;follow_count=0
    def equal(actual,expected):
        nonlocal worst
        for a,b in zip(actual,expected):
            error=abs(a-b)/max(1,abs(b));worst=max(worst,error)
            assert error<2e-6,(a,b,error)
    # Unit-band edges and short-vector normalization near the native threshold.
    special=[[v,0,z] for v in [0,1e-4,.1,1] for z in [1e-4,.999994,.999996,1,1.000004,1.000006,2]]
    for index in range(2048):
        world=I.copy();world[12:15]=[f(rng.uniform(-200,200)) for _ in range(3)]
        target=[f(rng.uniform(-200,200)) for _ in range(3)]
        if index<len(special):world[12:15]=[0,0,0];target=[f(v) for v in special[index]]
        out=(C.c_float*16)();target_c=(C.c_float*3)(*target)
        assert lib.bk_camera_aim(out,(C.c_float*16)(*world),target_c)
        equal(out,native.aim(world,target));aim_count+=1
        previous=[f(rng.uniform(-200,200)) for _ in range(3)]
        sample=[f(rng.uniform(-200,200)) for _ in range(3)]
        correction=[f(rng.uniform(-200,200)) for _ in range(3)] if index%2 else None
        dt=f([0,1/120,1/60,.1,.25,.5,1][index%7])
        pose=Pose((C.c_float*16)(*world),(C.c_float*3)(*previous))
        assert lib.bk_camera_follow_pose(C.byref(pose),(C.c_float*3)(*sample),target_c,(C.c_float*3)(*correction) if correction else None,dt)
        equal(list(pose.world)+list(pose.position),native.follow(world,previous,sample,target,correction,dt));follow_count+=1
    report={'passed':True,'exe_sha256':hashlib.sha256(exe).hexdigest(),'native_functions':['0x425196(mode 0)','0x522922','0x425aa0','0x4bdc12','0x422c49','0x424020'],'aim_checks':aim_count,'follow_pose_checks':follow_count,'max_normalized_error':worst,'scope':'Actual mode-0 world aim, controller smoothing, native position setter and matrix snapshot; frame parents use identity. Actor/model traversal, animation advance, obstacle query and FOV setter are boundary mocks. Gameplay dispatch is not covered.'}
    (ROOT/'local/original-aim-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
