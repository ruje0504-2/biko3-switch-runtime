"""Whole4b8a89 routing with nine actual camera controllers and native math.

FOV, device keys and TRACK timeline advance are supplied service boundaries.
Aim, rotation matrices, root writes, visibility and all branches execute x86.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_aim_oracle import Pose,I
from model_binding import ROOT,library

Vec=C.c_float*3
class State(C.Structure):
    _fields_=[('pose',Pose),('yaw',C.c_float),('pitch',C.c_float),('target_distance',C.c_float),('distance',C.c_float),('matrix',C.c_float*16),('probe',Vec),('focus',Vec),('rays',Vec*8),('npc_distance',C.c_float),('npc_heading',C.c_float),('blocked',C.c_int32*8),('selected_ray',C.c_int32),('lean',C.c_float)]
class Input(C.Structure):
    _fields_=[('position',Vec),('head',Vec),('vertical',C.c_float),('yaw',C.c_float),('pitch',C.c_float),('npc_position',Vec),('npc_yaw',C.c_float),('npc_height',C.c_float),('npc_vertical',C.c_float),('seconds',C.c_float),('buttons',C.c_uint32),('track',Vec),('track_source',C.c_float),('track_end',C.c_float)]
class Effects(C.Structure):
    _fields_=[('root_hidden',C.c_int),('player_hidden',C.c_int),('write_return_yaw',C.c_int),('reset_mode',C.c_int),('return_yaw',C.c_float)]
class Flags(C.Structure):_fields_=[('mode',C.c_int8),('hidden',C.c_uint8)]
CONTROLLERS=[None,0x4b9175,0x4b9dc5,0x4b9edb,0x4ba03a,0x4ba19c,0x4ba2f2,0x4ba4be,0x4ba87c,0x4baa95]
FIELDS=[('yaw',0x42c,'f'),('pitch',0x430,'f'),('target_distance',0x43c,'f'),('distance',0x440,'f'),('matrix',0x4a4,'16f'),('probe',0x524,'3f'),('focus',0x530,'3f'),('npc_distance',0x59c,'f'),('npc_heading',0x5a0,'f'),('blocked',0x5a4,'8i'),('selected_ray',0x5c4,'i')]
KEYS=[[(0,2,0),(0x25,2,0),(0x30d42,2,0)],[(1,2,0),(0x27,2,0),(0x30d43,2,0)]]
class Native:
    camera,player,npc=0x71af38,0x71b510,0x728de8
    model,group,root,head=0x3001000,0x3002000,0x3003000,0x3004000
    clip,clip_group,clip_root,track,frame,context=0x3005000,0x3006000,0x3007000,0x3008000,0x3009000,0x300a000
    stack,stop=0x200e000,0x300f000
    def __init__(self,exe):
        self.u=machine(exe);self.counts=[0]*10
        for a in [0x42cf0e,0x4b76c2,0x4026fe]+CONTROLLERS[1:]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v))
    def vec(self,a,v):self.u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
    def hook(self,u,a,size,data):
        if a in CONTROLLERS:
            self.kind=CONTROLLERS.index(a);self.counts[self.kind]+=1;return
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];result=0
        if a==0x42cf0e:assert struct.unpack('<f',u.mem_read(sp+4,4))[0]==1
        elif a==0x4b76c2:result=struct.unpack('<3I',u.mem_read(sp+4,12)) in self.keys
        else:
            obj,seconds=struct.unpack('<If',u.mem_read(sp+4,8));assert obj==self.clip and seconds==self.input.seconds
            self.vec(obj+0x1f0,[self.input.track_source]);self.vec(obj+0x1e8,[self.input.track_end]);self.advances+=1
        u.reg_write(UC_X86_REG_EAX,int(result));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def step(self,state,inp,flags,camera_mode,npc_hidden,action,actions,rng):
        u=self.u;self.input=inp;self.kind=0;self.advances=0
        self.keys={rng.choice(keys) for i,keys in enumerate(KEYS) if inp.buttons&(1<<i)}
        u.mem_write(self.camera,bytes(0x5d0));u.mem_write(self.player,bytes(0x900));u.mem_write(self.npc,bytes(0x900));u.mem_write(0x3001000,bytes(0xa000))
        self.word(self.camera,self.clip);self.word(self.camera+8,self.track)
        self.word(self.player,self.model);self.word(self.player+8,self.head)
        for model,group,root in [(self.model,self.group,self.root),(self.clip,self.clip_group,self.clip_root)]:self.word(model+0x160,group);self.word(group+0x14,root)
        for frame in [self.root,self.clip_root,self.frame,self.context]:
            for off in [0x80,0xc0,0x100]:self.vec(frame+off,I)
        self.word(self.root+0x70,77)
        self.word(0x645600,self.context);self.word(0x645604,self.frame)
        self.vec(self.frame+0x80,state.pose.world);self.vec(self.frame+0xc0,state.pose.world);self.vec(self.camera+0x420,state.pose.position)
        for name,off,fmt in FIELDS:
            v=getattr(state,name);values=list(v) if isinstance(v,C.Array) else [v]
            u.mem_write(self.camera+off,struct.pack('<'+fmt,*values))
        for j,off in enumerate([0x53c,0x55c,0x57c]):self.vec(self.camera+off,[v[j] for v in state.rays])
        self.vec(0x559a80,[state.lean]);self.vec(0x733700,[inp.seconds])
        self.vec(self.player+0x29c,inp.position);self.vec(self.player+0x2a8,[inp.vertical,inp.yaw,inp.pitch]);self.vec(self.head+0xf0,inp.head)
        self.vec(self.npc+0x29c,inp.npc_position);self.vec(self.npc+0x298,[inp.npc_height]);self.vec(self.npc+0x2a8,[inp.npc_vertical,inp.npc_yaw]);self.vec(self.track+0xf0,inp.track)
        u.mem_write(self.player+0x579,struct.pack('<b',flags.mode));u.mem_write(self.player+0x328,bytes([flags.hidden]));u.mem_write(self.npc+0x328,bytes([npc_hidden]));u.mem_write(0x729780,struct.pack('<b',camera_mode))
        u.mem_write(self.player+0x14,bytes(actions));u.mem_write(self.player+0x10,struct.pack('<i',action));self.vec(self.player+0x7fc,[12345])
        u.mem_write(self.stack,struct.pack('<3I',self.stop,self.camera,self.player));u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(0x4b8a89,self.stop,count=500000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
        result=State.from_buffer_copy(state)
        result.pose.world[:]=struct.unpack('<16f',u.mem_read(self.frame+0xc0,64));result.pose.position[:]=struct.unpack('<3f',u.mem_read(self.camera+0x420,12))
        for name,off,fmt in FIELDS:
            values=struct.unpack('<'+fmt,u.mem_read(self.camera+off,struct.calcsize(fmt)));v=getattr(result,name)
            if isinstance(v,C.Array):v[:]=values
            else:setattr(result,name,values[0])
        for j,off in enumerate([0x53c,0x55c,0x57c]):
            values=struct.unpack('<8f',u.mem_read(self.camera+off,32))
            for i,v in enumerate(values):result.rays[i][j]=v
        result.lean=struct.unpack('<f',u.mem_read(0x559a80,4))[0]
        flags=Flags(struct.unpack('<b',u.mem_read(self.player+0x579,1))[0],u.mem_read(self.player+0x328,1)[0])
        hidden=struct.unpack('<I',u.mem_read(self.root+0x70,4))[0];return_yaw=struct.unpack('<f',u.mem_read(self.player+0x7fc,4))[0]
        if self.kind==6:assert self.advances==1 and tuple(inp.position)==struct.unpack('<3f',u.mem_read(self.clip_root+0xf0,12))
        else:assert self.advances==0
        self.counts[0]+=self.kind==0
        return result,flags,hidden,return_yaw,self.kind

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4b8a89)
    lib.bk_player_view_step.argtypes=[C.POINTER(State),C.c_int,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
    lib.bk_player_view_route.argtypes=[C.POINTER(Flags),C.POINTER(C.c_int),C.c_int8,C.c_uint8,C.c_int32,C.POINTER(C.c_int32)]
    worst=0;error=C.create_string_buffer(256);rejects=0
    def floats(values):
        for v in values:
            if isinstance(v,C.Array):yield from floats(v)
            else:yield v
    def equal(a,b,key):
        nonlocal worst
        for a,b in zip(a,b):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
            assert math.isfinite(d) and d<3e-5,(case,key,a,b,d)
    for case in range(18000):
        state=State();state.pose.world[:]=I;state.matrix[:]=I
        state.pose.world[12:15]=[rng.uniform(-500,500) for _ in range(3)];state.matrix[12:15]=[-7,-8,-9]
        state.pose.position[:]=[rng.uniform(-300,300) for _ in range(3)]
        state.yaw=rng.uniform(-360,360);state.pitch=rng.uniform(-80,80);state.distance=rng.uniform(-30,100);state.target_distance=rng.uniform(-30,100)
        state.probe[:]=[11,12,13];state.focus[:]=[rng.uniform(-300,300) for _ in range(3)]
        for i,r in enumerate(state.rays):r[:]=[rng.uniform(-300,300) for _ in range(3)]
        state.blocked[:]=[rng.choice([-1,0,0,1,2]) for _ in range(8)];state.selected_ray=-1;state.lean=rng.choice([-10,0,10])
        if case%13==0:state.blocked[:]=[1]*8
        inp=Input(position=Vec(*(rng.uniform(-300,300) for _ in range(3))),head=Vec(*(rng.uniform(-300,300) for _ in range(3))),vertical=rng.uniform(-100,100),yaw=rng.choice([-360,-180,0,180,360,rng.uniform(-500,500)]),pitch=rng.uniform(-85,85),npc_position=Vec(*(rng.uniform(-300,300) for _ in range(3))),npc_yaw=rng.uniform(-360,360),npc_height=18,npc_vertical=33,seconds=rng.choice([0,.001,.016,.1,1/3,.5,1,2]),buttons=case%4,track=Vec(*(rng.uniform(-300,300) for _ in range(3))),track_source=rng.choice([1,100,101]),track_end=100)
        flags=Flags(rng.choice([-1,0,1,2,3,4,5,6,7,8]),rng.randrange(256));camera_mode=rng.choice([-1,0,1,2]);npc_hidden=rng.choice([0,0,1,2,255])
        actions=(C.c_int32*21)(*range(21)) if case%3 else (C.c_int32*21)(*(rng.randrange(8) for _ in range(21)))
        action=rng.choice([actions[11],actions[13],actions[16],99])
        expected,wanted_flags,hidden,return_yaw,kind=native.step(state,inp,flags,camera_mode,npc_hidden,action,actions,rng)
        route=C.c_int(-1);effect=Effects()
        assert lib.bk_player_view_route(C.byref(flags),C.byref(route),camera_mode,npc_hidden,action,actions)
        assert route.value==kind,(case,route.value,kind)
        assert lib.bk_player_view_step(C.byref(state),route,C.byref(inp),C.byref(effect),error),(case,kind,error.value)
        if effect.player_hidden!=-1:flags.hidden=effect.player_hidden
        if effect.reset_mode:flags.mode=0
        assert bytes(flags)==bytes(wanted_flags),(case,'flags',bytes(flags),bytes(wanted_flags))
        assert (77 if effect.root_hidden==-1 else effect.root_hidden)==hidden
        assert (12345 if not effect.write_return_yaw else effect.return_yaw)==return_yaw
        equal(state.pose.world,expected.pose.world,'world');equal(state.pose.position,expected.pose.position,'position')
        for name,*_ in FIELDS:
            a,b=getattr(state,name),getattr(expected,name)
            equal(list(a) if isinstance(a,C.Array) else [a],list(b) if isinstance(b,C.Array) else [b],name)
        equal(floats(state.rays),floats(expected.rays),'rays');equal([state.lean],[expected.lean],'lean')
        if case%100==0:
            held=(bytes(state),bytes(effect));inp.seconds=float('nan')
            assert not lib.bk_player_view_step(C.byref(state),route,C.byref(inp),C.byref(effect),error)
            assert held==(bytes(state),bytes(effect));rejects+=1
    assert all(native.counts),native.counts
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=18000,controllers=native.counts,max_relative_error=worst,atomic_rejections=rejects,scope=__doc__,hooks=['FOV42cf0e','keys4b76c2','TRACK4026fe boundary supplies slot0 timings; cached track remains held'])
    (ROOT/'local/original-player-view-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
