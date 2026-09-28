"""Execute complete original4c2678/4c2849/4c29f7/4c32cc/4c5078 with no hooks."""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

class State(C.Structure):
    _fields_ = [('origin', C.c_float*4), ('target', C.c_float*4), ('prop_kind', C.c_int32)]
class Prop(C.Structure):
    _fields_ = [('active', C.c_int32), ('kind', C.c_int32), ('position', C.c_float*3)]
class Input(C.Structure):
    _fields_ = [('position', C.c_float*3), ('wall_heading', C.c_float),
                ('action', C.c_int32), ('actions', C.c_int32*21),
                ('group', C.c_int32), ('area', C.c_int32), ('wall_name', C.c_char_p), ('props', Prop*16)]

class Native:
    actor, model, stack, stop = 0x71b510, 0x3001000, 0x200e000, 0x300f000
    def __init__(self, exe):
        self.u = machine(exe)
        self.names = {}
        for key, start, end, offset, count in [('wall',0x4c29f7,0x4c2ee2,0x200c,32), ('cover',0x4c32cc,0x4c395c,0x2d00,45),('available',0x4c5078,0x4c5563,0x2000,32)]:
            self.u.reg_write(UC_X86_REG_ESP, self.stack)
            self.u.emu_start(start,end,count=100000)
            assert self.u.reg_read(UC_X86_REG_EIP)==end
            bp=self.u.reg_read(UC_X86_REG_EBP)
            self.names[key]=[bytes(self.u.mem_read(bp-offset+i*256,256)).split(b'\0')[0] for i in range(count)]
    def prepare(self, state, inp):
        u=self.u
        u.mem_write(self.actor,bytes(0x900))
        u.mem_write(self.actor,struct.pack('<I',self.model))
        u.mem_write(self.actor+0x10,struct.pack('<i',inp.action))
        u.mem_write(self.actor+0x14,bytes(inp.actions))
        u.mem_write(self.actor+0x29c,bytes(inp.position))
        u.mem_write(self.actor+0x6c4,inp.wall_name+b'\0')
        u.mem_write(self.actor+0x7d4,struct.pack('<f',inp.wall_heading))
        u.mem_write(self.actor+0x7d8,bytes(state.origin)+bytes(state.target))
        u.mem_write(self.actor+0x800,struct.pack('<i',state.prop_kind))
        for i,p in enumerate(inp.props):
            base=0x729d80+i*0x998
            u.mem_write(base,struct.pack('<i',p.active))
            u.mem_write(base+0xc,struct.pack('<i',p.kind))
            u.mem_write(base+0x29c,bytes(p.position))
    def read(self):
        state=State()
        C.memmove(C.addressof(state),bytes(self.u.mem_read(self.actor+0x7d8,32)),32)
        state.prop_kind=struct.unpack('<i',self.u.mem_read(self.actor+0x800,4))[0]
        return state
    def run(self,address,args):
        self.u.mem_write(self.stack,struct.pack('<'+'I'*(1+len(args)),self.stop,*args))
        self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(address,self.stop,count=150000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
        return self.u.reg_read(UC_X86_REG_EAX)&255
    def step(self,state,inp,query):
        self.prepare(state,inp)
        hit=self.run([0x4c2678,0x4c2849,0x4c29f7,0x4c32cc,0x4c5078][query],[self.actor,inp.group,inp.area])
        return hit,self.read()

SCENES=[[0,1,3,2],[2,0,1,3],[1,0,2,3],[3,1,0,2],[0,2,3,1]]
def fixture(native,rng,case):
    state=State()
    state.origin[:]=[rng.uniform(-100,100) for _ in range(4)]
    state.target[:]=[rng.uniform(-100,100) for _ in range(4)]
    state.prop_kind=rng.randrange(-1,30)
    inp=Input()
    inp.position[:]=[rng.uniform(-1000,1000) for _ in range(3)]
    inp.wall_heading=rng.choice([0,90,180,270,360,-180,720,rng.uniform(-360,720)])
    inp.actions[:]=[i+1 for i in range(21)] if case%3 else [rng.randrange(32) for _ in range(21)]
    inp.action=rng.choice(list(inp.actions)+[99])
    inp.group,inp.area=(case//9)%5,case%9
    names=native.names['wall']+native.names['cover']+[b'NULL',b'',b'miss',b'mesh_kabe_1@m00_10.x']
    if case%4==0 and inp.area<4:
        base=SCENES[inp.group][inp.area]*8
        inp.wall_name=native.names['wall'][base+(case//180)%8]
    elif case%4==1: inp.wall_name=native.names['cover'][inp.group*9+inp.area]
    else: inp.wall_name=rng.choice(names)
    for i,p in enumerate(inp.props):
        p.active=rng.choice([0,0,1,-1])
        p.kind=rng.choice([0,10,11,18,19,20])
        p.position[:]=[inp.position[j]+rng.uniform(-35,35) for j in range(3)]
        if case%7==0:p.position[:]=[inp.position[0]+rng.choice([0,20,20.0001]),100000,inp.position[2]]
    if case%5==0:
        for p in inp.props:p.active=0
    return state,inp

def compare(actual,wanted,case):
    worst=0
    for name in ['origin','target']:
        for a,b in zip(getattr(actual,name),getattr(wanted,name)):
            d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
            assert math.isfinite(d) and d<3e-6,(case,name,a,b)
    assert actual.prop_kind==wanted.prop_kind,(case,actual.prop_kind,wanted.prop_kind)
    return worst

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4c2678)
    lib.bk_player_trigger_prop.argtypes=[C.POINTER(State),C.POINTER(C.c_int),C.POINTER(Input),C.c_uint,C.c_void_p]
    lib.bk_player_trigger_wall.argtypes=[C.POINTER(State),C.POINTER(C.c_int),C.POINTER(Input),C.c_void_p]
    lib.bk_player_trigger_cover.argtypes=[C.POINTER(C.c_int),C.POINTER(Input)]
    lib.bk_player_trigger_wall_available.argtypes=[C.POINTER(C.c_int),C.POINTER(Input)]
    worst=0;hits=[0]*5;rejects=0;error=C.create_string_buffer(256)
    for case in range(9000):
        state,inp=fixture(native,rng,case)
        for query in range(5):
            actual=State.from_buffer_copy(state);expected_hit,expected=native.step(state,inp,query);hit=C.c_int(-1)
            if query<2:ok=lib.bk_player_trigger_prop(C.byref(actual),C.byref(hit),C.byref(inp),query,error)
            elif query==2:ok=lib.bk_player_trigger_wall(C.byref(actual),C.byref(hit),C.byref(inp),error)
            elif query==3:ok=lib.bk_player_trigger_cover(C.byref(hit),C.byref(inp))
            else:ok=lib.bk_player_trigger_wall_available(C.byref(hit),C.byref(inp))
            assert ok,error.value
            assert hit.value==expected_hit,(case,query,hit.value,expected_hit,inp.wall_name,inp.group,inp.area)
            hits[query]+=hit.value
            worst=max(worst,compare(actual,expected,(case,query)))
        if case%100==0:
            held=bytes(state);hit=C.c_int(-1);inp.wall_heading=float('nan')
            assert not lib.bk_player_trigger_wall(C.byref(state),C.byref(hit),C.byref(inp),error)
            assert bytes(state)==held and hit.value==-1;rejects+=1
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=9000,native_calls=45000,hits=hits,max_relative_error=worst,atomic_rejections=rejects,hooks=[],scope='All five complete trigger functions; literal wall tables extracted independently from original initializers; no gameplay claim.')
    (ROOT/'local/original-player-trigger-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
