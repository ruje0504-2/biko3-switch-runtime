"""Run original player request/time-scale/pose submission on its actual asset."""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from original_actor_phase_oracle import Native as ActorNative, bind
from playback_binding import library
from model_binding import ROOT, decode
from clip_binding import State
from bk3_assets import Archive
class Actions(C.Structure):
    _fields_=[('idle',C.c_int32),('walk',C.c_int32),('run',C.c_int32)]
class Native(ActorNative):
    def actions(self):
        self.word(self.stack+8,self.actor)
        self.fragment(0x4bf290,0x4bf2cd)
        return [struct.unpack('<i',self.uc.mem_read(self.actor+off,4))[0] for off in [0x14,0x18,0x20]]
    def step_player(self,position,yaw,action,seconds):
        self.place_actor(position,yaw);self.word(self.actor+0x10,action);self.vector(0x733700,[seconds])
        before=self.floats(self.node+0xc0,16)
        self.fragment(0x4bffd0,0x4c0078)
        assert before==self.floats(self.node+0xc0,16)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();bind(lib);arc=Archive(args.data/'bk3_01.pp');error=C.create_string_buffer(256)
    lib.bk_player_animation_actions.argtypes=[C.POINTER(Actions)];lib.bk_player_animation_actions.restype=C.c_int
    lib.bk_player_animation_step.argtypes=[C.c_void_p,C.POINTER(C.c_float),C.c_float,C.POINTER(Actions),C.c_int32,C.c_float,C.c_char_p];lib.bk_player_animation_step.restype=C.c_int
    data=arc.read(next(e for e in arc.entries if e.name=='h00_80.x'));xan=arc.read(next(e for e in arc.entries if e.name=='h00_80.xan'))
    rc,model,message=decode(lib,data);assert rc==1,message;clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value;actor=None
    matrices=rejects=steps=0;worst=0
    try:
        native.bind(data,model.contents);head=C.c_uint32();assert lib.bk_model_find_frame(model,b'qqq21_atama',C.byref(head),error)
        native.bind_clip(xan,head.value,0,True)
        actions=Actions();assert lib.bk_player_animation_actions(C.byref(actions));assert [actions.idle,actions.walk,actions.run]==native.actions()
        origin=(C.c_float*3)(-34,0,-38);yaw=C.c_float(283).value;native.place_actor(origin,yaw)
        actor=lib.bk_actor_pose_create(model,clips,native.root,b'qqq21_atama',origin,yaw,0,1,error);assert actor,error.value
        def equal(actual,expected,label):
            nonlocal worst
            for a,b in zip(actual,expected):
                diff=abs(a-b)/max(1,abs(b));worst=max(worst,diff)
                assert math.isfinite(diff) and diff<3e-5,(label,steps,a,b,diff)
        def check():
            nonlocal matrices
            for frame in range(model.contents.frame_count):
                equal(lib.bk_actor_pose_frame(actor,frame)[:16],native.floats(native.frames+frame*0x400+0xc0,16),('frame',frame));matrices+=1
            state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));equal([getattr(state,n) for n,_ in State._fields_],native.state(),'clip')
        check();native.publish();lib.bk_actor_pose_publish(actor);check()
        slots=[i for i in range(128) if struct.unpack_from('<2f',xan,512+0x190+i*156+0x54)!=(0,0)]
        for case in range(len(slots)*6):
            action=slots[case//6];seconds=C.c_float([0,1/60,.1,.25,2,15][case%6]).value
            origin=(C.c_float*3)(-34+case*.125,case%3,-38+case*.0625);yaw=C.c_float(283+case*.37).value
            before=bytes(lib.bk_actor_pose_placement(actor).contents);state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));saved=bytes(state)
            cached=lib.bk_actor_pose_head(actor)[:3]
            for bad in [-1,float('nan'),float('inf'),3.4e38]:
                assert not lib.bk_player_animation_step(actor,origin,yaw,C.byref(actions),slots[(case//6+1)%len(slots)],bad,error)
                assert before==bytes(lib.bk_actor_pose_placement(actor).contents) and cached==lib.bk_actor_pose_head(actor)[:3]
                assert lib.bk_actor_pose_state(actor,C.byref(state)) and bytes(state)==saved
                rejects+=1
            assert lib.bk_player_animation_step(actor,origin,yaw,C.byref(actions),action,seconds,error),error.value
            native.step_player(origin,yaw,action,seconds);check()
            if case%7!=2:
                native.publish();lib.bk_actor_pose_publish(actor);check()
            steps+=1
        report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),model_sha256=hashlib.sha256(data).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest(),slots=slots,steps=steps,matrices=matrices,rejected_steps=rejects,max_normalized_error=worst,native_functions=['0x4bf290..0x4bf2cd','0x4bffd0..0x4c0078','0x4018c8','0x401b0a','0x4026fe','ANIM SRT','0x42273b'],scope='Primary player request policy/time scaling on actual h00_80, with unhooked native scheduler/SRT/hierarchy and previous published head. Does not implement action/input/physics/material/audio decisions or secondary accessory update.',x87_control_word='0x037f')
        (ROOT/'local/original-player-animation-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,'player steps,',matrices,'matrices,',rejects,'atomic rejections; max error',worst,flush=True)
    finally:lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
if __name__=='__main__':main()
