"""Full51B617/4E4472/4E533B/4E5744/4E6AC2, native constructors and geometry.
Only image/device, capture, input, sound, clock and scheduling are leaves.
Compares ordered calls, retained shared state and every emitted vertex; these
fixtures do not establish real screenshots, application flow48 or Switch use.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_effect_sprite_oracle import Native as Base, Effect, snapshot as effect_snapshot
from original_ending_ui_oracle import UiSprite, Draw
from original_player_hud_oracle import Timer as UiTimer
from original_common_hud_oracle import State as Common, Timer, snapshot as common_snapshot
from original_item_notice_oracle import Fade
from original_menu_camera_oracle import State as Camera
from original_special_event_oracle import State as Event, Timer as EventTimer
import original_special_event_oracle as event_oracle
from model_binding import ROOT, library

I,U,F,B,S,P = C.c_int32,C.c_uint32,C.c_float,C.c_uint8,C.c_int8,C.c_void_p
FP,IP,BP = C.POINTER(F),C.POINTER(I),C.POINTER(B)
class Ui(C.Structure):
    _fields_=[('sprites',UiSprite*30),('loaded',U),('cursor_wanted',B)]
class Control(C.Structure):
    _fields_=[('row',I),('open',S)]
class Presets(C.Structure):
    _fields_=[('active',(F*3)*4),('authored',(F*3)*4)]
class Bindings(C.Structure):
    _fields_=[('common',C.POINTER(Common)),('control',C.POINTER(Control)),
        ('camera',C.POINTER(Camera)),('presets',C.POINTER(Presets)),
        ('phase',IP),('camera_clip',IP),('camera_mode',IP),('photo_count',IP),
        ('special',C.POINTER(S)),('previous_flow',BP),('visibility',BP),('hover_latched',BP)]
Image=C.CFUNCTYPE(I,P,U,C.c_char_p,P)
Capture=C.CFUNCTYPE(I,P,I,P)
Vector=C.CFUNCTYPE(I,P,FP,P)
Key=C.CFUNCTYPE(I,P,U,U,C.POINTER(U),P)
Sound=C.CFUNCTYPE(I,P,U,P)
Clock=C.CFUNCTYPE(I,P,C.POINTER(U),P)
Warp=C.CFUNCTYPE(I,P,F,F,P)
Schedule=C.CFUNCTYPE(I,P,B,B,P)
class Ops(C.Structure):
    _fields_=[('context',P),('image',Image),('capture',Capture),('position',Vector),
        ('motion',Vector),('key',Key),('sound',Sound),('clock',Clock),('warp',Warp),('schedule',Schedule)]
class Draws(C.Structure):
    _fields_=[('count',U),('draws',Draw*128)]
class Frame(C.Structure):
    _fields_=[('sprites',Draws),('curtain',F),('complete',I)]
ADDR=[0x734058+i*0x16c for i in range(17)]+[0x719c80+i*0x16c for i in range(13)]+[0xbeea18]
SCALARS=[('phase',I,0xbfbba0),('camera_clip',I,0x721e08),('camera_mode',I,0x721e0c),
    ('photo_count',I,0x734050),('special',S,0xbef778),('previous_flow',B,0x721ad4),
    ('visibility',B,0x7220f8),('hover_latched',B,0xbef178)]
def timer(t): return (t.duration,t.deadline,t.armed)
def sprite(p): return (effect_snapshot(p.transform),tuple(p.rect),tuple(p.uv),p.rgb,timer(p.timer))
def differences(a,b):
    return [(k,[(i,x,y) for i,(x,y) in enumerate(zip(a[k],b[k])) if x!=y]) if k=='sprites' else (k,a[k],b[k]) for k in a if a[k]!=b[k]]
class Fixture:
    def __init__(self):
        self.ui=Ui();self.control=Control();self.camera=Camera();self.presets=Presets()
        self.common=Common();self.event=Event()
        for name,typ,_ in SCALARS:setattr(self,name,typ())
    def bindings(self):
        return Bindings(*(C.pointer(getattr(self,name)) for name,_ in Bindings._fields_))
    def snapshot(self):
        return dict(sprites=[sprite(p) for p in self.ui.sprites],loaded=self.ui.loaded,
            wanted=self.ui.cursor_wanted,control=(self.control.row,self.control.open),
            camera=bytes(self.camera).hex(),presets=bytes(self.presets).hex(),
            common=common_snapshot(self.common),event=(timer(self.event.sequence_timer),
                self.event.face_target,timer(self.event.face_timer),self.event.sequence),
            **{name:getattr(self,name).value for name,_,_ in SCALARS})
    def copy(self):
        f=Fixture()
        for name,value in self.__dict__.items():setattr(f,name,type(value).from_buffer_copy(value))
        return f

def views(f):
    for name,typ,a in SCALARS:yield a,C.addressof(getattr(f,name)),C.sizeof(typ)
    for obj,names in [(f.control,[('row',0x71adb8),('open',0x71ad9c)]),
        (f.camera,[(name,0x71b364+i*4) for i,name in enumerate(['yaw','pitch','radius','height'])]),
        (f.presets,[('active',0x71b37c),('authored',0x71b3ac)]),
        (f.common,[('wait',0xbeeb54),('gate',0xbeeb7c),('action',0xbeeb7e),('blocked',0xbeeb7f)]),
        (f.event,[('sequence_timer',0x71ad90),('face_target',0x71ada0),('face_timer',0x71ada8),('sequence',0x71adbc)])]:
        fields=dict(type(obj)._fields_)
        for name,a in names:
            p=C.addressof(obj)+getattr(type(obj),name).offset
            if name in ['wait','sequence_timer','face_timer']:
                for part,typ in Timer._fields_:
                    off=getattr(Timer,part).offset;yield a+off,p+off,C.sizeof(typ)
            else:yield a,p,C.sizeof(fields[name])

class Native(Base):
    def __init__(self,exe):
        super().__init__(exe);self.events=[];self.fail_at=0;self.stopped=False
        self.point=[0,0];self.motion=[0,0];self.keys={};self.now=0;self.mutate=False
        self.loaded=0;self.draw_meta=[];self.width=1280
        self.wi(0x53f10c,0x300e000)
        for slot in range(1,9):self.wi(0xbeee10+(slot-1)*0x120,100+slot)
        for a in [0x4af970,0x4af97a,0x4ad8ec,0x466805,0x466814,0x43e583,
            0x49d0eb,0x49d085,0x4affb8,0x4b75aa,0x4b757e,0x4b76c2,
            0x46435e,0x300e000,0x4b768d,0x51c47e]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def event_call(self,event):
        self.events.append(event)
        if self.fail_at and len(self.events)==self.fail_at:
            self.stopped=True;self.u.emu_stop();return False
        if self.mutate:
            if event[0]=='sound':self.wi(0x721e0c,(self.ri(0x721e0c)+1)%4)
            if event==('capture',0):self.wi(0x734050,self.ri(0x734050)+1)
            if event[0]=='schedule':self.u.mem_write(0xbeeb7e,b'\x63')
        return True
    def hook(self,u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.ri(sp)
        args=struct.unpack('<4I',u.mem_read(sp+4,16));result=0
        if a==0x43e583:
            i=ADDR.index(args[0]-0x100);name=self.text(args[1]).split('\\')[-1]
            if not self.event_call(('image',i,name)):return
            self.blank(i);self.wi(args[0],self.handle(i));self.loaded|=1<<i
        elif a==0x43e96a:
            i=(args[0]-0x4000000)//0x400
            self.draw_meta.append((i,bytes(u.mem_read(self.verts(i),192)),self.rf(ADDR[i]+0x12c)))
        elif a==0x4af970:result=self.width
        elif a==0x4af97a:result=self.width*3//4
        elif a==0x4ad8ec:u.mem_write(args[0],b'assets\0')
        elif a in [0x466805,0x466814]:pass
        elif a in [0x49d0eb,0x49d085,0x4affb8]:
            if not self.event_call(('capture',[0x49d0eb,0x49d085,0x4affb8].index(a))):return
        elif a in [0x4b75aa,0x4b757e]:
            name='position' if a==0x4b75aa else 'motion'
            if not self.event_call((name,)):return
            value=self.point if name=='position' else self.motion
            self.wf(args[0],value[0]);self.wf(args[1],value[1])
        elif a==0x4b76c2:
            assert args[2]==0
            if not self.event_call(('key',args[0],args[1])):return
            result=self.keys.get(args[:2],0)
        elif a==0x46435e:
            assert args[1]==0
            if not self.event_call(('sound',args[0]-100)):return
        elif a==0x300e000:
            if not self.event_call(('clock',)):return
            result=self.now
        elif a==0x4b768d:
            x,y=struct.unpack('<2f',u.mem_read(sp+4,8))
            if not self.event_call(('warp',x,y)):return
        elif a==0x51c47e:
            if not self.event_call(('schedule',args[0]&255,args[1]&255)):return
        else:return super().hook(u,a,size,_)
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def install_fixture(self,f,scale,dt):
        for i,p in enumerate(f.ui.sprites):
            super().install(ADDR[i],i,p.transform,p.rect)
            self.u.mem_write(ADDR[i]+0x13c,bytes(p.timer))
            for j,(x,y) in enumerate([(0,1),(2,1),(2,3),(0,3),(0,1),(2,3)]):
                v=self.verts(i)+32*j;self.wi(v+16,(int(p.transform.fade.alpha*255)<<24)|p.rgb)
                self.wf(v+24,p.uv[x]);self.wf(v+28,p.uv[y])
            if not f.ui.loaded&(1<<i):self.wi(ADDR[i]+0x100,0)
        effect=Effect();effect.fade=f.common.curtain;effect.scale[:]=[1,1];effect.enter=effect.exit=1
        super().install(ADDR[30],30,effect,[0,0,1280,960])
        for a,p,size in views(f):self.u.mem_write(a,C.string_at(p,size))
        self.u.mem_write(0x7341bf,bytes([f.ui.cursor_wanted]))
        self.wf(0x721ad0,scale);self.wf(0x733700,dt);self.u.mem_write(0x5767c8,b'\1')
        self.loaded=f.ui.loaded;self.events=[];self.draw_meta=[];self.stopped=False
    def read_fixture(self,f):
        out=f.copy()
        for i,p in enumerate(out.ui.sprites):
            a=ADDR[i];p.transform=super().read(a,i)
            p.rect[:]=[self.rf(a+off) for off in [0x114,0x118,0x10c,0x110]]
            p.timer=UiTimer.from_buffer_copy(self.u.mem_read(a+0x13c,C.sizeof(Timer)))
            p.uv[:]=[self.rf(self.verts(i)+off) for off in [24,28,88,92]]
            p.rgb=self.ri(self.verts(i)+16)&0xffffff
        for a,p,size in views(out):C.memmove(p,bytes(self.u.mem_read(a,size)),size)
        out.common.curtain=super().read(ADDR[30],30).fade
        out.ui.cursor_wanted=self.u.mem_read(0x7341bf,1)[0];out.ui.loaded=self.loaded
        return out
    def execute(self,address):
        self.u.mem_write(self.stack,struct.pack('<I',self.stop));self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.reg_write(UC_X86_REG_FPCW,0x37f);self.u.emu_start(address,self.stop,count=2000000)
        assert self.stopped or self.u.reg_read(UC_X86_REG_EIP)==self.stop
    def constructors(self,width):
        self.width=width;self.execute(0x4e6416)
        if self.stopped:return
        self.u.reg_write(UC_X86_REG_EBP,self.stack)
        self.u.reg_write(UC_X86_REG_ESP,self.stack-0x2000)
        self.u.mem_write(self.stack-0x170c,b'assets\0')
        self.u.emu_start(0x4e2dca,0x4e3a55,count=2000000)
        assert self.stopped or self.u.reg_read(UC_X86_REG_EIP)==0x4e3a55

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path)
    ap.add_argument('--cases',type=int,default=6000);ap.add_argument('--output',type=Path)
    args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256)
    lib.bk_special_ui_initialize.argtypes=[C.POINTER(Ui),C.POINTER(Control),C.POINTER(Event),U,S,C.POINTER(Ops),P]
    lib.bk_special_ui_step.argtypes=[C.POINTER(Ui),C.POINTER(Bindings),C.POINTER(Ops),F,F,C.POINTER(Frame),P]
    lib.bk_effect_sprite_initialize.argtypes=[C.POINTER(Effect),FP,B,B,B]
    rng=random.Random(0x4e4472);trace=[];current=None;counts=dict(constructors=0,constructor_failures=0,frames=0,draws=0,vertices=0,events=0,failure_prefixes=0,continuous=0,boundaries=0)
    digest=hashlib.sha256();coverage=set()
    def record(event):
        trace.append(event)
        if n.fail_at and len(trace)==n.fail_at:return 0
        if n.mutate:
            if event[0]=='sound':current.camera_mode.value=(current.camera_mode.value+1)%4
            if event==('capture',0):current.photo_count.value+=1
            if event[0]=='schedule':current.common.action=99
        return 1
    @Image
    def image(_,slot,name,e):return record(('image',slot,name.decode()))
    @Capture
    def capture(_,op,e):return record(('capture',op))
    @Vector
    def position(_,out,e):
        if not record(('position',)):return 0
        out[0],out[1]=n.point;return 1
    @Vector
    def motion(_,out,e):
        if not record(('motion',)):return 0
        out[0],out[1]=n.motion;return 1
    @Key
    def key(_,code,mode,out,e):
        if not record(('key',code,mode)):return 0
        out[0]=n.keys.get((code,mode),0);return 1
    @Sound
    def sound(_,slot,e):return record(('sound',slot))
    @Clock
    def clock(_,out,e):
        if not record(('clock',)):return 0
        out[0]=n.now;return 1
    @Warp
    def warp(_,x,y,e):return record(('warp',x,y))
    @Schedule
    def schedule(_,target,mode,e):return record(('schedule',target,mode))
    ops=Ops(None,image,capture,position,motion,key,sound,clock,warp,schedule)
    def initialize(f,width,original=False):
        nonlocal current
        current=f;trace.clear()
        if original:
            n.install_fixture(f,F(width/1280).value,0);n.constructors(width);wanted=n.read_fixture(f).snapshot();wt=n.events.copy()
        assert lib.bk_special_ui_initialize(C.byref(f.ui),C.byref(f.control),C.byref(f.event),width,f.special.value,C.byref(ops),error),error.value
        if original:
            actual=f.snapshot()
            assert actual==wanted,('constructor state',differences(actual,wanted))
            assert trace==wt,('constructor calls',trace,wt);counts['constructors']+=1
        trace.clear()
    def check(f,scale=1,dt=1/60):
        nonlocal current
        scale=F(scale).value;dt=F(dt).value;current=f;trace.clear()
        n.install_fixture(f,scale,dt);n.execute(0x51b617)
        wanted=n.read_fixture(f).snapshot();wt=n.events.copy();wd=n.draw_meta.copy()
        b=f.bindings();frame=Frame()
        ok=lib.bk_special_ui_step(C.byref(f.ui),C.byref(b),C.byref(ops),scale,dt,C.byref(frame),error)
        assert bool(ok)==(not n.stopped),(counts,error.value,n.stopped)
        assert frame.complete==ok
        actual=f.snapshot()
        assert actual==wanted,('state',counts,differences(actual,wanted))
        assert trace==wt,('calls',counts,trace,wt)
        draws=[d for d in wd if d[0]!=30];curtain=[d for d in wd if d[0]==30]
        assert frame.sprites.count==len(draws),(counts,frame.sprites.count,[d[0] for d in wd])
        assert frame.curtain==(curtain[0][2] if curtain else 0)
        for d,(slot,vertices,alpha) in zip(frame.sprites.draws,draws):
            assert d.slot==slot and d.alpha==alpha
            for j,k in enumerate([0,1,2,3,0,2]):
                xy=struct.unpack_from('<2f',vertices,32*j);uv=struct.unpack_from('<2f',vertices,32*j+24)
                assert tuple(d.xy[2*k:2*k+2])==xy,(counts,slot,'xy',tuple(d.xy),xy)
                a,b=[(0,1),(2,1),(2,3),(0,3)][k]
                assert (d.uv[a],d.uv[b])==uv
                assert (int(d.alpha*255)<<24)|d.rgb==struct.unpack_from('<I',vertices,32*j+16)[0]
                counts['vertices']+=1
        counts['draws']+=len(draws);counts['events']+=len(trace)
        if ok:counts['frames']+=1
        else:counts['failure_prefixes']+=1
        for event in trace:
            if event[0] in ['sound','capture','schedule']:coverage.add(event)
        digest.update(json.dumps([actual,trace,[(d.slot,list(d.xy),d.alpha) for d in frame.sprites.draws[:frame.sprites.count]]],sort_keys=True).encode())
        return len(trace)
    # Native constructors include their raw filename-prefix writes at71ad90.
    f=Fixture()
    for i,p in enumerate(f.ui.sprites):
        p.rect[:]=[i+10,i+20,64,32];assert lib.bk_effect_sprite_initialize(C.byref(p.transform),p.rect,1,1,0)
        p.transform.direction=i%4;p.transform.motion[:]=[i*.01,-i*.001]
        p.timer=UiTimer(3000,0xfffffffa,i%2);p.uv[:]=[.1,.2,.7,.9];p.rgb=0x123456
    for width in [1,320,960,1001,1280,1920,16384]:
        for special in [1,0,1,-1,2]:
            f.special.value=special;f.event.sequence_timer=EventTimer(17,19,1)
            f.event.face_target=42;f.event.face_timer=EventTimer(37,39,1);f.event.sequence=7
            f.control=Control(4,1);f.ui.cursor_wanted=255;initialize(f,width,True)
    alias=f.snapshot()['event']
    constructor_event=Event.from_buffer_copy(f.event)
    # Every image failure preserves completed constructors and their alias writes.
    for special in [0,1]:
        for failed in range(1,(28 if special==1 else 30)+1):
            f=Fixture();f.special.value=special;current=f;trace.clear();n.fail_at=failed
            n.install_fixture(f,.75,0);n.constructors(960);wanted=n.read_fixture(f).snapshot();wt=n.events.copy()
            assert n.stopped
            assert not lib.bk_special_ui_initialize(C.byref(f.ui),C.byref(f.control),C.byref(f.event),960,special,C.byref(ops),error)
            assert f.snapshot()==wanted,('constructor failure',failed,differences(f.snapshot(),wanted))
            assert trace==wt;counts['constructor_failures']+=1
        n.fail_at=0
    # Fresh special mode leaves slots15/16 genuinely unconstructed; detached
    # fade updates still execute, but the native null image emits no geometry.
    for width in [320,960,1280]:
        f=Fixture();f.special.value=1;initialize(f,width,True);f.phase.value=2
        n.point=[0,0];n.motion=[0,0];n.keys={}
        for tick in range(90):n.now=tick*17;check(f,width/1280);counts['continuous']+=1
    # All valid phases, AL-only inputs, overlapping/moving rectangles, state retention.
    for case in range(args.cases):
        f=Fixture();width=[320,960,1001,1280,1920][case%5];scale=F(width/1280).value
        # Retained15/16 exist before switching into special mode.
        initialize(f,width);f.special.value=[0,1,-1,2][case%4];initialize(f,width)
        f.phase.value=2 if case%9 else [-1,0,1,3,9][case%5]
        f.camera_clip.value=case%3;f.camera_mode.value=case%5;f.photo_count.value=rng.choice([0,1,9,10,99,100,101,999])
        f.previous_flow.value=rng.choice([1,2,0x18,255]);f.visibility.value=rng.choice([0,1,2,255]);f.hover_latched.value=rng.choice([0,5,10,12,15,255])
        f.control=Control(rng.choice([-2147483648,-1,0,1,2,3,4,5,2147483647]),case%2)
        f.common.curtain=Fade(rng.choice([0,.25,.75,1]),rng.choice([0,.5,2,10]),case%6)
        f.common.blocked=rng.choice([0,1,2,255]);f.common.action=rng.choice([0,1,5,255])
        f.common.wait=Timer(123,456,255);f.common.gate=255
        for name in ['yaw','pitch','radius','height']:setattr(f.camera,name,rng.uniform(-30,100))
        for table in [f.presets.active,f.presets.authored]:
            for row in table:row[:]=[rng.uniform(-100,100) for _ in row]
        for i,p in enumerate(f.ui.sprites):
            p.transform.fade=Fade(rng.random(),rng.choice([0,.5,2,10]),rng.randrange(6))
            p.timer=UiTimer(rng.choice([0,3000,0xffffffff]),rng.getrandbits(32),rng.choice([0,1,97,255]))
            if 3<=i<=16:p.rect[0]=F(rng.uniform(900,1320)*scale).value
            if case%17==0 and 3<=i<=16:p.rect[:]=[1100*scale,20*scale,160*scale,400*scale]
        slot=[3,5,7,10,12,14,15][case%7];r=f.ui.sprites[slot].rect
        n.point=[F(r[0]+r[2]*rng.choice([0,.5,1])).value,F(r[1]+r[3]*rng.choice([0,.5,1])).value]
        n.motion=[0,0] if case%3 else [1,-.5];n.now=rng.choice([0,3000,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32)])
        n.keys={(code,mode):rng.choice([0,0,0,1,255,256,257]) for code in [0,1,0x5a,0x33450,0x43,0x33455,0x26,0x30d40,0x28,0x30d41] for mode in [1,2]}
        n.mutate=case%7==0;check(f,scale,rng.choice([0,1/60,.1,.5,1,2]))
    # Actual constructor layouts continuously slide, select each control and exit.
    seeds=[];n.mutate=False
    for mode in [0,1]:
        for width in [640,960,1280]:
            f=Fixture();initialize(f,width);f.special.value=mode;initialize(f,width)
            f.phase.value=2;f.camera_mode.value=0;f.previous_flow.value=2 if mode==0 else 0x18
            f.common.curtain=Fade(0,2,0);scale=F(width/1280).value
            for tick in range(900):
                n.keys={};n.motion=[0,0];n.now=1000+tick*17
                n.point=[1184*scale,40*scale] if tick<800 else [0,0]
                if tick in [220,280,340,400,460,520,580,640,760]:
                    slot={220:3,280:7,340:10,400:12,460:3,520:7,580:10,640:12,760:5}[tick]
                    r=f.ui.sprites[slot].rect;n.point=[F(r[0]+r[2]*.5).value,F(r[1]+r[3]*.5).value];n.keys={(0,1):1}
                    seeds.append((f.copy(),scale,n.point[:],n.keys.copy(),n.now))
                if tick==700:n.keys={(0x43,1):1};seeds.append((f.copy(),scale,n.point[:],n.keys.copy(),n.now))
                check(f,scale);counts['continuous']+=1
    # Explicit completed-curtain exits also exercise failure after blocked=0
    # but before phase/action reset, as well as signed keyboard row wrap.
    for previous in [2,0x18]:
        f=Fixture();initialize(f,1280);f.phase.value=2;f.previous_flow.value=previous
        f.common.curtain=Fade(1,2,3);f.common.blocked=1;f.common.action=5
        f.control.row=-2147483648
        seeds.append((f,1,[0,0],{(0x26,1):1},0xfffffffe))
    # Stop at every reached failing service of actual interaction frames.
    for seed,scale,point,keys,now in seeds:
        n.point=point;n.keys=keys;n.now=now;n.motion=[0,0];n.mutate=True
        calls=check(seed.copy(),scale)
        for fail_at in range(1,calls+1):
            n.fail_at=fail_at;check(seed.copy(),scale)
        n.fail_at=0
    # Explicit edges for the shared inclusive sidebar and signed row wrap.
    n.mutate=False;n.keys={};n.motion=[1,0]
    for width in [320,960,1001,1280,1920]:
        for x in [1095,1096,1280,1281]:
            for y in [0,248,432,433]:
                f=Fixture();initialize(f,width);f.phase.value=2;n.point=[F(x*width/1280).value,F(y*width/1280).value]
                check(f,width/1280);counts['boundaries']+=1
    # Feed the actual constructor result into51B647. Its first phase2 direct
    # Play explicitly disarms the filename-derived timer before polling, so
    # the original30-second sequence still starts normally. No compatibility
    # reset or guessed replacement of the raw constructor write is needed.
    lib.bk_special_event_step.argtypes=[C.POINTER(event_oracle.Bindings),C.POINTER(event_oracle.Ops),P]
    event_native=event_oracle.Native(exe);sequence_calls=0;counts['post_constructor_frames']=0
    for group in [0,1]:
        ef=event_oracle.fixture(random.Random(4),1);ef.group.value=group;ef.phase.value=2
        ef.state=Event.from_buffer_copy(constructor_event);ef.camera_mode.value=0
        ef.camera_clip.value=0;ef.seconds.value=F(1/60).value;ef.clock_step.value=0
        ef.present[:]=[1]*8;ef.paused.value=0;ef.triggered.value=0
        for tick in range(1900):
            ef.now.value=1000+tick*17;ef.timings[0][0]=tick%130
            wanted,wt=event_native.run(ef);ok,got,trace_event,err=event_oracle.portable(lib,ef)
            assert ok,err;event_oracle.compare(wanted,got,wt,trace_event,('after-ui-constructor',group,tick))
            if group==1:
                sequence_calls+=sum(event[0][:2]==('audio',3) for event in trace_event)
                if tick==0:assert got.state.sequence==1 and timer(got.state.sequence_timer)==(30000,31000,1)
            ef=got;digest.update(b''.join(got.snapshot()));counts['post_constructor_frames']+=1
        if group==1:assert ef.state.sequence==2
    assert sequence_calls==2,sequence_calls
    # Original invalid array/index or uninitialized-stack inputs are explicit guards.
    guards=0
    for kind in ['negative-counter','invalid-preset','invalid-open','missing-position','missing-capture','missing-schedule']:
        f=Fixture();initialize(f,1280);f.phase.value=2;f.camera_mode.value=1
        n.keys={};n.point=[0,0];n.motion=[0,0];current=f;trace.clear();bad=Ops.from_buffer_copy(ops)
        if kind=='negative-counter':f.photo_count.value=-1
        if kind=='invalid-preset':f.camera_clip.value=3;f.camera_mode.value=0
        if kind=='invalid-open':f.control.open=2;n.keys={(0,2):1}
        if kind=='missing-position':bad.position=Vector()
        if kind=='missing-capture':bad.capture=Capture()
        if kind=='missing-schedule':
            f.common.curtain=Fade(1,2,3);f.common.blocked=1;f.common.action=5;f.previous_flow.value=0x18;bad.schedule=Schedule()
        b=f.bindings();frame=Frame()
        assert not lib.bk_special_ui_step(C.byref(f.ui),C.byref(b),C.byref(bad),1,F(1/60),C.byref(frame),error),(kind,error.value)
        assert frame.complete==0;guards+=1
    expected={('sound',1),('sound',3),('sound',4),('sound',6),('sound',8),('capture',0),('capture',1),('capture',2),('schedule',2,1),('schedule',0x18,1)}
    if args.cases>=6000:assert expected<=coverage,expected-coverage
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),**counts,guards=guards,
        state_sha256=digest.hexdigest(),constructor_alias=alias,sequence_direct_plays=sequence_calls,
        coverage=sorted(coverage),max_error=0,scope=__doc__)
    output=args.output or ROOT/'local/original-special-ui-oracle.json'
    output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
