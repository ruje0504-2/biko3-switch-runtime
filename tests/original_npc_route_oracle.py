"""Execute actual post-AI NPC route substep with original callees and ordering."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_npc_point_oracle import Native as PointNative, Point, Effects
from original_npc_motion_oracle import Timer, State, Actions, Motion as Distance
from original_route_motion_oracle import Motion
from model_binding import ROOT, library

class Npc(C.Structure):
    _fields_=[('path',Motion),('point',Point)]
class Result(C.Structure):
    _fields_=[('movement',Distance),('point',Effects),('sound_cursor',C.c_uint32),('sound_position',C.c_float*3)]
class Native(PointNative):
    def __init__(self,exe):
        super().__init__(exe)
        self.u.hook_add(UC_HOOK_CODE,self.selected,begin=0x4fc994,end=0x4fc994)
    def selected(self,u,address,size,user):
        self.allowed=u.reg_read(UC_X86_REG_EAX)
    def media(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        if address==0x46435e:
            assert struct.unpack('<II',u.mem_read(sp+4,8))==(0xdeadbeef,0)
        else:
            cursor=struct.unpack('<I',u.mem_read(self.actor+0x830,4))[0]
            assert struct.unpack('<III',u.mem_read(sp+4,12))==(2,7,cursor)
            self.sound_position=struct.unpack('<3f',u.mem_read(self.actor+0x29c,12))
        self.calls.append(address)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,0)
    def run(self,state,actions,background,seconds,now):
        u=self.u;s=state.path
        self.write_point(state.point,actions,s.run_remaining,background,s.cursor)
        u.mem_write(self.actor+0x29c,struct.pack('<3f',*s.position))
        u.mem_write(self.actor+0x2ac,struct.pack('<f',s.yaw_degrees))
        u.mem_write(0xbf3c80,struct.pack('<II',s.segment_start,s.segment_end))
        u.mem_write(0xbf3c74,struct.pack('<I',s.last_crossed));u.mem_write(0xbf3c78,bytes([s.crossed]))
        u.mem_write(0x733700,struct.pack('<f',seconds))
        self.now,self.clock_calls,self.calls,self.allowed=now,0,[],-1
        self.sound_position=(0,0,0)
        u.mem_write(self.stack+8,struct.pack('<I',self.actor))
        u.reg_write(UC_X86_REG_EBP,self.stack);u.reg_write(UC_X86_REG_ESP,self.stack-0x100);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(0x4fc97f,0x4fca12,count=200000)
        assert u.reg_read(UC_X86_REG_EIP)==0x4fca12
        data=bytes(u.mem_read(self.actor,0x900))
        state=Npc()
        state.path=Motion((C.c_float*3)(*struct.unpack_from('<3f',data,0x29c)),struct.unpack_from('<f',data,0x2ac)[0],struct.unpack_from('<I',data,0x830)[0],struct.unpack('<i',u.mem_read(0xbf3c6c,4))[0],*struct.unpack('<II',u.mem_read(0xbf3c80,8)),struct.unpack('<I',u.mem_read(0xbf3c74,4))[0],u.mem_read(0xbf3c78,1)[0])
        state.point=Point(State(struct.unpack_from('<i',data,0x10)[0],struct.unpack_from('<i',data,0x850)[0],data[0x328],data[0x578],data[0x84c],Timer.from_buffer_copy(data[0x840:0x84c])),Timer.from_buffer_copy(data[0x854:0x860]),data[0x330],data[0x331],data[0x870])
        return state,self.allowed,bytes(u.mem_read(self.stack-0xc,4)),self.calls

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4fc97f)
    lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p];lib.bk_route_decode.restype=C.c_void_p
    lib.bk_route_destroy.argtypes=[C.c_void_p]
    lib.bk_npc_route_move.argtypes=[C.POINTER(Npc),C.c_void_p,C.POINTER(Actions),C.c_int32,C.c_float,C.c_uint32,C.POINTER(Result),C.c_char_p];lib.bk_npc_route_move.restype=C.c_int
    actions=Actions(0,1,4,(C.c_int32*4)(10,7,12,9));checks=failures=media=0;worst=0
    for file in sorted(args.data.glob('*.ckp')):
        raw=file.read_bytes();points=[]
        for offset in range(0,len(raw),20):
            p=struct.unpack_from('<4fB',raw,offset)
            if not any(p):break
            points.append(p)
        if not points:continue
        native.u.mem_write(0xbe9a18,raw+bytes(20480-len(raw)));buf=C.create_string_buffer(raw);error=C.create_string_buffer(256)
        route=lib.bk_route_decode(buf,len(raw),error);assert route,error.value
        try:
            for sample in range(16):
                cursor=rng.randrange(0,len(points));p=points[max(0,cursor-1)]
                state=Npc(Motion((C.c_float*3)(p[0],-3,p[2]),rng.uniform(-360,360),cursor,rng.choice([0,0,1,3]),0,1,0,rng.choice([0,1])),Point(State(rng.choice([0,1,1,4,7,9,10,12]),rng.randrange(3),rng.choice([0,0,0,1,2]),rng.choice([0,1,1,2]),rng.choice([0,0,1,2,3,4,5]),Timer(rng.choice([0,100,2000,5000]),0,0)),Timer(3000,0,0),0,rng.randrange(3),rng.choice([0,1])))
                for step in range(20):
                    old=Npc.from_buffer_copy(state);result=Result();before=bytes(result)
                    seconds=C.c_float(rng.choice([0,1/60,.125,.5,1,2,4,8])).value;background=rng.choice([0,1,2]);now=(0xfffff800+step*1000)&0xffffffff
                    ok=lib.bk_npc_route_move(C.byref(state),route,C.byref(actions),background,seconds,now,C.byref(result),error)
                    if not ok:
                        assert bytes(old)==bytes(state) and bytes(result)==before
                        assert b'past decoded route sentinel' in error.value,(file.name,error.value)
                        failures+=1;break
                    wanted,allowed,distance,calls=native.run(old,actions,background,seconds,now)
                    assert bytes(state.point)==bytes(wanted.point),(file.name,sample,step,'point')
                    for field in ['cursor','run_remaining','segment_start','segment_end','last_crossed','crossed']:
                        assert getattr(state.path,field)==getattr(wanted.path,field),(file.name,sample,step,field)
                    for a,b in zip([*state.path.position,state.path.yaw_degrees],[*wanted.path.position,wanted.path.yaw_degrees]):
                        err=abs(a-b)/max(1,abs(b));worst=max(worst,err);assert err<2e-5,(file.name,sample,step,a,b)
                    assert result.movement.allowed==allowed and struct.pack('<f',result.movement.distance)==distance
                    assert result.point.play_wait_sound==(0x46435e in calls) and result.point.play_route_sound==(0x4f6226 in calls)
                    assert result.sound_cursor==(state.path.cursor if result.point.play_route_sound else 0)
                    for a,b in zip(result.sound_position,native.sound_position):assert abs(a-b)/max(1,abs(b))<2e-5
                    checks+=1;media+=len(calls)
        finally:lib.bk_route_destroy(route)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=checks,bounds_rejections=failures,media_commands=media,max_normalized_error=worst,native_functions=['0x4fc97f..0x4fca12','0x4fd5f1','0x50132f','0x50164b','0x4f5410','0x4adbb9'],hooks=['GetTickCount','0x46435e audio playback boundary','0x4f6226 route audio selection boundary'],scope='Actual post-AI route substep ordering, including native action selection, wait clock, interpolation, run countdown, last-point action and snap; recurrent state over 53 real route files. AI/head/collision/later actor stages excluded.',x87_control_word='0x037f')
    (ROOT/'local/original-npc-route-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'NPC route substeps,',failures,'bounds rejections; max error',worst)
if __name__=='__main__':main()
