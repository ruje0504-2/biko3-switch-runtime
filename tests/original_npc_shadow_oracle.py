"""Actual kage_01 mesh shadow: native placement, hidden/full-speed clock and traversal."""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from original_actor_phase_oracle import Native, bind as actor_bind
from original_placement_oracle import Placement, bind as place_bind
from original_eye_assets_oracle import bind as resources_bind, Archive
from playback_binding import library
from clip_binding import State
from model_binding import ROOT, decode

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();actor_bind(lib);place_bind(lib);resources_bind(lib)
    for name,args,result in [
        ('bk_npc_shadow_create',[C.c_void_p,C.c_void_p],C.c_void_p),('bk_npc_shadow_destroy',[C.c_void_p],None),
        ('bk_npc_shadow_pose',[C.c_void_p],C.c_void_p),('bk_npc_shadow_place',[C.c_void_p,C.POINTER(Placement),C.c_void_p],C.c_int),
        ('bk_npc_shadow_step',[C.c_void_p,C.c_uint8,C.c_float,C.c_void_p],C.c_int),('bk_npc_shadow_publish',[C.c_void_p],None),
        ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],C.POINTER(C.c_float)),
        ('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)],C.c_int)]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result
    error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store
    assert not lib.bk_npc_shadow_create(store,error)
    assert lib.bk_resources_mount(store,b'bk3_01',str(a.data/'bk3_01.pp').encode(),error)
    shadow=lib.bk_npc_shadow_create(store,error);assert shadow,error.value
    # Resource archive may close while the owned CPU model/pose stays usable.
    lib.bk_resources_destroy(store);ar=Archive(a.data/'bk3_01.pp')
    data=ar.read(next(e for e in ar.entries if e.name=='kage_01.x'));xan=ar.read(next(e for e in ar.entries if e.name=='kage_01.xan'))
    ok,m,msg=decode(lib,data);assert ok,msg
    steps=matrices=rejects=0;worst=0
    try:
        n.bind(data,m.contents);assert not n.tracks
        n.bind_clip(xan,1,0,True);n.word(n.model+0x148,0);n.word(n.actor+4,n.clip)
        pose=lib.bk_npc_shadow_pose(shadow);assert pose and not lib.bk_actor_pose_head(pose)
        def compare(label):
            nonlocal matrices,worst
            for i in range(m.contents.frame_count):
                for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                    actual=getattr(lib,fn)(pose,i)[:16];expected=n.floats(n.frames+i*0x400+off,16)
                    for j,(v,w) in enumerate(zip(actual,expected)):
                        d=abs(v-w)/max(1,abs(w));worst=max(worst,d);assert d<3e-6,(label,i,fn,j,v,w)
                    matrices+=1
                hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(pose,i,C.byref(hidden))
                assert hidden.value==struct.unpack('<I',n.uc.mem_read(n.frames+i*0x400+0x70,4))[0]
            state=State();assert lib.bk_actor_pose_state(pose,C.byref(state))
            assert [getattr(state,k) for k,_ in State._fields_]==n.state(),(label,'clock')
        compare('created')
        for step in range(360):
            position=(C.c_float*3)(math.sin(step*.1)*50,(step%9-4)*.25,math.cos(step*.1)*50)
            yaw=C.c_float(step*2.9).value;body=Placement();assert lib.bk_actor_placement(C.byref(body),position,yaw)
            n.place_actor(position,yaw);n.word(n.actor+4,n.clip);n.word(n.stack+8,n.actor)
            n.fragment(0x4fcdd0,0x4fce25)
            assert lib.bk_npc_shadow_place(shadow,C.byref(body),error),error.value
            compare(('place',step))
            hidden=[0,0,1,255,0][step%5];seconds=C.c_float([0,1/60,.1,1,3][step%5]).value
            n.call(0x423a99,struct.pack('<II',n.frames+n.root*0x400,hidden))
            n.vector(0x733700,[seconds]);n.word(n.stack+8,n.actor);n.fragment(0x4fc773,0x4fc792)
            assert lib.bk_npc_shadow_step(shadow,hidden,seconds,error),error.value
            compare(('step',step))
            if step%4!=1:
                n.publish();lib.bk_npc_shadow_publish(shadow);compare(('publish',step))
            for bad in [-1,float('nan'),float('inf'),1e30]:
                assert not lib.bk_npc_shadow_step(shadow,not hidden,bad,error)
                compare(('rejected',step));rejects+=1
            steps+=1
    finally:lib.bk_npc_shadow_destroy(shadow);lib.bk_model_destroy(m)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),model_sha256=hashlib.sha256(data).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest(),steps=steps,matrices=matrices,rejected_steps=rejects,max_normalized_error=worst,native_functions=['0x4fcdd0..0x4fce25','0x4fc773..0x4fc792','0x423a99','0x4026fe','0x42273b'],scope='Graphics0/1 kage_01 only. Actual XAN with no SRT group, original Y+0.1 placement, full seconds versus hidden pause and actual D3DX traversal. Explicit body placement input; no graphics2 projected shadow, action/audio or full game frame.')
    (ROOT/'local/original-npc-shadow-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,'shadow steps;',matrices,'matrices;',rejects,'rejected steps; max error',worst)
if __name__=='__main__':main()
