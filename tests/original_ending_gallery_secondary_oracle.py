"""Full4843AA and nested47D9EE against the portable gallery controller.

Original state dispatch, clock deltas, x87 comparisons, sprintf/atoi delay,
camera table copies and speech helper execute. RNG, timeGetTime, actor,
camera, expression and audio leaves are observing fixtures. Every callback
and final alias is compared, including live mutations and failed prefixes.
This verifies CPU services, not real scene assets, PCM or playable phase8.
"""
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
import math
from pathlib import Path
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_gallery_control_oracle import (
    Scene, REGIONS, I, U, F, B, P, Expression, Fov, Camera as CameraCall,
    Target, Status, Load, bits, floating, fixture as parent_fixture)
from original_ending_gallery_normal_oracle import Native as NormalNative, Play, PRIMARY, TARGET, SOUNDS
from original_ending_gallery_effect_oracle import Random
from original_menu_camera_oracle import State as Camera
from original_ending_secondary_speech_oracle import Ops as SpeechOps


class State(C.Structure):
    _fields_ = [('remaining',I),('alternate',I),('fov',F),('cycles',B)]


class Saved(C.Structure):
    _fields_ = [('orbit',F*4),('toggle',B),('target',U*3)]


class Clip(C.Structure):
    _fields_ = [('end',F),('source',F),('chain',I),('loop',I),('finish_crossing',I)]


class Bindings(C.Structure):
    _fields_ = [(n,P) for n in ['frame','control','auxiliary','camera','saved',
        'substate','opening','cursor','counters','previous_clock','current_clock','elapsed']]


Clock = C.CFUNCTYPE(I,P,C.POINTER(U),P)
Voice = C.CFUNCTYPE(I,P,U,U,I,P)
Active = C.CFUNCTYPE(I,P,C.POINTER(I),P)
ReadClip = C.CFUNCTYPE(I,P,I,C.POINTER(Clip),P)
Request = C.CFUNCTYPE(I,P,I,P)


class Ops(C.Structure):
    _fields_ = [('context',P),('clock',Clock),('random',Random),('present',Status),
        ('status',Status),('voice',Voice),('expression',Expression),('fov',Fov),
        ('camera',CameraCall),('target',Target),('active',Active),('clip',ReadClip),
        ('request',Request),('restart',Request)]


class Fixture:
    owners = ['scene','state','saved','camera','target','volume','active','clips',
              'randoms','random_index','clocks','clock_index','restart_count']
    @property
    def final(self):return self.scene.retained.final

    def clone(self):
        f=Fixture()
        for name in self.owners:
            value=getattr(self,name);setattr(f,name,type(value).from_buffer_copy(value))
        for name in ['present','playing','hr','loaded']:setattr(f,name,list(getattr(self,name)))
        for name in ['seconds','camera_result','mutate_at','mutations','clip_capacity']:
            setattr(f,name,getattr(self,name))
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self,n)) for n in self.owners)+(
            tuple(self.present),tuple(self.playing))

    def effect(self,event,index):
        kind=event[0]
        if kind=='random':self.random_index.value+=1
        elif kind=='clock':self.clock_index.value+=1
        elif kind=='fov':self.camera.fov=floating(event[1])
        elif kind=='expression':
            self.scene.auxiliary.expression_a,self.scene.auxiliary.expression_b=event[1:3]
            self.scene.face_mode=event[3]
        elif kind=='camera':self.camera.yaw=F(self.camera.yaw+3.25).value
        elif kind=='load':self.present[event[1]],self.playing[event[1]]=self.loaded[event[1]],0
        elif kind=='play':self.playing[event[1]]=self.present[event[1]]
        elif kind in ['request','restart']:
            self.active.value=event[1]
            if kind=='restart':self.restart_count.value+=1
        if index==self.mutate_at:
            for path,value in self.mutations.items():
                obj=self;keys=path.split('.')
                for key in keys[:-1]:obj=obj[int(key)] if key.isdigit() else getattr(obj,key)
                key=keys[-1]
                if key.isdigit():obj[int(key)]=value
                else:setattr(obj,key,value)


