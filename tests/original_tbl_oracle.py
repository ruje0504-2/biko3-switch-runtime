"""Compare table bytes against the original isolated x86 decompression routine.

Unicorn executes only the in-memory decoder, with a local memset shim. No
Windows process, installer, game loop, import, filesystem or network executes.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

sys.path.insert(0, str(Path(__file__).resolve().parent.parent/'tools'))
from bk3_assets import decode_tbl


def run(exe, source):
    pe = pefile.PE(data=exe)
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    machine.mem_map(base, (len(image)+4095)&~4095)
    machine.mem_write(base, image)
    stack, input_, output, stop = 0x2000000, 0x3000000, 0x4000000, 0x5000000
    for address, size in [(stack,65536), (input_,1048576), (output,1048576), (stop,4096)]:
        machine.mem_map(address,size)
    machine.mem_write(input_,source)
    esp = stack+32768
    machine.mem_write(esp,struct.pack('<4I',stop,input_,len(source),output))
    machine.reg_write(UC_X86_REG_ESP,esp)
    def hook(uc, address, size, unused):
        if address == 0x534340:  # statically identified memset
            sp=uc.reg_read(UC_X86_REG_ESP)
            ret,dest,value,count=struct.unpack('<4I',uc.mem_read(sp,16))
            if not (base<=dest<base+len(image) and count<=4096):
                raise ValueError('unexpected native memset call')
            uc.mem_write(dest,bytes([value&255])*count)
            uc.reg_write(UC_X86_REG_EAX,dest)
            uc.reg_write(UC_X86_REG_ESP,sp+4)
            uc.reg_write(UC_X86_REG_EIP,ret)
    machine.hook_add(UC_HOOK_CODE,hook,begin=0x534340,end=0x534340)
    machine.emu_start(0x497f63,stop,timeout=10_000_000,count=20_000_000)
    if machine.reg_read(UC_X86_REG_EIP)!=stop:
        raise ValueError('decoder did not return within instruction/time budget')
    return bytes(machine.mem_read(output,machine.reg_read(UC_X86_REG_EAX)))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path)
    parser.add_argument('data',type=Path)
    args=parser.parse_args()
    exe=args.exe.read_bytes()
    if hashlib.sha256(exe).hexdigest()!='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e':
        raise ValueError('unsupported EXE revision for fixed native addresses')
    count=0
    for p in sorted(args.data.glob('*.tbl')):
        expected=decode_tbl(p.read_bytes())
        native=run(exe,p.read_bytes())
        if len(native)<len(expected) or native[:len(expected)]!=expected:
            raise ValueError(f'original decoder disagreement: {p}')
        print(f'{p.name}: native/Python match, {len(expected)} table bytes',flush=True)
        count+=1
    print(f'PASS: {count} original-native decoder comparisons')


if __name__=='__main__':
    main()
