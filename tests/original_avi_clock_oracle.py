"""Run original521d78/521ed2, including clock selection and both surface copies.

AVI/VFW/DDraw are explicit deterministic service fixtures, not codec proof.
The original code, including x87 conversion/copy loops, executes unchanged.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine

class Info(C.Structure):
    _fields_ = [(x,C.c_uint32) for x in ('width','height','frames','scale','rate')]
class Clock(C.Structure):
    _fields_ = [('start_seconds',C.c_float),('last_frame',C.c_uint32)]

class Native:
    object=0x3000000
    surface=0x3000400
    table=0x3000500
    dib=0x3002000
    pixels=0x3004000
    stop=0x300ff00
    def __init__(self,exe):
        self.u=machine(exe);self.reads=[];self.events=[];self.get_fail=False;self.lock_fail=False
        self.green565=False;self.source=[(i*7919+0x8123)&65535 for i in range(34)]
        self.u.mem_write(self.dib,bytes(40)+struct.pack('<34H',*self.source))
        self.u.mem_write(self.surface,struct.pack('<I',self.table))
        self.u.mem_write(self.table+0x64,struct.pack('<I',0x300f100))
        self.u.mem_write(self.table+0x80,struct.pack('<I',0x300f200))
        for a in [0x5227cc,0x5227c6,0x5227c0,0x5227ba,0x52043e,0x535dcc,0x5227e4,0x300f100,0x300f200]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP)
        stack=struct.unpack('<8I',u.mem_read(sp,32));argc=0;result=0
        if a==0x535dcc:
            result=self.reads.pop(0)&0xffffffff;self.events.append(['clock',result])
        elif a==0x5227cc:pass
        elif a==0x5227c6:
            argc=6;u.mem_write(stack[1],struct.pack('<I',0x12340000))
        elif a==0x5227c0:argc=2;result=0x12345678
        elif a==0x5227ba:
            argc=3;u.mem_write(stack[2],self.native_info)
        elif a==0x52043e:
            assert stack[1]==160;result=self.object;u.mem_write(result,bytes(160))
        elif a==0x5227e4:
            argc=2;result=0xffffffff if self.get_fail else self.dib
            self.events.append(['frame',stack[2]])
        elif a==0x300f100:
            argc=5;self.events.append(['lock']);result=0xffffffff if self.lock_fail else 0
            descriptor=bytearray(124)
            struct.pack_into('<4I',descriptor,0,124,0,4,8)
            struct.pack_into('<I',descriptor,0x24,self.pixels)
            struct.pack_into('<I',descriptor,0x5c,0x7e0 if self.green565 else 0x3e0)
            u.mem_write(stack[3],bytes(descriptor))
        elif a==0x300f200:argc=2;self.events.append(['unlock'])
        u.reg_write(UC_X86_REG_EAX,result)
        u.reg_write(UC_X86_REG_ESP,sp+4*(argc+1));u.reg_write(UC_X86_REG_EIP,stack[0])
    def call(self,addr,*args):
        u=self.u;u.mem_write(0x2008000,struct.pack('<'+'I'*(1+len(args)),self.stop,*args))
        u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(addr,self.stop,count=20000)
        assert u.reg_read(UC_X86_REG_EIP)==self.stop
        return u.reg_read(UC_X86_REG_EAX)
    def create(self,info,ms):
        data=bytearray(140);struct.pack_into('<4I',data,20,info.scale,info.rate,0,info.frames)
        self.native_info=bytes(data);self.reads=[ms];self.events=[]
        assert self.call(0x521d78,0x3006000)==self.object
        self.call(0x521d2c,self.object,self.surface)
    def step(self,now,restart):
        self.reads=[now,restart];self.events=[]
        self.u.mem_write(self.pixels,bytes([0xa5])*80)
        result=self.call(0x521ed2,self.object)
        return result,bytes(self.u.mem_read(self.object+0x98,8)),self.events

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);a=p.parse_args()
    exe=a.exe.read_bytes();native=Native(exe);lib=library();error=C.create_string_buffer(256)
    lib.bk_avi_clock_init.argtypes=[C.POINTER(Clock),C.c_int32]
    lib.bk_avi_clock_select.argtypes=[C.POINTER(Clock),C.POINTER(Info),C.c_int32,C.c_int32,C.POINTER(C.c_uint32),C.c_void_p]
    Read=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int32),C.c_void_p)
    lib.bk_avi_clock_poll.argtypes=[C.POINTER(Clock),C.POINTER(Info),Read,C.c_void_p,C.POINTER(C.c_uint32),C.c_void_p]
    lib.bk_avi_surface_rgba.argtypes=[C.POINTER(C.c_uint16),C.c_uint32,C.c_uint32,C.c_int,C.c_void_p,C.c_size_t,C.c_void_p]
    topdown=(C.c_uint16*32)(*[v for y in reversed(range(4)) for v in native.source[y*8:(y+1)*8]])
    rgba=C.create_string_buffer(128)
    assert native.call(0x525579,5,0x3007000)==0
    format5=struct.unpack('<8I',native.u.mem_read(0x3007000,32))
    assert format5==(32,0x40,0,16,0x7c00,0x3e0,0x1f,0)
    rng=random.Random(20260928);frames=uploads=loops=0
    for rate,scale,length in [(30,1,90),(30000,1001,90),(1,3,2),(0xffffffff,1,90),(60,7,65536)]:
        info=Info(8,4,length,scale,rate)
        for origin in [0,1000,2**24-1000,2**31-1000,-2**31,-10000]:
            native.create(info,origin);c=Clock();lib.bk_avi_clock_init(C.byref(c),origin)
            assert bytes(c)==native.u.mem_read(native.object+0x98,8)
            now=origin
            for i in range(600):
                now=C.c_int32(now+rng.choice([0,1,16,17,33,34,100,3001,-100,50000])).value
                restart=C.c_int32(now+rng.randrange(3)).value
                native.get_fail=i%19==0;native.lock_fail=i%23==0;native.green565=bool(i%2)
                expected,state,events=native.step(now,restart)
                index=C.c_uint32(0xcafebabe)
                live=Clock.from_buffer_copy(bytes(c));reads=[]
                @Read
                def read_clock(ctx,out,err):
                    value=now if not reads else restart
                    reads.append(['clock',value&0xffffffff]);out[0]=value
                    return 1
                live_index=C.c_uint32(0xcafebabe)
                live_result=lib.bk_avi_clock_poll(C.byref(live),C.byref(info),read_clock,None,C.byref(live_index),error)
                result=lib.bk_avi_clock_select(C.byref(c),C.byref(info),now,restart,C.byref(index),error)
                assert result>=0,error.value
                assert reads==[e for e in events if e[0]=='clock'],(reads,events)
                assert live_result==result and live_index.value==index.value and bytes(live)==bytes(c)
                assert bytes(c)==state,(rate,scale,origin,i,now,bytes(c).hex(),state.hex(),events)
                requests=[e[1] for e in events if e[0]=='frame']
                assert requests==([index.value] if result else []),(requests,index.value,result)
                assert expected==int(bool(result and not native.get_fail and not native.lock_fail))
                if expected:
                    # Source starts at44 even though this fixture's DIB pixels
                    # begin at40. This proves copy arithmetic, not VFW layout.
                    out=[((v&31)|((v&0xffe0)<<1))&65535 if native.green565 else v for v in native.source[2:]]
                    assert native.u.mem_read(native.pixels,64)==struct.pack('<32H',*out)
                    assert native.u.mem_read(native.pixels+64,16)==bytes([0xa5])*16
                    assert lib.bk_avi_surface_rgba(topdown,8,4,int(native.green565),rgba,128,error),error.value
                    expected_rgba=[]
                    for v in out[:30]:
                        r=(v>>(11 if native.green565 else 10))&31
                        g=(v>>5)&(63 if native.green565 else 31)
                        b=v&31;gm=63 if native.green565 else 31
                        expected_rgba += [(r*255+15)//31,(g*255+gm//2)//gm,(b*255+15)//31,255]
                    expected_rgba += [0,0,0,255]*2
                    assert rgba.raw==bytes(expected_rgba)
                    uploads+=1
                else:assert native.u.mem_read(native.pixels,80)==bytes([0xa5])*80
                loops+=len([e for e in events if e[0]=='clock'])==2
                frames+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,
                surface_copies=uploads,loops=loops,max_clock_error=0,scope=__doc__,
                surface_format5=list(format5),defined_surface_pixels=uploads*30,zero_tail_pixels=uploads*2,
                limitation='VFW fixture proves source+44 copy. Compact positive RGB555 DIB, RGB555 device target, initial black and two black tail pixels are explicit portable policies; Windows codec layout is not proved')
    (ROOT/'local/original-avi-clock-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':main()
