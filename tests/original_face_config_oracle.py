"""Execute original FAM loader with only Windows file/string calls replaced."""
import argparse
import ctypes as C
import hashlib
import json
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_matrix_oracle import machine
from model_binding import ROOT, library
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class Slot(C.Structure):
    _fields_ = [('target', C.c_char*256), ('source', C.c_char*256), ('selection', C.c_char*256)]
class Config(C.Structure):
    _fields_ = [('texture_mode', C.c_uint32), ('actor_clip', C.c_char*256), ('source_clip', C.c_char*256), ('eye_materials', (C.c_char*256)*2), ('eye_textures', (C.c_char*256)*2), ('counts', C.c_uint32*2), ('slots', (Slot*4)*2)]
def bind(lib):
    lib.bk_face_config_decode.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(Config), C.c_void_p]
    lib.bk_face_config_decode.restype = C.c_int
class Native:
    stack, stop, texture, config = 0x200e000, 0x300f000, 0x3000000, 0x3001000
    def __init__(self, exe):
        self.u = machine(exe)
        self.imports = {}
        for i, (slot, count, name) in enumerate([(0x53f208,7,'open'),(0x53f210,5,'read'),(0x53f20c,1,'close'),(0x53f214,2,'copy'),(0x53f0f8,2,'compare')]):
            addr=0x300e000+i*16;self.word(slot,addr);self.imports[addr]=(count,name)
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
    def word(self,p,v): self.u.mem_write(p,struct.pack('<I',v))
    def read(self,p): return struct.unpack('<I',self.u.mem_read(p,4))[0]
    def string(self,p):
        data=bytearray()
        while self.u.mem_read(p,1)!=b'\0':data+=self.u.mem_read(p,1);p+=1
        return bytes(data)
    def hook(self,u,addr,size,user):
        count,name=self.imports[addr];sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp)
        args=[self.read(sp+4+i*4) for i in range(count)];result=1
        if name=='open':self.offset=0
        elif name=='read':
            data=self.data[self.offset:self.offset+args[2]];assert len(data)==args[2]
            u.mem_write(args[1],data);self.word(args[3],len(data));self.offset+=len(data)
        elif name=='copy':u.mem_write(args[0],self.string(args[1])+b'\0');result=args[0]
        elif name=='compare':result=int(self.string(args[0])!=self.string(args[1]))
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4+4*count);u.reg_write(UC_X86_REG_EIP,ret)
    def decode(self,data):
        self.data=data;self.u.mem_write(self.texture,bytes(64));self.u.mem_write(self.config,bytes(0x3a02))
        self.u.mem_write(self.stack,struct.pack('<4I',self.stop,self.texture,self.config,0x300d000))
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.emu_start(0x4a6289,self.stop,count=1000000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop and self.u.reg_read(UC_X86_REG_EAX)==1

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();n=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);records=[]
    files=[('loose/'+p.name,p.read_bytes()) for p in sorted(args.data.glob('*.fam'))]
    ar=Archive(args.data/'fambom.pp');files += [('fambom/'+e.name,ar.read(e)) for e in ar.entries if e.name.lower().endswith('.fam')]
    # Empty target stops a group despite later populated slots. Copy bytes up
    # to first NUL, without incorrectly requiring it to be the final byte.
    for count in range(5):
        rows=[b'metadata',b'source.xan',b'left',b'right',b'l.bmp',b'r.bmp']+[b'']*24
        for g in range(2):
            for i in range(4):
                if i!=count:rows[6+g*12+i*3:9+g*12+i*3]=[b'target',b'source',b'-']
        raw=struct.pack('<I',count)
        for s in rows:raw+=struct.pack('<I',len(s)+3)+s+b'\0XY'
        files.append((f'synthetic{count}',raw))
    for name,data in files:
        config=Config();assert lib.bk_face_config_decode(data,len(data),C.byref(config),error),(name,error.value)
        n.decode(data);assert config.texture_mode==n.read(n.texture+0x28)
        for actual,offset in [(config.source_clip,0x200),(config.eye_materials[0].value,0x800),(config.eye_materials[1].value,0x900),(config.eye_textures[0].value,0x300),(config.eye_textures[1].value,0x400)]:assert actual==n.string(n.config+offset),(name,offset)
        for g in range(2):
            assert config.counts[g]==n.u.mem_read(n.config+0xa00+g,1)[0]
            for i in range(4):
                s=config.slots[g][i]
                for actual,offset in [(s.target,0xa02),(s.source,0x1202),(s.selection,0x1a02)]:assert actual==n.string(n.config+offset+g*0x1800+i*256),(name,g,i,offset)
        records.append(dict(name=name,size=len(data),sha256=hashlib.sha256(data).hexdigest(),counts=list(config.counts)))
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=len(files)-5,synthetic=5,records=records,native_function='0x4a6289',scope='Fresh zeroed FAM configuration; native counters, mapping and first-empty termination. Only Windows file IO and lstrcpy/lstrcmp replaced; no model loading or face initialization.')
    (ROOT/'local/original-face-config-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(files),'FAM configurations')
if __name__=='__main__':main()
