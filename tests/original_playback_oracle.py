"""End-to-end CPU playback vs original XAN, SRT and world frame traversal."""
import argparse,ctypes as C,hashlib,json,math,random,struct,sys
from pathlib import Path
from original_clip_oracle import Native as NativeClip
from original_placed_pose_oracle import Native as NativePose,I
from playback_binding import library
from clip_binding import State
from animation_binding import RootTransform
from model_binding import ROOT,decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native_clip=NativeClip(exe);native_pose=NativePose(exe);lib=library();rng=random.Random(353)
    arc=Archive(args.data/'bk3_01.pp');records=[]
    for actor in range(6):
        name=f'h0{actor}_80';xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'))
        data=arc.read(next(e for e in arc.entries if e.name==name+'.x'))
        rc,model,message=decode(lib,data);assert rc==1,message
        err=C.create_string_buffer(256);clips=lib.bk_clip_set_decode(xan,len(xan),err);assert clips,err.value
        player=lib.bk_model_playback_create(model,clips,1,err);assert player,err.value
        try:
            m=model.contents;native_clip.bind(xan,True);native_pose.bind(data,m)
            root=RootTransform(native_pose.root,(C.c_float*16)(*I));state=State();worst=0;checks=0;rejections=0
            desired=0 if actor==0 else 1
            if actor==0:
                native_clip.select(desired,1);assert lib.bk_model_playback_select(player,desired,1,err),err.value
            else:
                native_clip.select(desired,2);assert lib.bk_model_playback_request(player,desired,err),err.value
            def snapshot():
                assert lib.bk_model_playback_state(player,C.byref(state))
                pose=lib.bk_model_playback_frame(player,0)
                return bytes(state),C.string_at(pose,m.frame_count*64)
            steps=[0,1/120,1/120,.1,.5,1,10,100]*5
            for case,seconds in enumerate(steps):
                desired=4 if case==10 else 0 if case==20 else desired
                native_clip.select(desired,2);assert lib.bk_model_playback_request(player,desired,err),err.value
                before=snapshot()
                for invalid in [-1,math.nan,math.inf,1e30]:
                    assert not lib.bk_model_playback_advance(player,invalid,C.byref(root),err)
                    assert before==snapshot();rejections+=1
                bad_root=RootTransform(0xffffffff,root.world)
                assert not lib.bk_model_playback_advance(player,seconds,C.byref(bad_root),err)
                assert before==snapshot();rejections+=1
                root.world[12:15]=[rng.uniform(-200,200) for _ in range(3)]
                native_pose.root_world=list(root.world)
                seconds=C.c_float(seconds).value;sample=native_clip.step(seconds)
                assert lib.bk_model_playback_advance(player,seconds,C.byref(root),err),err.value
                want=native_pose.sample(m,sample[1],1,(sample[2],sample[3]) if sample[0] else None)
                actual=lib.bk_model_playback_frame(player,0)
                assert lib.bk_model_playback_state(player,C.byref(state))
                values=[getattr(state,n) for n,_ in State._fields_]
                for j,(v,w) in enumerate(zip(values+actual[:m.frame_count*16],native_clip.state()+want)):
                    e=abs(v-w)/max(1,abs(w));worst=max(worst,e)
                    assert e<=3e-5,(name,case,j,v,w,e)
                assert not lib.bk_model_playback_frame(player,m.frame_count)
                checks+=1
            records.append(dict(entry=name,poses=checks,matrices=checks*m.frame_count,
                                transactional_rejections=rejections,max_normalized_error=worst))
            print(name,checks,'PASS',worst,flush=True)
        finally:lib.bk_model_playback_destroy(player);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,
                poses=sum(r['poses'] for r in records),matrices=sum(r['matrices'] for r in records),
                transactional_rejections=sum(r['transactional_rejections'] for r in records),
                max_normalized_error=max(r['max_normalized_error'] for r in records),
                scope='Combined authored XAN/request/SRT/root placement and original matrix-stack traversal. No gameplay dispatch or GPU skinning.')
    (ROOT/'local/original-playback-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['poses'],report['matrices'],report['transactional_rejections'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
