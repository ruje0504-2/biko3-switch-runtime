"""Compare real entry material instances and marker bindings with native fade."""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_npc_fade_oracle import Native, bind, Archive
from original_entry_oracle import Request
from original_npc_spatial_oracle import State
from model_binding import ROOT, Material, library, decode

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native,lib=Native(exe),library();bind(lib)
    for name,arguments,result in [
        ('bk_resources_create',[C.c_void_p],C.c_void_p),('bk_resources_destroy',[C.c_void_p],None),
        ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
        ('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
        ('bk_entry_assets_create',[C.c_void_p,C.POINTER(Request),C.c_void_p],C.c_void_p),
        ('bk_entry_assets_destroy',[C.c_void_p],None),
        ('bk_entry_assets_actor',[C.c_void_p],C.c_void_p),
        ('bk_actor_pose_hidden',[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)],C.c_int),
        ('bk_entry_assets_step_npc_visibility',[C.c_void_p,C.POINTER(State),C.c_int8,C.c_int8,C.c_void_p],C.c_int),
        ('bk_entry_assets_actor_material',[C.c_void_p,C.c_uint32],C.POINTER(Material)),
        ('bk_entry_assets_step_npc_fade',[C.c_void_p,C.POINTER(State),C.c_float,C.c_void_p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=arguments;fn.restype=result
    error=C.create_string_buffer(256);store=lib.bk_resources_create(error);assert store,error.value
    arc=Archive(args.data/'bk3_01.pp');cases=materials=failures=visibility_frames=0
    try:
        for pack in ['bk3_01','bk3_04']:
            assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
        assert lib.bk_resources_mount_directory(store,b'routes',str(args.data).encode(),20480,error),error.value
        assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,error),error.value
        for group in range(5):
            name=f'h0{group+1}_80.x';raw=arc.read(next(e for e in arc.entries if e.name==name));ok,model,message=decode(lib,raw);assert ok==1,message
            try:
                root=next(i for i in range(model.contents.frame_count) if model.contents.frames[i].parent_index==0xffffffff)
                marks=[]
                for node in [b'mark_02',b'mark_01',b'mark_00',b'mark_03']:
                    index=C.c_uint32();assert lib.bk_model_find_frame(model,node,C.byref(index),error);marks.append(index.value)
                for area in range(9):
                    native.create(model.contents,root,marks)
                    assets=lib.bk_entry_assets_create(store,C.byref(Request(group,area,0,8)),error);assert assets,error.value
                    try:
                        state=State();state.alpha=1
                        for case in range(24):
                            state.alpha=[0,.2,.99,1,1.25,-.5][case%6]
                            state.ai.point.fade_out=[0,1,0,1,2,255][case%6]
                            state.ai.point.motion.hidden=case%2
                            state.ai.point.motion.behavior=case%7-1
                            interface,phase=[0,2,2][case%3],case%6-1
                            native.u.mem_write(native.actor+0x328,bytes([state.ai.point.motion.hidden]))
                            native.word(native.actor+0x850,state.ai.point.motion.behavior&0xffffffff)
                            native.u.mem_write(0xbeeb84,bytes([interface]));native.u.mem_write(0x71ba88,bytes([phase&255]))
                            native.word(0x7219ac,area);native.word(native.stack+8,native.actor)
                            native.u.reg_write(UC_X86_REG_EBP,native.stack);native.u.reg_write(UC_X86_REG_ESP,native.stack-0x4000)
                            native.u.emu_start(0x4fc37f,0x4fc6a3,count=8000000)
                            assert native.u.reg_read(UC_X86_REG_EIP)==0x4fc6a3
                            held=bytes(state)
                            assert lib.bk_entry_assets_step_npc_visibility(assets,C.byref(state),interface,phase,error),error.value
                            assert bytes(state)==held
                            actor=lib.bk_entry_assets_actor(assets)
                            for i,ptr in enumerate(native.frames):
                                hidden=C.c_uint32();assert lib.bk_actor_pose_hidden(actor,i,C.byref(hidden))
                                assert hidden.value==struct.unpack('<I',native.u.mem_read(ptr+0x70,4))[0],(group,area,case,i)
                                visibility_frames+=1
                            seconds=C.c_float([0,1/60,.25,1][case%4]).value
                            expected=State.from_buffer_copy(state)
                            expected.alpha=native.fade(state.alpha,state.ai.point.fade_out,group,seconds)
                            assert lib.bk_entry_assets_step_npc_fade(assets,C.byref(state),seconds,error),error.value
                            assert bytes(state)==bytes(expected)
                            for i,wanted in enumerate(native.values()):
                                m=lib.bk_entry_assets_actor_material(assets,i).contents
                                actual=C.string_at(C.addressof(m)+Material.diffuse.offset,68)
                                assert actual==wanted,(group,area,case,i);materials+=1
                            held=bytes(state);first=bytes(lib.bk_entry_assets_actor_material(assets,0).contents)
                            assert not lib.bk_entry_assets_step_npc_fade(assets,C.byref(state),float('nan'),error)
                            assert bytes(state)==held and bytes(lib.bk_entry_assets_actor_material(assets,0).contents)==first
                            cases+=1;failures+=1
                    finally:lib.bk_entry_assets_destroy(assets)
                print('group',group,'entry fade PASS',flush=True)
            finally:lib.bk_model_destroy(model)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),entry_profiles=45,cases=cases,material_comparisons=materials,visibility_frames=visibility_frames,atomic_rejections=failures,native_functions=['0x4fc37f..0x4fc6a3','0x4fc7a2 and recursive material callees'],hooks=[],scope='All45real entry instances: marker frames, visibility, per-instance materials and fade state vs original instructions, including hidden actors. No GPU upload, accessory animation or complete actor loop.')
    (ROOT/'local/original-entry-fade-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'entry fade steps;',materials,'materials;',failures,'rejections')

if __name__=='__main__':main()
