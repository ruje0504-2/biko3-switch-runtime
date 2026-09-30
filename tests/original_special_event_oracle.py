"""Run complete51B647 with its five helpers, native timers and target envelope.

Only model/camera/input/audio/clock leaves are fixtures. Compare every ordered
call and persistent state, including callback mutations and failure prefixes.
No real assets, PCM playback, flow48 loader/UI or Switch validation is claimed.
"""
from __future__ import annotations
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine

I,U,F,B,S,P=C.c_int32,C.c_uint32,C.c_float,C.c_uint8,C.c_int8,C.c_void_p
FP,IP,BP,UP=C.POINTER(F),C.POINTER(I),C.POINTER(B),C.POINTER(U)
class Timer(C.Structure):
    _fields_=[('duration',U),('deadline',U),('armed',B)]
class State(C.Structure):
    _fields_=[('sequence_timer',Timer),('face_target',F),('face_timer',Timer),('sequence',S)]
class Envelope(C.Structure):
    _fields_=[('target',F),('smoothed',F)]
class Bindings(C.Structure):
    _fields_=[('state',C.POINTER(State)),('group',IP),('phase',IP),('camera_clip',IP),('camera_mode',IP),
        ('seconds',FP),('paused',C.POINTER(S)),('music_wanted',C.POINTER(S)),('packed',C.POINTER(S)),
        ('visibility',BP),('effect_loop',BP),('effect_volume',IP),('listener',FP),
        ('effect_positions',FP*4),('envelope',C.POINTER(Envelope))]
class AudioCall(C.Structure):
    _fields_=[('operation',I),('slot',U),('pack',C.c_char_p),('name',C.c_char_p),('volume',I),
        ('flags',U),('amount',F),('source',F*4),('listener',F*4)]
Camera=C.CFUNCTYPE(I,P,I,I,FP,F,BP,P)
Timing=C.CFUNCTYPE(I,P,U,FP,FP,P)
Key=C.CFUNCTYPE(I,P,U,BP,P)
Present=C.CFUNCTYPE(I,P,I,IP,P)
Place=C.CFUNCTYPE(I,P,U,FP,F,P)
Audio=C.CFUNCTYPE(I,P,C.POINTER(AudioCall),P)
Cue=C.CFUNCTYPE(I,P,I,BP,P)
Movie=C.CFUNCTYPE(I,P,P)
Clock=C.CFUNCTYPE(I,P,I,UP,P)
Random=C.CFUNCTYPE(I,P,IP,P)
Level=C.CFUNCTYPE(I,P,U,F,FP,P)
Request=C.CFUNCTYPE(I,P,I,P)
Advance=C.CFUNCTYPE(I,P,F,P)
Gaze=C.CFUNCTYPE(I,P,F,F,P)
Face=C.CFUNCTYPE(I,P,I,I,F,U,P)
Hide=C.CFUNCTYPE(I,P,U,P)
class Ops(C.Structure):
    _fields_=[('context',P),('camera',Camera),('timing',Timing),('key',Key),('present',Present),
        ('place',Place),('audio',Audio),('cue',Cue),('movie',Movie),('clock',Clock),('random',Random),
        ('level',Level),('request',Request),('advance',Advance),('gaze',Gaze),('face',Face),('hide',Hide)]

SCALARS=[('group',0x7219a8),('phase',0xbfbba0),('camera_clip',0x721e08),('camera_mode',0x721e0c),
    ('seconds',0x733700),('paused',0xbeeb4c),('music_wanted',0x72221c),('packed',0x5767c8),
    ('visibility',0x7220f8),('effect_loop',0x72257d),('effect_volume',0xbe9a10),('listener',0x71b358),
    ('envelope',0x708878)]
OWNERS=[n for n,_ in SCALARS]+['state','positions','present','timings','keys','camera_done','triggered',
    'level_value','random_value','random_index','clock_index','now','clock_step']
