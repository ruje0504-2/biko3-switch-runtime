"""Execute complete NPC fade and recursive material writes without hooks."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
import sys
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, Model, Material, library, decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def bind(lib):
    for name,args,result in [
        ('bk_material_pose_create',[C.POINTER(Model),C.c_void_p],C.c_void_p),
        ('bk_material_pose_destroy',[C.c_void_p],None),
        ('bk_material_pose_material',[C.c_void_p,C.c_uint32],C.POINTER(Material)),
        ('bk_npc_fade_apply',[C.POINTER(C.c_float),C.c_uint8,C.c_uint,C.c_float,C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32),C.c_void_p],C.c_int),
        ('bk_model_find_frame',[C.POINTER(Model),C.c_char_p,C.POINTER(C.c_uint32),C.c_void_p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result

class Native:
    stack,stop,actor,clip,group,scratch=0x2008000,0x300f000,0x3000000,0x3001000,0x3002000,0x3003000
    def __init__(self,exe):
        self.u=machine(exe);self.u.mem_map(0x4000000,0x800000)
    def word(self,address,value):self.u.mem_write(address,struct.pack('<I',value))
    def alloc(self,size):
        result=self.cursor;self.cursor+=(size+15)&~15;assert self.cursor<0x4800000
        self.u.mem_write(result,bytes(size));return result
    def call(self,address,*args):
        self.u.mem_write(self.stack,struct.pack('<'+'I'*(len(args)+1),self.stop,*args))
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(address,self.stop,count=8000000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
    def create(self,model,root,marks):
        self.cursor=0x4000000;u=self.u;self.materials=[]
        for i in range(model.material_count):
            m=model.materials[i];ptr=self.alloc(0xd0);self.materials.append(ptr)
            self.word(ptr+4,0x3ee);self.word(ptr+0x6c,1)
            u.mem_write(ptr+8,bytes(m.name)+b'\0')
            u.mem_write(self.scratch,C.string_at(C.addressof(m)+Material.diffuse.offset,68))
            self.call(0x43041a,ptr,self.scratch)
        meshes=[]
        for i in range(model.mesh_count):
            mesh=model.meshes[i];children=[]
            for j in range(mesh.first_submesh,mesh.first_submesh+mesh.submesh_count):
                child=self.alloc(0x100);self.word(child+4,0x3ea)
                self.word(child+0xc8,self.materials[model.submeshes[j].material_index]);children.append(child)
            if len(children)==1:meshes.append(children[0])
            else:
                ptr=self.alloc(0x100);refs=self.alloc(len(children)*4)
                if children:u.mem_write(refs,struct.pack('<'+'I'*len(children),*children))
                self.word(ptr+4,0x3f5);self.word(ptr+0x70,len(children));self.word(ptr+0x74,refs);meshes.append(ptr)
        self.frames=[self.alloc(0x280) for _ in range(model.frame_count)]
        children=[[] for _ in self.frames]
        for i,ptr in enumerate(self.frames):
            f=model.frames[i]
            if f.mesh_index!=0xffffffff:self.word(ptr+0x244,meshes[f.mesh_index])
            if f.parent_index!=0xffffffff:children[f.parent_index].append(i)
        for i,indices in enumerate(children):
            self.word(self.frames[i]+0x238,len(indices));previous=None
            for child in indices:
                link=self.alloc(12);self.word(link,self.frames[child])
                if previous is None:self.word(self.frames[i]+0x230,link)
                else:self.word(previous+8,link)
                previous=link
        u.mem_write(self.actor,bytes(0x900));self.word(self.actor,self.clip)
        self.word(self.clip+0x160,self.group);self.word(self.group+0x14,self.frames[root])
        for i,frame in enumerate(marks):self.word(self.actor+0x860+i*4,self.frames[frame])
    def fade(self,alpha,mode,group,seconds):
        self.u.mem_write(self.actor+0x32c,struct.pack('<f',alpha));self.u.mem_write(self.actor+0x331,bytes([mode]))
        self.word(0x7219a8,group);self.u.mem_write(0x733700,struct.pack('<f',seconds))
        self.call(0x4fc7a2,self.actor)
        return struct.unpack('<f',self.u.mem_read(self.actor+0x32c,4))[0]
    def values(self):return [bytes(self.u.mem_read(m+0x70,68)) for m in self.materials]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native,lib,rng=Native(exe),library(),random.Random(0x4fc7a2);bind(lib)
    arc=Archive(args.data/'bk3_01.pp');error=C.create_string_buffer(256);cases=materials=rejections=0;records=[]
    for group in range(5):
        name=f'h0{group+1}_80.x';raw=arc.read(next(e for e in arc.entries if e.name==name))
        ok,model,message=decode(lib,raw);assert ok==1,message
        root=next(i for i in range(model.contents.frame_count) if model.contents.frames[i].parent_index==0xffffffff)
        marks=(C.c_uint32*4)()
        for j,node in enumerate([b'mark_02',b'mark_01',b'mark_00',b'mark_03']):
            value=C.c_uint32();assert lib.bk_model_find_frame(model,node,C.byref(value),error),(name,node,error.value);marks[j]=value.value
        pose=lib.bk_material_pose_create(model,error);assert pose,(name,error.value)
        try:
            originals=[bytes(model.contents.materials[i]) for i in range(model.contents.material_count)]
            native.create(model.contents,root,marks)
            def check():
                nonlocal materials
                for i,wanted in enumerate(native.values()):
                    m=lib.bk_material_pose_material(pose,i).contents
                    actual=C.string_at(C.addressof(m)+Material.diffuse.offset,68)
                    assert actual==wanted,(name,case,i,m.name,struct.unpack('<17f',actual),struct.unpack('<17f',wanted))
                    assert originals[i]==bytes(model.contents.materials[i]);materials+=1
            case=-1;check()
            for case in range(600):
                alpha=C.c_float(rng.choice([-2,-0.0,0,.2,.99,1,1.00001,2,rng.uniform(-2,3)]))
                mode=rng.choice([0,0,0,1,1,1,2,127,128,255]);seconds=C.c_float(rng.choice([0,1/60,.01,.25,1,5])).value
                actor_group=group if case%2==0 else rng.randrange(5)
                wanted=native.fade(alpha.value,mode,actor_group,seconds)
                assert lib.bk_npc_fade_apply(C.byref(alpha),mode,actor_group,seconds,pose,root,marks,error),(name,case,error.value)
                assert struct.pack('<f',alpha.value)==struct.pack('<f',wanted),(name,case,alpha.value,wanted)
                check();cases+=1
                if case%50==0:
                    saved=[bytes(lib.bk_material_pose_material(pose,i).contents) for i in range(model.contents.material_count)]
                    alpha=C.c_float(.5);bad_marks=(C.c_uint32*4)(*marks);bad_marks[3]=0xffffffff
                    assert not lib.bk_npc_fade_apply(C.byref(alpha),0,group,.1,pose,root,bad_marks,error)
                    assert alpha.value==.5 and saved==[bytes(lib.bk_material_pose_material(pose,i).contents) for i in range(model.contents.material_count)];rejections+=1
            records.append(dict(file=name,sha256=hashlib.sha256(raw).hexdigest(),frames=model.contents.frame_count,materials=model.contents.material_count,mark_frames=list(marks)))
            print(name,'600 fade updates PASS',flush=True)
        finally:lib.bk_material_pose_destroy(pose);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=cases,material_comparisons=materials,atomic_rejections=rejections,records=records,native_functions=['0x4fc7a2','0x4b0f6e','0x4b0de5','0x43041a','material getter/reference handling'],hooks=[],x87_control_word='0x037f',scope='Full native fade and recursive material edits over five actual NPC models, preserving shared material identity and original shader parameter blocks. Model/frame/object graph fixture supplied from decoded assets. No GPU upload, per-frame visibility, accessory animation or complete actor loop implied.')
    (ROOT/'local/original-npc-fade-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',cases,'fade calls;',materials,'materials;',rejections,'atomic rejections')

if __name__=='__main__':main()
