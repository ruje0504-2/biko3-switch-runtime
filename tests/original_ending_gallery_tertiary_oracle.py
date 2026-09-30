"""Complete48758C and nested479739 with real retained-state bindings.

Native dispatch, comparisons, signed arithmetic, table addressing, filename
selection and shared writes execute. Actor/material/expression/audio/RNG
leaves are fixtures. This does not validate actual XAN, PCM, GPU or phase8.
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
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from model_binding import ROOT,library
from original_matrix_oracle import machine
from original_ending_gallery_control_oracle import (Scene,REGIONS,I,U,F,B,P,
    Expression,Target,Status,Load,Voice,bits,floating,fixture as parent_fixture)
from original_ending_gallery_normal_oracle import Native as Base,State as Normal,Actor,PRIMARY,TARGET
from original_ending_gallery_effect_oracle import Random,Timing
from original_ending_gallery_secondary_oracle import Active

ACTORS=[PRIMARY,0x3007000,0x3007400];NODES=[0,0x3009000,0x3009200]
SOUNDS=[0x300a000+i*32 for i in range(48)]
TABLES=[(0x551b2c,2),(0x54fcb4,6),(0x552554,4)]
NAMES=[]
class Extra(C.Structure):
    _fields_=[('reverse',I),('expression_override',I),('expression_latch',B),('eye_mode',I)]
class Bindings(C.Structure):
    _fields_=[(n,P) for n in ['frame','control','auxiliary','substate','clip','transition_latch',
        'cycles','expression_latch','cursor','workspace']]+[('workspace_capacity',U)]+[(n,P) for n in [
        'counters','elapsed_bits','reverse','face_mode','expression_override','voice_volume','effect_volume']]
class Views(C.Structure):
    _fields_=[(n,P) for n in ['reverse','expression_override','expression_latch','voice_volume','effect_volume']]
Play=C.CFUNCTYPE(I,P,U,I,P)
Material=C.CFUNCTYPE(I,P,C.c_char_p,U,F,P)
ReadTiming=C.CFUNCTYPE(I,P,I,C.POINTER(Timing),P)
class Ops(C.Structure):
    _fields_=[('context',P),('random',Random),('present',Status),('status',Status),('voice',Voice),
        ('play',Play),('expression',Expression),('material',Material),('hidden',Actor),
        ('request',Actor),('active',Active),('timing',ReadTiming),('target',Target)]

def pointer(obj,name=None):return C.addressof(obj)+(getattr(type(obj),name).offset if name else 0)
class Fixture:
    owners=['scene','normal','extra','target','volume','effect_volume','active','timings',
        'hidden','materials','randoms','random_index','waits']
    @property
    def final(self):return self.scene.retained.final
    def clone(self):
        f=Fixture()
        for n in self.owners:
            v=getattr(self,n);setattr(f,n,type(v).from_buffer_copy(v))
        for n in ['present','playing','hr','loaded']:setattr(f,n,list(getattr(self,n)))
        for n in ['capacity','timing_capacity','mutate_at','mutations','animate_requests']:setattr(f,n,getattr(self,n))
        return f
    def snapshot(self):return tuple(bytes(getattr(self,n)) for n in self.owners)+(tuple(self.present),tuple(self.playing))
    def effect(self,event,index):
        k=event[0]
        if k=='random':self.random_index.value+=1
        elif k=='load':self.present[event[1]],self.playing[event[1]]=self.loaded[event[1]],0
        elif k=='play':
            self.playing[event[1]]=self.present[event[1]]
            if self.animate_requests:self.waits[event[1]]=3
        elif k=='request':
            self.active[event[1]]=event[2]
            if self.animate_requests and event[1]==0 and 0<=event[2]<128:
                self.timings[event[2]].source=self.timings[event[2]].start
        elif k=='hidden':self.hidden[event[1]]=event[2]
        elif k=='expression':
            self.scene.auxiliary.expression_a,self.scene.auxiliary.expression_b=event[1:3]
            self.extra.eye_mode=event[3]
        elif k=='material':
            #Empty names are observed queries with no matching material.
            if event[1]:self.materials[NAMES.index(event[1])][:]=event[2:4]
        if index==self.mutate_at:
            for path,value in self.mutations.items():
                obj=self;keys=path.split('.')
                for key in keys[:-1]:obj=obj[int(key)] if key.isdigit() else getattr(obj,key)
                key=keys[-1]
                if key.isdigit():obj[int(key)]=value
                else:setattr(obj,key,value)

class Native:
    word=Base.word;integer=Base.integer;string=Base.string
    def __init__(self,exe):
        self.u=machine(exe);self.stack,self.stop=0x2008000,0x300f000
        self.clip_bytes,self.clip_blob=None,None
        for a,v in [(0x53f2f0,0x300d010),(0x53f214,0x300d020),(0x300b024,0x300d100),
            (0x721b28,ACTORS[0]),(0x721b2c,ACTORS[1]),(0x721b30,ACTORS[2]),(0x721f08,TARGET),
            (ACTORS[1]+0x160,0x3008800),(ACTORS[2]+0x160,0x3008900),
            (0x3008814,NODES[1]),(0x3008914,NODES[2])]:self.word(a,v)
        for sound in SOUNDS:self.word(sound,0x300b000)
        self.records=[0x48760c,0x4881b0,0x48823f,0x4883fb]
        self.timings=[0x487b79,0x487e27,0x4880be,0x488198]
        self.entry=0x48758c;self.effect_read=0x487c3f
        for a in [self.entry,self.effect_read,*self.records,*self.timings,0x534a34,0x479739,
            0x4e0956,0x4ad2bf,0x4dfb96,0x4a7d10,0x423a99,0x4018c8,0x300d010,0x300d020,0x300d100]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def sync(self,read):
        f=self.f;views=[(a,pointer(f.scene)+off,n) for off,a,n,_ in REGIONS]
        for obj,fields in [(f.normal,[('fov',0x5545e4,4),('delta',0x6c7f7d,1),('clip',0x6c7f7e,1)]),
            (f.extra,[('reverse',0x725704,4),('expression_override',0x6c7f78,4),('expression_latch',0x6dde52,1)])]:
            views += [(a,pointer(obj,name),n) for name,a,n in fields]
        views += [(TARGET+0xf0,pointer(f.target),12),(0xbe9a08,pointer(f.volume),4),
                  (0xbe9a10,pointer(f.effect_volume),4)]
        views += [(actor+0x140,pointer(f.active)+i*4,4) for i,actor in enumerate(ACTORS)]
        for a,p,n in views:
            if read:C.memmove(p,bytes(self.u.mem_read(a,n)),n)
            else:self.u.mem_write(a,C.string_at(p,n))
        if read:assert self.u.mem_read(PRIMARY+0x190,128*156)==self.clip_blob
        else:
            raw=bytes(f.timings)
            if raw!=self.clip_bytes:
                block=bytearray(128*156)
                for i in range(128):
                    for off,k in [(0x54,0),(0x58,4),(0x60,8)]:block[i*156+off:i*156+off+4]=raw[i*12+k:i*12+k+4]
                self.clip_bytes,self.clip_blob=raw,bytes(block);self.u.mem_write(PRIMARY+0x190,self.clip_blob)
            for i,sound in enumerate(SOUNDS):self.word(0x722334+i*0x120,sound if f.present[i] else 0)
    def halt(self,why):self.reason=why;self.u.emu_stop()
    def hook(self,u,address,_size,_context):
        self.sync(True);f=self.f
        if address==self.entry:self.entry_group=f.scene.frame.group;return
        if address==self.effect_read:
            if not 1<=self.entry_group<=4:self.halt('undefined entry effect')
            return
        if address in self.records:
            if not 0<=f.final.word_6c7f74<f.capacity:self.halt('record bound')
            return
        if address in self.timings:
            slot=f.active[0] if address==0x487b79 else (10 if address==0x488198 else C.c_int8(f.normal.clip).value)
            if not 0<=slot<f.timing_capacity:self.halt('timing bound')
            return
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.integer(sp);args=struct.unpack('<5I',u.mem_read(sp+4,20))
        result,cleanup,event=0,4,None
        if address==0x300d010:
            fmt=self.string(args[1]);assert fmt in [b'PH%d31%02d.wav',b'PH%d32%02d.wav'],fmt
            name=fmt%(I(args[2]).value,I(args[3]).value);u.mem_write(args[0],name+b'\0');result=len(name)
        elif address==0x300d020:
            assert args[0] in [0x722224,0x722344]
            u.mem_write(args[0],self.string(args[1])+b'\0');self.sync(True);result,cleanup=args[0],12
        elif address==0x534a34:result=f.randoms[f.random_index.value%16];event=('random',result)
        elif address==0x479739:event=('voice',I(args[0]).value,args[1],I(args[2]).value,I(args[3]).value)
        elif address==0x4e0956:self.last_slot=args[1];event=('load',args[1],self.string(args[0]))
        elif address==0x4ad2bf:
            #Return-site selects the semantic lane even when a buffer is null.
            slots={0x487656:0,0x4876be:0,0x487747:0,0x4877b2:0,0x4879a5:0,0x487a10:0,
                0x487c54:self.entry_effect_slot(),0x487c8a:0,0x487cd2:6,0x487cf9:0,
                0x487ebe:41,0x487efb:7,0x487f20:40,0x487f83:0,0x487fc9:7,0x487fee:40,
                0x488013:41,0x488057:0,0x4881d4:40,0x48847a:1,0x4884bf:1,0x488507:1,0x48854b:1}
            slot=slots[ret];assert args[0]==(SOUNDS[slot] if f.present[slot] else 0) and args[1]==0,(hex(ret),args,slot)
            event=('play',slot,I(args[2]).value)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0]);self.word(args[1],f.playing[slot]);result,cleanup=f.hr[slot],12;event=('status',slot)
        elif address==0x4dfb96:event=('expression',*[I(x).value for x in args[:3]])
        elif address==0x4a7d10:
            if f.scene.frame.group>=5 or not any(a<=args[0]<a+5*n*260 and (args[0]-a)%260==0 for a,n in TABLES):self.halt('material bound');return
            event=('material',self.string(args[0]),args[1],args[2])
        elif address==0x423a99:event=('hidden',NODES.index(args[0]),I(args[1]).value)
        elif address==0x4018c8:event=('request',ACTORS.index(args[0]),I(args[1]).value)
        else:raise AssertionError(hex(address))
        if event:
            self.trace.append((event,f.snapshot()))
            if self.fail_at==len(self.trace):self.halt('injected failure');return
            f.effect(event,len(self.trace))
        self.sync(False)
        if address==0x479739:
            if f.scene.frame.group>=5:self.halt('voice group bound')
            return
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def entry_effect_slot(self):return {1:10,2:11,3:9,4:8}.get(self.entry_group,-1)
    def run(self,original,fail_at=0):
        self.f,self.trace=original.clone(),[];self.reason=None;self.fail_at=fail_at
        self.entry_group=self.f.scene.frame.group;self.last_slot=0
        self.sync(False);self.u.mem_write(self.stack-2048,b'\xcd'*2048);self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(self.entry,self.stop,count=1000000)
        assert self.reason or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True);return self.f,self.trace

def portable(lib,original,fail_at=0,missing=None,query_fail=None):
    f,trace,errors,callbacks=original.clone(),[],[],[]
    def emit(event):
        trace.append((event,f.snapshot()))
        if fail_at==len(trace):return 0
        f.effect(event,len(trace));return 1
    def decorate(typ):
        def wrap(fn):
            def checked(*args):
                try:return fn(*args)
                except Exception as exc:errors.append(repr(exc));return 0
            cb=typ(checked);callbacks.append(cb);return cb
        return wrap
    @decorate(Random)
    def random(_,out,_e):out[0]=f.randoms[f.random_index.value%16];return emit(('random',out[0]))
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot];return 1
    @decorate(Status)
    def status(_,slot,out,_e):out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1));return emit(('status',slot))
    @decorate(Voice)
    def voice(_,cue,slot,bank,select,e):
        if not emit(('voice',cue,slot,bank,select)):return 0
        name=C.create_string_buffer(32)
        if not lib.bk_ending_sound_tertiary_voice(f.scene.frame.group,cue,select,name,e):return 0
        C.memmove(pointer(f.scene,'speech_names')+slot*32,name,len(name.value)+1)
        return emit(('load',slot,name.value))
    @decorate(Play)
    def play(_,slot,volume,_e):return emit(('play',slot,volume))
    @decorate(Expression)
    def expression(_,a,b,m,_e):return emit(('expression',a,b,m))
    @decorate(Material)
    def material(_,name,hidden,alpha,_e):return emit(('material',name,hidden,bits(alpha)))
    @decorate(Actor)
    def hidden(_,actor,value,_e):return emit(('hidden',actor,value))
    @decorate(Actor)
    def request(_,actor,value,_e):return emit(('request',actor,value))
    @decorate(Active)
    def active(_,out,_e):
        if query_fail=='active':return 0
        out[0]=f.active[0];return 1
    @decorate(ReadTiming)
    def timing(_,slot,out,_e):
        if query_fail=='timing' or not 0<=slot<f.timing_capacity:return 0
        out[0]=f.timings[slot];return 1
    @decorate(Target)
    def target(_,out,_e):
        if query_fail=='target':return 0
        for i in range(3):out[i]=f.target[i]
        return 1
    b=Bindings();v=Views(pointer(f.extra,'reverse'),pointer(f.extra,'expression_override'),
        pointer(f.extra,'expression_latch'),pointer(f.volume),pointer(f.effect_volume))
    assert lib.bk_ending_state_gallery_tertiary_bindings(C.byref(f.scene),C.byref(f.normal),C.byref(v),C.byref(b))
    assert b.workspace==pointer(f.final,'workspace_6c7f80') and b.clip==pointer(f.normal,'clip')
    b.workspace_capacity=f.capacity
    o=Ops(None,random,present,status,voice,play,expression,material,hidden,request,active,timing,target)
    if missing:setattr(o,missing,dict(Ops._fields_)[missing]())
    e=C.create_string_buffer(256);ok=lib.bk_ending_gallery_tertiary_step(C.byref(b),C.byref(o),e)
    assert not errors,errors
    return bool(ok),f,trace,e.value.decode()

def fixture(rng,case):
    f=Fixture();old=parent_fixture(rng,case);f.scene=old.scene
    f.scene.frame.state_721ee0=7;f.final.byte_6d1bd4=case%6
    f.final.byte_6dde50=rng.choice([0,1,2,128,255]);f.final.byte_6dde51=rng.choice([0,1,2,3,127,128,255])
    f.final.word_6c7f74=0;f.final.workspace_6c7f80[0]=rng.choice([0,14,15,15,16,16,17,17,18,255])
    for i in range(3):f.final.words_6dde24[i]=rng.choice([-1,0,0,1,2,0x7fffffff])
    f.final.word_6d1bcc=I(bits(rng.choice([0,1,float('nan')]))).value
    f.scene.auxiliary.progress=rng.choice([0,.18999998,.19,.19000002,1,float('nan')])
    f.normal=Normal(1.25,19,rng.choice([0,2,5,7,9,127,128,255]))
    f.extra=Extra(rng.choice([0,1,2,-1]),3,rng.choice([0,1,2,255]),0)
    f.target=(U*3)(bits(1),bits(3),bits(5));f.volume=I(-351);f.effect_volume=I(-173)
    f.active=(I*3)(rng.choice([0,2,3,4,5,7,9,10,11,127]),4,6)
    f.timings=(Timing*128)(*[Timing(10,19,rng.choice([9,10,11,18,19,20,371,float('nan')])) for _ in range(128)])
    f.hidden=(I*3)(1,1,1);f.materials=((U*2)*len(NAMES))(*[(U*2)(1,bits(1)) for _ in NAMES])
    f.randoms=(I*16)(*[rng.choice([0,25,26,999,-1,-999,1000,-1000,0x7fffffff,-0x80000000]) for _ in range(16)]);f.random_index=I(0)
    f.present=[rng.randrange(2) for _ in range(48)];f.playing=[rng.randrange(4) for _ in range(48)]
    f.hr=[rng.choice([0,0,0x80004005]) for _ in range(48)];f.loaded=[rng.randrange(2) for _ in range(48)]
    f.capacity=10000;f.timing_capacity=128;f.mutate_at=0;f.mutations={}
    f.waits=(I*48)();f.animate_requests=False
    if case%7==0:
        f.mutate_at=case//7%12+1
        f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,'volume.value':-492,
            'effect_volume.value':-297,'scene.control.toggles.5':0,'scene.control.toggles.7':128,
            'scene.auxiliary.index':72,'scene.auxiliary.progress':.2,
            'final.byte_6d1bd4':4,'final.byte_6dde50':1,'final.words_6dde24.0':0,
            'normal.clip':9,'extra.reverse':0,'extra.expression_latch':19,
            'target.0':bits(19.5)}
    return f

def compare(want,got,expected,actual,label):
    assert [e for e,_ in expected]==[e for e,_ in actual],(label,'calls',[e for e,_ in expected],[e for e,_ in actual])
    names=Fixture.owners+['present','playing']
    for index,((event,a),(_,b)) in enumerate(zip(expected,actual)):
        if a!=b:raise AssertionError((label,index,event,[(names[j],[(k,x[k],y[k]) for k in range(len(x)) if x[k]!=y[k]][:25]) for j,(x,y) in enumerate(zip(a,b)) if x!=y]))
    assert want.snapshot()==got.snapshot(),(label,'final',[(n,[(i,a,b) for i,(a,b) in enumerate(zip(bytes(getattr(want,n)),bytes(getattr(got,n)))) if a!=b][:25]) for n in Fixture.owners if bytes(getattr(want,n))!=bytes(getattr(got,n))])

def main():
    faulthandler.enable();p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path);p.add_argument('--cases',type=int,default=6000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-tertiary.json')
    args=p.parse_args()
    if args.cases<1:p.error('cases must be positive')
    exe=args.exe.read_bytes();native,lib=Native(exe),library()
    lib.bk_ending_state_gallery_tertiary_bindings.argtypes=[P,P,P,C.POINTER(Bindings)]
    lib.bk_ending_state_gallery_tertiary_bindings.restype=I
    lib.bk_ending_gallery_tertiary_step.argtypes=[C.POINTER(Bindings),C.POINTER(Ops),P];lib.bk_ending_gallery_tertiary_step.restype=I
    lib.bk_ending_sound_tertiary_voice.argtypes=[U,I,I,P,P];lib.bk_ending_sound_tertiary_voice.restype=I
    lib.bk_ending_tertiary_material_name.argtypes=[I,U,U];lib.bk_ending_tertiary_material_name.restype=C.c_char_p
    assert struct.unpack('<4I',native.u.mem_read(0x488664,16))==(0x48760c,0x487b59,0x488108,0x4885d3)
    for table,((address,width),old) in enumerate(zip(TABLES,[0x54ae00,0x548f88,0x54b828])):
        assert native.u.mem_read(address,5*width*260)==native.u.mem_read(old,5*width*260)
        for g in range(5):
            for k in range(width):
                name=native.string(address+(g*width+k)*260)
                assert lib.bk_ending_tertiary_material_name(table,g,k)==name
                if name not in NAMES:NAMES.append(name)
    rng=random.Random(0x48758c);digest=hashlib.sha256();operations=set();examples={}
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,live_mutations=0,failure_prefixes=0,bounded_rejections=0,reverse_frames=0)
    def check(f,label,fail_at=0):
        want,expected=native.run(f,fail_at);ok,got,actual,error=portable(lib,f,fail_at)
        assert ok==(native.reason is None),(label,native.reason,error)
        compare(want,got,expected,actual,label)
        for event,_ in actual:operations.add(event[0]);examples.setdefault(event[0],f.clone())
        for blob in got.snapshot():
            if isinstance(blob,bytes):digest.update(blob)
        stats['calls']+=len(actual)
        if f.mutate_at and f.mutate_at<=len(actual) and fail_at!=f.mutate_at:stats['live_mutations']+=1
        if fail_at:stats['failure_prefixes']+=1
        elif native.reason:stats['bounded_rejections']+=1
        return got,actual
    for case in range(args.cases):
        f=fixture(rng,case);got,events=check(f,('random',case));stats['random_frames']+=1
        if case<100 or case%79==0:
            for i in range(1,len(events)+1):check(f,('failed',case,i),i)
        if case and case%1000==0:print('gallery tertiary: random',case,flush=True)
    #Cover each named record, group, both passes, native timing relations,
    #three reverse-loop counts and direct-load/Play separation.
    for group in range(5):
     for action in [15,16,17]:
      for state in [1,2,3,4]:
       for latch in [0,1]:
        for source in [9,10,19,20,float('nan')]:
         f=fixture(rng,1);f.scene.frame.group=group;f.final.workspace_6c7f80[0]=action
         f.final.byte_6d1bd4=state;f.final.byte_6dde50=latch;f.final.byte_6dde51=2
         f.normal.clip=(2 if not latch else {15:5,16:7,17:9}[action]);f.active[0]=f.normal.clip if state==2 else 4
         f.present=[1]*48;f.playing=[0]*48;f.hr=[0]*48;f.loaded=[1]*48
         f.scene.control.toggles[5]=0;f.scene.control.toggles[7]=1;f.scene.auxiliary.progress=.2
         for t in f.timings:t.source=source
         got,events=check(f,('boundary',group,action,state,latch,source));stats['boundary_frames']+=1
         if source==19:
          for i in range(1,len(events)+1):check(f,('boundary-fail',group,action,state,latch,i),i)
    #Explicit queries at equal, unordered and adjacent branch thresholds.
    for active in [2,5,7,9,10]:
     for source in [9,10,10.000001,18.999998,19,19.000002,369.99997,370,370.00003,float('nan'),float('inf'),-float('inf')]:
      for cycles in [0,1,2,3,127,128,255]:
        f=fixture(rng,1);f.scene.frame.group=2;f.normal.clip=active;f.active[0]=active
        f.final.byte_6d1bd4=3 if active==10 else 2;f.final.byte_6dde51=cycles
        f.final.word_6d1bcc=0;f.final.workspace_6c7f80[0]=17;f.present=[0]*48
        f.timings[active].source=source
        check(f,('timing',active,source,cycles));stats['boundary_frames']+=1
    for progress in [.18999998,.19,.19000002,float('nan'),float('inf'),-float('inf')]:
     for counter in [-0x80000000,-1,0,1,2,0x7fffffff]:
        f=fixture(rng,1);f.final.byte_6d1bd4=3;f.active[0]=4;f.present=[0]*48
        f.final.words_6dde24[1]=0;f.final.words_6dde24[2]=counter;f.scene.auxiliary.progress=progress
        check(f,('counter-progress',progress,counter));stats['boundary_frames']+=1
    for random_value in [-0x80000000,-1001,-1000,-999,-1,0,25,26,999,1000,1025,1026,0x7fffffff]:
     for counter in [0,1]:
        f=fixture(rng,1);f.final.byte_6d1bd4=3;f.final.byte_6dde50=1
        f.final.workspace_6c7f80[0]=15;f.active[0]=4;f.present=[0]*48
        f.final.words_6dde24[0]=counter;f.scene.control.toggles[7]=1
        f.randoms=(I*16)(*([random_value]*16))
        check(f,('random-threshold',random_value,counter));stats['boundary_frames']+=1
    #Change each live alias after every observed leaf of complete branches.
    #Capture-on-entry effect selection and live material/volume selection differ.
    for group in range(5):
     for action,state,clip in [(15,2,2),(15,2,5),(16,1,7),(17,1,9),(16,2,7),(17,2,9),(15,3,4)]:
        f=fixture(rng,1);f.scene.frame.group=group;f.final.workspace_6c7f80[0]=action
        f.final.byte_6d1bd4=state;f.final.byte_6dde50=1;f.final.byte_6dde51=2
        f.normal.clip=clip;f.active[0]=clip;f.timings[clip].source=19
        f.present=[1]*48;f.playing=[0]*48;f.hr=[0]*48;f.loaded=[1]*48
        f.final.words_6dde24[0]=0;f.scene.control.toggles[7]=1;f.scene.control.toggles[5]=0
        _,events=check(f,('mutation-base',group,action,state,clip))
        for index in range(1,len(events)+1):
            f.mutate_at=index
            f.mutations={'scene.frame.group':(group+1)%5,'volume.value':-901,
                'effect_volume.value':-733,'scene.control.toggles.7':0,'scene.auxiliary.index':73,
                'final.workspace_6c7f80.0':17,'final.byte_6dde50':0,
                'final.words_6dde24.1':0,'final.words_6dde24.2':0x7fffffff,
                'active.0':5,'normal.clip':127,'extra.reverse':1,
                'extra.expression_latch':127,'scene.face_mode':39}
            check(f,('mutation',group,action,state,clip,index))
    #Retain the same real state aliases through both passes. Synthetic actor
    #and sound progression is explicitly outside the native controller.
    for group in range(5):
     for action in [15,16,17]:
      for rate in [30,60,120]:
        f=fixture(rng,1);f.scene.frame.group=group;f.final.workspace_6c7f80[0]=action
        f.final.byte_6d1bd4=1;f.final.byte_6dde50=0;f.final.byte_6dde51=0
        f.final.word_6d1bcc=0;f.extra.reverse=0;f.scene.auxiliary.progress=.2
        f.scene.control.toggles[5]=0;f.scene.control.toggles[7]=1
        f.active[0]=0;f.present=[1]*48;f.playing=[0]*48;f.hr=[0]*48;f.loaded=[1]*48
        for i in range(10):f.final.words_6dde24[i]=0
        for t in f.timings:t.start=10;t.end=19;t.source=10
        f.timings[10]=Timing(360,380,360)
        f.animate_requests=True;seen=set();reverse=0
        for frame in range(rate*8):
            for i in range(48):
                if f.waits[i]>0:
                    f.waits[i]-=1
                    if not f.waits[i]:f.playing[i]=0
            state=f.final.byte_6d1bd4;seen.add(state)
            t=f.timings[f.active[0]]
            if state in [2,3]:
                amount=F(30/rate).value
                if state==2 and f.extra.reverse==1:
                    t.source=max(t.start,F(t.source-amount).value);reverse+=1
                else:t.source=min(t.end,F(t.source+amount).value)
                if state==3 and f.active[0] in [3,6,8,10] and t.source>=t.end:f.active[0]=4
            f,_=check(f,('retained',group,action,rate,frame));stats['retained_frames']+=1
            if not f.scene.frame.state_721ee0:break
        assert not f.scene.frame.state_721ee0 and f.final.word_6c7f74==1,(group,action,rate,frame,state,f.active[0],f.normal.clip)
        assert seen=={1,2,3,4} and f.final.word_6d1bcc==0 and f.final.words_6dde24[1]==0
        assert f.normal.clip=={15:6,16:8,17:10}[action]
        assert f.normal.fov==1.25 and f.normal.delta==19
        assert (reverse>0)==(action!=15),(group,action,rate,reverse)
        stats['reverse_frames']+=reverse
      print('gallery tertiary: retained',group,action,'complete',flush=True)
    #Native array accesses are stopped at the first unsafe boundary, not
    #claimed to have a native safe-rejection behavior.
    for state in [1,3]:
     for cursor in [-1,10000,0x7fffffff]:
        f=fixture(rng,1);f.final.byte_6d1bd4=state;f.active[0]=4;f.present=[0]*48
        f.final.word_6c7f74=cursor
        check(f,('record-bound',state,cursor));assert native.reason
    for active in [-1,128,0x7fffffff]:
        f=fixture(rng,1);f.final.byte_6d1bd4=2;f.normal.clip=2;f.active[0]=active
        check(f,('timing-bound',active));assert native.reason
    for clip in [7,9]:
        f=fixture(rng,1);f.final.byte_6d1bd4=2;f.normal.clip=clip;f.timing_capacity=clip
        check(f,('fixed-timing-bound',clip));assert native.reason
    #Group0 can legitimately skip its uninitialized effect selector. If a
    #callback changes group before the read, reject exactly at that read.
    f=fixture(rng,1);f.scene.frame.group=0;f.final.byte_6d1bd4=2;f.normal.clip=2
    f.active[0]=2;f.timings[2].source=19;f.present=[0]*48
    got,events=check(f,'group0-valid');assert native.reason is None
    f.mutate_at=1;f.mutations={'scene.frame.group':1}
    check(f,'undefined-effect');assert native.reason=='undefined entry effect'
    #Table address arithmetic can land in an adjacent valid table; it still
    #is an invalid group of the original requested table.
    f=fixture(rng,1);f.scene.frame.group=1;f.final.byte_6d1bd4=1
    f.final.byte_6dde50=1;f.final.workspace_6c7f80[0]=16
    f.mutate_at=3;f.mutations={'scene.frame.group':5}
    check(f,'material-adjacent-bound');assert native.reason=='material bound'
    for state in [0,5,127,128,255]:
        f=fixture(rng,1);f.final.byte_6d1bd4=state
        got,events=check(f,('unknown-state',state));assert not events and got.snapshot()==f.snapshot()
    f=fixture(rng,1);f.final.byte_6d1bd4=4;f.final.byte_6dde50=1;f.present=[0]*48
    f.final.word_6c7f74=0x7fffffff
    got,_=check(f,'cursor-wrap');assert got.final.word_6c7f74==-0x80000000
    for clip in [127,255]:
        f=fixture(rng,1);f.final.byte_6d1bd4=2;f.normal.clip=clip
        f.active[0]=0;f.timings[0].source=19;f.present=[0]*48
        got,_=check(f,('clip-byte-wrap',clip));assert got.normal.clip==(clip+1)%256
    missing_count=0
    for name,_ in Ops._fields_[1:]:
        if name in ['active','timing','target']:
            f=fixture(rng,1);f.final.byte_6d1bd4=3 if name=='target' else 2
            f.scene.frame.group=0;f.active[0]=3;f.normal.clip=2
        else:f=examples['status' if name=='present' else name].clone()
        ok,got,events,error=portable(lib,f,missing=name)
        assert not ok and error.endswith('missing '+name+' service'),(name,error)
        missing_count+=1
    rejected=0
    for name in ['present','active','timing','target']:
        f=fixture(rng,1);f.final.byte_6d1bd4=3 if name=='target' else (4 if name=='present' else 2)
        f.scene.frame.group=0;f.active[0]=3;f.normal.clip=2
        ok,got,events,error=portable(lib,f,query_fail=name);assert not ok and not events
        expected=f.clone()
        if name=='target':expected.scene.frame.camera_mode=2
        assert got.snapshot()==expected.snapshot();rejected+=1
    f=fixture(rng,1)
    for arg in range(4):
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        v=Views(*([pointer(f.volume)]*5));inputs=[C.byref(f.scene),C.byref(f.normal),C.byref(v),C.byref(out)];inputs[arg]=None
        assert not lib.bk_ending_state_gallery_tertiary_bindings(*inputs) and bytes(out)==before;rejected+=1
    for name,_ in Views._fields_:
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        v=Views(*([pointer(f.volume)]*5));setattr(v,name,None)
        assert not lib.bk_ending_state_gallery_tertiary_bindings(C.byref(f.scene),C.byref(f.normal),C.byref(v),C.byref(out)) and bytes(out)==before;rejected+=1
    assert operations=={'random','voice','load','play','status','expression','material','hidden','request'},operations

    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        material_table_entries=60,missing_services=missing_count,input_query_rejections=rejected,operations=sorted(operations),state_sha256=digest.hexdigest(),
        max_error=0,full_gallery=False,real_assets=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS gallery tertiary:',stats,digest.hexdigest(),flush=True)
if __name__=='__main__':main()
