"""Native actor root/XAN/ANIM/traversal with cached head publication.

Uses actual six actor assets, explicit action requests and positions. NPC
half-speed call fragment runs unmodified; AI/physics/material/audio do not.
"""
import argparse,ctypes as C,hashlib,json,math,struct,sys
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_follow_phase_oracle import Native as PhaseNative
from original_placement_oracle import Placement
from playback_binding import library
from model_binding import ROOT,Model,decode
from clip_binding import State
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def bind(lib):
    fp=C.POINTER(C.c_float)
    for name,args,result in [
        ('bk_actor_pose_create',[C.POINTER(Model),C.c_void_p,C.c_uint32,C.c_char_p,fp,C.c_float,C.c_uint,C.c_int,C.c_void_p],C.c_void_p),
        ('bk_actor_pose_destroy',[C.c_void_p],None),
        ('bk_actor_pose_step',[C.c_void_p,fp,C.c_float,C.c_int,C.c_float,C.c_void_p],C.c_int),
        ('bk_actor_pose_place',[C.c_void_p,fp,C.c_float,C.c_void_p],C.c_int),
        ('bk_actor_pose_publish',[C.c_void_p],None),
        ('bk_actor_pose_frame',[C.c_void_p,C.c_uint32],fp),
        ('bk_actor_pose_local',[C.c_void_p,C.c_uint32],fp),
        ('bk_actor_pose_head',[C.c_void_p],fp),
        ('bk_actor_pose_placement',[C.c_void_p],C.POINTER(Placement)),
        ('bk_actor_pose_state',[C.c_void_p,C.POINTER(State)],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
class Native(PhaseNative):
    def fragment(self,start,end):
        self.uc.reg_write(UC_X86_REG_EBP,self.stack);self.uc.reg_write(UC_X86_REG_ESP,self.stack-0x4000);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(start,end,count=400000000);assert self.uc.reg_read(UC_X86_REG_EIP)==end,(hex(start),hex(self.uc.reg_read(UC_X86_REG_EIP)))
    def place_actor(self,position,yaw):
        self.word(self.actor,self.clip);self.word(self.stack+8,self.actor);self.vector(self.stack-0x3824,[0,1,0]);self.vector(self.actor+0x29c,list(position)+[0,yaw])
        self.fragment(0x4fbd9d,0x4fbe4a)
    def step_actor(self,position,yaw,slot,seconds):
        self.place_actor(position,yaw);self.word(self.actor+0x10,slot);self.vector(0x733700,[seconds])
        previous=self.floats(self.node+0xc0,16)
        self.fragment(0x4fc6c7,0x4fc6fd)
        assert previous==self.floats(self.node+0xc0,16)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();bind(lib);arc=Archive(args.data/'bk3_01.pp');records=[];error=C.create_string_buffer(256)
    for index,head in enumerate(['qqq21_atama','atama','kubiX','kubiX','atama','atama']):
        name=f'h0{index}_80';data=arc.read(next(e for e in arc.entries if e.name==name+'.x'));xan=arc.read(next(e for e in arc.entries if e.name==name+'.xan'))
        rc,model,message=decode(lib,data);assert rc==1,message;clips=lib.bk_clip_set_decode(xan,len(xan),error);assert clips,error.value;actor=None
        try:
            native.bind(data,model.contents);head_index=C.c_uint32();assert lib.bk_model_find_frame(model,head.encode(),C.byref(head_index),error)
            native.bind_clip(xan,head_index.value,0 if index==0 else 1,index==0)
            origin=(C.c_float*3)(-16.25,0,-.5);yaw=C.c_float(268.87).value
            native.place_actor(origin,yaw)
            actor=lib.bk_actor_pose_create(model,clips,native.root,head.encode(),origin,yaw,0 if index==0 else 1,index==0,error);assert actor,error.value
            worst=0;matrices=0
            def equal(actual,expected):
                nonlocal worst
                for j,(a,b) in enumerate(zip(actual,expected)):
                    e=abs(a-b)/max(1,abs(b));worst=max(worst,e);assert math.isfinite(e) and e<3e-5,(name,j,a,b,e)
            def check():
                nonlocal matrices
                for frame in range(model.contents.frame_count):
                    equal(lib.bk_actor_pose_frame(actor,frame)[:16],native.floats(native.frames+frame*0x400+0xc0,16));matrices+=1
                    equal(lib.bk_actor_pose_local(actor,frame)[:16],native.floats(native.frames+frame*0x400+0x80,16));matrices+=1
                equal(lib.bk_actor_pose_head(actor)[:3],native.floats(native.node+0xf0,3))
                state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));equal([getattr(state,n) for n,_ in State._fields_],native.state())
            check()
            # First publication must apply placement to BASE locals without
            # sampling or consuming the initial clip request.
            native.publish();lib.bk_actor_pose_publish(actor);check()
            rejects=0
            slots=[i for i in range(128) if struct.unpack_from("<2f",xan,512+0x190+i*156+0x54)!=(0,0)]
            for case in range(90):
                origin=(C.c_float*3)(-16.25+case*.125,case%3,-.5+case*.0625);yaw=C.c_float(268.87+case*.37).value
                slot=slots[(case//11)%2];other=slots[1-(case//11)%2];seconds=C.c_float([0,1/60,.1,.25][case%4]).value
                if case%11 == 0:
                    assert lib.bk_actor_pose_place(actor,origin,yaw,error),error.value
                    native.place_actor(origin,yaw);check()
                before=bytes(lib.bk_actor_pose_placement(actor).contents);state=State();assert lib.bk_actor_pose_state(actor,C.byref(state));saved=bytes(state)
                locals_before=[tuple(lib.bk_actor_pose_local(actor,f)[:16]) for f in range(model.contents.frame_count)]
                for bad_dt in [-1,float('nan'),1e30]:
                    assert not lib.bk_actor_pose_step(actor,origin,yaw,other,bad_dt,error)
                    assert before==bytes(lib.bk_actor_pose_placement(actor).contents)
                    assert locals_before==[tuple(lib.bk_actor_pose_local(actor,f)[:16]) for f in range(model.contents.frame_count)]
                    assert lib.bk_actor_pose_state(actor,C.byref(state)) and bytes(state)==saved;rejects+=1
                assert lib.bk_actor_pose_step(actor,origin,yaw,slot,C.c_float(seconds*.5).value,error),error.value
                native.step_actor(origin,yaw,slot,seconds);check()
                if case%9!=2:
                    native.publish();lib.bk_actor_pose_publish(actor);check()
            records.append(dict(entry=name,frames=model.contents.frame_count,head=head,steps=90,matrices=matrices,rejected_steps=rejects,max_normalized_error=worst,sha256=hashlib.sha256(data).hexdigest()))
            print(name,'PASS',worst,flush=True)
        finally:lib.bk_actor_pose_destroy(actor);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,steps=sum(r['steps'] for r in records),matrices=sum(r['matrices'] for r in records),rejected_steps=sum(r['rejected_steps'] for r in records),max_normalized_error=max(r['max_normalized_error'] for r in records),native_functions=['0x4fbd9d..0x4fbe4a','0x4fc6c7..0x4fc6fd','0x401b0a','0x4026fe','ANIM SRT','0x42273b'],scope='Six actual actor assets: root immediately visible, child/head caches held until traversal, base placement without timeline consumption, repeated requests and transactional rejection. Explicit pose/action inputs; not player/NPC policy, AI, physics, dynamic mesh, audio or complete gameplay.')
    (ROOT/'local/original-actor-phase-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['steps'],report['matrices'],report['rejected_steps'])
if __name__=='__main__':main()
