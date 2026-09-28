"""Native ENVL loader and indexed skin deformation over original model assets."""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT,Model,Submesh,Vertex,library,decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Influence(C.Structure):
    _fields_=[('index',C.c_uint32),('position',C.c_float*3),('normal',C.c_float*3),('weight',C.c_float)]
class Bone(C.Structure):
    _fields_=[('frame',C.c_uint32),('count',C.c_uint32),('influences',C.POINTER(Influence))]
class Entry(C.Structure):
    _fields_=[('frame',C.c_uint32),('submesh',C.c_uint32),('bone_count',C.c_uint32),('vertex_count',C.c_uint32)]
def bind(lib):
    for name,args,result in [
        ('bk_model_skin_create',[C.POINTER(Model),C.c_void_p],C.c_void_p),
        ('bk_model_skin_destroy',[C.c_void_p],None),
        ('bk_model_skin_count',[C.c_void_p],C.c_uint32),
        ('bk_model_skin_entry',[C.c_void_p,C.c_uint32],C.POINTER(Entry)),
        ('bk_model_skin_bone',[C.c_void_p,C.c_uint32,C.c_uint32],C.POINTER(Bone)),
        ('bk_skin_mesh_create',[C.c_void_p,C.c_uint32,C.POINTER(Submesh),C.c_void_p],C.c_void_p),
        ('bk_skin_mesh_destroy',[C.c_void_p],None),
        ('bk_skin_mesh_vertices',[C.c_void_p],C.POINTER(Vertex)),
        ('bk_skin_mesh_apply',[C.c_void_p,C.POINTER(C.c_float),C.c_size_t,C.POINTER(Vertex),C.c_size_t,C.c_void_p],C.c_int),
        ('bk_model_animation_create',[C.POINTER(Model),C.c_void_p],C.c_void_p),
        ('bk_model_animation_destroy',[C.c_void_p],None),
        ('bk_model_animation_sample',[C.c_void_p,C.c_float,C.c_int,C.POINTER(C.c_float),C.c_size_t,C.c_void_p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
class Native:
    stack,stop,data,out,heap=0x200e000,0x300f000,0x4000000,0x3000000,0x7000000
    def __init__(self,exe):
        self.u=machine(exe);self.u.mem_map(self.data,0x2000000);self.u.mem_map(self.heap,0x4000000)
        for p in [0x42d217,0x42d270,0x415ed8,0x42a0cd]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=p,end=p)
    def word(self,p,n):self.u.mem_write(p,struct.pack('<I',n))
    def read(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
    def alloc(self,n):
        p=self.cursor;self.cursor+=(n+15)&~15;assert self.cursor<self.heap+0x4000000
        if n:self.u.mem_write(p,bytes(n))
        return p
    def hook(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);arg=self.read(sp+4);result=0
        if address==0x42d217:result=self.alloc(arg)
        elif address==0x415ed8:result=self.ids[arg]
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret);u.reg_write(UC_X86_REG_EAX,result)
    def call(self,addr,*args):
        self.u.mem_write(self.stack,struct.pack('<'+'I'*(len(args)+1),self.stop,*args));self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(addr,self.stop,count=100000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
    def load(self,raw,model,chunk):
        data=raw[chunk.offset:chunk.offset+chunk.size];self.cursor=self.heap;self.ids={};self.frames=[];self.meshes=[]
        for frame in model.frames[:model.frame_count]:
            p=self.alloc(0x200);self.ids[frame.id]=p;self.frames.append(p)
        for sub in model.submeshes[:model.submesh_count]:
            p=self.alloc(0x200);v=self.alloc(sub.vertex_count*60);self.u.mem_write(v,C.string_at(sub.vertices,sub.vertex_count*60));self.word(p+0x84,sub.vertex_count);self.word(p+0x7c,v);self.ids[sub.id]=p;self.meshes.append(p)
        self.u.mem_write(self.data,data);self.word(self.stack-0x164,self.data);self.word(self.stack-0x118,self.out)
        self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x3000);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x41b454,0x41b7a9,count=100000000);assert self.u.reg_read(UC_X86_REG_EIP)==0x41b7a9
        assert self.read(self.stack-0x164)==self.data+len(data)
        group=self.read(self.out);count=self.read(group+0x70);items=self.read(group+0x74);self.entries=[self.read(items+i*4) for i in range(count)]
    def pose(self,world):
        for i,p in enumerate(self.frames):self.u.mem_write(p+0xc0,struct.pack('<16f',*world[i*16:i*16+16]))
    def apply(self,index):
        entry=self.entries[index];self.call(0x410a0a,entry);self.call(0x41075e,entry)
        mesh=self.read(entry+0x74);return bytes(self.u.mem_read(self.read(mesh+0x7c),self.read(mesh+0x84)*60))

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256);records=[];entries=bones=influences=samples=vertices=rejects=0;worst=0
    for path in sorted(args.data.glob('*.pp')):
        archive=Archive(path)
        for asset in archive.entries:
            if not asset.name.lower().endswith('.x'):continue
            raw=archive.read(asset)
            if raw[:4]!=b'OBJM':continue
            pos=12;has_skin=False
            while pos<len(raw):
                if raw[pos:pos+4]==b'ENVL':has_skin=True;break
                pos+=8+struct.unpack_from('<I',raw,pos+4)[0]
            if not has_skin:continue
            ok,model,message=decode(lib,raw);assert ok==1,message
            skin=lib.bk_model_skin_create(model,error);assert skin,(path.name,asset.name,error.value)
            meshes=[];animation=None
            try:
                m=model.contents;chunk=next(m.chunks[i] for i in range(m.chunk_count) if m.chunks[i].tag==b'ENVL');native.load(raw,m,chunk)
                count=lib.bk_model_skin_count(skin);assert count==len(native.entries);entries+=count
                for i in range(count):
                    info=lib.bk_model_skin_entry(skin,i).contents;obj=native.entries[i]
                    assert native.read(obj+0x70)==native.frames[info.frame] and native.read(obj+0x74)==native.meshes[info.submesh]
                    assert native.read(native.meshes[info.submesh]+0x108)==obj
                    assert native.read(obj+0x78)==info.bone_count
                    mesh=lib.bk_skin_mesh_create(skin,i,C.byref(m.submeshes[info.submesh]),error);assert mesh,error.value;meshes.append(mesh)
                    for j in range(info.bone_count):
                        bone=lib.bk_model_skin_bone(skin,i,j).contents;ptr=native.read(obj+0x7c)+j*24
                        frame,n,ix,weights,positions,normals=struct.unpack('<6I',native.u.mem_read(ptr,24));assert frame==native.frames[bone.frame] and n==bone.count
                        positions=bytes(native.u.mem_read(positions,n*12));normals=bytes(native.u.mem_read(normals,n*12));ix=bytes(native.u.mem_read(ix,n*4));weights=bytes(native.u.mem_read(weights,n*4))
                        wanted=b''.join(ix[k*4:k*4+4]+positions[k*12:k*12+12]+normals[k*12:k*12+12]+weights[k*4:k*4+4] for k in range(n))
                        assert C.string_at(bone.influences,n*32)==wanted,(path.name,asset.name,i,j)
                        bones+=1;influences+=n
                world=(C.c_float*(m.frame_count*16))();assert lib.bk_model_world_matrices(model,world,len(world),error)
                animated=path.name=='bk3_01.pp' and asset.name in ['h00_80.x','h01_80.x','h02_80.x','h03_80.x','h04_80.x','h05_80.x']
                if animated:animation=lib.bk_model_animation_create(model,error);assert animation,error.value
                times=[None,0,1,37,129] if animated else [None]
                for time in times:
                    if time is not None:assert lib.bk_model_animation_sample(animation,time,1,world,len(world),error),error.value
                    native.pose(world)
                    for i,mesh in enumerate(meshes):
                        info=lib.bk_model_skin_entry(skin,i).contents;wanted=native.apply(i)
                        assert lib.bk_skin_mesh_apply(mesh,world,len(world),None,0,error),(path.name,asset.name,i,error.value)
                        actual=C.string_at(lib.bk_skin_mesh_vertices(mesh),info.vertex_count*60)
                        if actual!=wanted:
                            for v in range(info.vertex_count):
                                a=struct.unpack_from('<7f',actual,v*60);b=struct.unpack_from('<7f',wanted,v*60)
                                for j,(x,y) in enumerate(zip(a,b)):
                                    delta=abs(x-y)/max(1,abs(y));worst=max(worst,delta);assert math.isfinite(delta) and delta<3e-6,(path.name,asset.name,time,i,v,j,x,y)
                                assert actual[v*60+28:(v+1)*60]==wanted[v*60+28:(v+1)*60]
                        if time is None:
                            assert not lib.bk_skin_mesh_apply(mesh,world,len(world)-1,None,0,error)
                            assert C.string_at(lib.bk_skin_mesh_vertices(mesh),len(actual))==actual;rejects+=1
                        samples+=1;vertices+=info.vertex_count
                records.append(dict(pack=path.name,name=asset.name,sha256=hashlib.sha256(raw).hexdigest(),entries=count,poses=len(times)));print(path.name,asset.name,'PASS',flush=True)
            finally:
                for mesh in meshes:lib.bk_skin_mesh_destroy(mesh)
                lib.bk_model_animation_destroy(animation);lib.bk_model_skin_destroy(skin);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),models=len(records),entries=entries,bones=bones,influences=influences,samples=samples,vertex_comparisons=vertices,atomic_rejections=rejects,max_normalized_error=worst,records=records,native_functions=['0x41b454..0x41b7a9','0x410ad0','0x410400','0x410576','0x410b9b','0x410a0a','0x41075e','0x522b0d','0x522bd9'],hooks=['allocator/free, exported-ID lookup and reference release; native storage/memcpy, mesh lock/unlock and all deformation math execute'],scope='All original ENVL chunks against native loader and base-pose deformation; six actors also receive four sampled SRT world poses. No GPU, visibility/draw traversal or application integration.')
    (ROOT/'local/original-skin-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',entries,'entries;',influences,'influences;',samples,'samples;',vertices,'vertices; max error',worst)
if __name__=='__main__':main()
