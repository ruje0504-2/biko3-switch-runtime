"""Original NPC footstep dispatch, shared latches, surfaces and spatial sound.

Only audio release/load/play and DirectSound volume/pan setters are captured.
4fd796,4afe00,surface string tables,4ff4af and50d2a0 run original instructions.
No audio decoder/device playback or full gameplay is claimed.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,library
class Actions(C.Structure):
    _fields_=[('walk',C.c_int32*2),('run',C.c_int32*2),('special',C.c_int32*2),('extra',C.c_int32*2)]
class Input(C.Structure):
    _fields_=[('group',C.c_uint),('area',C.c_uint),('action',C.c_int32),('source_tick',C.c_float),('surface_name',C.c_char_p),('actions',Actions)]
class Output(C.Structure):
    _fields_=[('count',C.c_uint32),('ticks',C.c_int32*8),('sound_file',C.c_char_p)]
class Audio(C.Structure):
    _fields_=[('volume',C.c_int32),('pan',C.c_int32)]
class Native:
    stack,stop,actor,clip,sound,vtable=0x2008000,0x300f000,0x3000000,0x3001000,0x3009000,0x300a000
    volume_stub,pan_stub=0x300b000,0x300b100
    def __init__(self,exe):
        self.u=machine(exe);self.word(self.actor,self.clip);self.word(self.sound,self.vtable)
        self.word(self.vtable+0x3c,self.volume_stub);self.word(self.vtable+0x40,self.pan_stub)
        for p in [0x50dac1,0x50d858,0x46435e,self.volume_stub,self.pan_stub]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=p,end=p)
        self.u.hook_add(UC_HOOK_CODE,self.tick,begin=0x4afe00,end=0x4afe00)
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def string(self,p):return bytes(self.u.mem_read(p,260)).split(b'\0')[0] if p else None
    def tick(self,u,addr,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);self.current_tick=struct.unpack('<i',u.mem_read(sp+8,4))[0]
    def hook(self,u,addr,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0];cleanup=4
        args=struct.unpack('<5I',u.mem_read(sp+4,20))
        if addr==0x50dac1:
            assert args[0]==self.actor+0x338;self.events.append(('release',))
        elif addr==0x50d858:
            assert args[0]==self.actor+0x338 and args[4]==0
            self.events.append(('load',self.current_tick,self.string(args[1]),self.string(args[2]),args[3]))
            self.word(self.actor+0x448,self.sound)
        elif addr==0x46435e:
            assert args[:2]==(self.sound,0);self.events.append(('play',))
        else:
            assert args[0]==self.sound;cleanup=12
            value=C.c_int32(args[1]).value
            if addr==self.volume_stub:self.volume=value;self.events.append(('volume',value))
            else:self.pan=value;self.events.append(('pan',value))
        u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def call(self,addr,data):
        self.u.mem_write(self.stack,struct.pack('<I',self.stop)+data);self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(addr,self.stop,count=10000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
        return self.u.reg_read(UC_X86_REG_EAX)
    def audio(self,source,listener,yaw,volume,attenuation):
        self.events=[];self.word(0xbe9a10,volume)
        self.call(0x50d2a0,struct.pack('<I8fIf',self.sound,*source,0,*listener,yaw,2,attenuation))
        return Audio(self.volume,self.pan)
    def step(self,inp,latches,source,listener,yaw,master,packed):
        self.events=[];self.word(self.actor+0xc,inp.group);self.word(self.actor+0x10,inp.action)
        for offset,v in zip([0x18,0x1c,0x20,0x24,0x68,0x6c,0x70,0x74],[*inp.actions.walk,*inp.actions.run,*inp.actions.special,*inp.actions.extra]):self.word(self.actor+offset,v)
        self.word(self.clip+0x140,0);self.u.mem_write(self.clip+0x1f0,bytes(C.c_float(inp.source_tick)))
        self.u.mem_write(0x709084,bytes(latches));self.u.mem_write(self.actor+0x5c0,inp.surface_name+b'\0')
        self.u.mem_write(self.actor+0x29c,struct.pack('<5f',*source,0,43))
        self.u.mem_write(0x71b7ac,struct.pack('<5f',*listener,0,yaw));self.word(0xbe9a10,master)
        self.u.mem_write(0x5767c8,bytes([packed]));self.call(0x4fd796,struct.pack('<III',self.actor,inp.group,inp.area))
        return bytes(self.u.mem_read(0x709084,426))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);args=p.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x4fd796)
    fp=C.POINTER(C.c_float);bp=C.POINTER(C.c_uint8)
    lib.bk_spatial_audio.argtypes=[C.POINTER(Audio),fp,fp,C.c_float,C.c_int32,C.c_float];lib.bk_spatial_audio.restype=C.c_int
    lib.bk_npc_footsteps.argtypes=[bp,C.c_size_t,C.POINTER(Input),C.POINTER(Output)];lib.bk_npc_footsteps.restype=C.c_int
    lib.bk_timeline_event.argtypes=[bp,C.c_float,C.c_int32,C.c_int8,C.POINTER(C.c_int)];lib.bk_timeline_event.restype=C.c_int
    latches=(C.c_uint8*426)();source=(C.c_float*3)(3,1,6);listener=(C.c_float*3)(-10,2,-4)
    sound_tests=0
    for case in range(720):
        s=(C.c_float*3)(*[rng.uniform(-1000,1000) for _ in range(3)]);l=(C.c_float*3)(*([*s] if case%12==0 else [rng.uniform(-1000,1000) for _ in range(3)]))
        yaw=C.c_float(rng.uniform(-720,720)).value;master=rng.choice([-10000,-6000,-1,0,123]);decay=C.c_float(rng.choice([0,.01,1,6,10])).value
        actual=Audio();assert lib.bk_spatial_audio(C.byref(actual),s,l,yaw,master,decay)
        expected=n.audio(s,l,yaw,master,decay)
        assert bytes(actual)==bytes(expected),(case,actual.volume,actual.pan,expected.volume,expected.pan);sound_tests+=1
    latch_tests=0
    for case in range(6000):
        tick=rng.randrange(426);time=C.c_float(rng.choice([tick,tick-.001,tick+.001,rng.uniform(-1,500)])).value
        mode=rng.choice([-128,-1,0,0,1,1,2,127]);latch=rng.choice([0,1,2,255]);old=C.c_uint8(latch);fired=C.c_int(7)
        n.u.mem_write(0x709084+tick,bytes([latch]));n.u.mem_write(n.clip+0x1f0,bytes(C.c_float(time)))
        expected=n.call(0x4afe00,struct.pack('<III',n.clip,tick,mode&0xffffffff))&255
        assert lib.bk_timeline_event(C.byref(old),time,tick,mode,C.byref(fired))
        assert (old.value,fired.value)==(n.u.mem_read(0x709084+tick,1)[0],expected);latch_tests+=1
    surfaces=[b'',b'NULL',b'Mesh_m2_sound_kareha@M01_01.X',b'Mesh_otibaALL@M04_01.X',b'Mesh_otibaALL@M05_01.X',b'Mesh_m3_mizu_tamari_0@M01_02.X',b'Mesh_mizutamari@M05_01.X',b'Mesh_m1_sound_nuki@M01_00.X',b'Mesh_jariALL@M02_01.X',b'Mesh_jariALL@M03_02.X',b'Mesh_jariALL@M04_01.X',b'Mesh_KusattaALL@M01_03.X',b'mesh_KusattaALL@M01_03.X']
    frames=events=0
    for group in range(5):
        for area in range(9):
            actions=Actions((C.c_int32*2)(1,2),(C.c_int32*2)(4,5),(C.c_int32*2)(10,12),(C.c_int32*2)(7,9))
            for row,action in enumerate([1,2,4,5,10,12,7,9,0]):
                for surface in surfaces:
                    C.memset(latches,0,426)
                    # Large jumps, holds, rewinds, shared latches and descending
                    # group3 early exit all retain their native differences.
                    for tick in [0,40,54,105,185,425,425,70,500]:
                        inp=Input(group,area,action,tick,surface,actions);out=Output()
                        expected=n.step(inp,latches,source,listener,268.87,-50,frames%2)
                        assert lib.bk_npc_footsteps(latches,426,C.byref(inp),C.byref(out))
                        assert bytes(latches)==expected,(group,area,action,tick,'latches')
                        loads=[e for e in n.events if e[0]=='load']
                        assert list(out.ticks)[:out.count]==[x[1] for x in loads],(group,area,action,tick,list(out.ticks)[:out.count],loads)
                        spatial=Audio();assert lib.bk_spatial_audio(C.byref(spatial),source,listener,C.c_float(268.87),-50,6)
                        for i,event in enumerate(loads):
                            expected_name=out.sound_file if frames%2 else b'\\wav\\'+out.sound_file
                            assert event[2]==(b'\\bk3_02.pp' if frames%2 else None) and event[3]==expected_name,(event,out.sound_file)
                            assert n.events[i*5:i*5+5]==[('release',),event,('volume',spatial.volume),('pan',spatial.pan),('play',)]
                        frames+=1;events+=out.count
        print('PASS footsteps group',group,flush=True)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,events=events,spatial_audio_cases=sound_tests,latch_cases=latch_tests,native_functions=['0x4fd796','0x4afe00','0x4c3996','0x4c4116','0x4c489b','0x4c5013','0x4ff4af','0x50d2a0'],scope=__doc__)
    (ROOT/'local/original-footsteps-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',frames,'frames',events,'events',sound_tests,'spatial audio',latch_tests,'latches')
if __name__=='__main__':main()