BODY,CAM0,CAM1=0x3001000,0x3004000,0x3005000
MODELS=[0x3007000,0x3007300,0x3007600]
ROOTS=[0x3008000,0x3008400,0x3008800]
FACEPTR,MOVIEPTR,REFERENCE=0x3009000,0x300a000,0x300b000
SOUNDS=[0x300c000+i*0x100 for i in range(4)]
VTABLE,DIRECT,TIMECLOCK,TICKCLOCK,FLOATRET=0x300d000,0x300d100,0x300d200,0x300d300,0x300d400

def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def floats(v):return tuple(bits(x) for x in v)
def string(v):return v.decode() if v else None

def views(f):
    for name,addr in SCALARS:
        value=getattr(f,name);yield addr,C.addressof(value),C.sizeof(value)
    for name,addr in [('sequence_timer',0x71ad90),('face_timer',0x71ada8)]:
        base=C.addressof(f.state)+getattr(State,name).offset
        for field,typ in Timer._fields_:
            off=getattr(Timer,field).offset
            yield addr+off,base+off,C.sizeof(typ)
    for name,addr in [('face_target',0x71ada0),('sequence',0x71adbc)]:
        yield addr,C.addressof(f.state)+getattr(State,name).offset,C.sizeof(dict(State._fields_)[name])
    for slot in range(4):yield 0x722564+slot*0x120,C.addressof(f.positions[slot]),16
    for slot,actor in enumerate([BODY,CAM1]):
        yield actor+0x1f0,C.addressof(f.timings[slot]),4
        yield actor+0x1e8,C.addressof(f.timings[slot])+4,4

class Fixture:
    def clone(self):
        f=Fixture()
        for name in OWNERS:
            value=getattr(self,name);setattr(f,name,type(value).from_buffer_copy(value))
        f.mutate_at,f.mutations=self.mutate_at,self.mutations.copy()
        return f
    def snapshot(self):return tuple(bytes(getattr(self,n)) for n in OWNERS)
    def effect(self,event,index):
        if event[0]=='clock':self.clock_index.value+=1
        elif event[0]=='random':self.random_index.value+=1
        elif event[0]=='audio' and event[1]==1:self.present[5]=1
        elif event[0]=='level':
            self.envelope.target,self.envelope.smoothed=self.level_value.value,self.level_value.value
        if index==self.mutate_at:
            for path,value in self.mutations.items():
                fields=path.split('.');owner=self
                for field in fields[:-1]:owner=owner[int(field)] if field.isdigit() else getattr(owner,field)
                if fields[-1].isdigit():owner[int(fields[-1])]=value
                else:setattr(owner,fields[-1],value)
    def clock(self):return (self.now.value+self.clock_index.value*self.clock_step.value)&0xffffffff

def audio_event(op,slot=0,pack=None,name=None,volume=0,flags=0,amount=0,source=(0,)*4,listener=(0,)*4):
    return ('audio',op,slot,pack,name,volume,flags,bits(amount),floats(source),floats(listener))

