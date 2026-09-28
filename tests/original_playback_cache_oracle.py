"""Check animation-group zero initialization and cached plain/blended locals."""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_follow_phase_oracle import Native as PhaseNative
from playback_binding import library
from model_binding import ROOT,decode
from bk3_assets import Archive
class Native(PhaseNative):
    def call(self,address,args):
        self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(address,self.stop,count=400000000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        return self.uc.reg_read(UC_X86_REG_EAX)
    def constructor(self):
        def allocate(u,address,size,user):
            sp=u.reg_read(UC_X86_REG_ESP);ret,n=struct.unpack('<II',u.mem_read(sp,8));assert n==0x88
            u.reg_write(UC_X86_REG_EAX,self.group);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
        self.uc.mem_write(self.group,b'\xff'*0x88)
        hook=self.uc.hook_add(UC_HOOK_CODE,allocate,begin=0x42d217,end=0x42d217)
        try:self.call(0x409660,struct.pack('<I',self.argument))
        finally:self.uc.hook_del(hook)
        assert struct.unpack('<I',self.uc.mem_read(self.argument,4))[0]==self.group
        assert bytes(self.uc.mem_read(self.group+0x70,8))==bytes(8)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);native.constructor();lib=library();arc=Archive(args.data/'bk3_01.pp');error=C.create_string_buffer(256)
    data=arc.read(next(e for e in arc.entries if e.name=='h00_80.x'));rc,model,message=decode(lib,data);assert rc==1,message
    xan=bytearray(0x5190);xan[:9]=b'h00_80.x\0';xan[256:265]=b'h00_80.x\0'
    for i in range(3):
        p=512+0x190+i*156;struct.pack_into('<i',xan,p+0x50,20);struct.pack_into('<2f',xan,p+0x54,10*i,30 if i==2 else 20);struct.pack_into('<f',xan,p+0x7c,5 if i==2 else 0)
    clips=lib.bk_clip_set_decode(bytes(xan),len(xan),error);assert clips,error.value;playback=None
    try:
        native.bind(data,model.contents);head=C.c_uint32();assert lib.bk_model_find_frame(model,b'qqq21_atama',C.byref(head),error)
        native.bind_clip(bytes(xan),head.value,0,False)
        playback=lib.bk_model_playback_create(model,clips,0,error);assert playback,error.value
        sequence=[(0,0),(None,0),(1,0),(2,0),(None,1/6),(1,0),(None,0)]
        matrices=0;worst=0;snapshots=[]
        for slot,seconds in sequence:
            if slot is not None:
                native.call(0x401f71,struct.pack('<II',native.clip,slot));assert lib.bk_model_playback_select(playback,slot,0,error)
            seconds=C.c_float(seconds).value
            native.call(0x4026fe,struct.pack('<If',native.clip,seconds));native.publish()
            assert lib.bk_model_playback_advance(playback,seconds,None,error),error.value
            values=[]
            for frame in range(model.contents.frame_count):
                actual=lib.bk_model_playback_frame(playback,frame)[:16];expected=native.floats(native.frames+frame*0x400+0xc0,16);values+=actual
                for a,b in zip(actual,expected):
                    diff=abs(a-b)/max(1,abs(b));worst=max(worst,diff);assert diff<3e-5,(len(snapshots),frame,a,b)
                matrices+=1
            snapshots.append(values)
        assert snapshots[0]==snapshots[1] and snapshots[4]==snapshots[5]==snapshots[6]
        assert snapshots[2]!=snapshots[4]
        report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=len(sequence),matrices=matrices,max_normalized_error=worst,native_functions=['0x409660','0x401f71','0x4026fe','0x4097d6','0x409a94','ANIM SRT','0x42273b'],hooks=['0x42d217 allocation during constructor check only'],scope='Original group constructor initializes plain cache to zero; first plain zero keeps base; blend leaves cache unchanged; matching later plain request keeps blended locals. Synthetic XAN drives actual player ANIM/hierarchy.',x87_control_word='0x037f')
        (ROOT/'local/original-playback-cache-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(sequence),'cache steps,',matrices,'matrices; max error',worst)
    finally:lib.bk_model_playback_destroy(playback);lib.bk_clip_set_destroy(clips);lib.bk_model_destroy(model)
if __name__=='__main__':main()
