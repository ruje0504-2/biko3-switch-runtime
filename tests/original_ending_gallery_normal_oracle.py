"""Execute complete483AF0 and nested48C8C2 against portable gallery control.

The original jump tables, signed byte arithmetic, float timer, voice selection
and names execute unchanged. Only actor/camera/audio/RNG leaves are fixtures.
Compare all retained aliases and callback prefixes, including callback writes.
The first undefined native cue read is an explicit failure boundary. This does
not cover real assets, the other four children or a complete playable gallery.
"""
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from model_binding import ROOT, library
from original_matrix_oracle import machine
from original_ending_gallery_control_oracle import (
    Scene, REGIONS, I, U, F, B, P, Expression, Fov, Camera, Target, Status, Load,
    bits, floating, fixture as parent_fixture)
from original_ending_gallery_effect_oracle import Timing, ReadTiming, Random

PRIMARY, SECONDARY, TARGET = 0x3001000, 0x3007000, 0x3008000
SOUNDS = [0x300a000+i*32 for i in range(6)]


class State(C.Structure):
    _fields_ = [('fov', F), ('delta', B), ('clip', B)]


class Bindings(C.Structure):
    _fields_ = [(n,P) for n in ['frame','control','auxiliary','substate','opening',
        'cursor','workspace']]+[('workspace_capacity',U)]+[(n,P) for n in [
        'elapsed_bits','speech_names','voice_volume','effect_volume']]


Play = C.CFUNCTYPE(I,P,U,I,I,P)
Actor = C.CFUNCTYPE(I,P,U,I,P)
Slot = C.CFUNCTYPE(I,P,U,P)


class Ops(C.Structure):
    _fields_ = [('context',P),('random',Random),('load',Load),('play',Play),
        ('expression',Expression),('fov',Fov),('camera',Camera),('target',Target),
        ('hidden',Actor),('request',Actor),('timing',ReadTiming),
        ('present',Status),('status',Status),('pause',Slot),('stop',Slot)]


class VoiceBindings(C.Structure):
    _fields_ = [('group',P),('speech_names',P),('volume',P)]


class VoiceOps(C.Structure):
    _fields_ = Ops._fields_[:4]


class Fixture:
    @property
    def final(self): return self.scene.retained.final

    owners = ['scene','state','target','volume','camera_fov','effect_volume',
              'timings','randoms','random_index','hidden','requested']
    def clone(self):
        f=Fixture()
        for name in self.owners:
            value=getattr(self,name)
            setattr(f,name,type(value).from_buffer_copy(value))
        for name in ['present','playing','hr','loaded']:
            setattr(f,name,list(getattr(self,name)))
        for name in ['seconds','camera_result','mutate_at','mutations','capacity','timing_capacity','voice']:
            setattr(f,name,getattr(self,name))
        return f

    def snapshot(self):
        return tuple(bytes(getattr(self,n)) for n in self.owners)+(
            tuple(self.present),tuple(self.playing))

    def effect(self,event,index):
        kind=event[0]
        if kind=='random':self.random_index.value+=1
        elif kind=='fov':self.camera_fov.value=floating(event[1])
        elif kind=='expression':
            self.scene.auxiliary.expression_a,self.scene.auxiliary.expression_b=event[1:3]
            self.scene.face_mode=event[3]
        elif kind=='load':self.present[event[1]],self.playing[event[1]]=self.loaded[event[1]],0
        elif kind=='play':self.playing[event[1]]=self.present[event[1]]
        elif kind in ['pause','stop']:self.playing[event[1]]=0
        elif kind=='hidden':self.hidden[event[1]]=event[2]
        elif kind=='request':self.requested[event[1]]=event[2]
        if index==self.mutate_at:
            for path,value in self.mutations.items():
                obj=self
                keys=path.split('.')
                for key in keys[:-1]:obj=obj[int(key)] if key.isdigit() else getattr(obj,key)
                key=keys[-1]
                if key.isdigit():obj[int(key)]=value
                else:setattr(obj,key,value)


