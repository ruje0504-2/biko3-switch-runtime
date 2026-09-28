"""Compare original reached-point policy, preserving media calls as commands."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_npc_motion_oracle import Native as MotionNative, Timer, Actions, State
from model_binding import ROOT, library

class Point(C.Structure):
    _fields_ = [('motion', State), ('action_wait', Timer), ('background_wait', C.c_uint8), ('fade_out', C.c_uint8), ('gate_state', C.c_uint8)]
class Effects(C.Structure):
    _fields_ = [('snap_to_point', C.c_uint8), ('play_wait_sound', C.c_uint8), ('play_route_sound', C.c_uint8)]

class Native(MotionNative):
    def __init__(self, exe):
        super().__init__(exe)
        for a in [0x46435e, 0x4f6226]:self.u.hook_add(UC_HOOK_CODE, self.media, begin=a, end=a)
    def media(self, u, address, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', u.mem_read(sp,4))[0]
        args = struct.unpack('<'+'I'*(2 if address==0x46435e else 3),u.mem_read(sp+4,8 if address==0x46435e else 12))
        if address==0x46435e:assert args==(0xdeadbeef,0)
        else:assert args==(2,7,121)
        self.calls.append(address)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
    def write_point(self, state, actions, remaining, background, cursor=121):
        data=bytearray(0x900);m=state.motion
        struct.pack_into('<i',data,0x10,m.action)
        for i,value in zip([0,1,3,21,23,22,24],[actions.idle,actions.walk,actions.run,*actions.stationary]):struct.pack_into('<i',data,0x14+i*4,value)
        data[0x328],data[0x578],data[0x84c]=m.hidden,m.mode&255,m.route_flag&255
        struct.pack_into('<i',data,0x850,m.behavior)
        struct.pack_into('<I',data,0x830,cursor)
        data[0x840:0x84c]=bytes(m.wait);data[0x854:0x860]=bytes(state.action_wait)
        data[0x330],data[0x331],data[0x870]=state.background_wait,state.fade_out,state.gate_state
        u=self.u;u.mem_write(self.actor,bytes(data));u.mem_write(0xbf3c6c,struct.pack('<i',remaining))
        u.mem_write(0x725718,struct.pack('<I',self.background));u.mem_write(self.background+0x140,struct.pack('<i',background))
        u.mem_write(0xbef050,struct.pack('<I',0xdeadbeef));u.mem_write(0x7219a8,struct.pack('<II',2,7))
        return data
    def point(self, state, actions, remaining, flag, background):
        data=self.write_point(state,actions,remaining,background)
        u=self.u
        self.calls=[]
        snap=self.call(0x4f5410,self.actor,0x3f800000,0x40000000,0x40400000,0,flag&255)&255
        result=bytes(u.mem_read(self.actor,len(data)))
        wanted=bytearray(data)
        # Only these native fields may change in this isolated policy.
        for start,size in [(0x10,4),(0x330,2),(0x840,12),(0x84c,1),(0x850,4),(0x854,12)]:wanted[start:start+size]=result[start:start+size]
        assert bytes(wanted)==result
        return snap,result,self.calls

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4f5410)
    lib.bk_npc_point_apply.argtypes=[C.POINTER(Point),C.POINTER(Actions),C.c_int32,C.c_int8,C.c_int32,C.POINTER(Effects)];lib.bk_npc_point_apply.restype=C.c_int
    checks=snaps=sounds=0
    for flag in range(-128,128):
        for variant in range(32):
            a=Actions(0,1,4,(C.c_int32*4)(10,7,12,9)) if variant<16 else Actions(*[rng.randint(-3,3) for _ in range(3)],(C.c_int32*4)(*[rng.randint(-3,3) for _ in range(4)]))
            def timer():return Timer(rng.getrandbits(32),rng.getrandbits(32),rng.choice([0,1,255]))
            state=Point(State(rng.randrange(-4,15),rng.randrange(-4,5),rng.choice([0,1,255]),rng.choice([-128,-1,0,0,1,1,2,127]),rng.randrange(-128,128),timer()),timer(),rng.choice([0,1,255]),rng.choice([0,1,2,255]),rng.choice([0,0,1,255]))
            remaining=rng.choice([-2,0,0,0,1,3]);background=rng.choice([-1,0,1,2,2,3])
            snap,w,calls=native.point(state,a,remaining,flag,background);effects=Effects(255,255,255)
            assert lib.bk_npc_point_apply(C.byref(state),C.byref(a),remaining,flag,background,C.byref(effects))
            m=state.motion
            assert m.action==struct.unpack_from('<i',w,0x10)[0] and m.behavior==struct.unpack_from('<i',w,0x850)[0],(flag,variant)
            assert bytes(m.wait)==w[0x840:0x84c] and bytes(state.action_wait)==w[0x854:0x860]
            assert (m.route_flag&255)==w[0x84c] and (m.mode&255)==w[0x578] and m.hidden==w[0x328]
            assert [state.background_wait,state.fade_out,state.gate_state]==[w[0x330],w[0x331],w[0x870]]
            assert effects.snap_to_point==snap and effects.play_wait_sound==(0x46435e in calls) and effects.play_route_sound==(0x4f6226 in calls)
            checks+=1;snaps+=snap;sounds+=len(calls)
    report=dict(x87_control_word="0x037f",passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=checks,snap_cases=snaps,media_commands=sounds,native_functions=['0x4f5410'],hooks=['0x46435e audio playback boundary','0x4f6226 route audio selection boundary'],scope='Reached-point action, two timers, fade toggle, background wait and gate flags; sound calls exported as commands. No audio backend or full AI implied.')
    (ROOT/'local/original-npc-point-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'point policies,',snaps,'snaps,',sounds,'media commands')
if __name__=='__main__':main()
