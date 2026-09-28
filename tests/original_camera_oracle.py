"""Original camera math/selection oracle. Asset I/O and device calls are isolated.
No Windows process, original rasterization, or complete camera controller runs.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import library, decode, ROOT
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

class Lens(C.Structure):
    _fields_=[(n,C.c_float) for n in ('fov_y','height_over_width','near_z','far_z')]

class Oracle:
    def __init__(self,exe):
        self.uc=machine(exe);self.paths=[];self.nodes=[];self.transforms=[]
        self.device=0x300a000;self.vtable=0x300b000;self.submit=0x300c000
        self.model=0x3007000;self.link=0x3008000;self.node=0x3009000
        self.write(0x6455a0,self.device);self.write(self.device,self.vtable)
        self.write(self.vtable+0x2c,self.submit)
        self.uc.mem_write(self.submit,b'\xc2\x0c\x00')
        self.write(self.model+0x160,self.link);self.write(self.link+0x14,self.node)
        self.uc.hook_add(UC_HOOK_CODE,self.hook)
        self.loading=False;self.lens_setup=None;self.advance=None
    def write(self,addr,value):self.uc.mem_write(addr,struct.pack('<I',value))
    def u32(self,addr):return struct.unpack('<I',self.uc.mem_read(addr,4))[0]
    def string(self,addr):
        if not addr:return ''
        return bytes(self.uc.mem_read(addr,260)).split(b'\0')[0].decode('ascii')
    def ret(self,value=0):
        sp=self.uc.reg_read(UC_X86_REG_ESP)
        self.uc.reg_write(UC_X86_REG_EAX,value)
        self.uc.reg_write(UC_X86_REG_EIP,self.u32(sp))
        self.uc.reg_write(UC_X86_REG_ESP,sp+4)
    def hook(self,uc,addr,size,user):
        sp=uc.reg_read(UC_X86_REG_ESP)
        if addr==self.submit:
            self.transforms.append((self.u32(sp+8),bytes(uc.mem_read(self.u32(sp+12),64))))
            return
        if self.lens_setup is not None and addr in (0x42cf0e,0x42f18c,0x42cf6c):
            values=struct.unpack('<2f' if addr==0x42cf6c else '<f',uc.mem_read(sp+4,8 if addr==0x42cf6c else 4))
            self.lens_setup[hex(addr)]=values;self.ret();return
        if self.advance is not None and addr==0x4026fe:
            self.advance.append(struct.unpack('<f',uc.mem_read(sp+8,4))[0]);self.ret();return
        if not self.loading:return
        def args(n):return struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4))
        if addr==0x4ad8ec:
            dest,prefix,suffix=args(3)
            joined=self.string(prefix)+self.string(suffix)
            uc.mem_write(dest,joined.encode()+b'\0');self.ret(dest)
        elif addr==0x4ad97e:
            directory,filename,source=args(3);path=self.string(source)
            folder,_,name=path.rpartition('\\')
            uc.mem_write(directory,(folder+'\\').encode()+b'\0')
            uc.mem_write(filename,name.encode()+b'\0');self.ret()
        elif addr==0x401074:
            self.paths.append(self.string(args(3)[0]));self.ret(self.model)
        elif addr==0x425904:
            _,name,dest=args(3);self.nodes.append(self.string(name))
            self.write(dest,self.node);self.ret()
        elif addr in {0x42cf0e,0x4230bd,0x422c49,0x424020,0x42892a,0x428a70,0x428bb6,
                      0x4a7109,0x428caf,0x428b69,0x428a23,0x401d24}:
            self.ret()
    def call(self,addr,args):
        stack,stop=0x200e000,0x300f000
        self.uc.mem_write(stack,struct.pack('<I',stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(addr,stop,timeout=1_000_000,count=100000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==stop,hex(self.uc.reg_read(UC_X86_REG_EIP))
    def projection(self,lens):
        self.call(0x523a10,struct.pack('<I4f',0x3000000,*lens))
        values=list(struct.unpack('<16f',self.uc.mem_read(0x3000000,64)))
        values[5]=-values[5]
        return values
    def view(self,world):
        self.uc.mem_write(0x3000100,struct.pack('<16f',*world));self.transforms=[]
        self.call(0x4224cd,struct.pack('<I',0x3000100))
        assert len(self.transforms)==1 and self.transforms[0][0]==2
        return struct.unpack('<16f',self.transforms[0][1])
    def setup_lens(self):
        self.lens_setup={};self.uc.reg_write(UC_X86_REG_ESP,0x200e000)
        self.uc.emu_start(0x4662dd,0x466306,count=100)
        assert self.uc.reg_read(UC_X86_REG_EIP)==0x466306
        result=self.lens_setup;self.lens_setup=None
        return [result['0x42cf0e'][0],result['0x42f18c'][0],*result['0x42cf6c']]
    def camera_delta(self,seconds):
        bp=0x200d000;self.write(bp+8,0x3001000);self.write(0x3001000,self.model)
        self.uc.mem_write(0x733700,struct.pack('<f',seconds))
        self.uc.reg_write(UC_X86_REG_EBP,bp);self.uc.reg_write(UC_X86_REG_ESP,bp-0x1000)
        self.advance=[]
        try:
            self.uc.emu_start(0x4bdc6c,0x4bdc8d,count=100)
            assert self.uc.reg_read(UC_X86_REG_EIP)==0x4bdc8d and len(self.advance)==1
            return self.advance[0]
        finally:self.advance=None
    def selection(self,group,area):
        self.write(0x7219a8,group);self.write(0x7219ac,area)
        self.uc.mem_write(0x5767c8,b'\x01')
        self.uc.mem_write(0x3001000,bytes(0x600));self.paths=[];self.nodes=[]
        self.loading=True
        try:self.call(0x4b79e0,struct.pack('<II',0x3001000,2))
        finally:self.loading=False
        return {'group':group,'area':area,'camera_path':self.paths,'nodes':self.nodes}
    def background(self,group,area):
        bp=0x200d000;stage=0x3005000
        self.uc.mem_write(stage,struct.pack('<2I',group,area));self.write(bp+12,stage)
        self.uc.reg_write(UC_X86_REG_EBP,bp);self.uc.reg_write(UC_X86_REG_ESP,bp-0x1000)
        self.uc.emu_start(0x4f6bfa,0x4f6c15,count=30)
        assert self.uc.reg_read(UC_X86_REG_EIP)==0x4f6c15
        return self.string(self.uc.reg_read(UC_X86_REG_EAX))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path)
    args=p.parse_args();o=Oracle(args.exe.read_bytes());lib=library()
    fp=C.POINTER(C.c_float)
    lib.bk_camera_projection.argtypes=[fp,C.POINTER(Lens)]
    lib.bk_camera_view.argtypes=[fp,fp]
    rng=random.Random(1432);worst={'projection':0,'view':0};out=(C.c_float*16)()
    def compare(kind,expected):
        for a,b in zip(out,expected):
            err=abs(a-b)/max(1,abs(b));worst[kind]=max(worst[kind],err)
            assert err<=2e-5,(kind,a,b,err)
    effective=o.setup_lens();assert effective==[1,.75,.5,126384],effective
    lenses=[effective,[1,.75,1,100000],[.5,.75,1,200000]]
    for _ in range(256):
        near=rng.uniform(.1,100);lenses.append([rng.uniform(.15,2.9),rng.uniform(.25,2),near,near+rng.uniform(1,200000)])
    for values in lenses:
        lens=Lens(*values);assert lib.bk_camera_projection(out,C.byref(lens))
        compare('projection',o.projection([getattr(lens,n) for n,_ in lens._fields_]))
    view_count=0
    for _ in range(256):
        # Diagonally dominant, with scale, shear, reflection and translation.
        world=[0.0]*16;world[15]=1
        for i in range(3):
            for j in range(3):world[i*4+j]=rng.uniform(2,5)*rng.choice([-1,1]) if i==j else rng.uniform(-.2,.2)
            world[12+i]=rng.uniform(-10000,10000)
        w=(C.c_float*16)(*world);assert lib.bk_camera_view(out,w)
        compare('view',o.view(w));view_count+=1
    camera_frames=[];arc=Archive(args.data/'bk3_04.pp')
    for entry in arc.entries:
        if not(entry.name.startswith('cam') and entry.name.endswith('.x')):continue
        data=arc.read(entry)
        if data[:4]!=b'OBJM':continue
        result,model,error=decode(lib,data);assert result==1,error
        try:
            m=model.contents;world=(C.c_float*(16*m.frame_count))();message=C.create_string_buffer(256)
            assert lib.bk_model_world_matrices(model,world,len(world),message)
            for i in range(m.frame_count):
                name=m.frames[i].name.decode()
                if not name.lower().startswith('cam'):continue
                w=(C.c_float*16)(*world[i*16:(i+1)*16]);assert lib.bk_camera_view(out,w)
                compare('view',o.view(w));view_count+=1
                camera_frames.append({'asset':entry.name,'node':name,'frame_index':i})
        finally:lib.bk_model_destroy(model)
    selections=[]
    for group in range(5):
        for area in range(9):
            actual=o.selection(group,area)
            expected='cam00_01.xan' if group==4 and area in (4,5) else 'cam00_02.xan'
            assert actual['camera_path']==['\\'+expected] and actual['nodes']==['Cam_AUTO'],actual
            actual['background_path']=o.background(group,area);selections.append(actual)
    office=next(s for s in selections if s['group']==0 and s['area']==8)
    assert office['background_path']=='\\m01_04\\m01_04.xan'
    for _ in range(128):
        seconds=struct.unpack('<f',struct.pack('<f',rng.uniform(0,.22)))[0]
        assert o.camera_delta(seconds)==struct.unpack('<f',struct.pack('<f',seconds*.5))[0]
    result={'passed':True,'main_loop_lens':effective,'camera_advance_factor':.5,'camera_advance_checks':128,'projection_checks':len(lenses),'view_checks':view_count,'original_base_camera_frames':camera_frames,
            'normalized_max_error':worst,'initialization_mode2_checks':len(selections),'office_initialization':office,
            'selection_scope':'mode 2 loader branches with file/path/device boundaries mocked; not active gameplay/cutscene dispatch',
            'math_scope':'original 0x523a10 projection; 0x4224cd -> 0x522eea view inverse and captured SetTransform(2); clip-Y adapted for Vulkan',
            'camera_track_sampling_implemented':False,'office_active_pose_verified':False,
            'native_addresses':['0x523a10','0x4224cd','0x522eea','0x4b79e0','0x4bef54','0x4f6bfa..0x4f6c15','0x4662dd..0x466306','0x4bdc6c..0x4bdc8d']}
    (ROOT/'local/original-camera-oracle.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='original_base_camera_frames'},indent=2))
if __name__=='__main__':main()
