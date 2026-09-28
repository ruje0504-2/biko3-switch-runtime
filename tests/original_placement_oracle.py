"""Original CKP spawn arithmetic and root setter; no math hooks.

Input route cursor/height are explicit. This is not a game/save initializer.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct,sys
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class Placement(C.Structure):
    _fields_=[('position',C.c_float*3),('yaw_degrees',C.c_float),('world',C.c_float*16)]
def bind(lib):
    fp=C.POINTER(C.c_float)
    for name,args,result in [
        ('bk_route_heading',[fp,C.c_float,C.c_float,C.c_float,C.c_float],C.c_int),
        ('bk_actor_placement',[C.POINTER(Placement),fp,C.c_float],C.c_int),
        ('bk_route_placement',[C.POINTER(Placement),C.c_void_p,C.c_uint32,C.c_float],C.c_int),
        ('bk_route_decode',[C.c_void_p,C.c_size_t,C.c_void_p],C.c_void_p),
        ('bk_route_count',[C.c_void_p],C.c_uint32),('bk_route_destroy',[C.c_void_p],None)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
class Native:
    stack=0x2008000;stop=0x300f000;frame=0x3003000;actor=0x3000000;model=0x3002000;clip=0x3001000
    def __init__(self,exe):
        self.uc=machine(exe)
        # Return consumer for a cdecl float return: fstp dword [result]. The
        # heading function itself, CRT inverse trig and x87 FSINCOS are native.
        self.uc.mem_write(self.stop,b'\xd9\x1d'+struct.pack('<I',0x300e000))
    def word(self,a,v):self.uc.mem_write(a,struct.pack('<I',v))
    def vector(self,a,v):self.uc.mem_write(a,struct.pack('<'+'f'*len(v),*v))
    def floats(self,a,n):return list(struct.unpack('<'+'f'*n,self.uc.mem_read(a,n*4)))
    def run(self,start,end):
        self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(start,end,count=500000);assert self.uc.reg_read(UC_X86_REG_EIP)==end
    def heading(self,values):
        self.uc.mem_write(self.stack,struct.pack('<I4f',self.stop,*values));self.uc.reg_write(UC_X86_REG_ESP,self.stack)
        self.run(0x4aeba9,self.stop+6);return self.floats(0x300e000,1)[0]
    def root(self,position,yaw):
        self.vector(self.actor+0x29c,position+[0,yaw]);self.vector(self.stack-0x3824,[0,1,0])
        self.word(self.actor,self.clip);self.word(self.clip+0x160,self.model);self.word(self.model+0x14,self.frame)
        for offset in [0x80,0xc0,0x100]:self.vector(self.frame+offset,I)
        self.word(self.stack+8,self.actor);self.uc.reg_write(UC_X86_REG_EBP,self.stack);self.uc.reg_write(UC_X86_REG_ESP,self.stack-0x4000)
        self.run(0x4fbd9d,0x4fbe4a)
        result=self.floats(self.actor+0x2d8,16);assert result==self.floats(self.frame+0xc0,16)
        return result
    def spawn(self,data,index,height):
        self.uc.mem_write(0xbe9a18,data+bytes(20480-len(data)))
        self.word(self.actor+0x830,index);self.vector(0x58321c,[height]);self.uc.mem_write(self.stack+8,struct.pack('<3I',self.actor,0,0))
        self.uc.reg_write(UC_X86_REG_EBP,self.stack);self.uc.reg_write(UC_X86_REG_ESP,self.stack-0x4000)
        self.run(0x4fb93e,0x4fba03)
        position=self.floats(self.actor+0x29c,3);yaw=self.floats(self.actor+0x2ac,1)[0]
        return position+[yaw]+self.root(position,yaw)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();bind(lib);rng=random.Random(361);worst=0;checks=0
    def equal(actual,expected):
        nonlocal worst
        for j,(a,b) in enumerate(zip(actual,expected)):
            e=abs(a-b)/max(1,abs(b));worst=max(worst,e);assert math.isfinite(e) and e<3e-6,(checks,j,a,b,e)
    headings=[[0,0,x,z] for x,z in [(0,0),(0,1),(1,0),(0,-1),(-1,0),(1,1),(-1,1),(1,-1),(-1,-1)]]
    headings += [[C.c_float(rng.uniform(-2000,2000)).value for _ in range(4)] for _ in range(1024)]
    for values in headings:
        result=C.c_float();assert lib.bk_route_heading(C.byref(result),*values)
        equal([result.value],[native.heading(values)]);checks+=1
        p=Placement();position=[values[0],values[1],values[2]];assert lib.bk_actor_placement(C.byref(p),(C.c_float*3)(*position),result.value)
        equal(p.world,native.root(position,result.value))
    records=[]
    for file in sorted(args.data.glob('*.ckp')):
        data=file.read_bytes();error=C.create_string_buffer(256);route=lib.bk_route_decode(data,len(data),error);assert route,error.value
        count=lib.bk_route_count(route)
        try:
            for i in range(count):
                height=C.c_float((i%9-4)*1.25).value;p=Placement()
                assert lib.bk_route_placement(C.byref(p),route,i,height),(file.name,i)
                equal(list(p.position)+[p.yaw_degrees]+list(p.world),native.spawn(data,i,height));checks+=1
            records.append(dict(file=file.name,points=count,sha256=hashlib.sha256(data).hexdigest()))
        finally:lib.bk_route_destroy(route)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),heading_cases=len(headings),route_placements=sum(r['points'] for r in records),max_normalized_error=worst,records=records,
                native_functions=['0x4aeba9','0x4fb93e..0x4fba03','0x4fbd9d..0x4fbe4a','0x5234e5','0x42407a'],
                scope='Explicit route cursors and heights, original spawn arithmetic/root matrix/frame setter. No game/save cursor selection or actor update/render timing.')
    (ROOT/'local/original-placement-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'placements; max error',worst)
if __name__=='__main__':main()