class Native:
    word=NormalNative.word
    integer=NormalNative.integer
    string=NormalNative.string
    def __init__(self,exe):
        self.u=machine(exe)
        self.stack,self.stop=0x2008000,0x300f000
        self.clip_bytes,self.clip_blob=None,None
        for a,v in [(0x53f2f0,0x300d010),(0x53f214,0x300d020),
                    (0x53f358,0x300d030),(0x300b024,0x300d100),
                    (0x721ef4,TARGET),(0x721b28,PRIMARY)]:self.word(a,v)
        for sound in SOUNDS[:2]:self.word(sound,0x300b000)
        self.preset_reads=[0x484d57,0x484f47,0x48507d,0x48553a]
        self.clip_reads=[0x484c33,0x484e24,0x484fc7,0x485125,0x48534e]
        for a in [0x534a34,0x4018c8,0x401f71,0x42cf0e,0x4dfb96,0x4e0ecb,
                  0x47d9ee,0x4e0956,0x4ad2bf,0x300d010,0x300d020,
                  0x300d030,0x300d100,0x4844e9,*self.preset_reads,*self.clip_reads]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)

    def sync(self,read):
        f=self.f
        views=[(a,C.addressof(f.scene)+off,size) for off,a,size,_ in REGIONS]
        for obj,name,a,n in [(f.state,'remaining',0x5545e8,4),(f.state,'alternate',0x5545ec,4),
                            (f.state,'fov',0x5545f0,4),(f.state,'cycles',0x6dde59,1),
                            (f.saved,'orbit',0x6dde14,16),(f.saved,'toggle',0x6c7f7c,1),
                            (f.saved,'target',0x6d1bc0,12)]:
            views.append((a,C.addressof(obj)+getattr(type(obj),name).offset,n))
        for i,name in enumerate(['yaw','pitch','radius','height']):
            views.append((0x71b364+i*4,C.addressof(f.camera)+getattr(Camera,name).offset,4))
        views += [(a,C.addressof(obj),C.sizeof(obj)) for obj,a in [
            (f.target,TARGET+0xf0),(f.volume,0xbe9a08),(f.active,PRIMARY+0x140)]]
        for a,p,n in views:
            if read:C.memmove(p,bytes(self.u.mem_read(a,n)),n)
            else:self.u.mem_write(a,C.string_at(p,n))
        if read:assert self.u.mem_read(PRIMARY+0x190,128*156)==self.clip_blob
        else:
            raw=bytes(f.clips)
            if raw!=self.clip_bytes:
                block=bytearray(128*156)
                for i in range(128):
                    for offset,field in [(0x58,0),(0x60,4),(0x70,8),(0,12)]:
                        at=i*C.sizeof(Clip)+field
                        block[i*156+offset:i*156+offset+4]=raw[at:at+4]
                self.clip_bytes,self.clip_blob=raw,bytes(block)
                self.u.mem_write(PRIMARY+0x190,self.clip_blob)
            for i,sound in enumerate(SOUNDS[:2]):self.word(0x722334+i*0x120,sound if f.present[i] else 0)

    def halt(self,why):self.reason=why;self.u.emu_stop()

    def hook(self,u,address,_size,_context):
        self.sync(True);f=self.f
        if address in self.preset_reads:
            index=f.state.remaining if address==0x48553a else 0
            if f.scene.frame.group>=5 or not 0<=index<3:self.halt('preset bound')
            return
        if address in self.clip_reads:
            slot=14 if address==0x484fc7 else f.active.value
            if not 0<=slot<f.clip_capacity:self.halt('clip bound')
            return
        if address==0x4844e9:
            if f.scene.frame.group>=5:self.halt('camera bound')
            return
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.integer(sp)
        args=struct.unpack('<6I',u.mem_read(sp+4,24))
        result,cleanup,event=0,4,None
        if address==0x300d010:
            fmt=self.string(args[1])
            assert fmt in [b'PH%d03%02d.wav',b'%d000'],fmt
            name=fmt%((I(args[2]).value,I(args[3]).value) if fmt.startswith(b'PH') else I(args[2]).value)
            u.mem_write(args[0],name+b'\0');result=len(name)
        elif address==0x300d020:
            assert args[0] in [0x722224,0x722344]
            u.mem_write(args[0],self.string(args[1])+b'\0')
            self.sync(True);result,cleanup=args[0],12
        elif address==0x534a34:
            result=f.randoms[f.random_index.value%16];event=('random',result)
        elif address==0x300d030:
            result=f.clocks[f.clock_index.value%8];event=('clock',result)
        elif address in [0x4018c8,0x401f71]:
            assert args[0]==PRIMARY
            event=('request' if address==0x4018c8 else 'restart',I(args[1]).value)
        elif address==0x42cf0e:event=('fov',args[0])
        elif address==0x4dfb96:event=('expression',*[I(x).value for x in args[:3]])
        elif address==0x4e0ecb:
            assert args[0]==0x71af38
            event=('camera',1,I(args[1]).value,tuple(args[2:5]),args[5]);result=f.camera_result
        elif address==0x47d9ee:event=('voice',args[0],args[1],I(args[2]).value)
        elif address==0x4e0956:
            self.last_slot=args[1];event=('load',args[1],self.string(args[0]))
        elif address==0x4ad2bf:
            slot=self.last_slot
            assert args[0]==(SOUNDS[slot] if f.present[slot] else 0)
            event=('play',slot,I(args[1]).value,I(args[2]).value)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0]);assert f.present[slot]
            self.word(args[1],f.playing[slot]);result,cleanup=f.hr[slot],12
            event=('status',slot)
        else:raise AssertionError(hex(address))
        if event:
            self.trace.append((event,f.snapshot()))
            if self.fail_at==len(self.trace):self.halt('injected failure');return
            f.effect(event,len(self.trace))
        self.sync(False)
        if address==0x47d9ee:
            #Observe the helper entry, then execute its actual instructions.
            if f.scene.frame.group>=5:self.halt('voice group bound')
            return
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff)
        u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)

    def run(self,original,fail_at=0):
        self.f,self.trace=original.clone(),[]
        self.fail_at,self.reason,self.last_slot=fail_at,None,0
        self.sync(False)
        self.u.mem_write(0x733700,struct.pack('<f',self.f.seconds))
        self.u.mem_write(self.stack-2048,b'\xcd'*2048);self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(0x4843aa,self.stop,count=1000000)
        assert self.reason or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True);return self.f,self.trace


