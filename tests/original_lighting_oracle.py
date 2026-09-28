"""Compare parsed light parameters, node attachment and ambient packing with x86.

Original D3D calls are captured, not executed. This verifies submitted state,
not Direct3D rasterization or gameplay-dependent light selection.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_FPCW
from model_binding import ROOT,library,decode
from environment_binding import bind
from original_matrix_oracle import machine
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

class Oracle:
    def __init__(self,exe):
        self.uc=machine(exe)
        self.light,self.frame,self.device,self.table=0x3000000,0x3001000,0x3002000,0x3002100
        self.set_light,self.set_state=0x3003000,0x3003010
        self.write(self.device,self.table);self.write(self.table+0x48,self.set_light);self.write(self.table+0x50,self.set_state)
        self.write(0x6455a0,self.device)
        for address in (self.set_light,self.set_state):self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=address,end=address)
    def write(self,addr,value):self.uc.mem_write(addr,struct.pack('<I',value))
    def hook(self,uc,address,size,_):
        sp=uc.reg_read(UC_X86_REG_ESP);ret,device,key,value=struct.unpack('<4I',uc.mem_read(sp,16));assert device==self.device
        if address==self.set_light:self.submitted=bytes(uc.mem_read(value,104))
        else:self.states[key]=value
        uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_ESP,sp+16);uc.reg_write(UC_X86_REG_EIP,ret)
    def call(self,address,*args):
        stack,stop=0x2008000,0x300f000
        self.uc.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args))
        self.uc.reg_write(UC_X86_REG_ESP,stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(address,stop,count=20000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==stop,hex(self.uc.reg_read(UC_X86_REG_EIP))
    def light_state(self,payload,world):
        self.states={};self.submitted=None
        self.uc.mem_write(self.light,bytes(0xe4));self.uc.mem_write(self.frame,bytes(0x434))
        self.uc.mem_write(self.light+0x78,payload);self.uc.mem_write(self.frame+0xc0,struct.pack('<16f',*world))
        self.call(0x425f79,self.light)
        self.call(0x421dd4,self.frame,self.light)
        return self.submitted,self.states
    def disabled_fog(self,record):
        self.states={};bp=0x2008000;source=0x3004000
        self.uc.mem_write(source,record);self.write(bp-0x164,source);self.write(0x645638,0)
        self.uc.reg_write(UC_X86_REG_EBP,bp);self.uc.reg_write(UC_X86_REG_ESP,bp-0xb00)
        self.uc.emu_start(0x41ae69,0x41af06,count=2000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==0x41af06
        assert self.states=={}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path)
    args=p.parse_args();oracle=Oracle(args.exe.read_bytes());lib=bind(library())
    archive=Archive(args.data/'bk3_03.pp');data=archive.read(next(e for e in archive.entries if e.name=='m01_04.x'))
    result,model,error=decode(lib,data);assert result==1,error
    message=C.create_string_buffer(256);world=(C.c_float*(model.contents.frame_count*16))()
    assert lib.bk_model_world_matrices(model,world,len(world),message),message.value
    env=lib.bk_model_environment_create(model,world,message);assert env,message.value
    checks=0;ambient_color=None;source_payloads=[]
    try:
        m=model.contents
        chunks={bytes(m.chunks[i].tag):(m.chunks[i].offset,m.chunks[i].size) for i in range(m.chunk_count)}
        offset,size=chunks[b'LIGH']
        for i in range(env.contents.light_count):
            l=env.contents.lights[i];payload=data[offset+i*172+68:offset+(i+1)*172];source_payloads.append(payload)
            submitted,states=oracle.light_state(payload,list(world[l.frame_index*16:(l.frame_index+1)*16]))
            if l.type==0xffffffff:
                ambient_color=states[139]
                expected=(0xff000000 | round(env.contents.lighting.ambient[0]*255)<<16 |
                          round(env.contents.lighting.ambient[1]*255)<<8 | round(env.contents.lighting.ambient[2]*255))
                assert expected==ambient_color,(hex(expected),hex(ambient_color))
            else:
                expected=struct.pack('<I25f',l.type,*l.diffuse,*l.specular,*l.ambient,*l.position,*l.direction,
                                     l.range,l.falloff,*l.attenuation,l.theta,l.phi)
                assert expected==submitted,(i,expected,submitted)
            checks+=1
        fog_offset,fog_size=chunks[b'FOG '];assert fog_size==32
        oracle.disabled_fog(data[fog_offset:fog_offset+fog_size])
        # Nonidentity node transforms distinguish frame-origin replacement from
        # incorrectly transforming the already exported world-space light position.
        rng=random.Random(1401)
        for _ in range(64):
            w=[2,0,0,0,0,3,0,0,0,0,4,0,*[rng.uniform(-100,100) for _ in range(3)],1]
            w=struct.unpack('<16f',struct.pack('<16f',*w))
            submitted,_=oracle.light_state(source_payloads[1],w)
            assert submitted[52:64]==struct.pack('<3f',*w[12:15])
            checks+=1
        ambient_checks=0
        for _ in range(128):
            rgb=struct.unpack('<3f',struct.pack('<3f',*[rng.random() for _ in range(3)]))
            mutated=bytearray(data);struct.pack_into('<3f',mutated,offset+72,*rgb)
            result,other,error=decode(lib,bytes(mutated));assert result==1,error
            actual=lib.bk_model_environment_create(other,world,message);assert actual,message.value
            try:
                payload=bytes(mutated[offset+68:offset+172]);_,states=oracle.light_state(payload,[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])
                color=0xff000000|round(actual.contents.lighting.ambient[0]*255)<<16|round(actual.contents.lighting.ambient[1]*255)<<8|round(actual.contents.lighting.ambient[2]*255)
                assert color==states[139]
                ambient_checks+=1
            finally:lib.bk_model_environment_destroy(actual);lib.bk_model_destroy(other)
        summary={'passed':True,'original_light_records':7,'point_records_bytes_compared':6,'light_state_and_transform_checks':checks,
                 'ambient_quantization_checks':ambient_checks,'office_ambient_argb':hex(ambient_color),'disabled_fog_no_state_submission':True,
                 'native_functions':['0x425f79 light submission','0x421dd4 frame attachment -> 0x4260b6','0x41ae69..0x41af06 disabled FOG loader'],
                 'scope':'submitted light bytes, frame origins, packed ambient RGB and disabled fog; not original rasterization or per-pass gameplay light selection'}
        (ROOT/'local/original-lighting-oracle.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
    finally:lib.bk_model_environment_destroy(env);lib.bk_model_destroy(model)
if __name__=='__main__':main()