class Native:
    def __init__(self,exe):
        self.u=machine(exe);self.stack,self.stop=0x2008000,0x300f000
        self.word(0x721b28,BODY);self.word(0x721b38,ROOTS[0]);self.word(0x645600,REFERENCE)
        for actor,model,root in zip([BODY,CAM0,CAM1],MODELS,ROOTS):
            self.word(actor+0x160,model);self.word(model+0x14,root);self.word(actor+0x140,0)
        self.word(0x53f10c,TIMECLOCK);self.word(0x53f358,TICKCLOCK)
        for sound in SOUNDS:self.word(sound,VTABLE)
        self.word(VTABLE+0x30,DIRECT)
        self.u.mem_write(0x4af18f,b'\xd9\x05'+struct.pack('<I',FLOATRET)+b'\xc3')
        for address in [0x4bb82e,0x4bc444,0x4bb0a4,0x4bb612,0x4b76c2,0x422c49,0x4241e3,
                0x50db23,0x4afe00,0x50d858,0x46435e,0x50d2a0,0x521ed2,DIRECT,TIMECLOCK,TICKCLOCK,
                0x534a34,0x4af18f,0x401b0a,0x4026fe,0x4f3bb9,0x4f3c49,0x411de1,0x410fd8,0x411985,0x4110ef,0x423b01]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=address,end=address)
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def words(self,a,n):return struct.unpack('<'+'I'*n,self.u.mem_read(a,n*4))
    def f32(self,a,n=1):return struct.unpack('<'+'f'*n,self.u.mem_read(a,n*4))
    def text(self,a):return bytes(self.u.mem_read(a,256)).split(b'\0')[0].decode() if a else None
    def sync(self,read):
        for addr,data,size in views(self.f):
            if read:C.memmove(data,bytes(self.u.mem_read(addr,size)),size)
            else:self.u.mem_write(addr,C.string_at(data,size))
        if not read:
            self.word(MODELS[0]+0x180,FACEPTR if self.f.present[0] else 0)
            self.word(0x71af38,CAM0 if self.f.present[1] else 0)
            # Opening requires secondary camera regardless of the placement fixture.
            self.word(0x71af3c,CAM1 if self.f.present[2] else 0)
            self.word(0x71adc0,MOVIEPTR if self.f.present[3] else 0)
            for i in range(4):self.word(0x722574+0x120*i,SOUNDS[i] if self.f.present[i+4] else 0)
    def emit(self,event):
        self.trace.append((event,self.f.snapshot()))
        if self.fail_at==len(self.trace):self.failed=True;self.u.emu_stop();return False
        self.f.effect(event,len(self.trace));self.sync(False);return True
    def hook(self,u,address,_size,_ctx):
        self.sync(True);f=self.f;sp=u.reg_read(UC_X86_REG_ESP);ret=self.words(sp,1)[0];a=self.words(sp+4,12)
        result,cleanup=0,4
        if address in [0x4bb82e,0x4bc444,0x4bb0a4,0x4bb612]:
            assert a[0]==0x71af38
            op={0x4bb82e:0,0x4bc444:1,0x4bb0a4:2,0x4bb612:3}[address]
            event=('camera',op,I(a[1]).value if op in [0,1] else 0,
                tuple(a[2:5]) if op==1 else tuple(a[1:4]) if op==2 else (0,0,0),a[2] if op==0 else bits(f.seconds.value))
            result=f.camera_done.value
        elif address==0x4b76c2:
            assert a[1:3]==(1,0);event=('key',a[0]);result=f.keys[[0,0x5a,0x33450].index(a[0])]
        elif address==0x422c49:
            assert a[:2]==(ROOTS[1],REFERENCE);event=('place',0,tuple(a[2:5]),0)
        elif address==0x4241e3:
            assert a[:2]==(ROOTS[2],REFERENCE);event=('place',1,self.words(a[2],3),a[3])
        elif address==0x50db23:
            assert a[0]==0x722114;event=audio_event(0,flags=a[2]&255,amount=self.f32(sp+8)[0])
        elif address==0x4afe00:
            assert a[0]==BODY and a[2]==0;event=('cue',I(a[1]).value);result=f.triggered.value
        elif address==0x50d858:
            assert a[0]==0x722584 and a[4]==0
            event=audio_event(1,1,self.text(a[1]),self.text(a[2]),I(a[3]).value)
        elif address==0x46435e:
            assert a[0]==(SOUNDS[1] if f.present[5] else 0);event=audio_event(2,1,flags=a[1])
        elif address==0x50d2a0:
            event=audio_event(4,SOUNDS.index(a[0]),flags=a[9],amount=self.f32(sp+44)[0],
                source=self.f32(sp+8,4),listener=self.f32(sp+24,4))
        elif address==DIRECT:
            assert a[:3]==(SOUNDS[0],0,0);event=audio_event(3,flags=a[3]);cleanup=20
        elif address==0x521ed2:
            assert a[0]==MOVIEPTR;event=('movie',)
        elif address in [TIMECLOCK,TICKCLOCK]:event=('clock',int(address==TIMECLOCK));result=f.clock()
        elif address==0x534a34:event=('random',);result=f.random_value.value
        elif address==0x4af18f:
            assert a[0]==(SOUNDS[1] if f.present[5] else 0)
            event=('level',1,bits(f.seconds.value));self.u.mem_write(FLOATRET,bytes(f.level_value))
        elif address==0x401b0a:
            assert a[0]==BODY;event=('request',I(a[1]).value)
        elif address==0x4026fe:
            assert a[0]==BODY;event=('advance',a[1])
        elif address==0x4f3bb9:
            assert a[0]==1;event=('gaze',a[1],a[2])
        elif address==0x4f3c49:event=('face',0,a[0]&255,0,0)
        elif address==0x411de1:
            assert a[0]==FACEPTR and a[1]==0;event=('face',1,0,a[2],0)
        elif address==0x410fd8:
            assert a[0]==FACEPTR;event=('face',2,I(a[1]).value,0,0)
        elif address==0x411985:
            assert a[0]==FACEPTR;event=('face',3,0,a[1],a[2])
        elif address==0x4110ef:
            assert a[0]==FACEPTR;event=('face',4,0,0,a[1])
        elif address==0x423b01:
            assert a[0]==ROOTS[0];event=('hide',a[1])
        else:raise AssertionError(hex(address))
        if not self.emit(event):return
        if address==0x4af18f:return #the fld/ret fixture returns a genuine x87 float
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,original,fail_at=0):
        self.f,self.trace=original.clone(),[];self.fail_at,self.failed=fail_at,False;self.sync(False)
        self.u.mem_write(self.stack-1024,b'\xcd'*1024);self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(0x51b647,self.stop,count=30000)
        assert self.failed or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True);return self.f,self.trace