def portable(lib,original,fail_at=0,missing=None,query_fail=None):
    f,trace,errors,callbacks=original.clone(),[],[],[]
    def emit(event):
        trace.append((event,f.snapshot()))
        if fail_at==len(trace):return 0
        f.effect(event,len(trace));return 1
    def decorate(typ):
        def wrapper(fn):
            def checked(*args):
                try:return fn(*args)
                except Exception as exc:errors.append(repr(exc));return 0
            cb=typ(checked);callbacks.append(cb);return cb
        return wrapper
    @decorate(Clock)
    def clock(_,out,_e):
        out[0]=f.clocks[f.clock_index.value%8];return emit(('clock',out[0]))
    @decorate(Random)
    def rand(_,out,_e):
        out[0]=f.randoms[f.random_index.value%16];return emit(('random',out[0]))
    @decorate(Load)
    def load(_,slot,name,_e):return emit(('load',slot,name))
    @decorate(Play)
    def play(_,slot,flags,volume,_e):return emit(('play',slot,flags,volume))
    speech=SpeechOps(None,load,play)
    @decorate(Voice)
    def voice(_,cue,slot,flags,e):
        if not emit(('voice',cue,slot,flags)):return 0
        return lib.bk_ending_secondary_speech(f.scene.frame.group,cue,slot,flags,
            f.scene.speech_names,C.byref(f.volume),C.byref(speech),e)
    @decorate(Expression)
    def expression(_,a,b,m,_e):return emit(('expression',a,b,m))
    @decorate(Fov)
    def fov(_,value,_e):return emit(('fov',bits(value)))
    @decorate(CameraCall)
    def camera(_,kind,choice,offset,extra,out,_e):
        out[0]=f.camera_result
        return emit(('camera',kind,choice,tuple(offset[:3]),extra))
    @decorate(Target)
    def target(_,out,_e):
        if query_fail=='target':return 0
        for i in range(3):out[i]=f.target[i]
        return 1
    @decorate(Active)
    def active(_,out,_e):
        if query_fail=='active':return 0
        out[0]=f.active.value;return 1
    @decorate(ReadClip)
    def clip(_,index,out,_e):
        if query_fail=='clip' or not 0<=index<f.clip_capacity:return 0
        out[0]=f.clips[index];return 1
    @decorate(Request)
    def request(_,value,_e):return emit(('request',value))
    @decorate(Request)
    def restart(_,value,_e):return emit(('restart',value))
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot];return 1
    @decorate(Status)
    def status(_,slot,out,_e):
        out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1));return emit(('status',slot))
    b=Bindings()
    assert lib.bk_ending_state_gallery_secondary_bindings(C.byref(f.scene),C.byref(f.camera),C.byref(f.saved),C.byref(b))
    o=Ops(None,clock,rand,present,status,voice,expression,fov,camera,target,active,clip,request,restart)
    if missing:setattr(o,missing,dict(Ops._fields_)[missing]())
    error=C.create_string_buffer(256)
    ok=lib.bk_ending_gallery_secondary_step(C.byref(f.state),C.byref(b),f.seconds,C.byref(o),error)
    assert not errors,errors
    return bool(ok),f,trace,error.value.decode()


