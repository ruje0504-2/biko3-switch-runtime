"""Original eye FRAM lookup, alternate texture paths, selection and release."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_face_config_oracle import Native as ConfigNative, Config
from original_face_assets_oracle import bind as face_bind
from model_binding import ROOT, Model, library, decode
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

class Binding(C.Structure):
    _fields_ = [('frames', C.c_uint32*2), ('target', C.c_uint32), ('mode', C.c_uint32)]
class Image(C.Structure):
    _fields_ = [('width', C.c_uint32), ('height', C.c_uint32), ('rgba', C.c_void_p)]
def bind(lib):
    face_bind(lib)
    for name,args,result in [
        ('bk_eye_assets_create',[C.c_void_p,C.c_char_p,C.POINTER(Model),C.c_char_p,C.POINTER(Config),C.c_void_p],C.c_void_p),
        ('bk_eye_assets_destroy',[C.c_void_p],None),
        ('bk_eye_assets_binding',[C.c_void_p],C.POINTER(Binding)),
        ('bk_eye_assets_select',[C.c_void_p,C.c_uint32,C.c_void_p],C.c_int),
        ('bk_eye_assets_selected',[C.c_void_p],C.c_uint32),
        ('bk_eye_assets_image',[C.c_void_p,C.c_uint32,C.POINTER(C.c_int)],C.POINTER(Image))]:
        f=getattr(lib,name);f.argtypes=args;f.restype=result

class Native(ConfigNative):
    stack=0x2009000
    def __init__(self,exe):
        super().__init__(exe)
        self.u.mem_map(0x4000000,0x200000)
        for a in [0x44237e,0x443185]:self.u.hook_add(UC_HOOK_CODE,self.resource,begin=a,end=a)
        self.paths=[];self.freed=[]
    def resource(self,u,addr,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);arg=self.read(sp+4)
        if addr==0x44237e:
            name=self.string(self.read(sp+8));self.paths.append(name)
            self.word(arg,0x4f00000+len(self.paths)*0x100)
        else:self.freed.append(arg)
        u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.read(sp))
    def call(self,address,*args):
        self.call_bytes(address,struct.pack('<'+'I'*len(args),*args))
        return self.u.reg_read(UC_X86_REG_EAX)
    def call_bytes(self,address,args):
        self.u.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.emu_start(address,self.stop,count=3000000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
    def seed(self,model,filename):
        self.paths=[];self.freed=[]
        self.u.mem_write(0x4000000,bytes(0x200000));root=None
        self.frames={0x4000000+i*0x400:i for i in range(model.frame_count)}
        children={i:[] for i in range(model.frame_count)}
        for i in range(model.frame_count):
            f=model.frames[i];p=0x4000000+i*0x400
            self.u.mem_write(p+8,f.name+b'\0')
            if f.parent_index==0xffffffff:
                assert root is None;root=p
            else:children[f.parent_index].append(i)
        node=0x4100000
        for i,items in children.items():
            p=0x4000000+i*0x400;self.word(p+0x238,len(items));self.word(p+0x230,node if items else 0)
            for j,child in enumerate(items):
                self.word(node,0x4000000+child*0x400);self.word(node+8,node+16 if j+1<len(items) else 0);node+=16
        args=struct.pack('<I',self.texture)+bytes(self.u.mem_read(self.config,0x3a02))+b'\0\0'
        self.call_bytes(0x4a07da,args+struct.pack('<I',root))
        self.word(0x645614,0x4120000);self.word(0x645618,model.submesh_count)
        for i in range(model.submesh_count):
            s=model.submeshes[i];m=model.meshes[s.mesh_index]
            name=m.name+(b'_'+str(i-m.first_submesh).encode() if m.submesh_count>1 else b'')+b'@'+filename.upper()
            r=0x4120000+i*0x88;self.u.mem_write(r,name+b'\0');self.word(r+0x80,0x4140000+i*0x100);self.word(r+0x84,1002)
        target=self.call(0x429c6b,self.config+0xa02,1002);self.word(self.texture+0x14,target)
        index=(target-0x4140000)//0x100 if target else 0xffffffff
        base=0x4e00000 if target and model.submeshes[index].texture_count and model.submeshes[index].texture_indices[0]!=0xffffffff else 0
        self.word(self.texture,base)
        if target:
            self.word(target+0x98,base);self.u.mem_write(0x300c000,b'bk3_01\0')
            # Builder only calls the replacement loader after target lookup succeeds.
            args=struct.pack('<I',self.texture)+bytes(self.u.mem_read(self.config,0x3a02))+b'\0\0'
            self.call_bytes(0x4f2430,args+struct.pack('<I',0x300c000))
        return index,target

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);error=C.create_string_buffer(256)
    store=lib.bk_resources_create(error);assert store
    assert lib.bk_resources_mount(store,b'bk3_01',str(a.data/'bk3_01.pp').encode(),error),error.value
    ar=Archive(a.data/'bk3_01.pp');entries={e.name.encode():e for e in ar.entries};records=[];commands=0
    rng=random.Random(0x4a07a9)
    try:
        for path in sorted(a.data.glob('*.fam')):
            raw=path.read_bytes();c=Config();assert lib.bk_face_config_decode(raw,len(raw),C.byref(c),error)
            if c.actor_clip not in entries:continue
            name=ar.read(entries[c.actor_clip])[:256].split(b'\0')[0];data=ar.read(entries[name])
            ok,m,msg=decode(lib,data);assert ok,msg
            e=lib.bk_eye_assets_create(store,b'bk3_01',m,name,C.byref(c),error);assert e,(path.name,error.value)
            try:
                n.decode(raw);target_index,target=n.seed(m.contents,name);binding=lib.bk_eye_assets_binding(e).contents
                assert list(binding.frames)==[n.frames.get(n.read(n.texture+off),0xffffffff) for off in [0x18,0x1c]],path.name
                assert binding.target==target_index and binding.mode==n.read(n.texture+0x28)
                expected_path=b'bk3_01\\'+c.eye_textures[1].value
                assert n.paths==([expected_path] if target and c.eye_textures[1].value else []),(path.name,n.paths,expected_path)
                assert bool(lib.bk_eye_assets_image(e,1,None))==bool(n.read(n.texture+4))
                for slot in [1,4,0,2,3]+[rng.randrange(5) for _ in range(251)]:
                    n.call(0x4a07a9,n.texture,slot);assert lib.bk_eye_assets_select(e,slot,error)
                    actual=lib.bk_eye_assets_selected(e)
                    if target:assert n.read(target+0x98)==n.read(n.texture+actual*4),(path.name,slot,actual)
                    assert n.read(n.texture+0x24)==0;commands+=1
                alternate=n.read(n.texture+4);base=n.read(n.texture)
                n.call(0x4a0ddc,n.texture)
                if target:assert n.read(target+0x98)==base
                assert n.freed==([alternate] if alternate else []) and not any(n.read(n.texture+4*i) for i in range(1,5))
                records.append(dict(config=path.name,config_sha256=hashlib.sha256(raw).hexdigest(),model=name.decode(),model_sha256=hashlib.sha256(data).hexdigest(),frames=list(binding.frames),target=binding.target,texture_mode=binding.mode,alternate=c.eye_textures[1].value.decode(),paths=[p.decode() for p in n.paths]))
                print(path.name,'PASS',flush=True)
            finally:lib.bk_eye_assets_destroy(e);lib.bk_model_destroy(m)
    finally:lib.bk_resources_destroy(store)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=len(records),selection_commands=commands,records=records,native_functions=['0x4a6289','0x4a07da','0x425904','0x429c6b','0x4f2430','0x4a07a9','0x4a0ddc'],scope='Actual 20 primary bk3_01 FAM/model bindings, original frame hierarchy lookup, registry target lookup, texture filenames, pointer switches and release ownership. File/texture resource boundaries substituted. No gaze or full game frame.')
    (ROOT/'local/original-eye-assets-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',len(records),'eye profiles;',commands,'selection commands')
if __name__=='__main__':main()
