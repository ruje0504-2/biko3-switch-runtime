"""Run original NPC contact queries including geometry and state-code writes."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Input(C.Structure):
    _fields_=[('group',C.c_int32),('area',C.c_int32),('cursor',C.c_uint32),('actor_position',C.c_float*3),('alpha',C.c_float),('player_position',C.c_float*3),('player_direction',C.c_float*3),('interaction_df',C.c_int8),('interaction_e0',C.c_int8)]
class State(C.Structure):
    _fields_=[('behavior',C.c_int32),('prompt',C.c_uint8),('response',C.c_uint8),('outcome',C.c_uint8)]
class Native:
    actor=0x3000000;stack=0x2008000;stop=0x300f000
    def __init__(self,exe):self.u=machine(exe)
    def run(self,state,inp,query):
        u=self.u
        u.mem_write(self.actor+0x29c,struct.pack('<3f',*inp.actor_position));u.mem_write(self.actor+0x32c,struct.pack('<f',inp.alpha));u.mem_write(self.actor+0x830,struct.pack('<I',inp.cursor));u.mem_write(self.actor+0x850,struct.pack('<i',state.behavior))
        u.mem_write(0x71b7ac,struct.pack('<3f',*inp.player_position));u.mem_write(0x71b7c4,struct.pack('<3f',*inp.player_direction));u.mem_write(0x71bcdf,bytes([inp.interaction_df&255,inp.interaction_e0&255]))
        u.mem_write(0x71ba8c,bytes([state.prompt]));u.mem_write(0x71bcda,bytes([state.response]));u.mem_write(0x71bcd8,bytes([state.outcome]))
        u.mem_write(self.stack,struct.pack('<IIii',self.stop,self.actor,inp.group,inp.area));u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start([0x500b50,0x500e74,0x500ff5][query],self.stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
        return u.reg_read(UC_X86_REG_EAX)&255,State(struct.unpack('<i',u.mem_read(self.actor+0x850,4))[0],u.mem_read(0x71ba8c,1)[0],u.mem_read(0x71bcda,1)[0],u.mem_read(0x71bcd8,1)[0])

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x500ff5)
    lib.bk_npc_contact_query.argtypes=[C.POINTER(State),C.POINTER(Input),C.c_int,C.POINTER(C.c_uint8)];lib.bk_npc_contact_query.restype=C.c_int
    count=0;codes=[[0]*4 for _ in range(3)]
    for case in range(14000):
        group=rng.choice([-1,0,1,2,3,4,5]);area=rng.choice([-1,0,4,5,6,7,8,8,9])
        cursor=rng.choice([0,47,67,83,195,231]);behavior=rng.choice([-1,0,0,1,2,3,4])
        if case%3==0:
            group=case%5;area=[4,5,5,6,5][group];cursor=[67,83,195,47,231][group];behavior=0
        player=[rng.uniform(-100,100) for _ in range(3)]
        actor=[player[0]+rng.choice([-16,-15,0,15,16,40]),player[1]+rng.choice([-20.00001,-20,0,20,20.00001]),player[2]+rng.choice([-16,-15,0,15,16,40])]
        direction=rng.choice([[0,0,0],[0,1,0],[0,0,1],[1,0,0],[rng.uniform(-1,1),0,rng.uniform(-1,1)]])
        inp=Input(group,area,cursor,(C.c_float*3)(*actor),rng.choice([0,.9,.98999995,.99,1]),(C.c_float*3)(*player),(C.c_float*3)(*direction),rng.choice([-128,-1,0,1,2]),rng.choice([-128,-1,0,1,2]))
        for query in range(3):
            state=State(behavior,rng.randrange(256),rng.randrange(256),rng.randrange(256));expected,wanted=native.run(state,inp,query);result=C.c_uint8(255)
            assert lib.bk_npc_contact_query(C.byref(state),C.byref(inp),query,C.byref(result))
            assert bytes(state)==bytes(wanted) and result.value==expected,(case,query,result.value,expected,bytes(state).hex(),bytes(wanted).hex())
            count+=1;codes[query][result.value]+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=count,result_counts=codes,native_functions=['0x500b50','0x500e74','0x500ff5','0x4ae783'],hooks=[],scope='Scripted cursor triggers, waiting interaction and area>=8 early-return code, native XZ segment geometry, height/alpha boundaries and original global state writes. Calling AI branch ordering not yet implied.',x87_control_word='0x037f')
    (ROOT/'local/original-npc-contact-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',count,'contact queries',codes)
if __name__=='__main__':main()
