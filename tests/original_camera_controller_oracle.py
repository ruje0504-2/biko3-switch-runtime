"""Isolate controller 0x4bdc12: actor anchor, smoothing and call ordering.

Resource traversal/animation, obstacle query and frame setters are boundary
mocks. This is a controller contract check, NOT gameplay camera integration.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_matrix_oracle import machine,ROOT

def f(x):return C.c_float(x).value
class Controller:
    def __init__(self,exe):
        self.uc=machine(exe);self.uc.mem_map(0x4000000,0x100000)
        self.uc.hook_add(UC_HOOK_CODE,self.hook);self.events=[]
    def words(self,a,n):return struct.unpack('<'+'I'*n,self.uc.mem_read(a,n*4))
    def floats(self,a,n):return struct.unpack('<'+'f'*n,self.uc.mem_read(a,n*4))
    def word(self,a,x):self.uc.mem_write(a,struct.pack('<I',x))
    def vector(self,a,x):self.uc.mem_write(a,struct.pack('<'+'f'*len(x),*x))
    def hook(self,uc,addr,size,user):
        if addr not in [0x42cf0e,0x422c49,0x4026fe,0x4bed86,0x4b61e5,0x425196,0x424020]:return
        sp=uc.reg_read(UC_X86_REG_ESP);ret=self.words(sp,1)[0];args=sp+4;value=0
        if addr==0x42cf0e:self.events.append(('fov',self.floats(args,1)[0]))
        elif addr==0x422c49:self.events.append(('position',self.words(args,2),self.floats(args+8,3)))
        elif addr==0x4026fe:
            self.events.append(('advance',self.floats(args+4,1)[0]))
            self.vector(0x4004000+0xf0,self.sample)
        elif addr==0x4bed86:value=int(self.obstacle)
        elif addr==0x4b61e5:value=1
        elif addr==0x425196:
            self.events.append(('aim',self.floats(self.words(args+4,1)[0],3)))
        elif addr==0x424020:self.events.append(('snapshot',))
        uc.reg_write(UC_X86_REG_EAX,value);uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
    def run(self,actor,head,previous,sample,correction,dt,obstacle,ended):
        self.events=[];self.sample=sample;self.obstacle=obstacle
        self.uc.mem_write(0x4000000,b'\0'*0x9000)
        for a,x in [(0x4000000,0x4001000),(0x4000008,0x4004000),(0x4001160,0x4002000),
                    (0x4002014,0x4003000),(0x4005008,0x4006000),(0x645600,0x4008000),
                    (0x645604,0x4007000),(0x7219a8,0),(0x7219ac,8),(0x726644,1),(0x726648,0x4009000)]:self.word(a,x)
        self.vector(0x4005000+0x29c,actor);self.vector(0x4006000+0xf0,head)
        self.vector(0x4000000+0x420,previous);self.vector(0x4000000+0x524,correction)
        self.vector(0x733700,[dt]);self.vector(0x4001000+0x1f0,[10 if ended else 9]);self.vector(0x4001000+0x1e8,[10])
        self.uc.mem_write(0x4005000+0x579,b'\1')
        stack=0x2008000;stop=0x300f000
        self.uc.mem_write(stack,struct.pack('<3I',stop,0x4000000,0x4005000))
        self.uc.reg_write(UC_X86_REG_ESP,stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(0x4bdc12,stop,count=100000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==stop
        result=self.floats(0x4000000+0x420,3)
        expected=[f(f(f(b-a)*dt)+a) for a,b in zip(previous,sample)]
        if obstacle:expected=[f(f(f(b-a)*(4*dt))+a) for a,b in zip(expected,correction)]
        for a,b in zip(result,expected):assert abs(a-b)<=2e-5*max(1,abs(b)),(a,b)
        assert self.events==[('fov',1.0),('position',(0x4003000,0x4008000),tuple(actor)),
                            ('advance',f(dt*.5)),('aim',tuple(head)),
                            ('position',(0x4007000,0x4008000),result),('snapshot',)],self.events
        assert self.uc.mem_read(0x4005000+0x579,1)==bytes([0 if ended else 1])
        return max(abs(a-b)/max(1,abs(b)) for a,b in zip(result,expected))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args();exe=args.exe.read_bytes()
    native=Controller(exe);rng=random.Random(303);worst=0;checks=0
    for _ in range(128):
        values=[[f(rng.uniform(-200,200)) for i in range(3)] for j in range(5)]
        dt=f(rng.uniform(.001,.22))
        for collision in [False,True]:
            for ended in [False,True]:
                worst=max(worst,native.run(*values,dt,collision,ended));checks+=1
    result={'passed':True,'checks':checks,'max_normalized_error':worst,'exe_sha256':hashlib.sha256(exe).hexdigest(),
            'native_controller':'0x4bdc12','actor_position_offsets':['0x29c','0x2a0','0x2a4'],
            'head_node':'actor+8, world translation +0xf0','smoothing':'previous + float((sample - previous) * seconds)',
            'obstacle_correction_factor':4,'ordering':['anchor animation root to actor','advance by half seconds','smooth toward track position','optional obstacle correction','aim using existing render-camera position','set smoothed camera position','snapshot'],
            'boundaries_mocked':['FOV setter','frame setters/aim/snapshot','animation advance and cached track world position','obstacle query/result'],
            'scope':'controller arithmetic and call ordering; no actual actor animation, obstacle geometry, active workflow dispatch or look-at execution'}
    (ROOT/'local/original-camera-controller-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
