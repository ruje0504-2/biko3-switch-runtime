"""Original50CAA2/50CC47 full five-group Gray records, including real50D245.
Only path and Win32 file IO are hooks. Missing/short native-file hazards are
observed separately, not reproduced as valid empty progress in the port.
"""
import argparse, ctypes as C, hashlib, json, random, struct, zlib
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Base
from model_binding import library
SIZE=600020
class View(C.Structure):
    _fields_=[('retained',C.POINTER(C.c_uint32)*2),('actions',C.POINTER(C.c_int32)),('count',C.POINTER(C.c_int32))]
def views(buf):
    v=(View*5)()
    for g in range(5):
        off=g*120004
        v[g]=View((C.POINTER(C.c_uint32)*2)(C.cast(C.byref(buf,off),C.POINTER(C.c_uint32)),C.cast(C.byref(buf,off+40000),C.POINTER(C.c_uint32))),C.cast(C.byref(buf,off+80000),C.POINTER(C.c_int32)),C.cast(C.byref(buf,off+120000),C.POINTER(C.c_int32)))
    return v
class Native(Base):
    def __init__(self,exe):
        super().__init__(exe)
        self.u.mem_map(0x1f00000,0x100000) # original600KB stack + chkstk pages
        for i,p in enumerate([0x53f208,0x53f130,0x53f20c,0x53f210]):
            self.word(p,0x300e000+i*16)
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x300e000+i*16,end=0x300e000+i*16)
        self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x4ad8ec,end=0x4ad8ec)
        self.handle=123;self.success=True;self.length=SIZE
    def word(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
    def string(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0]
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        args=struct.unpack('<7I',u.mem_read(sp+4,28));pop=4;result=0
        if a==0x4ad8ec:
            assert (self.string(args[1]),self.string(args[2]) if args[2] else b'')==((b'\\bk3_Gray.b3f',b'') if self.packed else (b'\\savefile',b'\\bk3_Gray.b3f'))
            u.mem_write(args[0],b'output/bk3_Gray.b3f\0')
        elif a==0x300e000:
            assert self.string(args[0])==b'output/bk3_Gray.b3f' and args[1:]==(0xc0000000,0,0,self.mode,0x80,0)
            self.events.append('open');result=self.handle;pop=32
        elif a==0x300e010:
            assert args[0]==123 and args[2]==SIZE and args[4]==0
            self.encoded=bytes(u.mem_read(args[1],SIZE));self.word(args[3],SIZE if self.success else 0)
            self.events.append('write');result=int(self.success);pop=24
        elif a==0x300e020:
            assert args[0]==self.handle&0xffffffff
            self.events.append('close');result=1;pop=8
        elif a==0x300e030:
            assert args[0]==123 and args[2]==SIZE and args[4]==0
            if self.length:u.mem_write(args[1],self.encoded[:self.length])
            self.word(args[3],self.length);self.events.append('read');result=int(self.success);pop=24
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,address,raw,packed,mode):
        self.events=[];self.packed=packed;self.mode=mode
        self.u.mem_write(0x5767c8,bytes([packed]));self.u.mem_write(0xb550b0,raw)
        self.u.mem_write(self.stack,struct.pack('<I',self.stop));self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.emu_start(address,self.stop,count=8000000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        return bytes(self.u.mem_read(0xb550b0,SIZE))
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--report',required=True,type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x50caa2)
    for name in ['encode','native_encode']:
        getattr(lib,'bk_record_'+name).argtypes=[C.POINTER(View),C.c_void_p,C.c_void_p]
    for name in ['decode','native_decode']:
        getattr(lib,'bk_record_'+name).argtypes=[C.POINTER(View),C.c_void_p,C.c_size_t,C.c_void_p]
    src=C.create_string_buffer(SIZE);dst=C.create_string_buffer(SIZE);out=C.create_string_buffer(SIZE);port=C.create_string_buffer(SIZE+32)
    v,w=views(src),views(dst)
    cases=[bytes(SIZE),bytes([255])*SIZE]+[rng.randbytes(SIZE) for _ in range(14)]
    for i,raw in enumerate(cases):
        C.memmove(src,raw,SIZE)
        assert n.run(0x50caa2,raw,i%2,4)==raw and n.events==['open','write','close']
        assert lib.bk_record_native_encode(v,out,e),e.value
        assert out.raw==n.encoded and src.raw==raw
        assert lib.bk_record_native_decode(w,out,SIZE,e),e.value
        assert dst.raw==n.run(0x50cc47,bytes([0xaa])*SIZE,i%2,3)==raw
        assert n.events==['open','read','close']
        assert lib.bk_record_encode(v,port,e) and port.raw[32:]==raw
        assert struct.unpack_from('<I',port.raw,24)[0]==zlib.crc32(port.raw[:24]+raw)
        assert lib.bk_record_decode(w,port,SIZE+32,e) and dst.raw==raw
    shipped=(a.data/'bk3_Gray.b3f').read_bytes();assert len(shipped)==SIZE
    n.encoded=shipped
    assert lib.bk_record_native_decode(w,shipped,SIZE,e)
    assert dst.raw==n.run(0x50cc47,bytes(SIZE),0,3)
    assert lib.bk_record_native_encode(w,out,e) and out.raw==shipped
    hazards=[]
    for handle,success,length in [(0,True,SIZE),(-1,True,SIZE),(123,False,0),(123,True,7)]:
        n.handle=handle;n.success=success;n.length=length
        got=n.run(0x50cc47,bytes([7])*SIZE,0,3)
        hazards.append(dict(handle=handle,success=success,length=length,retained=got==bytes([7])*SIZE,sha256=hashlib.sha256(got).hexdigest(),events=n.events.copy()))
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),native_writes=len(cases),native_reads=len(cases)+1,bytes_per_file=SIZE,compared_bytes=(len(cases)*2+1)*SIZE,max_error=0,shipped_sha256=hashlib.sha256(shipped).hexdigest(),hazards=hazards,scope=__doc__)
    a.report.write_text(json.dumps(report,indent=2)+'\n');print('PASS record native16 writes17 reads600020 bytes each,all words exact,4 failure hazards')
if __name__=='__main__':main()
