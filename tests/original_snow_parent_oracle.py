"""Camera-parented real snow: full original4fab7b and native42273b traversal.
Seven snow entries in both quality packs, held camera-parent caches, skipped
publication, root-local/world setters and invalid-parent atomic rejection.
Explicit moving camera matrices; not central game camera selection or GPU.
"""
import argparse, ctypes as C, hashlib, json, math, struct
from pathlib import Path
from original_actor_phase_oracle import bind
from original_prop_presentation_oracle import VM
from original_background_config_oracle import Config
from original_background_oracle import State, Input, Commands
from original_placed_pose_oracle import I
from original_placement_oracle import Placement
from clip_binding import State as ClipState
from model_binding import ROOT, Model
from playback_binding import library
from bk3_assets import Archive

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path)
    args=ap.parse_args();exe=args.exe.read_bytes();lib=library();bind(lib)
    for name, argtypes, result in [
        ('bk_resources_create',[C.c_void_p],C.c_void_p),
        ('bk_resources_destroy',[C.c_void_p],None),
        ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
        ('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
        ('bk_background_assets_create',[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint,C.c_int,C.c_void_p],C.c_void_p),
        ('bk_background_assets_destroy',[C.c_void_p],None),
        ('bk_background_assets_pose',[C.c_void_p,C.c_uint],C.c_void_p),
        ('bk_background_assets_config',[C.c_void_p],C.POINTER(Config)),
        ('bk_background_assets_step',[C.c_void_p,C.POINTER(State),C.POINTER(C.c_uint32),C.POINTER(Input),C.POINTER(Commands),C.c_void_p,C.c_void_p,C.c_void_p],C.c_int),
        ('bk_background_assets_publish_camera',[C.c_void_p,C.POINTER(C.c_float),C.c_void_p],C.c_int),
        ('bk_actor_pose_parent_world',[C.c_void_p,C.c_uint32],C.POINTER(C.c_float)),
        ('bk_actor_pose_place_exact',[C.c_void_p,C.POINTER(Placement),C.c_void_p],C.c_int),
        ('bk_actor_pose_model',[C.c_void_p],C.POINTER(Model))]:
        f=getattr(lib,name);f.argtypes=argtypes;f.restype=result
    error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store
    archives={p:Archive(args.data/(p+'.pp')) for p in ['bk3_03','bk3_17','bk3_20']}
    def read(pack,name):
        arc=archives[pack];return arc.read(next(e for e in arc.entries if e.name==name))
    vms=[VM(exe) for _ in range(3)];matrices=frames=profiles=0;worst=0;records=[]
    try:
        for pack in archives:assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
        assert lib.bk_resources_mount_directory(store,b'collision',str(args.data).encode(),4293124,error),error.value
        seen=set()
        for quality in [0,1]:
            pack='bk3_03' if quality else 'bk3_17'
            for g in range(1):
                for area in range(7):
                    a=lib.bk_background_assets_create(store,g,area,quality,1,error);assert a,(g,area,error.value)
                    try:
                        config=lib.bk_background_assets_config(a).contents
                        key=(pack,config.clip,g==2 and area==8,g==0 and area<=6)
                        
                        seen.add(key);objects=[]
                        for index in range(3):
                            pose=lib.bk_background_assets_pose(a,index)
                            if not pose:continue
                            source_pack='bk3_20' if index==2 else pack
                            filename=[config.clip.decode(),'m03_04_door.xan','yuki.xan'][index]
                            xan=read(source_pack,filename);modelname=xan[:256].split(b'\0')[0].decode();raw=read(source_pack,modelname)
                            model=lib.bk_actor_pose_model(pose).contents;n=vms[index];n.bind(raw,model)
                            n.uc.mem_write(n.rendered,bytes(0x19000));n.uc.mem_write(n.clip,xan[512:]);n.word(n.clip+0x18c,0);n.word(n.clip+0x160,n.model);n.word(n.model+0x14,n.frames+n.root*0x400)
                            n.word(n.model+0x148,n.group);n.vector(n.group+0x74,[0]);n.word(n.group+0x78,len(n.tracks));n.word(n.group+0x7c,n.track_array)
                            for i,(obj,_,f,_) in enumerate(n.tracks):n.word(n.track_array+i*4,obj);n.word(obj+0x74,n.frames+f*0x400)
                            if index==0:n.call(0x401d24,struct.pack('<II',n.clip,0))
                            if index==2:n.call(0x40168c,struct.pack('<II',n.clip,0))
                            n.publish();objects.append((index,pose,model,n))
                        def equal(actual,wanted,label):
                            nonlocal worst
                            for x,y in zip(actual,wanted):
                                if x==y:continue
                                d=abs(x-y)/max(1,abs(y));worst=max(worst,d)
                                assert math.isfinite(d) and d<3e-6,(pack,config.clip,label,x,y,d)
                        def check():
                            nonlocal matrices
                            for index,pose,model,n in objects:
                                st=ClipState();assert lib.bk_actor_pose_state(pose,C.byref(st));equal([getattr(st,k) for k,_ in st._fields_],n.state(),('timeline',index))
                                for f in range(model.frame_count):
                                    equal(lib.bk_actor_pose_local(pose,f)[:16],n.floats(n.frames+f*0x400+0x80,16),('local',index,f))
                                    equal(lib.bk_actor_pose_frame(pose,f)[:16],n.floats(n.frames+f*0x400+0xc0,16),('world',index,f));equal(lib.bk_actor_pose_parent_world(pose,f)[:16],n.floats(n.frames+f*0x400+0x100,16),('parent',index,f));matrices+=3
                        check();state=State();state.music_volume=-6000;random=C.c_uint32(73)
                        for frame in range(60):
                            inp=Input();inp.seconds=[0,1/60,.1,.25,1,.5][frame%6];inp.player[:]=[84 if frame%18<9 else 200,0,0];inp.npc[:]=[200,0,0];inp.weather_enabled=frame%13 not in [3,4]
                            out=Commands();assert lib.bk_background_assets_step(a,C.byref(state),C.byref(random),C.byref(inp),C.byref(out),None,None,error),error.value
                            for index,pose,model,n in objects:
                                seconds=inp.seconds
                                if index==1:
                                    n.word(0xbf3bd0,n.clip);n.word(0x7219a8,g);n.word(0x7219ac,area);n.vector(0x71b7ac,inp.player);n.vector(0x729084,inp.npc)
                                    n.vector(0x733700,[seconds])
                                    n.call(0x4fa753,b'')
                                    continue
                                if index==2:
                                    n.word(0xb54730,inp.weather_enabled);n.word(0xb53430,n.clip);n.vector(0x733700,[seconds])
                                    n.call(0x4fab7b,b'');continue
                                n.call(0x4026fe,struct.pack('<If',n.clip,seconds))
                            check()
                            # The camera changes AFTER snow's local setter. Root/children
                            # remain old until traversal refreshes their parent caches.
                            angle=frame*.031+area*.08;pitch=math.sin(frame*.07)*.2
                            c=math.cos(angle);s=math.sin(angle);cp=math.cos(pitch);sp=math.sin(pitch)
                            camera=(C.c_float*16)(c,0,-s,0,s*sp,cp,c*sp,0,s*cp,-sp,c*cp,0,frame*1.3,20+area,frame*-.7,1)
                            if frame%7!=3:
                                assert lib.bk_background_assets_publish_camera(a,camera,error),error.value
                                for index,_,_,n in objects:
                                    if index!=2:n.publish();continue
                                    n.word(n.matrix_stack+12,0);n.vector(n.matrices,list(camera))
                                    n.call(0x42273b,struct.pack('<I',n.frames+n.root*0x400))
                                check()
                            if frame%9==5:
                                # World setter uses the cached parent inverse, while
                                # advance alone must retain the resulting root local.
                                for index,pose,model,n in objects:
                                    if index!=2:continue
                                    place=Placement();place.world[:]=I;place.world[12:15]=[11+frame*.13,-3,23]
                                    place.position[:]=[12,34,56];place.yaw_degrees=17
                                    assert lib.bk_actor_pose_place_exact(pose,C.byref(place),error),error.value
                                    n.vector(n.argument,list(place.world));n.call(0x42407a,struct.pack('<II',n.frames+n.root*0x400,n.argument))
                                check()
                            if frame%11==0:
                                before=[(list(lib.bk_actor_pose_frame(pose,f)[:16]),list(lib.bk_actor_pose_parent_world(pose,f)[:16])) for _,pose,model,_ in objects for f in range(model.frame_count)]
                                bad=(C.c_float*16)(*camera);bad[0]=float('nan')
                                assert not lib.bk_background_assets_publish_camera(a,bad,error)
                                assert before==[(list(lib.bk_actor_pose_frame(pose,f)[:16]),list(lib.bk_actor_pose_parent_world(pose,f)[:16])) for _,pose,model,_ in objects for f in range(model.frame_count)]
                            frames+=1
                        profiles+=1;records.append(dict(pack=pack,clip=config.clip.decode(),objects=len(objects)))
                        print(pack,config.clip.decode(),'PASS',flush=True)
                    finally:lib.bk_background_assets_destroy(a)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=profiles,frames=frames,matrices=matrices,max_relative_error=worst,records=records,scope=__doc__)
    (ROOT/'local/original-snow-parent-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
