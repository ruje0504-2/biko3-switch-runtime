"""Whole4c0d9e, real4c20ee and root setters; no math/action callbacks replaced."""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP
from model_binding import ROOT,library
from original_player_interaction_oracle import Native as InteractionNative,Movement,State,Input,fixture,compare_movement
from original_player_trigger_oracle import compare

class Placement(C.Structure):
    _fields_=[('position',C.c_float*3),('yaw',C.c_float),('world',C.c_float*16)]
class Effects(C.Structure):
    _fields_=[('complete',C.c_int),('placements',C.c_uint),('roots',Placement*2)]
class Native(InteractionNative):
    root_group,root=0x3009000,0x300a000
    def __init__(self,exe):
        super().__init__(exe)
        self.u.hook_add(UC_HOOK_CODE,self.record,begin=0x42407a,end=0x42407a)
        self.script_branches={address:0 for address in [0x4c0df7,0x4c0e37,0x4c0e96,0x4c0f21,0x4c102a,0x4c1177,0x4c11e9,0x4c1225,0x4c1261,0x4c1268]}
        for address in self.script_branches:self.u.hook_add(UC_HOOK_CODE,self.script_branch,begin=address,end=address)
    def script_branch(self,u,address,size,data):self.script_branches[address]+=1
    def record(self,u,address,size,data):
        sp=u.reg_read(UC_X86_REG_ESP)
        root,matrix=struct.unpack('<II',u.mem_read(sp+4,8));assert root==self.root
        self.roots.append(struct.unpack('<16f',u.mem_read(matrix,64)))
    def step_script(self,m,s,inp,seconds,return_yaw,rng):
        self.prepare_interaction(m,s,inp,rng)
        self.roots=[]
        u=self.u
        u.mem_write(self.model+0x160,struct.pack('<I',self.root_group))
        u.mem_write(self.root_group+0x14,struct.pack('<I',self.root))
        u.mem_write(self.root,bytes(0x300))
        u.mem_write(self.root+0x100,struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
        u.mem_write(self.actor+0x7fc,struct.pack('<f',return_yaw))
        u.mem_write(0x733700,struct.pack('<f',seconds))
        complete=self.run(0x4c0d9e,[0x3000000,self.actor])
        expected_m,expected_s=self.read_interaction(m)
        if self.roots:assert self.roots[-1]==struct.unpack('<16f',u.mem_read(self.root+0xc0,64))
        return expected_m,expected_s,complete,self.roots

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x4c0d9e)
    lib.bk_player_script_step.argtypes=[C.POINTER(Movement),C.POINTER(State),C.c_float,C.c_float,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
    worst=0;counts=[0,0,0];rejects=0;error=C.create_string_buffer(256)
    for case in range(14000):
        m,s,inp=fixture(native,rng,case+90)
        # Independent timing of every slot; equal/end boundaries, progress
        # before start, and the special action10 test against active source.
        for t in inp.clips:
            t.start,t.end=(300,600) if case%2 else (600,300)
            t.source=rng.choice([0,299,300,369,369.0001,399.999,400,453,453.0001,533.999,534,600,601,rng.uniform(250,650)])
        s.script_phase=rng.choice([-1,0,1,2,3])
        seconds=C.c_float(rng.choice([0,.001,1/60,.1,.5,1])).value
        yaw=C.c_float(rng.choice([-360,-180,0,180,360,rng.uniform(-720,720)])).value
        if case<200:
            inp.trigger.actions[:]=range(21);m.action=[10,12,14,16,17][case%5];s.script_phase=2;inp.buttons=0
            inp.clips[m.action].source=[0,369,380,400,453,534,600,601][(case//5)%8]
        expected_m,expected_s,complete,roots=native.step_script(m,s,inp,seconds,yaw,rng)
        effect=Effects()
        assert lib.bk_player_script_step(C.byref(m),C.byref(s),seconds,yaw,C.byref(inp),C.byref(effect),error),(case,error.value)
        assert effect.complete==complete and effect.placements==len(roots),(case,effect.complete,complete,effect.placements,len(roots))
        assert s.script_phase==expected_s.script_phase
        worst=max(worst,compare_movement(m,expected_m,case),compare(s.trigger,expected_s.trigger,case))
        for actual,wanted in zip(effect.roots,roots):
            for a,b in zip(actual.world,wanted):
                d=abs(a-b)/max(1,abs(b));worst=max(worst,d)
                assert math.isfinite(d) and d<3e-6,(case,'root',a,b)
        counts[len(roots)]+=1
        if case%100==0:
            held=(bytes(m),bytes(s),bytes(effect))
            assert not lib.bk_player_script_step(C.byref(m),C.byref(s),-1,yaw,C.byref(inp),C.byref(effect),error)
            assert held==(bytes(m),bytes(s),bytes(effect));rejects+=1
    assert all(native.script_branches.values()),native.script_branches
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=14000,root_write_counts=counts,max_relative_error=worst,atomic_rejections=rejects,branches={hex(k):v for k,v in native.script_branches.items()},hooks=['device input4b76c2 only; root/branch hooks observe without replacing instructions'],scope='Complete4c0d9e plus4c20ee, trig, matrix-angle blend4adcd9 and root42407a. No collision/animation advance/full-game claim.')
    (ROOT/'local/original-player-script-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
