"""One native VM: actor XAN/SRT, root placement, gaze and cached traversal phases."""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from pathlib import Path
from original_actor_phase_oracle import Native, bind as actor_bind
from original_eye_assets_oracle import bind as eye_bind, Config, Archive
from playback_binding import library
from model_binding import ROOT, decode

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();actor_bind(lib);eye_bind(lib);error=C.create_string_buffer(256)
    fp=C.POINTER(C.c_float)
    lib.bk_actor_pose_parent_world.argtypes=[C.c_void_p,C.c_uint32];lib.bk_actor_pose_parent_world.restype=fp
    lib.bk_eye_assets_gaze.argtypes=[C.c_void_p,C.c_void_p,C.c_int8,fp,C.c_float,C.c_float,C.c_void_p];lib.bk_eye_assets_gaze.restype=C.c_int
    lib.bk_eye_assets_gaze_variant.argtypes=[C.c_void_p];lib.bk_eye_assets_gaze_variant.restype=C.c_uint32
    lib.bk_actor_pose_visibility.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_actor_pose_visibility.restype=C.c_int
    ar=Archive(a.data/'bk3_01.pp');entries={e.name.encode():e for e in ar.entries}
    store=lib.bk_resources_create(error);assert store
    assert lib.bk_resources_mount(store,b'bk3_01',str(a.data/'bk3_01.pp').encode(),error)
    records=[];matrices=0;steps=0;worst=0;rejects=0
    try:
        for path in sorted(a.data.glob('*.fam')):
            raw=path.read_bytes();c=Config();assert lib.bk_face_config_decode(raw,len(raw),C.byref(c),error)
            if c.actor_clip not in entries:continue
            xan=ar.read(entries[c.actor_clip]);name=xan[:256].split(b'\0')[0];data=ar.read(entries[name]);ok,m,msg=decode(lib,data);assert ok,msg
            eyes=lib.bk_eye_assets_create(store,b'bk3_01',m,name,C.byref(c),error);assert eyes,error.value
            actor=None;clips=None
            try:
                binding=lib.bk_eye_assets_binding(eyes).contents
                if 0xffffffff in binding.frames:continue
                clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value
                n.bind(data,m.contents);n.bind_clip(xan,binding.frames[0],0,True)
                n.uc.mem_write(0xbf3b10,bytes(0x2c));n.word(0xbf3b38,binding.mode)
                for i,index in enumerate(binding.frames):n.word(0xbf3b28+i*4,n.frames+index*0x400)
                origin=(C.c_float*3)(-16,0,3);yaw=C.c_float(220).value;n.place_actor(origin,yaw)
                # Named-head getter is irrelevant here; bind an existing eye
                # descendant to avoid inventing per-costume gameplay heads.
                node=m.contents.frames[binding.frames[0]].name.split(b' ',1)[-1]
                actor=lib.bk_actor_pose_create(m,clips,n.root,node,origin,yaw,0,1,error);assert actor,(path.name,error.value)
                def compare(label):
                    nonlocal matrices,worst
                    for frame in range(m.contents.frame_count):
                        for fn,off in [('bk_actor_pose_local',0x80),('bk_actor_pose_frame',0xc0),('bk_actor_pose_parent_world',0x100)]:
                            actual=getattr(lib,fn)(actor,frame)[:16];expected=n.floats(n.frames+frame*0x400+off,16)
                            for j,(v,w) in enumerate(zip(actual,expected)):
                                d=abs(v-w)/max(1,abs(w));worst=max(worst,d)
                                assert math.isfinite(d) and d<3e-5,(path.name,label,frame,fn,j,v,w,d)
                            matrices+=1
                compare('created')
                for step in range(24):
                    origin=(C.c_float*3)(-16+step*.05,0,3+step*.075);yaw=C.c_float(220+step*.12).value
                    hidden=1 if step%8 in [4,5] else 0
                    # Same subtree assignment used by actor visibility; root
                    # pausing must not be approximated by a zero time step.
                    n.call(0x423a99,struct.pack('<II',n.frames+n.root*0x400,hidden))
                    edits=(C.c_uint32*2)(n.root,hidden);assert lib.bk_actor_pose_visibility(actor,edits,1,error)
                    dt=C.c_float(0 if step%3==0 else 1/30).value
                    n.step_actor(origin,yaw,0,dt)
                    assert lib.bk_actor_pose_step(actor,origin,yaw,0,C.c_float(dt*.5).value,error),error.value
                    compare(('animation',step))
                    old=[tuple(lib.bk_actor_pose_local(actor,f)[:16]) for f in range(m.contents.frame_count)]
                    target=[1,0,0,0,0,1,0,0,0,0,1,0,-40+step*3,35+step,60,1]
                    command=[0,1,2,3,-1,0,1,2][step%8]
                    n.vector(n.rendered+0xc0,target)
                    n.call(0x4f3bb9,struct.pack('<Iff',command&255,.25,.4))
                    assert lib.bk_eye_assets_gaze(eyes,actor,command,(C.c_float*16)(*target),.25,.4,error),error.value
                    compare(('gaze',step))
                    assert lib.bk_eye_assets_gaze_variant(eyes)==struct.unpack('<I',n.uc.mem_read(0xbf3b34,4))[0]
                    assert lib.bk_eye_assets_selected(eyes)==0
                    if command in [3,-1]:assert old==[tuple(lib.bk_actor_pose_local(actor,f)[:16]) for f in range(m.contents.frame_count)]
                    if step%4!=1:
                        n.publish();lib.bk_actor_pose_publish(actor);compare(('publish',step))
                    # Failing gaze leaves its variant, locals and all published
                    # caches unchanged, including if the first eye was valid.
                    before=[tuple(getattr(lib,fn)(actor,f)[:16]) for fn in ['bk_actor_pose_local','bk_actor_pose_frame','bk_actor_pose_parent_world'] for f in range(m.contents.frame_count)]
                    variant=lib.bk_eye_assets_gaze_variant(eyes)
                    assert not lib.bk_eye_assets_gaze(eyes,actor,1,(C.c_float*16)(*target),float('nan'),.4,error)
                    assert variant==lib.bk_eye_assets_gaze_variant(eyes)
                    assert before==[tuple(getattr(lib,fn)(actor,f)[:16]) for fn in ['bk_actor_pose_local','bk_actor_pose_frame','bk_actor_pose_parent_world'] for f in range(m.contents.frame_count)]
                    rejects+=1;steps+=1
                records.append(dict(config=path.name,model=name.decode(),model_sha256=hashlib.sha256(data).hexdigest(),xan_sha256=hashlib.sha256(xan).hexdigest(),frames=m.contents.frame_count,eyes=list(binding.frames),steps=24))
                print(path.name,'PASS',flush=True)
            finally:
                lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_eye_assets_destroy(eyes);lib.bk_model_destroy(m)
    finally:lib.bk_resources_destroy(store)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=len(records),steps=steps,matrices=matrices,atomic_rejections=rejects,max_normalized_error=worst,records=records,native_functions=['0x4fc6c7..0x4fc6fd','0x401b0a','0x4026fe','ANIM SRT','0x4f3bb9','0x4a0823','0x423a99','0x42273b'],scope='Same VM executes unmodified authored actor XAN and original ANIM, native gaze dispatch and full matrix-stack traversal. Submitted local, published world and cached parent matrices compared throughout hidden pauses, skipped traversals and gaze edits. Explicit camera/placement/command fixtures; complete game-loop call-site conditions not recovered.')
    (ROOT/'local/original-eye-phase-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',steps,'eye/actor steps;',matrices,'matrices; max error',worst)
if __name__=='__main__':main()
