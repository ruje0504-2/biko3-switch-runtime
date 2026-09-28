"""Execute the original action-completion tail with only the clock supplied."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_npc_point_oracle import Native as PointNative,Point
from original_npc_motion_oracle import Actions,State,Timer
from model_binding import ROOT,library
class Native(PointNative):
    def finish(self,state,actions,clip,now):
        data=self.write_point(state,actions,0,0)
        self.u.mem_write(self.actor,struct.pack('<I',self.background))
        struct.pack_into('<I',data,0,self.background)
        self.u.mem_write(self.background+0x140,struct.pack('<i',clip))
        self.now,self.clock_calls=now,0
        self.u.mem_write(self.stack+8,struct.pack('<I',self.actor))
        self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x100);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x4fd3d2,0x4fd5ed,count=10000)
        assert self.u.reg_read(UC_X86_REG_EIP)==0x4fd5ed
        result=bytes(self.u.mem_read(self.actor,len(data)))
        for start,size in [(0x10,4),(0x84c,1),(0x850,4),(0x854,12)]:data[start:start+size]=result[start:start+size]
        assert bytes(data)==result
        return result

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4fd3d2)
    lib.bk_npc_action_finish.argtypes=[C.POINTER(Point),C.POINTER(Actions),C.c_int32,C.c_uint32];lib.bk_npc_action_finish.restype=C.c_int
    checks=clock_queries=0
    for case in range(14000):
        a=Actions(0,1,4,(C.c_int32*4)(10,7,12,9)) if case<9000 else Actions(*[rng.randrange(-2,4) for _ in range(3)],(C.c_int32*4)(*[rng.randrange(-2,4) for _ in range(4)]))
        clocks=[0,1,99,100,2000,5000,0x7fffffff,0x80000000,0xffffffff]
        def timer():return Timer(rng.choice(clocks),rng.choice(clocks),rng.choice([0,1,2,255]))
        state=Point(State(rng.choice([a.idle,a.walk,a.run,*a.stationary,99]),rng.choice([-1,0,1,2,3,4,99]),rng.randrange(256),rng.randrange(-128,128),rng.choice([-1,0,1,2,3,4,5,7,9]),timer()),timer(),rng.randrange(256),rng.randrange(256),rng.randrange(256))
        clip=rng.choice([0,1,4,*a.stationary,99]);now=rng.choice(clocks)
        old=Point.from_buffer_copy(state);wanted=native.finish(state,a,clip,now)
        assert lib.bk_npc_action_finish(C.byref(state),C.byref(a),clip,now)
        assert state.motion.action==struct.unpack_from('<i',wanted,0x10)[0] and state.motion.behavior==struct.unpack_from('<i',wanted,0x850)[0],(case,clip,now)
        assert (state.motion.route_flag&255)==wanted[0x84c] and bytes(state.action_wait)==wanted[0x854:0x860]
        old.motion.action=state.motion.action;old.motion.behavior=state.motion.behavior;old.motion.route_flag=state.motion.route_flag;old.action_wait=state.action_wait
        assert bytes(old)==bytes(state)
        clock_queries+=native.clock_calls;checks+=1
    report=dict(x87_control_word="0x037f",passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=checks,timer_queries=clock_queries,native_functions=['0x4fd3d2..0x4fd5ed','0x4adbb9'],hooks=['GetTickCount'],scope='Action-completion tail only; preceding AI can bypass it. Exact branch ordering includes aliased action slots and repeated timer polling at a caller-supplied frame clock.')
    (ROOT/'local/original-npc-action-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'action completions,',clock_queries,'timer queries')
if __name__=='__main__':main()
