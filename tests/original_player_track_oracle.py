"""Player TRACK4ba2f2 with real XAN/SRT/cache publication and native setters."""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import struct
from original_follow_phase_oracle import Native as FollowNative,bind_follow
from original_player_view_oracle import State as View,Input,Effects
from original_aim_oracle import Pose,I
from playback_binding import library
from clip_binding import State as ClipState
from model_binding import ROOT,decode
from bk3_assets import Archive

class Native(FollowNative):
    controller,actor=0x71af38,0x71b510
    def step_player(self,inp):
        self.vector(self.actor+0x29c,inp.position);self.vector(0x733700,[inp.seconds])
        self.uc.mem_write(self.actor+0x579,b'\3')
        cached=self.floats(self.node+0xc0,16)
        self.call(0x4ba2f2,struct.pack('<2I',self.controller,self.actor))
        assert cached==self.floats(self.node+0xc0,16)
        return self.floats(self.rendered+0xc0,16)+self.floats(self.controller+0x420,3),self.uc.mem_read(self.actor+0x579,1)[0]==0

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();lib=library();bind_follow(lib);arc=Archive(args.data/'bk3_04.pp');error=C.create_string_buffer(256)
    lib.bk_follow_camera_player_view.argtypes=[C.c_void_p,C.POINTER(View),C.c_int,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
    records=[];worst=0;held=0;completed=0
    def equal(a,b,key):
        nonlocal worst
        for a,b in zip(a,b):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
            assert d<3e-5,(name,case,key,a,b,d)
    for name in ['cam00_01','cam00_02']:
        native=Native(exe)
        xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'));data=arc.read(next(e for e in arc.entries if e.name==name+'.x'))
        ok,model,msg=decode(lib,data);assert ok,msg
        clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value
        camera=None
        try:
            native.bind(data,model.contents);node=C.c_uint32();assert lib.bk_model_find_frame(model,b'Cam_AUTO',C.byref(node),error)
            native.bind_clip(xan,node.value)
            initial=I.copy();initial[13]=20
            pose=Pose((C.c_float*16)(*initial),(C.c_float*3)(0,20,0))
            native.vector(native.controller+0x4a4,initial)
            camera=lib.bk_follow_camera_create(model,clips,native.root,node.value,C.byref(pose),error);assert camera,error.value
            view=View();view.matrix[:]=initial;view.pose=pose;view.distance=40;view.target_distance=40;view.lean=10
            for case in range(720):
                inp=Input(position=(C.c_float*3)(math.sin(case*.03)*30,math.sin(case*.1)*2,math.cos(case*.04)*40),seconds=[.016,.1,.5,0,1][case%5])
                cache=list(lib.bk_follow_camera_track(camera)[:16]);before=bytes(lib.bk_follow_camera_pose(camera).contents)
                expected,reset=native.step_player(inp)
                effects=Effects()
                assert lib.bk_follow_camera_player_view(camera,C.byref(view),6,C.byref(inp),C.byref(effects),error),error.value
                equal(list(view.pose.world)+list(view.pose.position),expected,'pose')
                assert effects.reset_mode==reset;completed+=reset
                assert list(view.matrix)==initial # TRACK never snapshots +4a4.
                assert list(lib.bk_follow_camera_track(camera)[:16])==cache
                clip=ClipState();assert lib.bk_follow_camera_clip_state(camera,C.byref(clip))
                equal([getattr(clip,n) for n,_ in ClipState._fields_],native.state(),'timeline')
                if case%11!=3:
                    native.publish();lib.bk_follow_camera_publish(camera)
                    equal(lib.bk_follow_camera_track(camera)[:16],native.floats(native.node+0xc0,16),'published')
                else:held+=1
            records.append(dict(name=name,frames=720,xan_sha256=hashlib.sha256(xan).hexdigest(),model_sha256=hashlib.sha256(data).hexdigest()));print(name,'PASS',flush=True)
        finally:
            lib.bk_follow_camera_destroy(camera);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,frames=1440,held_publications=held,completion_frames=completed,max_relative_error=worst,hooks=['FOV42cf0e only; dormant inherited obstacle hooks not reached'],scope=__doc__)
    (ROOT/'local/original-player-track-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
