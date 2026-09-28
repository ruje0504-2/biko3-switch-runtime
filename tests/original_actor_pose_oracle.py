"""Validate original 0x4072d9 sparse preprocessing and animated actor frames.

Uses actual six actor models; native SRT, matrix and transition functions are
unhooked. Does not claim mesh skinning or live game-state initialization.
"""
import argparse,ctypes as C,hashlib,json,random,struct,sys
from pathlib import Path
from original_animation_oracle import Native
from animation_binding import library
from model_binding import ROOT,decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();arc=Archive(args.data/'bk3_01.pp');records=[];rng=random.Random(350)
    heads=['qqq21_atama','atama','kubiX','kubiX','atama','atama']
    for index in range(6):
        name=f'h0{index}_80.x';data=arc.read(next(e for e in arc.entries if e.name==name))
        rc,model,error=decode(lib,data);assert rc==1,error
        err=C.create_string_buffer(256);a=lib.bk_model_animation_create(model,err);assert a,(name,err.value)
        try:
            m=model.contents;native.bind(data,m);end=lib.bk_model_animation_duration(a)
            out=(C.c_float*(m.frame_count*16))();worst=0;matrices=0;poses=0
            head=next(i for i in range(m.frame_count) if m.frames[i].name.decode()==heads[index])
            def compare(expected,context):
                nonlocal worst,matrices,poses
                for i,(v,w) in enumerate(zip(out,expected)):
                    error=abs(v-w)/max(1,abs(w));worst=max(worst,error)
                    assert error<=3e-5,(name,context,i,v,w,error)
                matrices+=m.frame_count;poses+=1
            # Include every unique original key and each interval midpoint in
            # the head's ancestry, plus diverse complete-body random seeks.
            ancestry=set();node=head
            while node!=0xffffffff:ancestry.add(node);node=m.frames[node].parent_index
            times={0,end,end+.5,end+1.5,end*2,10,89,90,99}
            for obj,frame,target,ts in native.tracks:
                if target in ancestry:
                    times.update(ts)
                    times.update((x+y)/2 for x,y in zip(ts,ts[1:]))
            times.update(rng.uniform(0,end*2.5) for _ in range(32));times=sorted(times)
            # Execute complete poses for an evenly spaced subset, then all
            # ancestry key boundaries using only those native tracks.
            all_tracks=native.tracks
            full_times=[times[i] for i in sorted({0,len(times)-1,*[int((len(times)-1)*i/31) for i in range(32)]})]
            for loop in [0,1]:
                for t in full_times:
                    t=C.c_float(t).value
                    assert lib.bk_model_animation_sample(a,t,loop,out,len(out),err),err.value
                    compare(native.sample(m,t,loop),('full',t,loop))
            for _ in range(24):
                first=C.c_float(rng.uniform(0,end*2)).value;last=C.c_float(rng.uniform(0,end*2)).value;weight=C.c_float(rng.uniform(0,1)).value
                assert lib.bk_model_animation_blend(a,first,last,weight,1,out,len(out),err),err.value
                compare(native.sample(m,first,1,(last,weight)),('blend',first,last,weight))
            head_checks=0
            native.tracks=[t for t in all_tracks if t[2] in ancestry]
            for loop in [0,1]:
                for t in times:
                    t=C.c_float(t).value
                    assert lib.bk_model_animation_sample(a,t,loop,out,len(out),err),err.value
                    expected=native.sample(m,t,loop)
                    for i in ancestry:
                        for j in range(16):
                            v=out[i*16+j];w=expected[i*16+j];error=abs(v-w)/max(1,abs(w));worst=max(worst,error)
                            assert error<=3e-5,(name,'head ancestry',t,loop,i,j,v,w,error)
                    head_checks+=1
            records.append({'entry':name,'sha256':hashlib.sha256(data).hexdigest(),'frames':m.frame_count,'tracks':len(all_tracks),'keys':sum(len(t[3]) for t in all_tracks),'head_node':heads[index],'head_ancestry_frames':len(ancestry),'full_poses':poses,'full_pose_matrices':matrices,'head_ancestry_poses':head_checks,'max_normalized_error':worst})
            print(name,poses,'full poses',head_checks,'head poses','PASS',worst,flush=True)
        finally:lib.bk_model_animation_destroy(a);lib.bk_model_destroy(model)
    report={'passed':True,'exe_sha256':hashlib.sha256(exe).hexdigest(),'scope':'Six actual actor ANIMs, sparse preprocessing, non-unit quaternions, full poses and head ancestry; no skinning/actor initialization or gameplay dispatch. Native mathematical functions unhooked.','native_functions':['0x4072d9','0x406c6b','0x408927','0x407d71','0x522d9a'],'full_poses':sum(r['full_poses'] for r in records),'full_pose_matrices':sum(r['full_pose_matrices'] for r in records),'head_ancestry_poses':sum(r['head_ancestry_poses'] for r in records),'max_normalized_error':max(r['max_normalized_error'] for r in records),'records':records}
    (ROOT/'local/original-actor-pose-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['full_poses'],report['full_pose_matrices'],report['head_ancestry_poses'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
