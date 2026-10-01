"""Original46EB86/47011B/46FEB7/471A3F/47204F album controls and43ED45 vertices.
Filesystem/catalog generation, texture allocation, device draw and system audio
are explicit services. Native hit testing, page/tab logic, fades, deletion
selection and slideshow timing execute unchanged. No application acceptance.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_zoom_sprite_oracle import Native as Base
from model_binding import ROOT, library

class Sprite(C.Structure):
    _fields_ = [('rect',C.c_float*4),('uv',C.c_float*4),('scale',C.c_float*2),
                ('alpha',C.c_float),('hidden',C.c_uint32)]
class State(C.Structure):
    _fields_ = [('sprites',Sprite*42),('loaded',C.c_uint64)] + [(n,C.c_float) for n in
        ['scale','curtain','spare_alpha','spare_duration','progress']] + [(n,C.c_int32) for n in
        ['state','page','hover','group','exit']] + [('counts',C.c_int32*5),('count',C.c_int32),
        ('live_count',C.c_int32),('selected',C.c_int32*5),('tab_hover',C.c_int32*5),
        ('item_hover',C.c_int32*20),('button_hover',C.c_int32*4),('spare_hover',C.c_int32*3),
        ('deleted',(C.c_int32*100)*5)] + [(n,C.c_int32) for n in
        ['catalog_index','catalog_subpage','slide_index','erase']] + [('slide_ms',C.c_uint32),('image_generation',C.c_uint32*42)]
class Input(C.Structure):
    _fields_ = [('x',C.c_int32),('y',C.c_int32),('delta_ms',C.c_uint32),
                ('confirm',C.c_uint8),('back',C.c_uint8)]
class Draw(C.Structure):
    _fields_ = [('slot',C.c_uint),('generation',C.c_uint),('corners',C.c_float*4),('uv',C.c_float*4),('alpha',C.c_float)]
class Frame(C.Structure):
    _fields_ = [('count',C.c_uint),('draws',Draw*64)]
Scan=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_int32),C.c_void_p)
Catalog=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p)
Select=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int32),C.c_void_p)
Image=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int,C.c_uint,C.c_uint,C.c_uint32,C.POINTER(C.c_int),C.c_void_p)
Probe=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
Sound=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Playing=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
class Ops(C.Structure):
    _fields_ = [('context',C.c_void_p),('scan',Scan),('catalog',Catalog),('select',Select),
                ('image',Image),('probe',Probe),('remove',Catalog),('sound',Sound),('playing',Playing)]

SLOTS=[0x69db54+i*4 for i in range(30)]+[0x69dcd0+i*4 for i in range(10)]+[0x69db4c,0x69da2c]
GLOBALS=dict(scale=0x721af4,curtain=0x54860c,spare_alpha=0x548610,spare_duration=0x548614,
    progress=0x69e550,state=0x69dd04,page=0x69dcf8,hover=0x69dcfc,group=0x69dd00,exit=0x69dd08,
    count=0x69eb80,live_count=0x69eb88,selected=0x5485f8,tab_hover=0x69dd0c,
    item_hover=0x69e4f4,button_hover=0x69e554,spare_hover=0x69e564,deleted=0x69dd24,
    catalog_index=0x69e544,catalog_subpage=0x69e548,slide_index=0x69e54c,erase=0x69e574,slide_ms=0x69e570)

COUNTS=[0x69eb90,0x69eb84,0x69eb8c,0x69eb78,0x69eb74]

def snap(s):
    return bytes(s)[State.scale.offset:]

class Native(Base):
    def __init__(self,exe,names):
        super().__init__(exe);self.names=names;self.events=[];self.vertices=[];self.creations=[]
        self.counts=[21,42,0,65,100];self.live=list(self.counts);self.present=True;self.active=0
        self.u.mem_map(0x4200000,0x20000);self.u.mem_map(0x4300000,0x40000)
        self.u.mem_write(0x5767c8,b'\1')
        for i in range(6):self.wi(0xbeee10+i*0x120,0x4200000+i*0x40);self.wi(0x4200000+i*0x40,0x4201000)
        self.wi(0x4201024,0x300e500)
        for i,a in enumerate([0x53f214,0x53f1f4,0x53f21c]):self.wi(a,0x300e000+i*0x100)
        self.wi(0x53f0f8,0x300e300)
        for a in [0x4ad8ec,0x4a736b,0x4a7276,0x43e583,0x43e91b,0x4a0426,0x5225ad,
                  0x428bb6,0x428caf,0x4a6d1c,0x4a6d44,0x4a6d35,0x4a6d5d,
                  0x473990,0x472dc0,0x472dcf,0x47440d,0x4743fe,0x4741bc,0x4ad2bf,0x52043e,0x520538,
                  0x53491b,0x300e000,0x300e100,0x300e200,0x300e300,0x300e500]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def si(self,a):return C.c_int32(self.ri(a)).value
    def event(self,*args):self.events.append(args)
    def list_names(self,s):
        self.wi(0x69eb94,0x4310000);self.wi(0x69ebac,0x4320000)
        for g in range(5):
            self.wi(0x69eb98+g*4,0x4301000+g*0x1000)
            names=b''.join((f'g{g}_{i:03d}.bmp'.encode()+bytes(26))[:26] for i in range(102))
            self.u.mem_write(0x4301000+g*0x1000,names)
        self.u.mem_write(0x4310000,bytes(self.u.mem_read(0x4301000+s.group*0x1000,27*101)))
        self.u.mem_write(0x4320000,bytes(self.u.mem_read(0x4301000+s.group*0x1000,27*101)))
    def hook(self,u,a,size,data):
        sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<8I',u.mem_read(sp+4,32));pop=4;r=0
        if a in [0x443bb8,0x443be5]:return super().hook(u,a,size,data)
        if a==0x43e96a:
            if args[0] and not self.ri(args[0]+0x70):
                slot=next(i for i,p in enumerate(SLOTS) if self.ri(p)==args[0])
                i=(args[0]-0x4000000)//0x400
                self.vertices.append((slot,bytes(u.mem_read(self.verts(i),192))))
        elif a==0x4ad8ec:u.mem_write(args[0],b'assets\0')
        elif a==0x4a7276:r=args[0]
        elif a==0x300e000:u.mem_write(args[0],self.text(args[1]).encode()+b'\0');r=args[0];pop=12
        elif a==0x300e100:
            raw=self.text(args[1]).encode()[:args[2]];u.mem_write(args[0],raw+b'\0');r=args[0];pop=16
        elif a==0x300e200:
            u.mem_write(args[0],(self.text(args[0])+self.text(args[1])).encode()+b'\0');r=args[0];pop=12
        elif a==0x300e300:
            left,right=[self.text(p) for p in args[:2]];r=(left>right)-(left<right);pop=12
        elif a==0x53491b:
            fmt=self.text(args[1]);assert fmt=='%05d.bmp',fmt
            u.mem_write(args[0],(fmt%args[2]).encode()+b'\0')
        elif a==0x43e583:
            if self.ri(sp)==0x471594:
                self.event('probe',self.ri(0x69dd00),self.ri(0x69e54c))
                self.wi(args[0],self.handle(42) if self.present else 0)
            else:
                name=self.text(args[1]).split('\\')[-1];g=self.ri(0x69dd00)
                if name in self.names:slot=self.names.index(name);kind=1;index=0
                elif name=='ma_06.tga':slot=8;kind=1;index=1
                elif name[:5].isdigit():slot=29;kind=2;index=int(name[:5])
                else:
                    slot=41 if self.ri(0x69dd04)==7 else 40;kind=4 if slot==41 else 3
                    index=self.ri(0x69e54c if slot==41 else 0x69dcfc)
                self.event('image',slot,kind,index,g);self.creations.append((slot,name))
                if slot==40 and not self.present:self.wi(args[0],0)
                else:self.blank(slot);self.wi(args[0],self.handle(slot))
        elif a==0x43e91b:
            slot=(args[0]-0x4000000)//0x400
            if slot!=42:self.event('image',slot,0,0,self.ri(0x69dd00))
        elif a==0x473990:
            self.event('scan');self.wi(0x69eb80,self.counts[0])
            for p,v in zip(COUNTS,self.counts):self.wi(p,v)
        elif a==0x472dcf:self.event('catalog',args[0],args[1])
        elif a==0x47440d:
            g=args[1];self.event('select',g);self.wi(0x69eb88,self.live[g])
            u.mem_write(0x4320000,bytes(u.mem_read(0x4301000+g*0x1000,27*101)))
        elif a==0x4743fe:
            if not getattr(self,'releasing',False):self.event('remove',self.ri(0x69dd00),self.ri(0x69dcfc))
        elif a==0x4ad2bf:self.event('sound',(args[0]-0x4200000)//0x40)
        elif a==0x300e500:
            self.event('playing',(args[0]-0x4200000)//0x40);self.wi(args[1],self.active);pop=12
        elif a==0x52043e:r=0x4310000
        u.reg_write(UC_X86_REG_EAX,r);u.reg_write(UC_X86_REG_EIP,self.ri(sp));u.reg_write(UC_X86_REG_ESP,sp+pop)
    def install(self,s):
        for p,v in zip(COUNTS,s.counts):self.wi(p,v)
        for name,a in GLOBALS.items():
            off=getattr(State,name).offset;size=getattr(State,name).size
            self.u.mem_write(a,bytes(s)[off:off+size])
        for i,p in enumerate(s.sprites):
            self.wi(SLOTS[i],self.handle(i) if s.loaded>>i&1 else 0)
            if not s.loaded>>i&1:continue
            self.blank(i);h=self.handle(i)
            for off,v in zip([0x7c,0x80,0xf4,0xf8,0x8c,0x90,0x88],list(p.rect)+list(p.scale)+[p.alpha]):self.wf(h+off,v)
            self.wi(h+0x70,p.hidden);self.wi(h+0x74,1)
            for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
                v=self.verts(i)+j*32;self.wf(v+0x18,p.uv[x]);self.wf(v+0x1c,p.uv[y])
                self.wi(v+0x10,int(p.alpha*255)<<24|0xffffff)
        self.list_names(s)
    def read(self):
        s=State();s.counts[:]=[self.si(p) for p in COUNTS]
        for name,a in GLOBALS.items():
            C.memmove(C.addressof(s)+getattr(State,name).offset,bytes(self.u.mem_read(a,getattr(State,name).size)),getattr(State,name).size)
        for i,a in enumerate(SLOTS):
            h=self.ri(a)
            if not h:continue
            s.loaded|=1<<i;p=s.sprites[i];j=(h-0x4000000)//0x400
            p.rect[:]=[self.rf(h+o) for o in [0x7c,0x80,0xf4,0xf8]]
            p.scale[:]=[self.rf(h+o) for o in [0x8c,0x90]];p.alpha=self.rf(h+0x88);p.hidden=self.ri(h+0x70)
            p.uv[:]=[self.rf(self.verts(j)+o) for o in [0x18,0x1c,0x58,0x5c]]
        return s
    def initialize(self,s):
        self.install(s);self.events=[];self.creations=[]
        self.call(0x46eb86,b'');self.wi(0x69dd04,1);self.wi(0x69dd08,0)
        return self.read()
    def release(self,s):
        self.install(s);self.events=[];self.releasing=True
        self.call(0x46fc8e,b'');self.releasing=False
        return self.read(),list(self.events)
    def run(self,s,inp):
        self.install(s);self.events=[];self.vertices=[]
        self.wi(0x721af0,inp.delta_ms);self.wi(0x69db50,0x4204000)
        raw=bytearray(44);raw[20]=inp.confirm;raw[21]=inp.back;struct.pack_into('<2i',raw,36,inp.x,inp.y)
        self.u.mem_write(0x4204000,bytes(raw));self.call(0x47011b,b'')
        return self.read(),list(self.vertices),list(self.events)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path)
    ap.add_argument('--output',type=Path,default=ROOT/'local/original-album-menu-oracle.json');a=ap.parse_args()
    exe=a.exe.read_bytes();assert hashlib.sha256(exe).hexdigest()=="a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e"
    lib=library();e=C.create_string_buffer(256);rng=random.Random(0x47011b)
    lib.bk_album_menu_initialize.argtypes=[C.POINTER(State)]
    lib.bk_album_menu_load.argtypes=[C.POINTER(State),C.c_uint,C.POINTER(Ops),C.c_void_p]
    lib.bk_album_menu_step.argtypes=[C.POINTER(State),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
    lib.bk_album_menu_release.argtypes=[C.POINTER(State),C.POINTER(Ops),C.c_void_p]
    lib.bk_album_menu_image.argtypes=[C.c_uint,C.c_uint];lib.bk_album_menu_image.restype=C.c_char_p
    names=[(lib.bk_album_menu_image(i,0) or b'').decode() for i in range(42)]
    n=Native(exe,names);trace=[];steps=draws=services=constructors=releases=0;digest=hashlib.sha256()
    def event(*v):trace.append(v);return 1
    @Scan
    def scan(_,out,err):
        for i,v in enumerate(n.counts):out[i]=v
        return event('scan')
    @Catalog
    def catalog(_,page,index,err):return event('catalog',page,index)
    @Select
    def select(_,g,out,err):out[0]=n.live[g];return event('select',g)
    @Image
    def image(_,slot,kind,index,g,generation,out,err):out[0]=int(n.present if kind==3 else True);return event('image',slot,kind,index,g)
    @Probe
    def probe(_,g,index,out,err):out[0]=int(n.present);return event('probe',g,index)
    @Catalog
    def remove(_,g,index,err):return event('remove',g,index)
    @Sound
    def sound(_,slot,err):return event('sound',slot)
    @Playing
    def playing(_,slot,out,err):out[0]=n.active;return event('playing',slot)
    ops=Ops(None,scan,catalog,select,image,probe,remove,sound,playing)
    def same(s,w,label):
        assert list(s.counts)==list(w.counts),(label,'counts',list(s.counts),list(w.counts))
        for name in GLOBALS:
            off=getattr(State,name).offset;size=getattr(State,name).size
            assert bytes(s)[off:off+size]==bytes(w)[off:off+size],(label,name,bytes(s)[off:off+size].hex(),bytes(w)[off:off+size].hex())
        assert s.loaded==w.loaded,(label,'loaded',hex(s.loaded),hex(w.loaded))
        for i in range(42):
            if s.loaded>>i&1:assert bytes(s.sprites[i])==bytes(w.sprites[i]),(label,'sprite',i,bytes(s.sprites[i]).hex(),bytes(w.sprites[i]).hex())
    def init(width):
        nonlocal constructors
        s=State();lib.bk_album_menu_initialize(C.byref(s));s.scale=width/1280;wanted=n.initialize(s)
        trace.clear();assert lib.bk_album_menu_load(C.byref(s),width,C.byref(ops),e),e.value
        same(s,wanted,('init',width));assert trace==n.events,('init services',trace,n.events)
        assert all(names[slot]==name for slot,name in n.creations);constructors+=1;return s
    def check(s,inp):
        nonlocal steps,draws,services
        wanted,verts,events=n.run(s,inp);trace.clear();f=Frame()
        assert lib.bk_album_menu_step(C.byref(s),C.byref(inp),C.byref(ops),C.byref(f),e),e.value
        same(s,wanted,(steps,bytes(inp).hex()));assert trace==events,('services',steps,trace,events)
        assert f.count==len(verts),('draw count',steps,f.count,len(verts))
        for got,(slot,v) in zip(f.draws[:f.count],verts):
            assert got.slot==slot,('draw slot',steps,got.slot,slot)
            for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
                xy=struct.unpack_from('<2f',v,j*32);uv=struct.unpack_from('<2f',v,j*32+24)
                assert (got.corners[x],got.corners[y])==xy,('geometry',steps,slot,tuple(got.corners),xy)
                assert (got.uv[x],got.uv[y])==uv,('uv',steps,slot,tuple(got.uv),uv)
                assert int(got.alpha*255)==(struct.unpack_from('<I',v,j*32+16)[0]>>24),('alpha',steps,slot,got.alpha)
        digest.update(snap(s));digest.update(bytes(f)[:4+f.count*C.sizeof(Draw)]);digest.update(repr(trace).encode())
        steps+=1;draws+=f.count;services+=len(trace)
    for width in [640,960,1001,1280]:
        s=init(width)
        for _ in range(72):check(s,Input(-10,-10,16))
        assert s.state==0
        # Stateful page/tab navigation, viewing, deleting and timed slideshow.
        def at(slot,confirm=3,back=0,dt=16):
            p=s.sprites[slot].rect;check(s,Input(int(p[0]+2),int(p[1]+2),dt,confirm,back))
        for group in range(5):
            at(31+group*2)
            for _ in range(5):at(5)
            for _ in range(5):at(7)
            if s.count:
                x,y=int(241*s.scale),int(201*s.scale)
                check(s,Input(x,y,16,3));assert s.state==3
                check(s,Input(x,y,16,0,3));assert s.state==0
                at(19);check(s,Input(x,y,16,3));assert s.deleted[group][0]
                check(s,Input(x,y,16,0,3));assert not s.erase
                at(21)
                for _ in range(45):
                    n.present=s.slide_index<s.live_count
                    check(s,Input(-10,-10,100))
                check(s,Input(-10,-10,16,0,3));assert s.state==0
                n.present=True
        at(3)
        for _ in range(34):n.active=int(_<32);check(s,Input(-10,-10,16))
        assert s.exit
        # Independent states cover exact rectangle borders and retained timers.
        base=init(width);base.state=0;base.count=100;base.live_count=99;base.counts[:]=[100]*5
        for case in range(480):
            s=State.from_buffer_copy(base);s.group=case%5;s.page=s.group*5+case//5%5
            s.state=[0,0,0,3,4,6,7,8][case%8];s.slide_index=case%102;s.slide_ms=[0,1984,2000,0xfffffff8][case%4]
            s.curtain=[0,.01,.9,1][case%4];n.present=case%3!=0;n.active=case%2
            for slot in [29,40,41]:
                s.loaded|=1<<slot;s.sprites[slot]=Sprite((C.c_float*4)(0,0,width,width*.75),(C.c_float*4)(0,0,1,1),(C.c_float*2)(1,1),1,0)
            if case%3:
                q=s.sprites[rng.choice([3,5,7,19,21,31,33,35,37,39])].rect
                x=int(q[0]+rng.choice([-1,0,1,q[2]-1,q[2]]));y=int(q[1]+rng.choice([-1,0,1,q[3]-1,q[3]]))
            else:x=rng.randrange(width);y=rng.randrange(width*3//4)
            check(s,Input(x,y,[0,1,16,33,100][case%5],case%4,case//4%4))
        n.present=True
        # Release every construction prefix and a frame holding all dynamic images.
        for prefix in range(34):
            q=State.from_buffer_copy(base)
            order=[8,1,2,3,4,5,6,7,18,19,20,21,22,10,11,12,13,14,15,23,24,25,28,30,31,32,33,34,35,36,37,38,39]
            q.loaded=sum(1<<slot for slot in order[:prefix])
            if prefix==33:q.loaded|=(1<<29)|(1<<40)|(1<<41)
            wanted,events=n.release(q);trace.clear()
            assert lib.bk_album_menu_release(C.byref(q),C.byref(ops),e),e.value
            same(q,wanted,('release',prefix));assert trace==events,('release events',trace,events)
            assert not q.loaded;releases+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,
                releases=releases,frames=steps,draws=draws,vertices=draws*6,service_calls=services,
                max_geometry_error=0,digest=digest.hexdigest(),scope=__doc__)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