class Native:
    def __init__(self,exe):
        self.u=machine(exe)
        self.timing_bytes,self.timing_blob=None,None
        self.stack,self.stop=0x2008000,0x300f000
        for a,v in [(0x53f2f0,0x300d010),(0x53f214,0x300d020),
                    (0x300b024,0x300d100),(0x300b048,0x300d110),
                    (0x721f08,TARGET),(0x721b28,PRIMARY),(0x721b2c,SECONDARY),
                    (SECONDARY+0x160,0x3007500),(0x3007514,0x3007600)]:self.word(a,v)
        for sound in SOUNDS:self.word(sound,0x300b000)
        self.work_reads=[0x483cc9,0x483e6b,0x483f4b,0x483f5b,0x483f99,
                         0x483fc0,0x484013,0x4840d0,0x4840df]
        for a in [0x534a34,0x423a99,0x4018c8,0x42cf0e,0x4dfb96,0x4e0ecb,
                  0x4e0956,0x4ad2bf,0x4ad34a,0x300d010,0x300d020,
                  0x300d100,0x300d110,0x48c8c2,0x48cb1a,0x483eb4,0x484291,
                  0x483c35,*self.work_reads]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)

    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def integer(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
    def string(self,a):return bytes(self.u.mem_read(a,260)).split(b'\0')[0]

    def sync(self,read):
        f=self.f
        views=[(a,C.addressof(f.scene)+off,size) for off,a,size,_ in REGIONS]
        views += [(0x5545e4,C.addressof(f.state),4),
                  (0x6c7f7d,C.addressof(f.state)+State.delta.offset,2)]
        views += [(a,C.addressof(obj),C.sizeof(obj)) for obj,a in [
            (f.target,TARGET+0xf0),(f.volume,0xbe9a08),(f.effect_volume,0xbe9a10)]]
        for a,p,n in views:
            if read:C.memmove(p,bytes(self.u.mem_read(a,n)),n)
            else:self.u.mem_write(a,C.string_at(p,n))
        #The controller only reads descriptors. Compare the entire native
        #block at every boundary, and write it only when fixture leaves
        #change it. Avoid hundreds of redundant FFI transfers per callback.
        if read:
            assert self.u.mem_read(PRIMARY+0x1e4,128*156)==self.timing_blob
        else:
            raw=bytes(f.timings)
            if raw!=self.timing_bytes:
                block=bytearray(128*156)
                for i in range(128):
                    block[i*156:i*156+8]=raw[i*12:i*12+8]
                    block[i*156+12:i*156+16]=raw[i*12+8:i*12+12]
                self.timing_bytes,self.timing_blob=raw,bytes(block)
                self.u.mem_write(PRIMARY+0x1e4,self.timing_blob)
        if not read:
            for i,sound in enumerate(SOUNDS):self.word(0x722334+i*0x120,sound if f.present[i] else 0)

    def halt(self,why):
        self.reason=why
        self.u.emu_stop()

    def hook(self,u,address,_size,_context):
        sp=u.reg_read(UC_X86_REG_ESP)
        if address==0x48c8c2:
            self.voice_action=I(self.integer(sp+4)).value
            return
        self.sync(True)
        f=self.f
        if address==0x48cb1a:
            if not 0<=self.voice_action<=7:self.halt('undefined cue')
            return
        if address in self.work_reads:
            if not 0<=f.final.word_6c7f74<f.capacity:self.halt('workspace bound')
            return
        if address==0x483c35:
            if f.scene.frame.group>=5:self.halt('camera bound')
            return
        if address in [0x483eb4,0x484291]:
            clip=C.c_int8(f.state.clip).value
            if not 0<=clip<f.timing_capacity:self.halt('timing bound')
            return
        ret=self.integer(sp)
        args=struct.unpack('<6I',u.mem_read(sp+4,24))
        result,cleanup,event=0,4,None
        if address==0x300d010:
            fmt=self.string(args[1]);assert fmt==b'PH%d02%02d.wav',fmt
            name=fmt%(I(args[2]).value,I(args[3]).value)
            u.mem_write(args[0],name+b'\0');result=len(name)
        elif address==0x300d020:
            assert args[0] in [0x722224,0x722344]
            u.mem_write(args[0],self.string(args[1])+b'\0')
            self.sync(True);result,cleanup=args[0],12
        elif address==0x534a34:
            result=f.randoms[f.random_index.value%len(f.randoms)]
            event=('random',result)
        elif address==0x423a99:
            assert args[0]==0x3007600
            event=('hidden',1,I(args[1]).value)
        elif address==0x4018c8:event=('request',[PRIMARY,SECONDARY].index(args[0]),I(args[1]).value)
        elif address==0x42cf0e:event=('fov',args[0])
        elif address==0x4dfb96:event=('expression',*[I(x).value for x in args[:3]])
        elif address==0x4e0ecb:
            assert args[0]==0x71af38
            event=('camera',1,I(args[1]).value,tuple(args[2:5]),args[5]);result=f.camera_result
        elif address==0x4e0956:
            self.last_slot=args[1]
            event=('load',args[1],self.string(args[0]))
        elif address==0x4ad2bf:
            #NULL still reaches the native helper. Distinguish the effect3
            #call by its return address; all other calls follow load(slot).
            slot=5 if ret==0x483d88 else self.last_slot
            assert args[0]==(SOUNDS[slot] if f.present[slot] else 0)
            event=('play',slot,I(args[1]).value,I(args[2]).value)
        elif address==0x4ad34a:
            assert args[0]==(SOUNDS[0] if f.present[0] else 0)
            event=('pause',0)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0]);assert f.present[slot]
            self.word(args[1],f.playing[slot]);result,cleanup=f.hr[slot],12
            event=('status',slot)
        elif address==0x300d110:
            slot=SOUNDS.index(args[0]);assert slot==5 and f.present[slot]
            event=('stop',slot);cleanup=8
        else:raise AssertionError(hex(address))
        if event:
            self.trace.append((event,f.snapshot()))
            if self.fail_at==len(self.trace):self.halt('injected failure');return
            f.effect(event,len(self.trace))
        self.sync(False)
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff)
        u.reg_write(UC_X86_REG_ESP,sp+cleanup)
        u.reg_write(UC_X86_REG_EIP,ret)

    def run(self,original,fail_at=0):
        self.f,self.trace=original.clone(),[]
        self.fail_at,self.reason,self.last_slot=fail_at,None,0
        self.sync(False)
        self.u.mem_write(0x733700,struct.pack('<f',self.f.seconds))
        self.u.mem_write(self.stack-1024,b'\xcd'*1024)
        self.word(self.stack,self.stop)
        if self.f.voice is not None:
            for i,value in enumerate(self.f.voice):self.word(self.stack+4+i*4,value)
        self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(0x483af0 if self.f.voice is None else 0x48c8c2,self.stop,count=1000000)
        assert self.reason or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True)
        return self.f,self.trace


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
    @decorate(Random)
    def rand(_,out,_e):
        out[0]=f.randoms[f.random_index.value%len(f.randoms)]
        return emit(('random',out[0]))
    @decorate(Load)
    def load(_,slot,name,_e):return emit(('load',slot,name))
    @decorate(Play)
    def play(_,slot,flags,volume,_e):return emit(('play',slot,flags,volume))
    @decorate(Expression)
    def expression(_,a,b,m,_e):return emit(('expression',a,b,m))
    @decorate(Fov)
    def fov(_,value,_e):return emit(('fov',bits(value)))
    @decorate(Camera)
    def camera(_,kind,choice,offset,extra,out,_e):
        out[0]=f.camera_result
        return emit(('camera',kind,choice,tuple(offset[:3]),extra))
    @decorate(Target)
    def target(_,out,_e):
        if query_fail=='target':return 0
        for i in range(3):out[i]=f.target[i]
        return 1
    @decorate(Actor)
    def hidden(_,actor,value,_e):return emit(('hidden',actor,value))
    @decorate(Actor)
    def request(_,actor,value,_e):return emit(('request',actor,value))
    @decorate(ReadTiming)
    def timing(_,clip,out,_e):
        if query_fail=='timing' or clip>=f.timing_capacity:return 0
        out[0]=f.timings[clip];return 1
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot];return 1
    @decorate(Status)
    def status(_,slot,out,_e):
        out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1))
        return emit(('status',slot))
    @decorate(Slot)
    def pause(_,slot,_e):return emit(('pause',slot))
    @decorate(Slot)
    def stop(_,slot,_e):return emit(('stop',slot))
    b=Bindings()
    assert lib.bk_ending_state_gallery_normal_bindings(C.byref(f.scene),C.byref(f.volume),C.byref(f.effect_volume),C.byref(b))
    b.workspace_capacity=f.capacity
    ops=Ops(None,rand,load,play,expression,fov,camera,target,hidden,request,timing,present,status,pause,stop)
    if missing:setattr(ops,missing,dict(Ops._fields_)[missing]())
    error=C.create_string_buffer(256)
    if f.voice is None:
        ok=lib.bk_ending_gallery_normal_step(C.byref(f.state),C.byref(b),f.seconds,C.byref(ops),error)
    else:
        group=C.addressof(f.scene.frame)+type(f.scene.frame).group.offset
        vb=VoiceBindings(group,C.addressof(f.scene.speech_names),C.addressof(f.volume))
        vo=VoiceOps(None,ops.random,ops.load,ops.play)
        ok=lib.bk_ending_gallery_voice(C.byref(vb),*f.voice,C.byref(vo),error)
    assert not errors,errors
    return bool(ok),f,trace,error.value.decode()


