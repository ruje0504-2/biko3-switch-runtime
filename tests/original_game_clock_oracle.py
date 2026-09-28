"""Execute original4adc16/4adc96 with deterministic GetTickCount traces.

This recovers caller-level wall->game timing, which component oracles taking
an already supplied733700 delta cannot verify. No rendering/audio hooks.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
from model_binding import ROOT,library
from original_matrix_oracle import machine
class Timer(C.Structure):
    _fields_=[('duration',C.c_uint32),('deadline',C.c_uint32),('armed',C.c_uint8)]
class Clock(C.Structure):
    _fields_=[('step_gate',Timer),('fps_gate',Timer),('last_sample_ms',C.c_float),('seconds',C.c_float),('frames',C.c_uint32),('fps',C.c_uint32),('clamped_seconds',C.c_double)]
class Native:
    def __init__(self,exe):
        self.u=machine(exe);self.reads=[]
        self.u.mem_write(0x53f10c,struct.pack('<I',0x300fe00))
        self.u.hook_add(UC_HOOK_CODE,self.tick,begin=0x300fe00,end=0x300fe00)
    def tick(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_EAX,self.reads.pop(0));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def call(self,addr,reads):
        self.reads=list(reads);u=self.u
        u.mem_write(0x2008000,struct.pack('<I',0x300ff00));u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(addr,0x300ff00,count=20000);assert u.reg_read(UC_X86_REG_EIP)==0x300ff00
    def word(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
    def reset(self):
        self.u.mem_write(0x708850,bytes(48));self.u.mem_write(0x733700,bytes(4));self.u.mem_write(0xb53958,bytes(4))
    def step(self,now,sample):
        self.call(0x4adc16,[now,sample]);self.call(0x4adc96,[now])
        return struct.unpack('<f',self.u.mem_read(0x733700,4))[0]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);a=p.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library()
    lib.bk_game_clock_poll.argtypes=[C.POINTER(Clock),C.c_uint32,C.c_uint32]
    traces=[]
    for hz in [15,20,30,60,120,240]:traces.append((f'{hz}Hz',[(round(i*1000/hz),round(i*1000/hz)) for i in range(hz*12)],hz))
    rng=random.Random(20260928)
    for start in [0,1000,2**24-3000,2**31-3000,2**32-3000]:
        now=start;trace=[]
        for i in range(2400):
            now=(now+rng.choice([0,1,5,16,17,33,67,111,220,500,1500]))&0xffffffff
            trace.append((now,(now+rng.choice([0,0,0,1]))&0xffffffff))
        traces.append((f'variable-{start}',trace,0))
    records=[];checks=0
    for name,trace,hz in traces:
        n.reset();c=Clock();game=0
        for i,(now,sample) in enumerate(trace):
            expected=n.step(now,sample);lib.bk_game_clock_poll(C.byref(c),now,sample)
            assert struct.pack('<f',c.seconds)==struct.pack('<f',expected),(name,i,c.seconds,expected)
            assert bytes(c.step_gate)==n.u.mem_read(0x708850,12)
            assert bytes(c.fps_gate)==n.u.mem_read(0x708860,12)
            assert bytes(C.c_float(c.last_sample_ms))==n.u.mem_read(0x708870,4)
            assert c.frames==n.word(0x70886c) and c.fps==n.word(0xb53958)
            if hz and i>=hz:game+=expected
            checks+=1
        rate=game/11 if hz else None
        if hz:assert abs(rate-2)<.0005,(name,rate)
        records.append(dict(name=name,frames=len(trace),steady_game_per_wall=rate,fps=c.fps))
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=checks,max_step_error=0,traces=records,scope=__doc__)
    (ROOT/'local/original-game-clock-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
