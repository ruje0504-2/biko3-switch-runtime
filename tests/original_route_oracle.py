"""Native route filename dispatch, Windows read boundary and active count.

Original 0x4ff78e, 0x5011bc and actor count loop run unmodified. Only path
prefix construction and Windows file APIs are replaced with read-only bytes.
"""
import argparse,ctypes as C,hashlib,json,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Point(C.Structure):
    _fields_=[('position',C.c_float*3),('parameter',C.c_float),('flags',C.c_uint8)]
class Native:
    stack=0x2008000;stop=0x300f000;destination=0xbe9a18
    def __init__(self,exe,directory):
        self.uc=machine(exe);self.directory=directory;self.payload=None;self.path=None
        for offset,address in [(0x53f208,0x300e000),(0x53f210,0x300e010),(0x53f20c,0x300e020)]:
            self.word(offset,address);self.uc.mem_write(address,b'\xc3')
            self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=address,end=address)
        self.uc.hook_add(UC_HOOK_CODE,self.boundary,begin=0x4ad8ec,end=0x4ad8ec)
    def word(self,address,value):self.uc.mem_write(address,struct.pack('<I',value))
    def string(self,address):
        out=bytearray()
        while True:
            b=self.uc.mem_read(address,1)[0];address+=1
            if not b:return out.decode('ascii')
            out.append(b);assert len(out)<260
    def boundary(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        args=struct.unpack('<7I',u.mem_read(sp+4,28));value=1;cleanup=0
        if address==0x4ad8ec:
            assert args[2]==0
            u.mem_write(args[0],self.string(args[1]).encode()+b'\0')
        elif address==0x300e000:
            self.path=self.string(args[0]).split('\\')[-1]
            self.payload=(self.directory/self.path).read_bytes();value=42;cleanup=28
        elif address==0x300e010:
            assert args[0]==42 and args[2]==0x5000 and args[4]==0
            data=self.payload[:args[2]];u.mem_write(args[1],data);self.word(args[3],len(data));cleanup=20
        else:assert args[0]==42;cleanup=4
        u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def call(self,address,args):
        self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(address,self.stop,count=1000000);assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
    def count(self):
        self.word(0xbf3c7c,0)
        self.uc.emu_start(0x4fb89b,0x4fb93e,count=1000000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==0x4fb93e
        return struct.unpack('<I',self.uc.mem_read(0xbf3c7c,4))[0]
    def load(self,name):
        self.uc.mem_write(self.destination,bytes(0x5000));self.uc.mem_write(0x3001000,name.encode()+b'\0')
        self.call(0x5011bc,struct.pack('<2I',self.destination,0x3001000));return self.count()
    def select(self,group,area,loose):
        self.uc.mem_write(0x5767c8,bytes([loose]));self.call(0x4ff78e,struct.pack('<2I',group,area))
        return self.path,self.count()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe,args.data);lib=library();records=[];mapping=[]
    lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_route_decode.restype=C.c_void_p
    lib.bk_route_destroy.argtypes=[C.c_void_p];lib.bk_route_count.argtypes=[C.c_void_p];lib.bk_route_count.restype=C.c_uint32
    lib.bk_route_point.argtypes=[C.c_void_p,C.c_uint32];lib.bk_route_point.restype=C.POINTER(Point)
    for file in sorted(args.data.glob('*.ckp')):
        data=file.read_bytes();err=C.create_string_buffer(256);route=lib.bk_route_decode(data,len(data),err);assert route,(file.name,err.value)
        try:
            expected=native.load(file.name);assert lib.bk_route_count(route)==expected
            for i in range(expected+1):
                wanted=struct.unpack('<4fB3x',native.uc.mem_read(native.destination+i*20,20));point=lib.bk_route_point(route,i).contents
                assert tuple(point.position)+(point.parameter,point.flags)==wanted,(file.name,i)
            records.append(dict(file=file.name,size=len(data),sha256=hashlib.sha256(data).hexdigest(),points=expected))
        finally:lib.bk_route_destroy(route)
    for group in range(5):
        for area in range(9):
            first=native.select(group,area,1);second=native.select(group,area,0);assert first==second
            mapping.append(dict(group=group,area=area,file=first[0],points=first[1]))
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=len(records),active_points=sum(r['points'] for r in records),selection_checks=len(mapping)*2,records=records,mapping=mapping,
                scope='Native filename dispatch, file-read size and actor active-count loop. Short files use a pre-zeroed fixture destination; this does not prove native loading clears unread bytes. Windows I/O/path prefix are boundary substitutes; route AI/movement not exercised.')
    (ROOT/'local/original-route-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',report['files'],'files',report['active_points'],'points',report['selection_checks'],'selections')
if __name__=='__main__':main()