def fixture(rng,case):
    old=parent_fixture(rng,case)
    f=Fixture()
    f.__dict__.update(old.__dict__)
    del f.records
    f.state=State(rng.choice([0,.2,.20001,.5,1,2,float('nan')]),rng.randrange(256),rng.randrange(128))
    f.scene.frame.state_721ee0=4
    f.final.byte_6c7f70=case%8
    f.final.byte_6dde58=rng.choice([0,1,2,255])
    f.final.word_6d1bcc=I(bits(rng.choice([0,9.9,10,10.001,float('nan')]))).value
    f.final.workspace_6c7f80[f.final.word_6c7f74]=rng.choice(list(range(8))+[-1,8,256,0x7fffffff])
    f.effect_volume=I(rng.randrange(-10000,1))
    f.timings=(Timing*128)(*[Timing(0,19,rng.choice([18,19,20,float('nan')])) for _ in range(128)])
    f.randoms=(I*16)(*[rng.choice([0,1,2,3,-1,-2,-3,0x7fffffff,-0x80000000,rng.randrange(32768)]) for _ in range(16)])
    f.random_index=I(0);f.hidden=(I*2)(7,9);f.requested=(I*2)(3,5)
    f.present=[rng.randrange(2) for _ in range(6)]
    f.playing=[rng.randrange(4) for _ in range(6)]
    f.hr=[rng.choice([0,0,0,-1]) for _ in range(6)]
    f.loaded=[rng.randrange(2) for _ in range(6)]
    f.seconds=F(rng.choice([0,1/120,1/60,1/30,.99,1,1.1,6])).value
    f.voice=None;f.timing_capacity=128
    f.mutate_at=0;f.mutations={}
    if case%7==0:
        f.mutate_at=case//7%11+1
        f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,
            'volume.value':-437,'effect_volume.value':-963,'state.fov':.41,
            'state.clip':11,'state.delta':2,'scene.control.toggles.7':128,
            'target.0':bits(19.5),'final.byte_6c7f70':5,
            'final.workspace_6c7f80.'+str(f.final.word_6c7f74):6}
    return f


