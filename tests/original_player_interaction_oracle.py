"""Complete original4c20ee and native callees; only input-device queries hooked."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from model_binding import ROOT,library
from original_player_trigger_oracle import State as Trigger,Input as TriggerInput,Native as TriggerNative,fixture as trigger_fixture,compare
from original_player_movement_oracle import State as Movement,FIELDS

class Timing(C.Structure):
    _fields_=[('start',C.c_float),('end',C.c_float),('source',C.c_float)]
class State(C.Structure):
    _fields_=[('trigger',Trigger),('script_phase',C.c_int8)]
class Input(C.Structure):
    _fields_=[('trigger',TriggerInput),('normal',C.c_float*3),('buttons',C.c_uint32),('active_clip',C.c_int32),('clips',Timing*128)]
ALIASES=[[(1,1,2),(0x58,1,0),(0x33451,1,0)],[(0x5a,1,0),(0x33452,1,0)]]
class Native(TriggerNative):
    def __init__(self,exe):
        super().__init__(exe)
        self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x4b76c2,end=0x4b76c2)
        self.branches={address:0 for address in [0x4c21c1,0x4c21e7,0x4c22b1,0x4c22d7,0x4c232e,0x4c2354,0x4c23ab,0x4c23d1,0x4c2428,0x4c247a,0x4c24a9,0x4c251b,0x4c255e,0x4c25af,0x4c25ea]}
        for address in self.branches:self.u.hook_add(UC_HOOK_CODE,self.branch,begin=address,end=address)
    def branch(self,u,address,size,data):self.branches[address]+=1
    def hook(self,u,address,size,data):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        key=struct.unpack('<3I',u.mem_read(sp+4,12))
        u.reg_write(UC_X86_REG_EAX,int(key in self.keys));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def prepare_interaction(self,movement,state,inp,rng):
        query=TriggerInput.from_buffer_copy(inp.trigger);query.position[:]=movement.position;query.action=movement.action
        super().prepare(state.trigger,query)
        for name,offset,fmt in FIELDS:
            v=getattr(movement,name);vs=list(v) if isinstance(v,C.Array) else [v]
            self.u.mem_write(self.actor+offset,struct.pack('<'+fmt,*vs))
        self.u.mem_write(self.actor+0x31c,bytes(inp.normal))
        self.u.mem_write(self.actor+0x7f8,struct.pack('<b',state.script_phase))
        self.u.mem_write(self.model+0x140,struct.pack('<i',inp.active_clip))
        for i,t in enumerate(inp.clips):
            self.u.mem_write(self.model+i*0x9c+0x1e4,struct.pack('<2f',t.start,t.end))
            self.u.mem_write(self.model+i*0x9c+0x1f0,struct.pack('<f',t.source))
        self.u.mem_write(0x7219a8,struct.pack('<2i',inp.trigger.group,inp.trigger.area))
        self.keys={rng.choice(aliases) for i,aliases in enumerate(ALIASES) if inp.buttons&(1<<i)}
    def read_interaction(self,movement):
        m=Movement.from_buffer_copy(movement)
        for name,offset,fmt in FIELDS:
            v=struct.unpack('<'+fmt,self.u.mem_read(self.actor+offset,struct.calcsize(fmt)))
            field=getattr(m,name)
            if isinstance(field,C.Array):field[:]=v
            else:setattr(m,name,v[0])
        s=State();s.trigger=super().read();s.script_phase=struct.unpack('<b',self.u.mem_read(self.actor+0x7f8,1))[0]
        return m,s
    def step(self,movement,state,inp,rng):
        self.prepare_interaction(movement,state,inp,rng)
        allowed=self.run(0x4c20ee,[self.actor])
        return (*self.read_interaction(movement),allowed)

def fixture(native,rng,case):
    trigger,query=trigger_fixture(native,rng,case)
    m=Movement();s=State();inp=Input();inp.trigger=query;s.trigger=trigger;s.script_phase=rng.choice([-1,0,1,2,3])
    m.position[:]=query.position;m.previous[:]=[11,12,13];m.velocity[:]=[3,4,5]
    m.yaw=rng.uniform(-180,360);m.pitch=25;m.turn[:]=[9,10];m.acceleration=1;m.move_latch=-1
    m.action=query.action;m.interaction_mode=rng.choice([-1,0,1,2,5,6])
    inp.normal[:]=[rng.uniform(-1,1) for _ in range(3)]
    inp.buttons=case%4;inp.active_clip=rng.choice([0,query.actions[0],query.actions[20],query.action,99])
    for i,t in enumerate(inp.clips):
        t.start=rng.randrange(1,200);t.end=t.start+rng.randrange(1,200);t.source=rng.choice([t.start,t.end,t.end-.1,t.end+1])
    if case<90:
        inp.trigger.actions[:]=range(21);m.action=0 if case<45 else 20;m.interaction_mode=0 if case<45 else 5
        inp.buttons=2;inp.trigger.wall_name=native.names['cover'][inp.trigger.group*9+inp.trigger.area]
        for prop in inp.trigger.props:prop.active=0
    return m,s,inp

def compare_movement(actual,wanted,case):
    worst=0
    for name,_,_ in FIELDS:
        a,b=getattr(actual,name),getattr(wanted,name)
        pairs=zip(a,b) if isinstance(a,C.Array) else [(a,b)]
        for a,b in pairs:
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
            assert d<3e-6,(case,name,a,b)
    assert actual.move_latch==wanted.move_latch
    return worst

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4c20ee)
    lib.bk_player_interaction.argtypes=[C.POINTER(Movement),C.POINTER(State),C.POINTER(C.c_int),C.POINTER(Input),C.c_void_p]
    worst=0;allowed_count=0;rejects=0;error=C.create_string_buffer(256)
    for case in range(18000):
        m,s,inp=fixture(native,rng,case)
        expected_m,expected_s,expected_allowed=native.step(m,s,inp,rng)
        allowed=C.c_int(-1)
        assert lib.bk_player_interaction(C.byref(m),C.byref(s),C.byref(allowed),C.byref(inp),error),error.value
        assert allowed.value==expected_allowed,(case,allowed.value,expected_allowed)
        assert s.script_phase==expected_s.script_phase,(case,s.script_phase,expected_s.script_phase)
        worst=max(worst,compare_movement(m,expected_m,case),compare(s.trigger,expected_s.trigger,case))
        allowed_count+=allowed.value
        if case%100==0:
            held=(bytes(m),bytes(s),allowed.value);inp.buttons=4
            assert not lib.bk_player_interaction(C.byref(m),C.byref(s),C.byref(allowed),C.byref(inp),error)
            assert held==(bytes(m),bytes(s),allowed.value);rejects+=1
    assert all(native.branches.values()),native.branches
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=18000,controls_allowed=allowed_count,atomic_rejections=rejects,max_relative_error=worst,branches={hex(k):v for k,v in native.branches.items()},hooks=['device input4b76c2 only'],scope='Full4c20ee plus all native target/heading/string helpers; arbitrary aliases and per-slot retained timing, no animation advance.')
    (ROOT/'local/original-player-interaction-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
