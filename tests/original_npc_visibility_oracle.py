"""Native NPC visibility, animation and selective frame publication."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from original_actor_phase_oracle import Native, bind as bind_actor, Archive
from playback_binding import library
from clip_binding import State as ClipState
from model_binding import ROOT, decode

class Input(C.Structure):
    _fields_=[('hidden',C.c_uint8),('interface_mode',C.c_int8),('phase',C.c_int8),('area',C.c_int32),('behavior',C.c_int32)]
class Plan(C.Structure):
    _fields_=[('root_hidden',C.c_uint32),('marks',C.c_uint32*4),('override_marks',C.c_int)]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native,lib,rng=Native(exe),library(),random.Random(0x4fc37f);bind_actor(lib)
    lib.bk_npc_visibility_plan.argtypes=[C.POINTER(Plan),C.POINTER(Input)];lib.bk_npc_visibility_plan.restype=C.c_int
    lib.bk_npc_visibility_apply.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.POINTER(Input),C.c_void_p];lib.bk_npc_visibility_apply.restype=C.c_int
    lib.bk_actor_pose_hidden.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)];lib.bk_actor_pose_hidden.restype=C.c_int
    arc=Archive(args.data/'bk3_01.pp');error=C.create_string_buffer(256);cases=matrices=held=rejects=0;worst=0;records=[]
    def equal(actual,expected,label=''):
        nonlocal worst
        for a,b in zip(actual,expected):
            delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta)
            assert math.isfinite(delta) and delta<3e-5,(name,case,label,a,b,delta)
    for group,head_name in enumerate([b'atama',b'kubiX',b'kubiX',b'atama',b'atama']):
        name=f'h0{group+1}_80';raw=arc.read(next(e for e in arc.entries if e.name==name+'.x'));xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'))
        ok,model,message=decode(lib,raw);assert ok==1,message
        clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value;actor=None
        try:
            native.bind(raw,model.contents);head=C.c_uint32();assert lib.bk_model_find_frame(model,head_name,C.byref(head),error)
            marks=(C.c_uint32*4)()
            for i,node in enumerate([b'mark_02',b'mark_01',b'mark_00',b'mark_03']):
                index=C.c_uint32();assert lib.bk_model_find_frame(model,node,C.byref(index),error);marks[i]=index.value
            native.bind_clip(xan,head.value,1,False)
            origin=(C.c_float*3)(10,0,20);native.place_actor(origin,90)
            actor=lib.bk_actor_pose_create(model,clips,native.root,head_name,origin,90,1,0,error);assert actor,error.value
            for i,m in enumerate(marks):native.word(native.actor+0x860+i*4,native.frames+m*0x400)
            # A separate optional accessory root receives the same raw hide
            # byte before marker overrides on the primary actor.
            accessory,meta,node,child,link=0x3007000,0x3008000,0x3009000,0x300a000,0x300b000
            native.uc.mem_write(node,bytes(0x300));native.uc.mem_write(child,bytes(0x300));native.uc.mem_write(link,bytes(12))
            native.word(accessory+0x160,meta);native.word(meta+0x14,node);native.word(node+0x230,link);native.word(node+0x238,1);native.word(link,child)
            for case in range(180):
                inp=Input(rng.choice([0,0,0,1,2,255]),rng.choice([0,1,2,2,2,127,-128]),rng.choice([-1,0,1,1,1,2,3,4]),rng.choice([-1,0,7,8,9]),rng.choice([-1,0,1,2,3,4,5]))
                native.word(native.actor+4,accessory if case%2 else 0)
                native.uc.mem_write(native.actor+0x328,bytes([inp.hidden]));native.word(native.actor+0x850,inp.behavior&0xffffffff)
                native.uc.mem_write(0xbeeb84,bytes([inp.interface_mode&255]));native.uc.mem_write(0x71ba88,bytes([inp.phase&255]));native.word(0x7219ac,inp.area&0xffffffff)
                native.word(native.stack+8,native.actor);native.fragment(0x4fc37f,0x4fc6a3)
                assert lib.bk_npc_visibility_apply(actor,native.root,marks,C.byref(inp),error),error.value
                plan=Plan();assert lib.bk_npc_visibility_plan(C.byref(plan),C.byref(inp))
                if case%2:
                    assert struct.unpack('<I',native.uc.mem_read(node+0x70,4))[0]==plan.root_hidden
                    assert struct.unpack('<I',native.uc.mem_read(child+0x70,4))[0]==plan.root_hidden
                for frame in range(model.contents.frame_count):
                    value=C.c_uint32();assert lib.bk_actor_pose_hidden(actor,frame,C.byref(value))
                    assert value.value==struct.unpack('<I',native.uc.mem_read(native.frames+frame*0x400+0x70,4))[0],(name,case,frame)
                if case%20==0:
                    old=[C.c_uint32() for _ in range(model.contents.frame_count)]
                    for f,v in enumerate(old):assert lib.bk_actor_pose_hidden(actor,f,C.byref(v))
                    bad=(C.c_uint32*4)(*marks);bad[3]=0xffffffff
                    forced=Input(inp.hidden,0,0,8,0)
                    assert not lib.bk_npc_visibility_apply(actor,native.root,bad,C.byref(forced),error)
                    for f,v in enumerate(old):
                        now=C.c_uint32();assert lib.bk_actor_pose_hidden(actor,f,C.byref(now));assert now.value==v.value
                    rejects+=1
                cached=tuple(lib.bk_actor_pose_head(actor)[:3]);origin=(C.c_float*3)(10+case*.25,case%3,20-case*.5);yaw=C.c_float(90+case*.3).value
                slot=1 if case%20<10 else 4;seconds=C.c_float([0,1/60,.1,.5][case%4]).value
                assert lib.bk_actor_pose_step(actor,origin,yaw,slot,C.c_float(seconds*.5).value,error),error.value
                native.step_actor(origin,yaw,slot,seconds)
                clip=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(clip))
                equal([getattr(clip,n) for n,_ in ClipState._fields_],native.state(),('clip',[(n,getattr(clip,n)) for n,_ in ClipState._fields_],native.state()))
                native.publish();lib.bk_actor_pose_publish(actor)
                for frame in range(model.contents.frame_count):
                    equal(lib.bk_actor_pose_frame(actor,frame)[:16],native.floats(native.frames+frame*0x400+0xc0,16));matrices+=1
                    equal(lib.bk_actor_pose_local(actor,frame)[:16],native.floats(native.frames+frame*0x400+0x80,16),('local',frame,model.contents.frames[frame].name,bytes(inp).hex(),native.state()));matrices+=1
                equal(lib.bk_actor_pose_head(actor)[:3],native.floats(native.node+0xf0,3))
                if inp.hidden:
                    assert tuple(lib.bk_actor_pose_head(actor)[:3])==cached;held+=1
                cases+=1
            records.append(dict(file=name,frames=model.contents.frame_count,sha256=hashlib.sha256(raw).hexdigest()));print(name,'visibility/pose PASS',flush=True)
        finally:lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=cases,matrices=matrices,hidden_head_holds=held,atomic_rejections=rejects,max_normalized_error=worst,records=records,native_functions=['0x4fc37f..0x4fc6a3','0x423a99','0x4fc6c7..0x4fc6fd','0x42273b'],scope='Original NPC/root/accessory and marker visibility policy, actual actor animation, hidden-node own-world update and descendant cache retention. Optional accessory checked for visibility only; frame fixtures have no rendered meshes. No GPU drawing, accessory animation or full NPC update implied.',x87_control_word='0x037f')
    (ROOT/'local/original-npc-visibility-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'visibility steps;',matrices,'matrices;',held,'hidden head holds')

if __name__=='__main__':main()
