"""Isolated x86 queue branches and two sorting passes; no Windows/D3D execution.

The classifier starts inside the native single/group branch with a prepared mesh.
Sorting receives prepared finite distance keys: camera distance calculation and
full frame traversal are outside this oracle's scope.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS, UC_X86_REG_FPCW
from original_material_oracle import Oracle as MaterialOracle
from model_binding import ROOT, library, decode
from static_binding import bind
from test_model import fixture
from bk3_assets import Archive

class Oracle(MaterialOracle):
    def __init__(self, exe):
        super().__init__(exe)
        self.frame, self.texture = 0x3003000, 0x3003400
        self.uc.mem_map(0x4000000, 0x20000)
        self.entries, self.meshes = 0x4000000, 0x4010000
        for address in (0x42b582,0x42a65f):
            self.uc.hook_add(UC_HOOK_CODE,self.queue,begin=address,end=address)
    def queue(self, uc, address, size, _):
        sp = uc.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<5I',uc.mem_read(sp,20))
        assert args[1:4] == (self.frame+0xc0,self.frame,self.mesh)
        self.classified = (int(address==0x42b582),args[4])
        uc.emu_stop()
    def classify(self, material, has_texture, texture_alpha, grouped):
        self.apply(material,texture_alpha)  # Original setter normalizes encoded alpha.
        self.write(self.mesh+0x94,has_texture)
        self.write(self.mesh+0x98,self.texture if has_texture else 0)
        self.write(self.texture+0x74,texture_alpha)
        bp = 0x2008000
        self.write(bp+8,self.frame); self.write(bp-0x10,self.mesh); self.write(bp-8,self.mesh)
        self.uc.reg_write(UC_X86_REG_EBP,bp); self.uc.reg_write(UC_X86_REG_ESP,bp-0x100)
        self.classified = None
        self.uc.emu_start(0x422a17 if grouped else 0x4228de,0x422c41,count=1000)
        assert self.classified is not None
        return self.classified
    def sort(self, distances, priorities):
        count = len(distances)
        assert count <= 64
        data = bytearray(count*0x11c)
        meshes = bytearray(count*0x200)
        for i,(distance,priority) in enumerate(zip(distances,priorities)):
            struct.pack_into('<II',data,i*0x11c+0x100,i,self.meshes+i*0x200)
            struct.pack_into('<f',data,i*0x11c+0x110,distance)
            struct.pack_into('<I',meshes,i*0x200+0xf4,priority)
        if count:
            self.uc.mem_write(self.entries,bytes(data)); self.uc.mem_write(self.meshes,bytes(meshes))
        self.write(0x6429a8,count); self.write(0x645624,self.entries)
        self.uc.reg_write(UC_X86_REG_EBP,0x2008000)
        self.uc.reg_write(UC_X86_REG_ESP,0x2007000)
        self.uc.reg_write(UC_X86_REG_EFLAGS,2); self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(0x42c56a,0x42cbb9,count=2_000_000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==0x42cbb9
        return [self.read(self.entries+i*0x11c+0x100) for i in range(count)]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path); p.add_argument('data',type=Path)
    args=p.parse_args(); oracle=Oracle(args.exe.read_bytes()); lib=bind(library())
    result, model, message = decode(lib,fixture()); assert result==1,message
    error=C.create_string_buffer(256); flags=(C.c_int*1)(); checks=0
    def compare():
        nonlocal checks
        static=lib.bk_static_model_create(model,flags,error); assert static,error.value
        try:
            m=model.contents
            expected=(static.contents.parts[0].sorted,static.contents.parts[0].depth_write)
            for grouped in (False,True):
                actual=oracle.classify(m.materials[0],m.submeshes[0].texture_count,flags[0],grouped)
                assert expected==actual,(m.materials[0].diffuse[3],flags[0],grouped,expected,actual)
                checks+=1
        finally: lib.bk_static_model_destroy(static)
    values=[-1,-0.0,0,.5,.8,1,1.2,1.5,2]
    for boundary in (.999999,1.000001,.99999,1.00001):
        bits=struct.unpack('<I',struct.pack('<f',boundary))[0]
        values += [struct.unpack('<f',struct.pack('<I',bits+d))[0] for d in (-1,0,1)]
    rng=random.Random(271)
    values += [rng.uniform(-4,4) for _ in range(128)]
    try:
        for a in values:
            model.contents.materials[0].diffuse[3]=a
            for has_texture in (0,1):
                model.contents.submeshes[0].texture_count=has_texture
                for flags[0] in (0,1): compare()
    finally:lib.bk_model_destroy(model)
    synthetic_checks=checks
    archive=Archive(args.data/'bk3_03.pp')
    data=archive.read(next(e for e in archive.entries if e.name=='m01_04.x'))
    result,model,message=decode(lib,data);assert result==1,message
    try:
        m=model.contents
        flags=(C.c_int*m.texture_count)(*[int(m.textures[i].filename.lower().endswith(b'.tga')) for i in range(m.texture_count)])
        static=lib.bk_static_model_create(model,flags,error);assert static,error.value
        try:
            for i in range(m.submesh_count):
                sub=m.submeshes[i];part=static.contents.parts[i]
                ta=flags[sub.texture_indices[0]] if sub.texture_count else 0
                for grouped in (False,True):
                    assert oracle.classify(m.materials[sub.material_index],sub.texture_count,ta,grouped)==(part.sorted,part.depth_write)
                    checks+=1
        finally:lib.bk_static_model_destroy(static)
    finally:lib.bk_model_destroy(model)
    sort_cases=0
    for n in [0,1,2,3,30,64]+[rng.randrange(2,65) for _ in range(96)]:
        distances=(C.c_float*n)(*[rng.choice([1,2,3,4,100,rng.uniform(0,2000)]) for _ in range(n)])
        priorities=(C.c_uint32*n)(*[rng.choice([0,0,1,2,3,4,0xffffffff]) for _ in range(n)])
        expected=oracle.sort(list(distances),list(priorities))
        order=(C.c_uint32*n)(*range(n));lib.bk_static_sort_keys(order,distances,priorities,n)
        assert list(order)==expected,(n,list(order),expected)
        sort_cases+=1
    summary={'passed':True,'queue_checks':checks,'synthetic_queue_checks':synthetic_checks,
             'original_office_queue_checks':checks-synthetic_checks,'sort_cases':sort_cases,
             'native_regions':['0x4228de single mesh','0x422a17 group child','0x42c56a..0x42cbb9 sorting passes'],
             'scope':'branch classification and prepared-key ordering only; not full frame traversal or distance calculation'}
    (ROOT/'local/original-static-oracle.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
