"""Replay entry spatial binding with live actor clips and cached head matrices.

45 profiles share an explicit office collision fixture, not a claim that
these entries all belong in that background. Background dispatch is separate.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
from original_actor_phase_oracle import bind as bind_actor
from original_npc_spatial_oracle import Native, State, Input, Effects, Ai, Point, MotionState, PathState, Timer, Shared, Head, Vector, bind_collision, Archive
from original_entry_oracle import Request, Selection
from original_npc_head_oracle import Input as HeadInput
from clip_binding import State as ClipState
from model_binding import ROOT, library, decode

class Context(C.Structure):
    _fields_ = [('player_action',C.c_int32),('suppressed_actions',C.c_int32*6),('background_clip',C.c_int32),('short_range_action',C.c_int32),('player_direction',Vector),('interaction_df',C.c_int8),('interaction_e0',C.c_int8),('excluded_surface',C.c_char_p)]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();lib,native=library(),Native(exe);bind_actor(lib);bind_collision(lib)
    for name,arguments,result in [
        ('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),
        ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
        ('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
        ('bk_entry_assets_create',[C.c_void_p,C.POINTER(Request),C.c_void_p],C.c_void_p),('bk_entry_assets_destroy',[C.c_void_p],None),
        ('bk_entry_assets_actor',[C.c_void_p],C.c_void_p),('bk_entry_assets_player',[C.c_void_p],C.c_void_p),
        ('bk_entry_assets_selection',[C.c_void_p],C.POINTER(Selection)),
        ('bk_entry_assets_head_input',[C.c_void_p,C.c_float,C.POINTER(HeadInput),C.c_void_p],C.c_int),
        ('bk_entry_assets_step_npc_spatial',[C.c_void_p,C.POINTER(State),C.POINTER(Shared),C.POINTER(C.c_uint32),C.c_void_p,C.POINTER(Context),C.c_float,C.c_uint32,C.POINTER(Effects),C.c_void_p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=arguments;fn.restype=result
    error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store,error.value
    collision=model=None;cases=failures=0;worst=0
    def equal(a,b,label):
        nonlocal worst
        for x,y in zip(a,b):
            delta=abs(x-y)/max(1,abs(y));worst=max(worst,delta)
            assert math.isfinite(delta) and delta<3e-6,(label,x,y,delta)
    try:
        for pack in ['bk3_01','bk3_04']:
            assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
        assert lib.bk_resources_mount_directory(store,b'routes',str(args.data).encode(),20480,error),error.value
        assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,error),error.value
        archive=Archive(args.data/'bk3_03.pp');entry=next(e for e in archive.entries if e.name.lower()=='m01_04.x')
        ok,model,message=decode(lib,archive.read(entry));assert ok==1,message
        world=(C.c_float*(model.contents.frame_count*16))();assert lib.bk_model_world_matrices(model,world,len(world),error)
        atr=(args.data/'m01_04.atr').read_bytes();collision=lib.bk_collision_create(model,world,len(world),b'M01_04.X',atr,len(atr),error);assert collision,error.value
        native.create(model.contents,world,b'M01_04.X',atr)
        for group in range(5):
            for area in range(9):
                assets=lib.bk_entry_assets_create(store,C.byref(Request(group,area,0,8)),error);assert assets,error.value
                try:
                    actor=lib.bk_entry_assets_actor(assets);player=lib.bk_entry_assets_player(assets)
                    selected=lib.bk_entry_assets_selection(assets).contents
                    raw=(args.data/selected.route_file.decode()).read_bytes();native.u.mem_write(0xbe9a18,raw+bytes(20480-len(raw)))
                    placement=lib.bk_actor_pose_placement(actor).contents;base_height=lib.bk_actor_pose_head(actor)[1]
                    state=State(PathState(Vector(*placement.position),placement.yaw_degrees,0,0,0,0,0,0),Ai(Point(MotionState(1,1,0,0,raw[16],Timer()),Timer(2000,0,0),0,0,0),0),Head(),1,0,b'')
                    interaction=Shared();seed=C.c_uint32(12345+group*9+area)
                    context=Context(18,(C.c_int32*6)(18,19,20,21,23,27),0,1,Vector(0,0,1),0,0,b'')
                    for step in range(8):
                        # Explicit animation fixture, separate from spatial
                        # step. Published cached heads may lag this sampling.
                        if step:
                            assert lib.bk_actor_pose_step(actor,state.path.position,state.path.yaw_degrees,1,1/120,error),error.value
                        if step%3 != 1:
                            lib.bk_actor_pose_publish(actor);lib.bk_actor_pose_publish(player)
                        head=HeadInput();assert lib.bk_entry_assets_head_input(assets,state.path.yaw_degrees,C.byref(head),error)
                        clip=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(clip))
                        inp=Input(group,area,context.player_action,context.suppressed_actions,clip.slot,context.background_clip,context.short_range_action,head.player_position,context.player_direction,head.player_head,head.actor_head_world,head.actor_head_local,head.torso_local,base_height,context.interaction_df,context.interaction_e0,context.excluded_surface)
                        before=State.from_buffer_copy(state);shared=Shared.from_buffer_copy(interaction);prior_seed=seed.value
                        old_head=tuple(lib.bk_actor_pose_head(actor)[:3]);old_pose=bytes(lib.bk_actor_pose_placement(actor).contents)
                        effects=Effects();C.memset(C.byref(effects),0x55,C.sizeof(effects));held=bytes(effects)
                        context.excluded_surface=None
                        assert not lib.bk_entry_assets_step_npc_spatial(assets,C.byref(state),C.byref(interaction),C.byref(seed),collision,C.byref(context),1/60,step*17,C.byref(effects),error)
                        assert bytes(state)==bytes(before) and bytes(interaction)==bytes(shared) and seed.value==prior_seed and bytes(effects)==held
                        assert old_pose==bytes(lib.bk_actor_pose_placement(actor).contents);failures+=1
                        context.excluded_surface=b''
                        assert lib.bk_entry_assets_step_npc_spatial(assets,C.byref(state),C.byref(interaction),C.byref(seed),collision,C.byref(context),1/60,step*17,C.byref(effects),error),error.value
                        expected,expected_shared,expected_seed,placed,vertical,_=native.step(before,shared,inp,prior_seed,C.c_float(1/60).value,step*17)
                        assert bytes(state.ai)==bytes(expected.ai) and bytes(interaction)==bytes(expected_shared) and seed.value==expected_seed
                        assert (state.path.cursor,state.alpha,state.visible,state.surface_name)==(expected.path.cursor,expected.alpha,expected.visible,expected.surface_name)
                        equal([*state.path.position,state.path.yaw_degrees],[*expected.path.position,expected.path.yaw_degrees],'path')
                        equal([*state.head.sight.start,*state.head.sight.end,state.head.sight.distance,state.head.bearing,state.head.facing],[*expected.head.sight.start,*expected.head.sight.end,expected.head.sight.distance,expected.head.bearing,expected.head.facing],'head')
                        equal(lib.bk_actor_pose_placement(actor).contents.world,placed.world,'placed root')
                        equal([effects.vertical_position],[vertical],'height')
                        assert tuple(lib.bk_actor_pose_head(actor)[:3])==old_head
                        after=ClipState();assert lib.bk_actor_pose_state(actor,C.byref(after));assert bytes(clip)==bytes(after)
                        cases+=1
                finally:lib.bk_entry_assets_destroy(assets)
            print('group',group,'9 profiles PASS',flush=True)
    finally:
        lib.bk_collision_destroy(collision);lib.bk_model_destroy(model);lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=cases,atomic_rejections=failures,entry_profiles=45,max_normalized_error=worst,collision_fixture='m01_04.atr shared explicitly across profiles; not background selection',native_functions=['0x4fc8f9..0x4fcdb6'],scope='Real entry models/clips/head and torso bindings, live active clip, initial head offset, combined spatial update, atomic scene/pose commit, held head caches and unchanged animation clock. Animation inputs explicit; later event dispatch and complete gameplay excluded.')
    (ROOT/'local/original-entry-spatial-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'entry spatial steps;',failures,'atomic rejections; max error',worst)

if __name__=='__main__':main()
