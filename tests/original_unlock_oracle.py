"""Original50c6bc saved-row write,50c8a1 read,50ca48 working copy and50d245.
Only Windows file IO and path resolution are services. All table bytes/copy
and XOR execute original instructions. Missing/short/error IO are separately
observed native hazards, not a claim that the port emulates corrupt progress.
"""
import argparse, ctypes as C, hashlib, json, random, struct, zlib
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from model_binding import ROOT, library

class Table(C.Structure):
    _fields_=[('flags',(C.c_uint8*8)*5)]

class Native(Base):
    def __init__(self,exe):
        super().__init__(exe)
        for i,p in enumerate([0x53f208,0x53f130,0x53f20c,0x53f210]):
            self.word(p,0x300e000+i*16)
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x300e000+i*16,end=0x300e000+i*16)
        self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x4ad8ec,end=0x4ad8ec)
        self.handle=123;self.io_success=True;self.read_length=40;self.events=[]
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def string(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0]
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        args=struct.unpack('<7I',u.mem_read(sp+4,28));pop=4;result=0
        if a==0x4ad8ec:
            if self.special:
                assert self.string(args[1])==b'\\bk3_Yellow.b3f' and args[2]==0
            else:
                assert self.string(args[1])==b'\\savefile' and self.string(args[2])==b'\\bk3_Yellow.b3f'
            u.mem_write(args[0],b'output/bk3_Yellow.b3f\0')
        elif a==0x300e000:
            assert self.string(args[0])==b'output/bk3_Yellow.b3f'
            assert args[1:]==(0xc0000000,0,0,self.mode,0x80,0)
            self.events.append('open');result=self.handle;pop=32
        elif a==0x300e010:
            assert args[0]==123 and args[2]==40 and args[4]==0
            self.written=bytes(u.mem_read(args[1],40));self.word(args[3],40 if self.io_success else 0)
            self.events.append('write');result=int(self.io_success);pop=24
        elif a==0x300e020:
            assert args[0]==self.handle&0xffffffff
            self.events.append('close');result=1;pop=8
        elif a==0x300e030:
            assert args[0]==123 and args[2]==40 and args[4]==0
            u.mem_write(args[1],self.written[:self.read_length]);self.word(args[3],self.read_length)
            self.events.append('read');result=int(self.io_success);pop=24
        else:raise AssertionError(hex(a))
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
    def write(self,old,group,row,special):
        self.special=special;self.mode=4;self.events=[];self.written=b''
        self.u.mem_write(0x5767c8,bytes([special]));self.u.mem_write(0xb54738,old);self.u.mem_write(self.actor,row)
        self.call(0x50c6bc,struct.pack('<II',group,self.actor))
        return bytes(self.u.mem_read(0xb54738,40)),self.written
    def read(self,encoded,old,special):
        self.special=special;self.mode=3;self.events=[];self.written=encoded
        self.u.mem_write(0x5767c8,bytes([special]));self.u.mem_write(0xb54738,old)
        self.call(0x50c8a1,b'')
        return bytes(self.u.mem_read(0xb54738,40))

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();err=C.create_string_buffer(256);rng=random.Random(0x50c6bc)
    for name,args in [('update',[C.POINTER(Table),C.c_uint,C.c_void_p,C.c_void_p]),('native_encode',[C.POINTER(Table),C.c_void_p,C.c_void_p]),('native_decode',[C.POINTER(Table),C.c_void_p,C.c_size_t,C.c_void_p]),('encode',[C.POINTER(Table),C.c_void_p,C.c_void_p]),('decode',[C.POINTER(Table),C.c_void_p,C.c_size_t,C.c_void_p])]:
        getattr(lib,'bk_unlock_'+name).argtypes=args
    for case in range(6000):
        old=rng.randbytes(40);group=case%5;row=rng.randbytes(8) if case%3 else bytes(8);special=case%2
        wanted,encoded=n.write(old,group,row,special)
        assert n.events==['open','write','close']
        table=Table.from_buffer_copy(old)
        assert lib.bk_unlock_update(C.byref(table),group,row,err),err.value
        assert bytes(table)==wanted,(case,'row')
        output=C.create_string_buffer(40)
        assert lib.bk_unlock_native_encode(C.byref(table),output,err)
        assert output.raw==encoded,(case,'encode')
        actual=Table();assert lib.bk_unlock_native_decode(C.byref(actual),encoded,40,err)
        assert bytes(actual)==n.read(encoded,rng.randbytes(40),special)==wanted
        assert n.events==['open','read','close']
        n.u.mem_write(0x721dc6,rng.randbytes(40));n.call(0x50ca48,b'')
        assert bytes(n.u.mem_read(0x721dc6,40))==wanted
        port=C.create_string_buffer(72);assert lib.bk_unlock_encode(C.byref(table),port,err)
        raw=port.raw;assert raw[:8]==b'BK3UNLK\0' and raw[32:]==wanted
        assert struct.unpack_from('<I',raw,24)[0]==zlib.crc32(raw[:24]+raw[32:])
        assert lib.bk_unlock_decode(C.byref(actual),port,72,err) and bytes(actual)==wanted
    shipped=(a.data/'bk3_Yellow.b3f').read_bytes();assert len(shipped)==40
    actual=Table();assert lib.bk_unlock_native_decode(C.byref(actual),shipped,40,err)
    assert bytes(actual)==n.read(shipped,rng.randbytes(40),0)
    output=C.create_string_buffer(40);assert lib.bk_unlock_native_encode(C.byref(actual),output,err) and output.raw==shipped
    shipped_values=list(bytes(actual))
    hazards=[]
    for handle,success,length in [(0,True,40),(-1,True,40),(123,False,0),(123,True,7)]:
        n.handle=handle;n.io_success=success;n.read_length=length
        old=bytes([7])*40;out=n.read(shipped,old,0)
        hazards.append(dict(handle=handle,io_success=success,bytes_read=length,output_hex=out.hex(),events=n.events.copy(),retained=out==old))
    n.handle=123;n.io_success=False;n.read_length=40
    old=bytes([9])*40;row=bytes([0])*8
    changed,_=n.write(old,2,row,0);assert changed==old[:16]+row+old[24:]
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),native_writes=6000,native_reads=6001,working_copies=6000,crc_checks=6000,max_error=0,shipped_file_sha256=hashlib.sha256(shipped).hexdigest(),shipped_flags=shipped_values,native_read_hazards=hazards,native_write_mutates_memory_before_failed_io=True,scope=__doc__)
    (ROOT/'local/original-unlock-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS unlock:6000 writes,6001 reads,6000 working copies/CRC; native failure hazards recorded')
if __name__=='__main__':main()