def fixture(rng,case):
    old=parent_fixture(rng,case);f=Fixture()
    for name in ['scene','target','volume','seconds','camera_result']:setattr(f,name,getattr(old,name))
    f.scene.frame.state_721ee0=5;f.final.byte_6d1be0=case%10
    f.final.byte_6dde58=rng.choice([0,1,2,3,255])
    f.scene.auxiliary.pending=rng.choice([0,1,2,3,4,10,11,13])
    f.final.words_6dde24[0]=rng.randrange(2);f.final.words_6dde24[1]=rng.randrange(2)
    f.final.word_6dde54=I(rng.choice([0,100,4999,5000,0xffffffff,0x7fffffff])).value
    f.final.word_6d1bd8=I(rng.choice([0,5000,0xfffffffe,0x7fffffff])).value
    f.state=State(rng.choice([0,1,2,3,20,5000,-1,0x7fffffff]),rng.choice([0,1,-1]),
                  rng.choice([.2,.21,.5,1,float('nan')]),rng.choice([0,1,2,3,127,128,255]))
    f.camera=Camera.from_buffer_copy(rng.randbytes(C.sizeof(Camera)))
    f.camera.yaw=19.25;f.camera.pitch=-13.75;f.camera.radius=51.5;f.camera.height=3.25
    f.saved=Saved((F*4)(-7,11,52,3),rng.randrange(256),(U*3)(bits(11),bits(12),bits(13)))
    f.clips=(Clip*128)(*[Clip(19,rng.choice([18,19,20,float('nan')]),rng.choice([0,0,1,-1]),rng.choice([0,0,1,-1])) for _ in range(128)])
    f.active=I(rng.choice([0,2,8,9,11,12,13,14,15,16,17,127]));f.clip_capacity=128
    f.randoms=(I*16)(*[rng.choice([0,25,26,999,1000,-1,-6,-10,-11,0x7fffffff,-0x80000000,rng.randrange(32768)]) for _ in range(16)])
    f.random_index=I(0);f.clocks=(U*8)(*[rng.randrange(0x100000000) for _ in range(8)])
    f.clock_index=I(0);f.restart_count=I(0)
    for name in ['present','playing','hr','loaded']:setattr(f,name,getattr(old,name))
    f.mutate_at=0;f.mutations={}
    if case%7==0:
        f.mutate_at=case//7%12+1
        f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,
            'volume.value':-437,'state.fov':.41,'scene.control.toggles.7':128,
            'scene.control.toggles.1':37,'camera.pitch':19.5,'saved.orbit.2':31.25,
            'saved.toggle':51,'active.value':14,'scene.auxiliary.pending':3,
            'final.byte_6d1be0':6,'final.words_6dde24.0':0,'state.alternate':0,
            'final.word_6d1bd8':73,'target.0':bits(19.5),'state.remaining':1}
    return f


def compare(want,got,expected,actual,label):
    assert [e for e,_ in expected]==[e for e,_ in actual],(label,'calls',[e for e,_ in expected],[e for e,_ in actual])
    names=Fixture.owners+['present','playing']
    for index,((event,a),(_,b)) in enumerate(zip(expected,actual)):
        assert a==b,(label,index,event,'snapshot',[names[j] for j,(x,y) in enumerate(zip(a,b)) if x!=y])
    assert want.snapshot()==got.snapshot(),(label,'final',[(n,bytes(getattr(want,n))[:40],bytes(getattr(got,n))[:40]) for n in Fixture.owners if bytes(getattr(want,n))!=bytes(getattr(got,n))])