def portable(lib,original,fail_at=0,missing=None,query_fail=None):
    f,trace,callbacks,errors=original.clone(),[],[],[]
    def emit(event):
        trace.append((event,f.snapshot()))
        if fail_at==len(trace):return 0
        f.effect(event,len(trace));return 1
    def wrap(typ):
        def dec(fn):
            def checked(*args):
                try:return fn(*args)
                except Exception as exc:errors.append(repr(exc));return 0
            cb=typ(checked);callbacks.append(cb);return cb
        return dec
    @wrap(Camera)
    def camera(_,op,clip,center,dt,done,e):
        done[0]=f.camera_done.value;return emit(('camera',op,clip,floats(center[:3]),bits(dt)))
    @wrap(Timing)
    def timing(_,slot,source,end,e):
        if query_fail==('timing',slot):return 0
        source[0],end[0]=f.timings[slot];return 1
    @wrap(Key)
    def key(_,code,out,e):out[0]=f.keys[[0,0x5a,0x33450].index(code)];return emit(('key',code))
    @wrap(Present)
    def present(_,obj,out,e):
        if query_fail==('present',obj):return 0
        out[0]=f.present[obj];return 1
    @wrap(Place)
    def place(_,obj,v,degrees,e):return emit(('place',obj,floats(v[:3]),bits(degrees)))
    @wrap(Audio)
    def audio(_,ptr,e):
        a=ptr.contents
        return emit(audio_event(a.operation,a.slot,string(a.pack),string(a.name),a.volume,a.flags,a.amount,a.source,a.listener))
    @wrap(Cue)
    def cue(_,tick,out,e):out[0]=f.triggered.value;return emit(('cue',tick))
    @wrap(Movie)
    def movie(_,e):return emit(('movie',))
    @wrap(Clock)
    def clock(_,timer,out,e):out[0]=f.clock();return emit(('clock',timer))
    @wrap(Random)
    def random_(_,out,e):out[0]=f.random_value.value;return emit(('random',))
    @wrap(Level)
    def level(_,slot,dt,out,e):out[0]=f.level_value.value;return emit(('level',slot,bits(dt)))
    @wrap(Request)
    def request(_,clip,e):return emit(('request',clip))
    @wrap(Advance)
    def advance(_,dt,e):return emit(('advance',bits(dt)))
    @wrap(Gaze)
    def gaze(_,pitch,yaw,e):return emit(('gaze',bits(pitch),bits(yaw)))
    @wrap(Face)
    def face(_,op,v,a,t,e):return emit(('face',op,v,bits(a),t))
    @wrap(Hide)
    def hide(_,v,e):return emit(('hide',v))
    o=Ops(None,camera,timing,key,present,place,audio,cue,movie,clock,random_,level,request,advance,gaze,face,hide)
    if missing:setattr(o,missing,dict(Ops._fields_)[missing]())
    b=Bindings()
    for name,typ in Bindings._fields_:
        if name=='effect_positions':
            for i in range(4):b.effect_positions[i]=f.positions[i]
        else:setattr(b,name,C.cast(C.byref(getattr(f,name)),typ))
    e=C.create_string_buffer(256);ok=lib.bk_special_event_step(C.byref(b),C.byref(o),e)
    assert not errors,errors
    return bool(ok),f,trace,e.value.decode()

