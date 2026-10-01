"""Original4ab0c0/4a0590 construction,4ac224/43ed45 vertices and4ac6a7 controls.
Only asset creation, device draw/lock, DirectSound, rand and pointer warp are
boundaries. Native menu hit testing, slider math and event order execute.
This is a CPU/geometry oracle, not application/storage or Switch acceptance.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_zoom_sprite_oracle import Native as Base
from model_binding import ROOT, library
class State(C.Structure):
    _fields_ = [('rect', (C.c_float*4)*23), ('slider', C.c_float*3),
                ('minimum', C.c_float), ('maximum', C.c_float), ('values', C.c_int32*3),
                ('selected', C.c_int32), ('row', C.c_int32), ('mouse_result', C.c_int32),
                ('dragging', C.c_int32*3), ('previewing', C.c_int32), ('samples', C.c_int32*3),
                ('bar_rect', (C.c_int32*4)*3), ('disabled', C.c_int32*6),
                ('cursor', C.c_int32*2), ('sound_mask', C.c_uint32)]
class Input(C.Structure):
    _fields_ = [('x', C.c_int32), ('y', C.c_int32)] + [(n,C.c_uint8) for n in
        ['mouse','left','right','up','down','confirm','back','fast']]
class Draw(C.Structure):
    _fields_ = [('slot',C.c_uint),('corners',C.c_float*4)]
class Frame(C.Structure):
    _fields_ = [('count',C.c_uint),('draws',Draw*24)]
Play = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_int32,C.c_void_p)
Stop = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Playing = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.POINTER(C.c_int),C.c_void_p)
Random = C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(C.c_uint32),C.c_void_p)
Warp = C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_int32,C.c_int32,C.c_void_p)
class Ops(C.Structure):
    _fields_ = [('context',C.c_void_p),('play',Play),('stop',Stop),('gain',Play),
                ('playing',Playing),('random',Random),('warp',Warp)]
SAMPLE_GLOBALS = [0x7086f4,0x7087b8,0x7087bc]
MASK = sum(1<<i for i in [0,3,6,9,10,11])
def snap(s):
    return (tuple(s.slider),tuple(s.values),s.selected,s.row,s.mouse_result,
            tuple(s.dragging),s.previewing,tuple(s.samples),tuple(s.cursor))
def fbits(v): return struct.unpack('<I',struct.pack('<f',v))[0]
class Native(Base):
    def __init__(self,exe):
        super().__init__(exe); self.images=[]; self.sounds=[]; self.events=[]; self.record=False
        self.cfg = (-1500,-2500,-1999); self.draw_meta=[]; self.active=[0]*16; self.random=0
        self.u.mem_map(0x4200000,0x10000)
        self.wi(0x53f214,0x300e000);self.wi(0x53f0f8,0x300e300)
        for a in [0x4ad8ec,0x4a736b,0x4a7276,0x43e583,0x4a06df,0x4acf9a,
                  0x4a6d1c,0x4a6d44,0x4a6d35,0x4a6d5d,0x428bb6,0x428caf,
                  0x4ad2bf,0x4ad34a,0x49a63e,0x534a34,0x300e000,0x300e100,0x300e200,0x300e300]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
        self.wi(0x4201000+0x3c,0x300e100); self.wi(0x4201000+0x24,0x300e200)
        self.u.mem_write(0x5767c8,b'\1')
    def si(self,a):return C.c_int32(self.ri(a)).value
    def event(self,*v):
        if self.record: self.events.append((*v,snap(self.read())))
    def sh(self,slot): return 0x4200000+slot*0x40
    def hook(self,u,a,size,data):
        sp=u.reg_read(UC_X86_REG_ESP); args=struct.unpack('<8I',u.mem_read(sp+4,32));pop=4;r=0
        if a in [0x443bb8,0x443be5]:return super().hook(u,a,size,data)
        if a==0x43e96a:
            if not self.ri(args[0]+0x70):
                idx=self.handles.index(args[0]);i=(args[0]-0x4000000)//0x400
                raw=bytes(u.mem_read(self.verts(i),192))
                self.draw_meta.append((idx,(*struct.unpack_from('<2f',raw),*struct.unpack_from('<2f',raw,64))))
        elif a==0x4ad8ec:u.mem_write(args[0],b'assets\0')
        elif a==0x4a7276:r=args[0]
        elif a==0x300e000:u.mem_write(args[0],self.text(args[1]).encode('ascii')+b'\0');r=args[0];pop=12
        elif a==0x300e300:
            left,right=[self.text(p) for p in args[:2]];r=(left>right)-(left<right);pop=12
        elif a==0x43e583:
            i=len(self.images);self.blank(i);self.wi(args[0],self.handle(i))
            self.images.append(self.text(args[1]))
        elif a==0x4a06df:
            slot=[0,3,6,9,10,11][len(self.sounds)];r=self.sh(slot);self.wi(r,0x4201000)
            self.sounds.append((slot,self.text(args[1])))
        elif a==0x4acf9a:u.mem_write(0x708574,struct.pack('<3i',*self.cfg));r=1
        elif a in [0x4ad2bf,0x4ad34a]:
            if args[0]:
                slot=(args[0]-0x4200000)//0x40
                if a==0x4ad2bf:
                    assert args[1]==0;self.event('play',slot,C.c_int32(args[2]).value);self.active[slot]=1
                else:self.event('stop',slot);self.active[slot]=0
        elif a==0x300e100:
            self.event('gain',(args[0]-0x4200000)//0x40,C.c_int32(args[1]).value);pop=12
        elif a==0x300e200:
            slot=(args[0]-0x4200000)//0x40;self.event('playing',slot);self.wi(args[1],self.active[slot]);pop=12
        elif a==0x49a63e:self.event('warp',*[C.c_int32(x).value for x in args[:2]])
        elif a==0x534a34:self.event('random');r=self.random
        u.reg_write(UC_X86_REG_EAX,r);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,self.ri(sp))
    def initialize(self,width,values):
        self.record=False;self.images=[];self.sounds=[];self.cfg=values
        self.call(0x4ab0c0,struct.pack('<f',width/1280))
        self.handles=[self.ri(0x708738+4*i) for i in range(23)]
        s=self.read()
        for i,h in enumerate(self.handles):s.rect[i][:]=[self.rf(h+o) for o in [0x7c,0x80,0xf4,0xf8]]
        s.minimum=self.rf(0x7085a8);s.maximum=self.rf(0x7085ac)
        s.bar_rect[:]=[(C.c_int32*4)(*struct.unpack('<4i',self.u.mem_read(0x7085b8+16*i,16))) for i in range(3)]
        s.disabled[:]=[self.si(0x7083f4+4*i) for i in range(6)];s.sound_mask=MASK
        return s
    def read(self):
        s=State();s.slider[:]=[self.rf(0x70858c+4*i) for i in range(3)]
        s.values[:]=[self.si(0x708574+4*i) for i in range(3)]
        s.selected=self.si(0x7082e8);s.row=self.si(0x7082ec);s.mouse_result=self.si(0x7085b0)
        s.dragging[:]=[self.si(0x708598+4*i) for i in range(3)];s.previewing=self.si(0x7085b4)
        s.samples[:]=[(self.ri(p)-0x4200000)//0x40 if self.ri(p) else -1 for p in SAMPLE_GLOBALS]
        h=self.ri(0x708790)
        if h:s.cursor[:]=[int(self.rf(h+o)) for o in [0x7c,0x80]]
        return s
    def install(self,s):
        for i in range(3):
            self.wf(0x70858c+4*i,s.slider[i]);self.wi(0x708574+4*i,s.values[i]);self.wi(0x708598+4*i,s.dragging[i])
            self.wi(SAMPLE_GLOBALS[i],self.sh(s.samples[i]) if s.samples[i]>=0 else 0)
        for a,v in [(0x7082e8,s.selected),(0x7082ec,s.row),(0x7085b0,s.mouse_result),(0x7085b4,s.previewing)]:self.wi(a,v)
        for i in range(6):self.wi(0x7083f4+4*i,s.disabled[i])
        h=self.handles[22];self.wf(h+0x7c,s.cursor[0]);self.wf(h+0x80,s.cursor[1]);self.wi(h+0x74,1)
    def run(self,s,inp,active,rnd):
        self.install(s);self.active=list(active);self.random=rnd;self.events=[];self.record=True;self.draw_meta=[]
        self.call(0x4ac224,b'')
        mouse=bytearray(44);mouse[20]=inp.mouse;struct.pack_into('<2i',mouse,36,inp.x,inp.y)
        keys=bytes(getattr(inp,x) for x in ['left','right','up','down','confirm','back','fast'])+b'\0'
        self.call(0x4ac6a7,bytes(mouse)+keys)
        return snap(self.read()),self.draw_meta,self.events,C.c_int32(self.u.reg_read(UC_X86_REG_EAX)).value

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--output',type=Path,default=ROOT/'local/original-volume-menu-oracle.json');a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256);rng=random.Random(0x4ac6a7)
    lib.bk_volume_menu_initialize.argtypes=[C.POINTER(State),C.c_uint,C.POINTER(C.c_int32),C.c_void_p]
    lib.bk_volume_menu_prepare.argtypes=[C.POINTER(State),C.POINTER(Frame),C.c_void_p]
    lib.bk_volume_menu_step.argtypes=[C.POINTER(State),C.POINTER(Input),C.POINTER(Ops),C.POINTER(C.c_int),C.c_void_p]
    lib.bk_volume_menu_image.argtypes=[C.c_uint];lib.bk_volume_menu_image.restype=C.c_char_p
    lib.bk_volume_menu_sound.argtypes=[C.c_uint];lib.bk_volume_menu_sound.restype=C.c_char_p
    constructors=steps=draws=events=0;digest=hashlib.sha256();max_volume_delta=0
    def same_state(got,want,label):
        nonlocal max_volume_delta
        # x87 extended division can truncate one hundredth of a dB below the
        # double result at exact rational boundaries (e.g.2000 becomes1999).
        # Preserve control/geometry exactly; do not emulate80-bit FP for audio.
        delta=max(abs(a-b) for a,b in zip(got[1],want[1]));max_volume_delta=max(max_volume_delta,delta)
        assert delta<=1 and got[:1]+got[2:]==want[:1]+want[2:],(label,got,want)
    def init(width,values):
        nonlocal constructors
        s=State();assert lib.bk_volume_menu_initialize(C.byref(s),width,(C.c_int32*3)(*values),e),e.value
        want=n.initialize(width,values)
        assert bytes(s)==bytes(want),('init',width,snap(s),snap(want))
        assert [lib.bk_volume_menu_image(i).decode() for i in range(23)]==[n.images[(h-0x4000000)//0x400] for h in n.handles]
        assert n.sounds==[(i,lib.bk_volume_menu_sound(i).decode()) for i in range(16) if lib.bk_volume_menu_sound(i)]
        constructors+=1;return s
    def check(s,inp,active=None,rnd=None):
        nonlocal steps,draws,events
        active=list(active if active is not None else [rng.randrange(2) for _ in range(16)]);rnd=rng.randrange(32768) if rnd is None else rnd
        wanted,wd,we,wa=n.run(s,inp,active,rnd);trace=[]
        def event(*v):trace.append((*v,snap(s)));return 1
        @Play
        def play(_,slot,vol,err):event('play',slot,vol);active[slot]=1;return 1
        @Stop
        def stop(_,slot,err):event('stop',slot);active[slot]=0;return 1
        @Play
        def gain(_,slot,vol,err):return event('gain',slot,vol)
        @Playing
        def playing(_,slot,out,err):event('playing',slot);out[0]=active[slot];return 1
        @Random
        def rand(_,out,err):event('random');out[0]=rnd;return 1
        @Warp
        def warp(_,x,y,err):return event('warp',x,y)
        ops=Ops(None,play,stop,gain,playing,rand,warp);f=Frame();action=C.c_int(-1)
        before=bytes(s);assert lib.bk_volume_menu_prepare(C.byref(s),C.byref(f),e),e.value;assert bytes(s)==before
        got=[(d.slot,tuple(d.corners)) for d in f.draws[:f.count]]
        assert got==wd,('draw',steps,got,wd)
        assert lib.bk_volume_menu_step(C.byref(s),C.byref(inp),C.byref(ops),C.byref(action),e),e.value
        same_state(snap(s),wanted,('state',steps,bytes(inp).hex()))
        assert action.value==wa,('action',steps,action.value,wa)
        assert len(trace)==len(we),('event count',steps,trace,we)
        for actual,expected in zip(trace,we):
            same_state(actual[-1],expected[-1],('event state',steps,actual[0]))
            if actual[0] in ['play','gain']:
                assert actual[:2]==expected[:2] and abs(actual[2]-expected[2])<=1,(steps,actual,expected)
            else:assert actual[:-1]==expected[:-1],('event',steps,actual,expected)
        digest.update(repr((snap(s),got,trace,action.value)).encode());steps+=1;draws+=len(got);events+=len(trace)
    # Fresh real constructors at fractional and production widths; stateful input
    # sequences then compare every frame, including retained mouse-result bytes.
    for width in [320,640,960,1001,1280,1920]:
        for values in [(-1500,-2500,-1999),(0,-6000,-3000),(-1,-5999,-4137)]:
            s=init(width,values)
            for case in range(320):
                inp=Input(rng.randrange(-32,width+32),rng.randrange(-32,width*3//4+32))
                inp.mouse=rng.randrange(4)
                if case%3:
                    for name in ['left','right','up','down','confirm','back','fast']:setattr(inp,name,rng.randrange(4))
                if case%8==0:
                    row=case//8%6;inp.x=int(s.rect[10+row*2][0]+1);inp.y=int(s.rect[10+row*2][1]+1)
                    inp.mouse=3
                check(s,inp)
    # Exact knob/click/rectangle borders, all rows and both sample2 choices.
    boundaries=0
    for width in [640,960,1280]:
        s=init(width,(-1500,-2500,-1999))
        for row in range(3):
            for x in [int(s.slider[row])-int(s.rect[4+row][2]/2),int(s.slider[row]),
                      int(s.minimum)-1,int(s.minimum),int(s.maximum),int(s.maximum)+1]:
                for y in [int(s.rect[1+row][1]),s.bar_rect[row][1],s.bar_rect[row][1]+s.bar_rect[row][3]]:
                    for mouse in [3,1,2]:check(s,Input(x,y,mouse));boundaries+=1
        for row in range(6):
            s.row=row;s.mouse_result=-1
            for rnd in [0,1]:check(s,Input(-10,-10,0,0,0,0,0,3),rnd=rnd);boundaries+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),constructors=constructors,
                frames=steps,boundary_frames=boundaries,draws=draws,service_calls=events,max_geometry_error=0,
                max_volume_delta_hundredths_db=max_volume_delta,
                digest=digest.hexdigest(),scope=__doc__)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