def main():
    faulthandler.enable();p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path);p.add_argument('--cases',type=int,default=6000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-secondary.json')
    args=p.parse_args()
    if args.cases<1:p.error('cases must be positive')
    exe=args.exe.read_bytes();native,lib=Native(exe),library()
    lib.bk_ending_gallery_secondary_initial.argtypes=[];lib.bk_ending_gallery_secondary_initial.restype=State
    lib.bk_ending_gallery_camera_initial.argtypes=[];lib.bk_ending_gallery_camera_initial.restype=Saved
    lib.bk_ending_state_gallery_secondary_bindings.argtypes=[P,P,P,C.POINTER(Bindings)]
    lib.bk_ending_state_gallery_secondary_bindings.restype=I
    lib.bk_ending_gallery_secondary_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P]
    lib.bk_ending_gallery_secondary_step.restype=I
    lib.bk_ending_secondary_speech.argtypes=[U,I,U,U,C.POINTER(C.c_char*32),C.POINTER(I),C.POINTER(SpeechOps),P]
    lib.bk_ending_secondary_speech.restype=I
    lib.bk_ending_secondary_control_camera.argtypes=[U,U];lib.bk_ending_secondary_control_camera.restype=C.POINTER(F)
    initial=lib.bk_ending_gallery_secondary_initial();saved=lib.bk_ending_gallery_camera_initial()
    assert bytes(initial)[:12]==native.u.mem_read(0x5545e8,12)
    assert initial.cycles==native.u.mem_read(0x6dde59,1)[0]
    assert bytes(saved.target)==native.u.mem_read(0x6d1bc0,12)
    assert bytes(saved)[:16]==native.u.mem_read(0x6dde14,16) and saved.toggle==native.u.mem_read(0x6c7f7c,1)[0]
    assert struct.unpack('<8I',native.u.mem_read(0x4855a9,32))==(0x4843ef,0x484586,0x484702,0x484798,0x484b81,0x484dc3,0x4855a4,0x485320)
    for group in range(5):
        for row in range(3):
            assert C.string_at(lib.bk_ending_secondary_control_camera(group,row),16)==native.u.mem_read(0x553a80+group*48+row*16,16)
    rng,digest=random.Random(0x4843aa),hashlib.sha256()
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,
               live_mutations=0,failure_prefixes=0,bounded_rejections=0)
    operations,names=set(),set()
    def check(f,label,failure=False):
        want,expected=native.run(f);ok,got,trace,error=portable(lib,f)
        assert ok==(native.reason is None),(label,error,native.reason)
        compare(want,got,expected,trace,label)
        if native.reason:stats['bounded_rejections']+=1
        stats['calls']+=len(trace);stats['live_mutations']+=0<f.mutate_at<=len(trace)
        for event,_ in trace:
            operations.add(event[0])
            if event[0]=='load':names.add(event[2].decode())
        digest.update(bytes(got.scene)+bytes(got.state)+bytes(got.saved)+bytes(got.camera)+bytes(got.active))
        if failure:
            for index in range(1,len(trace)+1):
                want,expected=native.run(f,index);ok,bad,prefix,error=portable(lib,f,index)
                assert not ok
                compare(want,bad,expected,prefix,(label,'failure',index));stats['failure_prefixes']+=1
        return got
    for case in range(args.cases):
        check(fixture(rng,case),('random',case),case%47==0);stats['random_frames']+=1
    print('gallery secondary: randomized native comparison complete',flush=True)
    #Unsigned millisecond differences, first-sample suppression and wrap.
    for previous in [0,1,5000,0x7fffffff,0xfffffffe]:
        for current in [0,1,5000,0x80000000,0xffffffff]:
            for elapsed in [0,4999,5000,5001,0xffffffff]:
                for cycles in [0,2,128]:
                    f=fixture(rng,1);f.final.byte_6d1be0=3;f.state.remaining=5000
                    f.state.cycles=cycles;f.final.word_6d1bd8=I(previous).value
                    f.final.word_6dde54=I(elapsed).value;f.clocks[0]=current;f.clocks[1]=current+13
                    check(f,('clock',previous,current,elapsed,cycles));stats['boundary_frames']+=1
    for substate in [4,5,7]:
        for opening in [0,1,2]:
            for source in [18,19,20,float('nan')]:
                for chain,loop in [(0,0),(0,1),(1,0),(-1,-1)]:
                    f=fixture(rng,1);f.final.byte_6d1be0=substate;f.final.byte_6dde58=opening
                    f.active.value=12 if substate==4 else 15
                    f.clips[f.active.value]=Clip(19,source,chain,loop)
                    f.state.remaining=1 if substate==4 else 2;f.hr=[0,0]
                    f.present=[1,1];f.playing=[int(substate==4),0]
                    check(f,('descriptor',substate,opening,source,chain,loop));stats['boundary_frames']+=1
    #Neighboring float values around the x87 look-ahead threshold; the
    #comparison keeps its extended intermediate instead of rounding early.
    for seconds in [0,1/120,1/60,1/30,.125,.5,1,2]:
        seconds=F(seconds).value
        threshold=F(19-seconds*F(.3).value*60*2).value
        value=bits(threshold)
        for raw in [value-1,value,value+1]:
            f=fixture(rng,1);f.final.byte_6d1be0=4;f.active.value=12
            f.state.remaining=1;f.clips[12]=Clip(19,floating(raw),0,0)
            f.seconds=seconds;f.present=[1,1];f.playing=[1,0];f.hr=[0,0]
            check(f,('lookahead',seconds,raw));stats['boundary_frames']+=1
    for random_value in [-10,-9,-6,-5,-1,0,10,11,0x7fffffff,-0x80000000]:
        f=fixture(rng,1);f.final.byte_6d1be0=3;f.state.remaining=0;f.state.cycles=0
        f.state.alternate=1;f.final.word_6dde54=1;f.final.word_6d1bd8=0
        f.randoms[0]=random_value
        check(f,('decimal-delay',random_value),True);stats['boundary_frames']+=1
    #Sweep callbacks in the branches with camera saves/restores and timers.
    for group in range(5):
        for substate,opening,active in [(0,1,0),(1,0,0),(2,0,2),(3,0,8),(4,0,12),
                                       (5,0,13),(5,1,14),(5,1,15),(5,2,16),(7,0,13)]:
            base=fixture(rng,1);base.scene.frame.group=group;base.final.byte_6d1be0=substate
            base.final.byte_6dde58=opening;base.active.value=active
            base.clips[active]=Clip(19,19,0,0);base.state.remaining=1 if substate==4 else 2
            base.state.cycles=2;base.state.fov=.2;base.camera_result=1
            base.present=[1,1];base.playing=[int(substate==4),0];base.hr=[0,0]
            base.scene.control.toggles[7]=1;base.scene.auxiliary.pending=1
            base.final.word_6dde54=5001;base.final.word_6d1bd8=1
            check(base,('callback-base',group,substate,opening),True)
            _,events=native.run(base)
            for index in range(1,len(events)+1):
                f=base.clone();f.mutate_at=index
                f.mutations={'scene.frame.group':(group+1)%5,'volume.value':-891,
                    'camera.yaw':13.75,'saved.orbit.0':77.25,'saved.toggle':47,
                    'scene.control.toggles.1':21,'scene.control.toggles.7':0,
                    'active.value':15,'final.byte_6d1be0':6,'state.remaining':1,
                    'final.word_6d1bd8':119,'scene.auxiliary.pending':2}
                check(f,('mutation',group,substate,opening,index));stats['boundary_frames']+=1
    print('gallery secondary: boundary comparison complete',flush=True)
    #Retained caller-driven actors/voice/clocks. Explicit fixture chaining
    #11->12; this is not a simulation or validation of a real XAN scheduler.
    for group in range(5):
        for rate in [30,60,120]:
            f=fixture(rng,1);f.scene.frame.group=group;f.final.byte_6d1be0=0
            f.final.byte_6dde58=0;f.scene.auxiliary.pending=0
            f.state=lib.bk_ending_gallery_secondary_initial();f.saved=lib.bk_ending_gallery_camera_initial()
            f.camera_result=1;f.seconds=F(1/rate).value;f.active.value=0
            f.clips=(Clip*128)(*[Clip(19,19,0,0) for _ in range(128)])
            f.randoms=(I*16)(*range(16));f.hr=[0,0];f.present=[1,1];f.loaded=[1,1]
            f.final.word_6d1bd8=f.final.word_6dde54=0
            cursor=f.final.word_6c7f74;initial_toggle=f.scene.control.toggles[1]
            seen=set()
            for step in range(rate*90):
                seen.add(f.final.byte_6d1be0)
                if f.active.value==11:f.active.value=12
                f.playing=[int(f.final.byte_6d1be0==4),0]
                now=100+int(step*1000/rate);f.clocks=(U*8)(*[now+i for i in range(8)])
                f.clock_index.value=0
                f=check(f,('retained',group,rate,step));stats['retained_frames']+=1
                if f.scene.frame.state_721ee0==0:break
            assert f.scene.frame.state_721ee0==0 and f.final.word_6c7f74==cursor+1,(group,rate,step,f.final.byte_6d1be0)
            assert seen=={0,1,2,3,4,5,7},seen
            assert f.scene.control.toggles[1]==initial_toggle
            assert list(f.saved.orbit)==[0]*4 and list(f.final.words_6dde24)==[0]*10
        print('gallery secondary: retained group',group,'complete',flush=True)
    #Separate port policy assertions. All native comparisons above leave
    #finish_crossing zero, preserving the original instruction semantics.
    for crossing in [0,1]:
        f=fixture(rng,1);f.scene.frame.group=3;f.final.byte_6d1be0=4
        f.active.value=12;f.state.remaining=20;f.seconds=F(1/60).value
        f.clips[12]=Clip(330,328.9,0,1,crossing)
        f.present=[1,1];f.playing=[1,0];f.hr=[0,0];f.mutate_at=0
        ok,got,trace,error=portable(lib,f)
        assert ok and got.state.remaining==20-crossing,(crossing,error)
        assert got.final.byte_6d1be0==4
    rejected=0
    for seconds in [-1,float('nan'),float('inf')]:
        f=fixture(rng,1);f.seconds=seconds
        ok,got,trace,error=portable(lib,f);assert not ok and not trace and got.snapshot()==f.snapshot(),error
        rejected+=1
    for active in [-1,128,0x7fffffff]:
        f=fixture(rng,1);f.final.byte_6d1be0=7;f.active.value=active
        check(f,('clip-bound',active))
    f=fixture(rng,1);f.final.byte_6d1be0=0;f.final.byte_6dde58=1;f.scene.frame.group=255
    check(f,'camera-bound')
    #Finish does not read workspace and must wrap the shared cursor.
    f=fixture(rng,1);f.final.byte_6d1be0=5;f.final.byte_6dde58=2
    f.final.word_6c7f74=0x7fffffff;f.present=[0,0]
    got=check(f,'cursor-wrap');assert got.final.word_6c7f74==-0x80000000
    missing_count=0
    for member,_ in Ops._fields_[1:]:
        f=fixture(rng,1);f.final.byte_6d1be0=1;f.present=[1,1];f.playing=[0,0];f.hr=[0,0]
        if member in ['target','fov','camera']:
            f.final.byte_6d1be0=0;f.final.byte_6dde58=1
        elif member in ['clock','random']:
            f.final.byte_6d1be0=3;f.final.word_6dde54=100;f.state.remaining=0;f.state.cycles=0
        elif member in ['active','clip','restart']:
            f.final.byte_6d1be0=7;f.state.remaining=0;f.clips[f.active.value].source=19
        ok,got,trace,error=portable(lib,f,missing=member)
        assert not ok and error.endswith('missing '+member+' service'),(member,error)
        missing_count+=1
    for query in ['target','active','clip','present']:
        f=fixture(rng,1);f.final.byte_6d1be0=0 if query=='target' else 7
        f.final.byte_6dde58=1;f.clips[f.active.value].source=19
        ok,got,trace,error=portable(lib,f,query_fail=query)
        assert not ok and not trace and got.snapshot()==f.snapshot()
        rejected+=1
    f=fixture(rng,1)
    for arg in range(4):
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        inputs=[C.byref(f.scene),C.byref(f.camera),C.byref(f.saved),C.byref(out)];inputs[arg]=None
        assert not lib.bk_ending_state_gallery_secondary_bindings(*inputs) and bytes(out)==before
        rejected+=1
    assert operations=={'clock','random','request','restart','fov','expression','camera','voice','load','play','status'},operations
    assert stats['live_mutations'] and stats['failure_prefixes']
    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        input_query_rejections=rejected,missing_services=missing_count,camera_table_words=60,
        port_loop_crossing_checks=2,
        operations=sorted(operations),loaded_names=sorted(names),state_sha256=digest.hexdigest(),
        max_error=0,full_gallery=False,real_assets=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS gallery secondary:',stats,digest.hexdigest(),flush=True)


if __name__=='__main__':main()