def fixture(rng,case):
    f=Fixture();f.state=State();f.group=I(case%5);f.phase=I(rng.choice([0,1,2,2,2,3,-1]))
    f.camera_clip=I(rng.randrange(-1,8));f.camera_mode=I(rng.choice([0,1,2,3,-1]))
    f.seconds=F(rng.choice([0,1/60,1/30,.001,.1,1,6]));f.paused=S(rng.choice([0,1,-1]))
    f.music_wanted=S(rng.choice([0,1,-1,2]));f.packed=S(rng.choice([0,1,-1,2]))
    f.visibility=B(rng.choice([0,1,128,255]));f.effect_loop=B(rng.randrange(256));f.effect_volume=I(rng.randrange(-10000,1))
    f.listener=(F*4)(*[rng.uniform(-100,100) for _ in range(4)])
    f.positions=((F*4)*4)(*[(F*4)(*[rng.uniform(-100,100) for _ in range(4)]) for _ in range(4)])
    f.envelope=Envelope(rng.uniform(-10,20),rng.uniform(-10,20))
    f.present=(I*8)(*[rng.randrange(2) for _ in range(8)]);f.present[2]=1;f.present[4]=1
    f.timings=((F*2)*2)((F*2)(rng.choice([0,45,60,80,100,float('nan')]),100),
                            (F*2)(rng.choice([0,50,100,101,float('nan')]),100))
    f.keys=(B*3)(*[rng.choice([0,0,1,255]) for _ in range(3)])
    f.camera_done=B(rng.choice([0,1,128,255]));f.triggered=B(rng.choice([0,0,1,255]))
    f.level_value=F(rng.uniform(0,9));f.random_value=I(rng.randrange(32768));f.random_index=U();f.clock_index=U()
    f.now=U(rng.choice([0,1000,30000,0x7ffffff0,0x80000000,0xfffffff0,rng.randrange(2**32)]));f.clock_step=U(rng.choice([0,1,13]))
    f.state.sequence=rng.choice([0,0,1,2,-128,-1,127]);f.state.face_target=rng.uniform(-5,15)
    for timer in [f.state.sequence_timer,f.state.face_timer]:
        timer.duration=rng.randrange(50000);timer.deadline=(f.now.value+rng.choice([-1,0,1,30000]))&0xffffffff
        timer.armed=rng.choice([0,1,128,255])
    f.mutate_at,f.mutations=0,{}
    if case%3==0:
        f.mutate_at=case%12+1
        f.mutations={'group.value':(f.group.value+2)%5,'camera_mode.value':2,'phase.value':2,
            'packed.value':1-f.packed.value,'music_wanted.value':1,'visibility.value':255,
            'seconds.value':F(.125).value,'listener.0':77.,'positions.1.1':-33.,'present.0':1,
            'state.sequence':0,'effect_volume.value':-42}
    return f

def compare(want,got,expected,trace,label):
    assert [x[0] for x in trace]==[x[0] for x in expected],(label,'calls',[x[0] for x in expected],[x[0] for x in trace])
    for i,((event,a),(_,b)) in enumerate(zip(trace,expected)):
        assert a==b,(label,'snapshot',i,event,[(OWNERS[j],x.hex(),y.hex()) for j,(x,y) in enumerate(zip(a,b)) if x!=y][:3])
    assert got.snapshot()==want.snapshot(),(label,'final',[(OWNERS[j],x.hex(),y.hex()) for j,(x,y) in enumerate(zip(got.snapshot(),want.snapshot())) if x!=y][:3])