def compare(want,got,expected,actual,label):
    assert [e for e,_ in expected]==[e for e,_ in actual],(label,'calls',[e for e,_ in expected],[e for e,_ in actual])
    names=Fixture.owners+['present','playing']
    for index,((event,a),(_,b)) in enumerate(zip(expected,actual)):
        assert a==b,(label,index,event,'snapshot',[names[j] for j,(x,y) in enumerate(zip(a,b)) if x!=y])
    assert want.snapshot()==got.snapshot(),(label,'final',[
        (n,bytes(getattr(want,n))[:32],bytes(getattr(got,n))[:32]) for n in Fixture.owners
        if bytes(getattr(want,n))!=bytes(getattr(got,n))])


def main():
    faulthandler.enable()
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path)
    p.add_argument('--cases',type=int,default=6000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-normal.json')
    args=p.parse_args()
    if args.cases<1:p.error('cases must be positive')
    exe=args.exe.read_bytes();native,lib=Native(exe),library()
    lib.bk_ending_gallery_normal_initial.argtypes=[];lib.bk_ending_gallery_normal_initial.restype=State
    lib.bk_ending_state_gallery_normal_bindings.argtypes=[P,P,P,C.POINTER(Bindings)]
    lib.bk_ending_state_gallery_normal_bindings.restype=I
    lib.bk_ending_gallery_normal_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P]
    lib.bk_ending_gallery_normal_step.restype=I
    lib.bk_ending_gallery_voice.argtypes=[C.POINTER(VoiceBindings),I,I,I,I,C.POINTER(VoiceOps),P]
    lib.bk_ending_gallery_voice.restype=I
    initial=lib.bk_ending_gallery_normal_initial()
    assert bytes(initial)[:4]==native.u.mem_read(0x5545e4,4)
    assert bytes(initial)[4:6]==native.u.mem_read(0x6c7f7d,2)
    assert struct.unpack('<6I',native.u.mem_read(0x484372,24))==(0x483b62,0x483cc4,0x483eb4,0x484040,0x4840ca,0x484291)
    rng,digest=random.Random(0x483af0),hashlib.sha256()
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,voice_cases=0,
               calls=0,live_mutations=0,failure_prefixes=0,undefined_cues=0,bounded_rejections=0)
    operations,names=set(),set()
    def check(f,label,failure=False):
        want,expected=native.run(f)
        ok,got,trace,error=portable(lib,f)
        assert ok==(native.reason is None),(label,error,native.reason)
        compare(want,got,expected,trace,label)
        if native.reason=='undefined cue':
            assert error=='gallery voice: native cue is uninitialized'
            stats['undefined_cues']+=1
        elif native.reason:stats['bounded_rejections']+=1
        stats['calls']+=len(trace);stats['live_mutations']+=0<f.mutate_at<=len(trace)
        for event,_ in trace:
            operations.add(event[0])
            if event[0]=='load':names.add(event[2].decode())
        digest.update(bytes(got.scene)+bytes(got.state)+bytes(got.random_index))
        if failure:
            for index in range(1,len(trace)+1):
                want,expected=native.run(f,index)
                ok,bad,prefix,error=portable(lib,f,index)
                assert not ok
                compare(want,bad,expected,prefix,(label,'failure',index))
                stats['failure_prefixes']+=1
        return got
    for case in range(args.cases):
        check(fixture(rng,case),('random',case),case%43==0)
        stats['random_frames']+=1
    print('gallery normal: randomized native comparison complete',flush=True)
    #Every helper action, low-byte mode, signed group, slot and raw flags.
    for action in [-1,*range(9),256]:
        for mode in [0,1,256,-256,128,-1]:
            for slot in [-1,0,1,2]:
                f=fixture(rng,1);f.voice=(action,mode,slot,-0x80000000)
                f.scene.frame.group=rng.choice([0,4,127,128,255])
                check(f,('voice',action,mode,slot),action==0 and mode==0)
                stats['voice_cases']+=1
    #Exact equality/unordered must advance; greater-than alone must not.
    #Exercise every byte around request wrapping with no out-of-bounds read.
    for substate in [2,4,5]:
        for clip in [0,1,33,126,127,128,255]:
            for source in [18,19,20,float('nan')]:
                f=fixture(rng,1);f.final.byte_6c7f70=substate;f.state.clip=clip
                f.final.workspace_6c7f80[f.final.word_6c7f74]=0
                if clip<128:f.timings[clip].source=source
                f.present=f.playing=f.hr=[0]*6
                check(f,('timing',substate,clip,source));stats['boundary_frames']+=1
    for seconds in [0,1/60,1,6]:
        for value in [9.9,10,10.000001,float('nan')]:
            f=fixture(rng,1);f.final.byte_6c7f70=3;f.seconds=F(seconds).value
            f.final.word_6d1bcc=I(bits(value)).value;f.present=[0]*6
            check(f,('float-wait',seconds,value));stats['boundary_frames']+=1
    #Sweep each reachable callback rather than relying only on randomized
    #mutation placement. In particular, the second actor must reread clip;
    #voice gets an action by value but the following voice rereads workspace.
    for substate in range(6):
        for action in range(8):
            base=fixture(rng,1);base.final.byte_6c7f70=substate
            base.final.byte_6dde58=1;base.camera_result=1;base.state.fov=.2
            cursor=base.final.word_6c7f74
            base.final.workspace_6c7f80[cursor]=action
            base.timings[base.state.clip].source=19
            base.hr=[0]*6;base.present=[1]*6;base.playing=[0,0,0,0,0,1]
            base.scene.control.toggles[7]=1
            _,events=native.run(base)
            for index in range(1,len(events)+1):
                f=base.clone();f.mutate_at=index
                next_cursor=(cursor+1)%10000
                f.mutations={'state.clip':127,'state.delta':3,
                    'final.word_6c7f74':next_cursor,
                    'final.workspace_6c7f80.'+str(next_cursor):6,
                    'scene.frame.group':1,'volume.value':-657,'effect_volume.value':-339,
                    'scene.control.toggles.7':0,'target.0':bits(35.25)}
                check(f,('mutation',substate,action,index));stats['boundary_frames']+=1
    #No workspace read occurs in completion; preserve 32-bit cursor wrap.
    f=fixture(rng,1);f.final.byte_6c7f70=5;f.final.word_6c7f74=0x7fffffff
    f.timings[f.state.clip].source=19;f.present=[0]*6
    got=check(f,'cursor-wrap');assert got.final.word_6c7f74==-0x80000000
    stats['boundary_frames']+=1
    print('gallery normal: voice and boundary comparison complete',flush=True)
    #Every action must go through opening/start/loop/wait/end and advance
    #the actual shared workspace cursor. These are retained CPU service
    #fixtures, not an assertion about asset loading or natural playback.
    for group in range(5):
        for action in range(8):
            f=fixture(rng,1);f.scene.frame.group=group
            f.final.byte_6c7f70=f.final.byte_6dde58=0
            cursor=f.final.word_6c7f74
            f.final.workspace_6c7f80[cursor]=action
            f.state=State(1,0,0);f.camera_result=1;f.seconds=F(1/60).value
            f.hr=[0]*6;f.present=[0]*6;f.loaded=[1]*6
            f.randoms=(I*16)(*[i%3 for i in range(16)])
            for step in range(900):
                f.playing=[0]*6
                for t in f.timings:t.source=t.end
                f=check(f,('retained',group,action,step));stats['retained_frames']+=1
                if f.scene.frame.state_721ee0==0:break
            assert f.scene.frame.state_721ee0==0 and f.final.word_6c7f74==cursor+1
        print('gallery normal: retained group',group,'complete',flush=True)
    for substate in [1,2,4]:
        for cursor in [-1,10000,0x7fffffff]:
            f=fixture(rng,1);f.final.byte_6c7f70=substate;f.final.word_6c7f74=cursor
            f.timings[f.state.clip].source=19;f.present=[0]*6
            check(f,('workspace-bound',substate,cursor))
    f=fixture(rng,1);f.final.byte_6c7f70=0;f.final.byte_6dde58=1;f.scene.frame.group=255
    check(f,'camera-bound')
    rejected=0
    for seconds in [-1,float('nan'),float('inf')]:
        f=fixture(rng,1);f.seconds=seconds
        ok,got,trace,error=portable(lib,f)
        assert not ok and not trace and got.snapshot()==f.snapshot(),error
        rejected+=1
    missing_count=0
    for member,_ in Ops._fields_[1:]:
        f=fixture(rng,1);f.final.byte_6c7f70=1
        f.final.workspace_6c7f80[f.final.word_6c7f74]=0
        f.present=[1]*6;f.playing=[0]*6;f.hr=[0]*6
        if member in ['target','fov','camera']:
            f.final.byte_6c7f70=0;f.final.byte_6dde58=1
        elif member in ['timing','present','status']:
            f.final.byte_6c7f70=2;f.timings[f.state.clip].source=19
        elif member in ['pause','stop']:
            f.final.byte_6c7f70=4;f.playing[5]=1
        ok,got,trace,error=portable(lib,f,missing=member)
        assert not ok and error.endswith('missing '+member+' service'),(member,error)
        missing_count+=1
    for query in ['target','timing','present']:
        f=fixture(rng,1);f.final.byte_6c7f70=0 if query=='target' else 2
        f.final.byte_6dde58=1;f.timings[f.state.clip].source=19
        ok,got,trace,error=portable(lib,f,query_fail=query)
        assert not ok and [e[0] for e,_ in trace]==['random','hidden']
        assert got.random_index.value==1 and got.hidden[1]==0
        rejected+=1
    f=fixture(rng,1)
    for arg in range(4):
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        inputs=[C.byref(f.scene),C.byref(f.volume),C.byref(f.effect_volume),C.byref(out)]
        inputs[arg]=None
        assert not lib.bk_ending_state_gallery_normal_bindings(*inputs) and bytes(out)==before
        rejected+=1
    assert operations=={'random','hidden','request','fov','expression','camera','load','play','status','pause','stop'}
    assert stats['undefined_cues'] and stats['live_mutations'] and stats['failure_prefixes']
    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        input_query_rejections=rejected,missing_services=missing_count,operations=sorted(operations),
        loaded_names=sorted(names),state_sha256=digest.hexdigest(),max_error=0,
        full_gallery=False,real_assets=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS gallery normal:',stats,digest.hexdigest(),flush=True)


if __name__=='__main__':main()
