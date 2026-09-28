"""Original horizontal radius and point/segment predicates; no math hooks."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Native:
    stack=0x2008000;stop=0x300f000
    def __init__(self,exe):self.u=machine(exe)
    def run(self,start,end,point,radius):
        values=[*start,*end,0,*point,0,radius]
        self.u.mem_write(self.stack,struct.pack('<I12f',self.stop,*values));self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x4ae783,self.stop,count=100000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
        segment=self.u.reg_read(UC_X86_REG_EAX)&255
        self.u.mem_write(self.stack,struct.pack('<I5f',self.stop,start[0],start[2],point[0],point[2],radius));self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x4ae641,self.stop,count=100000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
        return segment,self.u.reg_read(UC_X86_REG_EAX)&255

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();fp=C.POINTER(C.c_float);rng=random.Random(0x4ae783)
    lib.bk_proximity_xz.argtypes=[C.POINTER(C.c_int),fp,fp,C.c_float];lib.bk_proximity_xz.restype=C.c_int
    lib.bk_proximity_segment_xz.argtypes=[C.POINTER(C.c_int),fp,fp,fp,C.c_float];lib.bk_proximity_segment_xz.restype=C.c_int
    count=inside=0
    def check(a,b,p,r):
        nonlocal count,inside
        a,b,p=[(C.c_float*3)(*v) for v in [a,b,p]];r=C.c_float(r).value
        segment,radial=native.run(a,b,p,r);hit=C.c_int(-1)
        assert lib.bk_proximity_segment_xz(C.byref(hit),a,b,p,r) and hit.value==segment,(list(a),list(b),list(p),r,hit.value,segment)
        assert lib.bk_proximity_xz(C.byref(hit),a,p,r) and hit.value==radial
        count+=1;inside+=segment
    for case in range(14000):
        scale=rng.choice([.001,1,100,10000]);a=[C.c_float(rng.uniform(-scale,scale)).value for _ in range(3)];b=[C.c_float(rng.uniform(-scale,scale)).value for _ in range(3)];p=[C.c_float(rng.uniform(-scale,scale)).value for _ in range(3)]
        if case%11==0:b[0],b[2]=a[0],a[2]
        if case%7==0:p[0],p[2]=a[0],a[2]
        if case%5==0:r=math.hypot(p[0]-a[0],p[2]-a[2])
        elif case%3==0:r=math.hypot(p[0]-b[0],p[2]-b[2])
        else:r=rng.uniform(0,2*scale)
        check(a,b,p,r)
    for x in [-15.000001,-15,-14.999999,0,5,10,24.999999,25,25.000001]:
        for z in [-15.000001,-15,-14.999999,0,14.999999,15,15.000001]:
            for radius in [0,1,15]:check([0,999,0],[10,-999,0],[x,100000,z],radius)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=count,predicate_comparisons=count*2,segment_hits=inside,native_functions=['0x4ae641','0x4ae783'],hooks=[],scope='XZ radius and finite-segment proximity, inclusive boundaries, zero-length rejection and height-independent geometry. Caller vertical/gameplay gates are separate.',x87_control_word='0x037f')
    (ROOT/'local/original-proximity-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',count*2,'proximity predicates,',inside,'segment hits')
if __name__=='__main__':main()