def main():
    faulthandler.enable();p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path)
    p.add_argument('--cases',type=int,default=6000);p.add_argument('--output',type=Path,default=ROOT/'local/original-special-event.json')
    args=p.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library()
    lib.bk_special_event_initial.argtypes=[];lib.bk_special_event_initial.restype=State
    lib.bk_special_event_step.argtypes=[C.POINTER(Bindings),C.POINTER(Ops),P];lib.bk_special_event_step.restype=I
    assert bytes(lib.bk_special_event_initial())==bytes(State())
    initial=fixture(random.Random(1),1);initial.state=lib.bk_special_event_initial()
    for addr,data,size in views(initial):
        if 0x71ad90<=addr<=0x71adbc:assert n.u.mem_read(addr,size)==C.string_at(data,size)
    rng=random.Random(0x51b647);digest=hashlib.sha256();coverage=set()
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,live_mutations=0,failure_prefixes=0,rejections=0)
    def check(f,label,failures=False):
        want,expected=n.run(f);ok,got,trace,error=portable(lib,f);assert ok,(label,error)
        compare(want,got,expected,trace,label);stats['calls']+=len(trace)
        stats['live_mutations']+=0<f.mutate_at<=len(trace)
        coverage.update((x[0][0],x[0][1] if len(x[0])>1 else None) for x in trace)
        digest.update(b''.join(got.snapshot()))
        if failures:
            for fail_at in range(1,len(trace)+1):
                want,prefix=n.run(f,fail_at);ok,bad,got_prefix,_=portable(lib,f,fail_at);assert not ok
                compare(want,bad,prefix,got_prefix,(label,'failure',fail_at));stats['failure_prefixes']+=1
        return got
    for case in range(args.cases):
        check(fixture(rng,case),('random',case),case%97==0);stats['random_frames']+=1
    for group in range(5):
        for phase in [0,1,2,3]:
            for mode in [0,1,2,3]:
                for source,end in [(44.999996,100),(45,100),(80,100),(80.00001,100),(100,100),(101,100),(float('nan'),100),(0,float('nan'))]:
                    f=fixture(rng,1);f.group.value,f.phase.value,f.camera_mode.value=group,phase,mode
                    f.timings[0][0]=source;f.timings[1][:]=[source,end];f.camera_done.value=1
                    f.keys[:]=[0,0,0];f.present[:]=[1]*8;f.state=State();f.state.face_timer.armed=1;f.state.face_timer.deadline=f.now.value
                    check(f,('boundary',group,phase,mode,source,end));stats['boundary_frames']+=1
    for group in range(5):
        for mask in range(4):
            f=fixture(rng,1);f.group.value=group;f.phase.value=1
            f.present[1],f.present[2]=mask&1,(mask>>1)&1
            check(f,('camera-presence',group,mask));stats['boundary_frames']+=1
    for group in [0,1]:
        for now in [0,499,500,30000,0x7ffffffe,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]:
            for armed in [0,1,128,255]:
                for delta in [-1,0,1]:
                    f=fixture(rng,1);f.group.value=group;f.phase.value=2;f.state.sequence=1
                    f.now.value=now;f.clock_step.value=0
                    for timer in [f.state.face_timer,f.state.sequence_timer]:
                        timer.armed=armed;timer.deadline=(now+delta)&0xffffffff
                    check(f,('timer',group,now,armed,delta));stats['boundary_frames']+=1
    # Retained timers across real frame calls, loading resets only sequence;
    # simulation uses explicit clock and camera service fixtures, not assets.
    for group in range(5):
        f=fixture(rng,1);f.group.value=group;f.phase.value=0;f.camera_mode.value=0
        f.state=State();f.now.value=1000;f.clock_step.value=1;f.seconds.value=F(1/60).value
        f.present[:]=[1]*8;f.keys[:]=[0,0,0];f.camera_done.value=1
        for frame in range(2400):
            f.now.value=(1000+frame*17)&0xffffffff;f.timings[1][0]=min(frame,100);f.timings[0][0]=frame%130
            f.camera_mode.value=(frame//200)%4;f.triggered.value=int(frame%53==0)
            f=check(f,('retained',group,frame));stats['retained_frames']+=1
        if group==1:assert f.state.sequence==2
    # Every required callback is exercised without silently accepting absence.
    for missing,_ in Ops._fields_[1:]:
        found=False
        for group in range(5):
            f=fixture(rng,1);f.group.value=group;f.phase.value=0;f.timings[1][0]=0;f.keys[:]=[0,0,0]
            f.present[:]=[1]*8;f.triggered.value=1;f.music_wanted.value=1
            f.state.face_timer.armed=1;f.state.face_timer.deadline=f.now.value
            ok,_,_,error=portable(lib,f,missing=missing)
            if not ok:assert error=='special event: missing '+missing+' service',(missing,error);found=True;break
        assert found,missing;stats['rejections']+=1
    for group in [-1,5,2**31-1]:
        f=fixture(rng,1);f.group.value=group;ok,_,trace,_=portable(lib,f);assert not ok and not trace;stats['rejections']+=1
    for query in [('timing',0),('timing',1)]+[('present',i) for i in range(8)]:
        f=fixture(rng,1);f.group.value=2 if query==('timing',0) else 1;f.phase.value=0
        f.present[:]=[1]*8;ok,_,_,_=portable(lib,f,query_fail=query);assert not ok,query;stats['rejections']+=1
    # Standalone4af2d1: target can exceed9 but smoothed is capped; shared owner.
    lib.bk_voice_envelope_target.argtypes=[C.POINTER(Envelope),F,F,FP,P];lib.bk_voice_envelope_target.restype=I
    envelope_cases=0
    for _ in range(4096):
        target=F(rng.uniform(-30,60)).value;dt=F(rng.choice([0,1/60,.01,.1,1,10])).value
        env=Envelope(rng.uniform(-100,100),rng.uniform(-30,60));n.u.mem_write(0x708878,bytes(env));n.u.mem_write(0x733700,bytes(F(dt)))
        n.u.mem_write(n.stack,struct.pack('<II',n.stop,bits(target)));n.u.reg_write(UC_X86_REG_ESP,n.stack);n.u.reg_write(UC_X86_REG_FPCW,0x037f)
        n.u.emu_start(0x4af2d1,n.stop,count=1000);assert n.u.reg_read(UC_X86_REG_EIP)==n.stop
        out=F();err=C.create_string_buffer(256);assert lib.bk_voice_envelope_target(C.byref(env),target,dt,C.byref(out),err)
        assert bytes(env)==bytes(n.u.mem_read(0x708878,8));assert out.value==env.smoothed;envelope_cases+=1
    for target,dt,held in [(float('nan'),0,0),(0,-1,0),(0,float('inf'),0),(0,0,float('nan'))]:
        env=Envelope(1,held);before=bytes(env);out=F(123);err=C.create_string_buffer(256)
        assert not lib.bk_voice_envelope_target(C.byref(env),target,dt,C.byref(out),err)
        assert bytes(env)==before and out.value==123;stats['rejections']+=1
    required={('camera',i) for i in range(4)}|{('audio',i) for i in range(5)}|{('face',i) for i in range(5)}|{('key',i) for i in [0,0x5a,0x33450]}|{('clock',0),('clock',1),('movie',None),('random',None)}
    assert required<=coverage,required-coverage
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),scope=__doc__,**stats,
                envelope_cases=envelope_cases,state_sha256=digest.hexdigest(),max_error=0,
                functions=['51b647','4e5c91','4e5901','4e5dc0','4e6154','4e620a','4adbb9','4af2d1'])
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS special event:',json.dumps(report,sort_keys=True),flush=True)
if __name__=='__main__':main()
