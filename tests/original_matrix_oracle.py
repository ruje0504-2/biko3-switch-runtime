"""Compare CPU frame composition to the original isolated x86 matrix function."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import sys
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from model_binding import library,decode

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def machine(exe):
    if hashlib.sha256(exe).hexdigest()!='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e':
        raise ValueError('unsupported original EXE revision')
    pe=pefile.PE(data=exe);image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    uc=Uc(UC_ARCH_X86,UC_MODE_32);uc.mem_map(base,(len(image)+4095)&~4095);uc.mem_write(base,image)
    uc.mem_map(0x2000000,65536);uc.mem_map(0x3000000,65536)
    return uc

def multiply(uc,a,b):
    stack,dest,left,right,stop=0x2008000,0x3000000,0x3000100,0x3000200,0x300f000
    uc.mem_write(left,struct.pack('<16f',*a));uc.mem_write(right,struct.pack('<16f',*b))
    uc.mem_write(stack,struct.pack('<4I',stop,dest,left,right))
    uc.reg_write(UC_X86_REG_ESP,stack);uc.reg_write(UC_X86_REG_FPCW,0x037f)
    uc.emu_start(0x522d9a,stop,timeout=1_000_000,count=10000)
    if uc.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError('native matrix function did not return')
    return struct.unpack('<16f',uc.mem_read(dest,64))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path)
    args=p.parse_args();uc=machine(args.exe.read_bytes());lib=library();results=[]
    # Noncommuting synthetic pair distinguishes local*parent from parent*local.
    a=[1,0,0,0,0,1,0,0,0,0,1,0,3,0,0,1];b=[2,0,0,0,0,1,0,0,0,0,1,0,2,0,0,1]
    assert multiply(uc,a,b)[12]==8
    for pack,name in [('bk3_03.pp','m00_10.x'),('bk3_03.pp','m01_04.x'),('bk3_07.pp','train_4ryou.x'),('bk3_01.pp','h00_80.x')]:
        archive=Archive(args.data/pack);data=archive.read(next(e for e in archive.entries if e.name==name))
        result,out,error=decode(lib,data);assert result==1,error
        try:
            m=out.contents;world=(C.c_float*(m.frame_count*16))();error=C.create_string_buffer(256)
            assert lib.bk_model_world_matrices(out,world,len(world),error),error.value
            native={};remaining=set(range(m.frame_count));composed=0;worst=0
            while remaining:
                ready=[i for i in remaining if m.frames[i].parent_index==0xffffffff or m.frames[i].parent_index in native]
                if not ready:raise ValueError('hierarchy unresolved')
                for i in ready:
                    f=m.frames[i];values=list(f.local)
                    if f.parent_index!=0xffffffff:
                        values=multiply(uc,values,native[f.parent_index]);composed+=1
                    native[i]=values;remaining.remove(i)
                    for j,v in enumerate(values):
                        diff=abs(world[i*16+j]-v);worst=max(worst,diff)
                        if diff>2e-5*max(1,abs(v)):raise ValueError(f'{name}: frame {i} matrix disagreement')
            results.append({'archive':pack,'name':name,'frames':m.frame_count,'native_compositions':composed,'max_absolute_error':worst})
        finally:lib.bk_model_destroy(out)
    summary={'checks':results,'native_function':'0x522d9a','native_compositions':sum(x['native_compositions'] for x in results),'passed':True}
    (ROOT/'local/original-matrix-oracle.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
