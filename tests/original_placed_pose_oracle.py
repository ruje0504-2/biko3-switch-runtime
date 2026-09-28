"""Actual native SRT + root setter + D3DX matrix stack + frame traversal.

No math/traversal hooks. Native hierarchy receives frame names/local matrices;
geometry/effect pointers are absent, so only pose traversal is exercised.
"""
import argparse,ctypes as C,hashlib,json,math,random,struct,sys
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_animation_oracle import Native as AnimationNative
from animation_binding import library,PoseSample,RootTransform
from model_binding import ROOT,decode
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
I=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
class Native(AnimationNative):
    frames=0x20000000;links=0x20100000;matrix_stack=0x20110000;matrices=0x20120000
    argument=0x20140000
    def __init__(self,exe):
        super().__init__(exe);self.uc.mem_map(self.frames,0x200000)
    def word(self,a,v):self.uc.mem_write(a,struct.pack('<I',v))
    def vector(self,a,v):self.uc.mem_write(a,struct.pack('<'+'f'*len(v),*v))
    def call(self,addr,args):
        self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(addr,self.stop,count=20000000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
        return self.uc.reg_read(UC_X86_REG_EAX)
    def bind(self,data,model):
        super().bind(data,model);self.uc.mem_write(self.frames,bytes(0x140000))
        children={i:[] for i in range(model.frame_count)}
        roots=[]
        for i in range(model.frame_count):
            frame=model.frames[i];p=self.frames+i*0x400
            self.uc.mem_write(p+8,frame.name+b'\0');self.vector(p+0x80,frame.local)
            if frame.parent_index==0xffffffff:roots.append(i)
            else:children[frame.parent_index].append(i)
        assert len(roots)==1;self.root=roots[0];self.root_world=None
        assert not any(t[2]==self.root for t in self.tracks)
        for parent,items in children.items():
            p=self.frames+parent*0x400;self.word(p+0x238,len(items))
            if items:self.word(p+0x230,self.links+items[0]*16)
            for j,index in enumerate(items):
                link=self.links+index*16;self.word(link,self.frames+index*0x400)
                self.word(link+8,self.links+items[j+1]*16 if j+1<len(items) else 0)
                self.word(self.frames+index*0x400+0x22c,p)
        # Native matrix stack constructor layout (0x523ec5 / 0x523ef5).
        self.uc.mem_write(self.matrix_stack,struct.pack('<5I',0x53fb10,1024,self.matrices,0,1))
        self.word(0x63f108,self.matrix_stack);self.word(0x63f0fc,1);self.word(0x63f104,1)
        self.word(0x63f100,0);self.word(0x6455f4,0)
    def find(self,name):
        self.uc.mem_write(self.argument,name.encode()+b'\0');self.word(self.argument+256,0)
        ok=self.call(0x425904,struct.pack('<3I',self.frames+self.root*0x400,self.argument,self.argument+256))
        pointer=struct.unpack('<I',self.uc.mem_read(self.argument+256,4))[0]
        return (pointer-self.frames)//0x400 if ok else None
    def compose_world(self,model,local):
        for i,values in enumerate(local):self.vector(self.frames+i*0x400+0x80,values)
        if self.root_world is not None:
            p=self.frames+self.root*0x400;self.vector(p+0x100,I);self.vector(self.argument,self.root_world)
            self.call(0x42407a,struct.pack('<2I',p,self.argument))
        self.word(self.matrix_stack+12,0);self.vector(self.matrices,I)
        self.call(0x42273b,struct.pack('<I',self.frames+self.root*0x400))
        assert struct.unpack('<I',self.uc.mem_read(self.matrix_stack+12,4))[0]==0
        return [v for i in range(model.frame_count) for v in struct.unpack('<16f',self.uc.mem_read(self.frames+i*0x400+0xc0,64))]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(352);records=[]
    arc=Archive(args.data/'bk3_01.pp');heads=['qqq21_atama','atama','kubiX','kubiX','atama','atama']
    for actor,name in enumerate([f'h0{i}_80.x' for i in range(6)]+['cam00_02.x']):
        if actor==6:arc=Archive(args.data/'bk3_04.pp')
        data=arc.read(next(e for e in arc.entries if e.name==name));rc,model,message=decode(lib,data);assert rc==1,message
        err=C.create_string_buffer(256);a=lib.bk_model_animation_create(model,err);assert a,err.value
        try:
            m=model.contents;native.bind(data,m);end=lib.bk_model_animation_duration(a);node_name=heads[actor] if actor<6 else 'Cam_AUTO'
            node=C.c_uint32();assert lib.bk_model_find_frame(model,node_name.encode(),C.byref(node),err),err.value
            assert native.find(node_name)==node.value
            assert native.find(node_name.upper()) is None
            # The actual D3DX traversal is also checked with the base root.
            out=(C.c_float*(m.frame_count*16))();worst=0;checks=0
            for case in range(72):
                root=RootTransform();root.frame=native.root
                angle=rng.uniform(-math.pi,math.pi);c=math.cos(angle);s=math.sin(angle)
                values=[c,0,-s,0,0,1,0,0,s,0,c,0,*[rng.uniform(-500,500) for _ in range(3)],1]
                root.world[:]=values
                native.root_world=list(root.world) if case else None
                request=PoseSample(rng.uniform(0,end*2),rng.uniform(0,end*2),rng.uniform(0,1),case%2,(case//2)%2)
                assert lib.bk_model_animation_pose(a,C.byref(request),C.byref(root) if case else None,out,len(out),err),err.value
                want=native.sample(m,request.from_tick,request.loop,(request.to_tick,request.weight) if request.blend else None)
                for j,(v,w) in enumerate(zip(out,want)):
                    error=abs(v-w)/max(1,abs(w));worst=max(worst,error)
                    assert error<=3e-5,(name,case,j,v,w,error)
                checks+=1
            records.append(dict(entry=name,sha256=hashlib.sha256(data).hexdigest(),frames=m.frame_count,
                                binding=node_name,poses=checks,matrices=checks*m.frame_count,max_normalized_error=worst))
            print(name,checks,'PASS',worst,flush=True)
        finally:lib.bk_model_animation_destroy(a);lib.bk_model_destroy(model)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,
                poses=sum(r['poses'] for r in records),matrices=sum(r['matrices'] for r in records),
                max_normalized_error=max(r['max_normalized_error'] for r in records),
                native_functions=['0x406c6b','0x408927','0x42407a','0x425904','0x42273b','original D3DX matrix-stack vtable 0x53fb10'],
                scope='Native animation, root setter, unique frame-name lookup and hierarchy traversal with original matrix stack. No geometry/effects or game-loop timing.')
    (ROOT/'local/original-placed-pose-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['poses'],report['matrices'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
