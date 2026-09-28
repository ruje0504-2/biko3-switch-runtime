"""Full509c25/509fd2 bank save/read,4af3dc timestamp and50d245 record xor.
Only Windows time/file I/O, path service and random source are intercepted.
All record copying, stores, string operations and xor execute original x86.
Synthetic occupied records exercise all50 slots; shipped five banks are empty.
Port envelope CRC is also compared independently with Python zlib.
"""
import argparse, ctypes as C, hashlib, json, random, struct, zlib
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from model_binding import ROOT, library

class Time(C.Structure):
    _fields_ = [(x, C.c_uint) for x in ['year','month','day','hour','minute','second']]
class Record(C.Structure):
    _fields_ = [('nonce',C.c_uint32),('area',C.c_uint32),('inventory',C.c_uint8*8),('opaque',C.c_uint8*8),('stamp',C.c_char*32)]
class Bank(C.Structure):
    _fields_ = [('slots',Record*10)]

class Native(Base):
    def __init__(self,exe):
        super().__init__(exe)
        imports=[0x53f208,0x53f130,0x53f20c,0x53f210,0x53f114,0x53f110,0x53f10c]
        for i,p in enumerate(imports):
            self.wi(p,0x300e000+i*16)
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x300e000+i*16,end=0x300e000+i*16)
        for a in [0x4ad8ec,0x534a34]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
        self.u.mem_write(0x5767c8,b'\1')
    def wi(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
    def string(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0]
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        args=struct.unpack('<7I',u.mem_read(sp+4,28));result=0;pop=4
        if a==0x4ad8ec:u.mem_write(args[0],b'output/'+self.string(args[1]).lstrip(b'\\')+b'\0')
        elif a==0x534a34:result=self.random
        elif a==0x300e000:
            self.path=self.string(args[0]);self.mode=args[4];assert self.mode in [3,4];result=123;pop=32
        elif a==0x300e010:
            assert args[0]==123 and args[2]==560
            self.written=bytes(u.mem_read(args[1],args[2]));self.wi(args[3],560);result=1;pop=24
        elif a==0x300e020:assert args[0]==123;result=1;pop=8
        elif a==0x300e030:
            assert args[0]==123 and args[2]==560
            u.mem_write(args[1],self.written);self.wi(args[3],560);result=1;pop=24
        elif a in [0x300e040,0x300e050]:
            t=self.time
            fmt=self.string(args[3])
            assert fmt==(b"yyyy'/'MM'/'dd'-'" if a==0x300e040 else b"hh':'mm':'ss"),fmt
            text=(f'{t.year:04d}/{t.month:02d}/{t.day:02d}-' if a==0x300e040 else f'{t.hour:02d}:{t.minute:02d}:{t.second:02d}').encode()+b'\0'
            u.mem_write(args[4],text);result=len(text);pop=28
        elif a==0x300e060:result=self.ticks
        else:raise AssertionError(hex(a))
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x509c25);e=C.create_string_buffer(256)
    lib.bk_checkpoint_update.argtypes=[C.POINTER(Bank),C.c_uint,C.c_uint,C.c_void_p,C.POINTER(Time),C.c_int32,C.c_void_p]
    lib.bk_checkpoint_native_encode.argtypes=[C.POINTER(Bank),C.c_void_p,C.c_void_p]
    lib.bk_checkpoint_native_decode.argtypes=[C.POINTER(Bank),C.c_void_p,C.c_size_t,C.c_void_p]
    lib.bk_checkpoint_encode.argtypes=[C.POINTER(Bank),C.c_uint,C.c_void_p,C.c_void_p]
    lib.bk_checkpoint_decode.argtypes=[C.POINTER(Bank),C.c_uint,C.c_void_p,C.c_size_t,C.c_void_p]
    lib.bk_checkpoint_stamp.argtypes=[C.c_void_p,C.POINTER(Time)]
    colors=['Blue','White','Black','Green','Red'];native_encodes=native_reads=0
    for color in colors:
        raw=(a.data/f'bk3_{color}.b3f').read_bytes();b=Bank();encoded=C.create_string_buffer(560)
        assert lib.bk_checkpoint_native_decode(C.byref(b),raw,len(raw),e),e.value
        assert not any(b.slots[i].stamp for i in range(10))
        assert lib.bk_checkpoint_native_encode(C.byref(b),encoded,e);assert encoded.raw==raw
    for case in range(1000):
        group=case%5;slot=(case//5)%10
        raw=bytearray(rng.randbytes(560))
        for i in range(10):
            struct.pack_into('<I',raw,i*56+4,rng.randrange(9))
            raw[i*56+24:i*56+44]=b'2000/02/29-23:59:59\0'
            if i%4==0:raw[i*56+24]=0
        bank=Bank.from_buffer_copy(raw)
        others=rng.randbytes(5*560);n.u.mem_write(0xb53c40,others);n.u.mem_write(0xb53c40+group*560,bytes(raw))
        time=Time(rng.choice([1,99,1900,2000,2026,9999]),rng.randrange(1,13),rng.randrange(1,29),rng.randrange(24),rng.randrange(60),rng.randrange(60))
        area=rng.randrange(9);inv=rng.randbytes(8)
        rand=rng.choice([rng.randrange(32768),-1,-65537,0x7fffffff,-0x80000000])
        n.time=time;n.random=rand;n.ticks=rng.getrandbits(32);n.written=b''
        n.wi(0x7219ac,area);n.u.mem_write(0x71bcdc,inv)
        n.call(0x509c25,struct.pack('<II',group,slot));native_encodes+=1
        assert n.path==f'output/bk3_{colors[group]}.b3f'.encode()
        assert n.mode==4
        assert lib.bk_checkpoint_update(C.byref(bank),slot,area,inv,C.byref(time),rand,e),e.value
        assert bytes(bank)==bytes(n.u.mem_read(0xb53c40+group*560,560)),case
        encoded=C.create_string_buffer(560);assert lib.bk_checkpoint_native_encode(C.byref(bank),encoded,e),e.value
        assert encoded.raw==n.written,(case,'native-write')
        n.u.mem_write(0xb53c40+group*560,bytes(560));n.call(0x509fd2,struct.pack('<II',group,slot));native_reads+=1
        assert n.mode==3;assert bytes(n.u.mem_read(0xb53c40+group*560,560))==bytes(bank)
        for other in range(5):
            if other!=group:assert n.u.mem_read(0xb53c40+other*560,560)==others[other*560:(other+1)*560]
        out=Bank();assert lib.bk_checkpoint_native_decode(C.byref(out),encoded,560,e);assert bytes(out)==bytes(bank)
        envelope=C.create_string_buffer(592);assert lib.bk_checkpoint_encode(C.byref(bank),group,envelope,e)
        data=envelope.raw;assert struct.unpack_from('<I',data,24)[0]==zlib.crc32(data[:24]+data[32:])
        assert data[32:]==bytes(bank);assert lib.bk_checkpoint_decode(C.byref(out),group,envelope,592,e);assert bytes(out)==bytes(bank)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),native_encodes=native_encodes,native_reads=native_reads,records_compared=native_encodes*10,original_empty_banks=5,port_crc_checks=1000,scope=__doc__)
    (ROOT/'local/original-checkpoint-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS checkpoint',report)
if __name__=='__main__':main()
