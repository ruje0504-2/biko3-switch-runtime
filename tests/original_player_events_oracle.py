"""Whole4c155d and both original tick helpers; only audio I/O is intercepted.

For native unmatched-action uninitialized direction, stack is zero-filled.
This is an explicit defined-port policy, not arbitrary-stack equivalence.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_matrix_oracle import machine
from model_binding import ROOT,library
class State(C.Structure):
    _fields_=[('loop_latched',C.c_int8),('noise',C.c_int8)]
class Input(C.Structure):
    _fields_=[('group',C.c_int32),('area',C.c_int32),('action',C.c_int32),('actions',C.c_int32*21),('source',C.c_float),('surface',C.c_char_p),('voice_present',C.c_int),('voice_playing',C.c_int)]
class Command(C.Structure):
    _fields_=[('file',C.c_char_p),('loop',C.c_int)]
class Output(C.Structure):
    _fields_=[('count',C.c_uint),('commands',Command*2),('default_direction',C.c_int)]
class Native:
    stack,stop,actor,clip,sound,vtable=0x200e000,0x300f000,0x3000000,0x3001000,0x3009000,0x300a000
    status_stub,stop_stub=0x300b000,0x300b100
    def __init__(self,exe):
        self.u=machine(exe);self.word(self.actor,self.clip);self.word(self.sound,self.vtable)
        self.word(self.vtable+0x24,self.status_stub);self.word(self.vtable+0x48,self.stop_stub)
        for p in [0x50dac1,0x50d858,0x46435e,self.status_stub,self.stop_stub]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=p,end=p)
        self.names=[]
        for start,end in [(0x4c3996,0x4c40dc),(0x4c4116,0x4c4860),(0x4c489b,0x4c4fd8)]:
            self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.emu_start(start,end,count=100000);bp=self.u.reg_read(UC_X86_REG_EBP)
            self.names.append([bytes(self.u.mem_read(bp-0x2d00+i*256,256)).split(b'\0')[0] for i in range(45)])
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def string(self,p):return bytes(self.u.mem_read(p,260)).split(b'\0')[0] if p else None
    def hook(self,u,a,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];args=struct.unpack('<5I',u.mem_read(sp+4,20));cleanup=4;result=0
        if a==0x50dac1:self.io.append('release');assert args[0]==self.actor+0x338
        elif a==0x50d858:
            assert args[0]==self.actor+0x338 and args[3]==0 and args[4]==0
            assert self.string(args[1])==(b'\\bk3_02.pp' if self.packed==1 else None)
            raw=self.string(args[2]);self.current_file=raw if self.packed==1 else raw.removeprefix(b'\\wav\\')
            self.word(self.actor+0x448,self.sound);self.io.append('load')
        elif a==0x46435e:
            assert args[0]==self.sound;self.commands.append((self.current_file,args[1]));self.io.append('play')
        elif a==self.status_stub:
            assert args[0]==self.sound;self.word(args[1],1 if self.playing else 0);cleanup=12
        elif a==self.stop_stub:
            assert args[0]==self.sound;self.commands.append((None,0));self.io.append('stop');cleanup=8
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def step(self,state,steps,loops,inp,packed):
        u=self.u;self.packed=packed;self.playing=inp.voice_playing;self.commands=[];self.io=[]
        self.word(self.actor+0x10,inp.action);u.mem_write(self.actor+0x14,bytes(inp.actions));u.mem_write(self.actor+0x450,bytes(C.c_int8(state.loop_latched)));u.mem_write(0x729101,bytes(C.c_int8(state.noise)))
        self.word(self.actor+0x448,self.sound if inp.voice_present else 0);u.mem_write(self.actor+0x5c0,inp.surface+b'\0')
        self.word(self.clip+0x140,0);u.mem_write(self.clip+0x1f0,bytes(C.c_float(inp.source)));u.mem_write(0x708884,bytes(steps));u.mem_write(0x709084,bytes(loops));u.mem_write(0x5767c8,bytes([packed&255]));self.word(0xbe9a10,0)
        # Unknown action leaves ebp-414 undefined. Deliberately define zero;
        # compare/label those cases separately from defined original branches.
        u.mem_write(self.stack-0x8000,bytes(0x8000));u.mem_write(self.stack,struct.pack('<4I',self.stop,self.actor,inp.group,inp.area));u.reg_write(UC_X86_REG_ESP,self.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(0x4c155d,self.stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==self.stop
        expected=State(C.c_int8(u.mem_read(self.actor+0x450,1)[0]).value,C.c_int8(u.mem_read(0x729101,1)[0]).value)
        return expected,bytes(u.mem_read(0x708884,544)),bytes(u.mem_read(0x709084,409))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4c155d)
    lib.bk_player_events.argtypes=[C.POINTER(State),C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(Input),C.POINTER(Output)]
    steps=(C.c_uint8*544)();loops=(C.c_uint8*409)();state=State();commands=defaults=rejects=0;files=set();counts=[0,0,0]
    for case in range(18000):
        inp=Input();inp.group=(case//9)%5;inp.area=case%9
        inp.actions[:]=list(range(21)) if case%7 else [rng.randrange(14) for _ in range(21)]
        inp.action=inp.actions[rng.randrange(21)] if case%11 else 99
        inp.source=rng.choice([0,106.99,107,123,185,200,246,258,368.99,369,400,403,408,443,474,515,543,600,rng.uniform(-100,700)])
        inp.surface=rng.choice([table[inp.group*9+inp.area] for table in n.names]+[b'Mesh_KusattaALL@M01_03.X',b'NULL',b'',b'miss'])
        inp.voice_present=rng.randrange(2);inp.voice_playing=rng.randrange(2)
        if case%4==0:
            state=State(rng.choice([-128,-1,0,1,2,127]),rng.choice([-1,0,1,7]))
            for i in range(544):steps[i]=rng.choice([0,0,1,2,255])
            for i in range(409):loops[i]=rng.choice([0,0,1,255])
        before=(bytes(state),bytes(steps),bytes(loops));expected,es,el=n.step(state,steps,loops,inp,rng.choice([-1,0,1,2]));out=Output()
        assert lib.bk_player_events(C.byref(state),steps,544,loops,409,C.byref(inp),C.byref(out))
        actual=[(out.commands[i].file,out.commands[i].loop) for i in range(out.count)]
        assert (bytes(state),bytes(steps),bytes(loops),actual)==(bytes(expected),es,el,n.commands),(case,inp.action,inp.source,actual,n.commands,bytes(state),bytes(expected))
        counts[out.count]+=1;commands+=out.count;defaults+=out.default_direction;files.update(f.decode() for f,l in actual if f)
        if case%100==0:
            snapshot=bytes(state),bytes(steps),bytes(loops),bytes(out);inp.source=float('nan')
            assert not lib.bk_player_events(C.byref(state),steps,544,loops,409,C.byref(inp),C.byref(out))
            assert snapshot==(bytes(state),bytes(steps),bytes(loops),bytes(out));rejects+=1
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=18000,commands=commands,command_counts=counts,files=sorted(files),default_direction_cases=defaults,atomic_rejections=rejects,hooks=['audio release/load/play/status/stop only'],scope=__doc__)
    (ROOT/'local/original-player-events-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
