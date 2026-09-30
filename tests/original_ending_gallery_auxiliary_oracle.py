"""Complete488674 versus native instructions and actual retained-state aliases.

Native dispatch/tables/descriptor stores and arithmetic execute; animation,
481E0A voice, sound, expression and camera services are observing fixtures.
This is not actual asset/PCM/GPU/production phase8 or hardware acceptance.
"""
import argparse
import ctypes as C
import faulthandler
import hashlib
import json
from pathlib import Path
import random
import re
import struct
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from model_binding import ROOT,library
from original_matrix_oracle import machine
from original_ending_gallery_control_oracle import Scene,REGIONS,I,U,F,B,P,Expression,Fov,Camera as CameraCall,Status,bits,floating
from original_ending_gallery_selected_oracle import (Saved,Camera,Presets,Clip,Target,Source,
    Stop,Request,Voice,Active,PRIMARY,TARGETS,NODES,pointer,fixture as selected_fixture)
from original_ending_gallery_normal_oracle import Native as Base,Play
from original_ending_gallery_effect_oracle import Timing
SOUNDS=[0x300a000+i*32 for i in range(48)]
class State(C.Structure):_fields_=[('fov',F),('view',B)]
class Extra(C.Structure):_fields_=[('expression_override',I),('eye_mode',I)]
class Bindings(C.Structure):
    _fields_=[(n,P) for n in ['frame','control','auxiliary','camera','presets','saved','substate','opening','cursor','counters','elapsed_bits','expression_override','face_mode','effect_volume']]
class Views(C.Structure):
    _fields_=[(n,P) for n in ['camera','presets','saved','expression_override','effect_volume']]
ReadTiming=C.CFUNCTYPE(I,P,I,C.POINTER(Timing),P)
class Ops(C.Structure):
    _fields_=[('context',P),('present',Status),('status',Status),('voice',Voice),('play',Play),
        ('stop',Stop),('expression',Expression),('fov',Fov),('camera',CameraCall),('target',Target),
        ('active',Active),('timing',ReadTiming),('source',Source),('request',Request),('restart',Request)]
class Fixture:
    owners=['scene','state','saved','camera','presets','extra','targets','volume','effect_volume','active','clips','restart_count','waits']
    @property
    def final(self):return self.scene.retained.final
    def clone(self):
        f=Fixture()
        for n in self.owners:
            v=getattr(self,n);setattr(f,n,type(v).from_buffer_copy(v))
        for n in ['present','playing','hr','loaded']:setattr(f,n,list(getattr(self,n)))
        for n in ['seconds','camera_result','mutate_at','mutations','clip_capacity','animate']:setattr(f,n,getattr(self,n))
        return f
    def snapshot(self):return tuple(bytes(getattr(self,n)) for n in self.owners)+(tuple(self.present),tuple(self.playing))
    def effect(self,event,index):
        kind=event[0]
        if kind=='fov':self.camera.fov=floating(event[1])
        elif kind=='camera':self.camera.yaw=F(self.camera.yaw+3.25).value
        elif kind=='expression':
            self.scene.auxiliary.expression_a,self.scene.auxiliary.expression_b=event[1:3]
            self.extra.eye_mode=event[3]
        elif kind=='voice':
            self.present[event[2]]=self.loaded[event[2]];self.playing[event[2]]=self.present[event[2]]
            if self.animate:self.waits[event[2]]=2
        elif kind=='play':
            self.playing[event[1]]=self.present[event[1]]
            if self.animate:self.waits[event[1]]=0 if event[2]&1 else 2
        elif kind=='stop':self.playing[event[1]]=0
        elif kind in ['request','restart']:
            self.active.value=event[1]
            if kind=='restart':self.restart_count.value+=1
            if self.animate and 0<=event[1]<128:self.clips[event[1]].source=self.clips[event[1]].start
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
        for a,v in [(0x300b024,0x300d100),(0x721b28,PRIMARY)]:self.word(a,v)
        for node,t in zip(NODES,TARGETS):self.word(0x721ef4+node*4,t)
        for sound in SOUNDS:self.word(sound,0x300b000)
        ins=list(Cs(CS_ARCH_X86,CS_MODE_32).disasm(bytes(self.u.mem_read(0x488674,0x48bc7f-0x488674)),0x488674))
        self.bounds={};self.audio={};self.stores=[0x4886ee,0x488706]
        for k,i in enumerate(ins):
            #Recover semantic slots at each original call, including NULL buffers.
            if i.mnemonic=='call' and i.op_str in ['0x4ad2bf','0x4ad34a']:
                preceding=ins[max(0,k-16):k]
                tables=[int(m.group(1),16) for q in preceding for m in [re.search(r'\+ (0x553(?:9[e-f][0-9a-f]|a0[0-4]))\]',q.op_str)] if m]
                if tables:self.audio[i.address+i.size]=tables[-1]
                else:
                    assert any('0x722d54' in q.op_str for q in preceding)
                    self.audio[i.address+i.size]=9
            if re.search(r'0x(?:5539[e-f][0-9a-f]|553a0[0-4]|553a[12][048c]|553a6c|553b7[048c]|709fd8)',i.op_str):
                self.bounds[i.address]='pass' if 0x48b3de<=i.address<0x48b45e else 'group'
            #Guard first native descriptor reads, not the preceding actor pointer load.
            for disp in [0x31c,0x320,0x328,0x3b8,0x3bc,0x3c4,0x4f4,0x4fc,0x590,0x598,0x62c,0x634,0x6d0,0x760,0x76c]:
                if re.search(r'\+ '+hex(disp)+r'\]',i.op_str) and i.mnemonic in ['fld','fcomp','mov']:
                    if i.address not in self.stores:self.bounds[i.address]=('clip',(disp-0x190)//156)
        self.bounds[0x48af71]='active'
        for a in set(self.bounds)|set(self.stores)|{0x4018c8,0x401f71,0x42cf0e,0x4dfb96,0x4e0ecb,0x481e0a,0x4ad2bf,0x4ad34a,0x300d100}:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def sync(self,read):
        f=self.f;views=[(a,pointer(f.scene)+off,n) for off,a,n,_ in REGIONS]
        for obj,fields in [(f.state,[('fov',0x5545fc,4),('view',0x6ddce1,1)]),
            (f.extra,[('expression_override',0x6c7f78,4)]),
            (f.saved,[('orbit',0x6dde14,16),('toggle',0x6c7f7c,1),('target',0x6d1bc0,12)]),
            (f.camera,[('yaw',0x71b364,16)]),(f.presets,[('active',0x71b37c,48)])]:
            views += [(a,pointer(obj,n),size) for n,a,size in fields]
        views += [(t+0xf0,pointer(f.targets)+i*12,12) for i,t in enumerate(TARGETS)]
        views += [(0xbe9a08,pointer(f.volume),4),(0xbe9a10,pointer(f.effect_volume),4),(PRIMARY+0x140,pointer(f.active),4)]
        for a,p,n in views:
            if read:C.memmove(p,bytes(self.u.mem_read(a,n)),n)
            else:self.u.mem_write(a,C.string_at(p,n))
        if read:
            raw=self.u.mem_read(PRIMARY+0x190,128*156);out=bytearray(128*16)
            for i in range(128):
                for off,k in [(0x54,0),(0x58,4),(0x60,8),(0x70,12)]:out[i*16+k:i*16+k+4]=raw[i*156+off:i*156+off+4]
            C.memmove(pointer(f.clips),bytes(out),len(out))
        else:
            raw=bytes(f.clips);out=bytearray(128*156)
            for i in range(128):
                for off,k in [(0x54,0),(0x58,4),(0x60,8),(0x70,12)]:out[i*156+off:i*156+off+4]=raw[i*16+k:i*16+k+4]
            self.u.mem_write(PRIMARY+0x190,bytes(out))
            for i,sound in enumerate(SOUNDS):self.word(0x722334+i*0x120,sound if f.present[i] else 0)
    def halt(self,why):self.reason=why;self.u.emu_stop()
    def hook(self,u,address,_size,_ctx):
        self.sync(True);f=self.f
        if address in self.stores:
            self.writes+=1
            if self.write_fail==self.writes:self.halt('injected store failure')
            return
        if address in self.bounds:
            bound=self.bounds[address]
            if isinstance(bound,tuple):valid=0<=bound[1]<f.clip_capacity
            elif bound=='active':valid=0<=f.active.value<f.clip_capacity
            else:valid=f.scene.frame.group<5 and (bound!='pass' or 0<=f.state.view<3)
            if not valid:self.halt(str(bound)+' bound')
            return
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.integer(sp);args=struct.unpack('<6I',u.mem_read(sp+4,24))
        result,cleanup,event=0,4,None
        if address in [0x4018c8,0x401f71]:
            assert args[0]==PRIMARY;event=('request' if address==0x4018c8 else 'restart',I(args[1]).value)
        elif address==0x42cf0e:event=('fov',args[0])
        elif address==0x4dfb96:event=('expression',*[I(v).value for v in args[:3]])
        elif address==0x4e0ecb:
            assert args[0]==0x71af38
            event=('camera',1,I(args[1]).value,tuple(args[2:5]),args[5]);result=f.camera_result
        elif address==0x481e0a:event=('voice',args[0],args[1],I(args[2]).value)
        elif address in [0x4ad2bf,0x4ad34a]:
            table=self.audio[ret]
            if table==9:slot=9
            else:
                width=5 if table>=0x553a00 else 3
                slot=2+struct.unpack('<b',u.mem_read(table+f.scene.frame.group*width,1))[0]
            assert args[0]==(SOUNDS[slot] if f.present[slot] else 0),(hex(ret),slot,args[0])
            event=('play',slot,I(args[1]).value,I(args[2]).value) if address==0x4ad2bf else ('stop',slot)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0]);assert f.present[slot]
            self.word(args[1],f.playing[slot]);result,cleanup=f.hr[slot],12;event=('status',slot)
        else:raise AssertionError(hex(address))
        self.trace.append((event,f.snapshot()))
        if self.fail_at==len(self.trace):self.halt('injected service failure');return
        f.effect(event,len(self.trace));self.sync(False)
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,original,fail_at=0,write_fail=0):
        self.f,self.trace=original.clone(),[];self.reason=None;self.fail_at=fail_at;self.write_fail=write_fail;self.writes=0
        self.sync(False);self.u.mem_write(0x733700,struct.pack('<f',self.f.seconds))
        self.u.mem_write(self.stack-2048,b'\xcd'*2048);self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(0x488674,self.stop,count=1000000)
        assert self.reason or self.u.reg_read(UC_X86_REG_EIP)==self.stop
        self.sync(True);return self.f,self.trace
def portable(lib,original,fail_at=0,missing=None,query_fail=None,write_fail=0):
    f,trace,errors,callbacks=original.clone(),[],[],[];writes=0
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
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot];return 1
    @decorate(Status)
    def status(_,slot,out,_e):out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1));return emit(('status',slot))
    @decorate(Voice)
    def voice(_,cue,slot,flags,_e):return emit(('voice',cue,slot,flags))
    @decorate(Play)
    def play(_,slot,flags,volume,_e):return emit(('play',slot,flags,volume))
    @decorate(Stop)
    def stop(_,slot,_e):return emit(('stop',slot))
    @decorate(Expression)
    def expression(_,a,b,m,_e):return emit(('expression',a,b,m))
    @decorate(Fov)
    def fov(_,v,_e):return emit(('fov',bits(v)))
    @decorate(CameraCall)
    def camera(_,kind,choice,offset,extra,out,_e):out[0]=f.camera_result;return emit(('camera',kind,choice,tuple(offset[:3]),extra))
    @decorate(Target)
    def target(_,node,out,_e):
        if query_fail=='target' or node not in NODES:return 0
        for i in range(3):out[i]=f.targets[NODES.index(node)][i]
        return 1
    @decorate(Active)
    def active(_,out,_e):
        if query_fail=='active':return 0
        out[0]=f.active.value;return 1
    @decorate(ReadTiming)
    def timing(_,index,out,_e):
        if query_fail=='timing' or not 0<=index<f.clip_capacity:return 0
        t=f.clips[index];out[0]=Timing(t.start,t.end,t.source);return 1
    @decorate(Source)
    def source(_,index,v,_e):
        nonlocal writes
        writes+=1
        if writes==write_fail or not 0<=index<f.clip_capacity:return 0
        f.clips[index].source=v;return 1
    @decorate(Request)
    def request(_,v,_e):return emit(('request',v))
    @decorate(Request)
    def restart(_,v,_e):return emit(('restart',v))
    b=Bindings();v=Views(pointer(f.camera),pointer(f.presets),pointer(f.saved),pointer(f.extra,'expression_override'),pointer(f.effect_volume))
    assert lib.bk_ending_state_gallery_auxiliary_bindings(C.byref(f.scene),C.byref(v),C.byref(b))
    assert b.substate==pointer(f.final,'byte_6d1c0d') and b.counters==pointer(f.final,'words_6dde24')
    o=Ops(None,present,status,voice,play,stop,expression,fov,camera,target,active,timing,source,request,restart)
    if missing:setattr(o,missing,dict(Ops._fields_)[missing]())
    e=C.create_string_buffer(256)
    ok=lib.bk_ending_gallery_auxiliary_step(C.byref(f.state),C.byref(b),f.seconds,C.byref(o),e)
    assert not errors,errors
    return bool(ok),f,trace,e.value.decode()
def fixture(rng,case):
    f=Fixture();old=selected_fixture(rng,case)
    for n in Fixture.owners:
        if n not in ['state','extra','waits']:setattr(f,n,getattr(old,n))
    f.scene.frame.state_721ee0=8;f.final.byte_6d1c0d=case%8
    f.final.byte_6dde58=case//8%6;f.state=State(rng.choice([1,.2,.21,.5,float('nan')]),rng.choice([0,1,2,3,127,255]))
    f.extra=Extra(19,2);f.active=I(rng.choice([0,2,3,4,5,6,7,8,9,10,127]))
    f.clips=(Clip*128)(*[Clip(30,300,rng.choice([30,32,60,65,95,105,120,125,133,140,152,154,160,162,195,220,225,257,264,299,300,301,float('nan')]),0) for _ in range(128)])
    for i in range(10):f.final.words_6dde24[i]=rng.choice([0,0,1,2,3,4,-1,0x7fffffff])
    f.final.word_6d1bcc=I(bits(rng.choice([0,19.99,20,21,float('nan')]))).value
    f.seconds=rng.choice([0,1/120,1/60,1/30,1,2]);f.camera_result=rng.choice([0,1,255,256,0xffffffff])
    f.present=[rng.randrange(2) for _ in range(48)];f.playing=[rng.randrange(4) for _ in range(48)]
    f.hr=[rng.choice([0,0,0x80004005]) for _ in range(48)];f.loaded=[rng.randrange(2) for _ in range(48)]
    f.clip_capacity=128;f.mutate_at=0;f.mutations={};f.waits=(I*48)();f.animate=False
    if case%7==0:
        f.mutate_at=case//7%10+1;f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,
            'effect_volume.value':-591,'scene.control.toggles.1':37,'scene.control.toggles.7':128,
            'camera.pitch':19.5,'saved.orbit.2':31.25,'saved.toggle':51,'saved.target.0':bits(22),
            'active.value':6,'scene.auxiliary.pending':3,'final.words_6dde24.0':0,
            'final.byte_6d1c0d':4,'final.byte_6dde58':2,'state.view':0,'targets.1.0':bits(19.5)}
    return f
def compare(want,got,expected,actual,label):
    assert [e for e,_ in expected]==[e for e,_ in actual],(label,'calls',[e for e,_ in expected],[e for e,_ in actual])
    names=Fixture.owners+['present','playing']
    def differences(a,b):return [(names[j],[(k,x[k],y[k]) for k in range(len(x)) if x[k]!=y[k]][:20]) for j,(x,y) in enumerate(zip(a,b)) if x!=y]
    for index,((event,a),(_,b)) in enumerate(zip(expected,actual)):
        assert a==b,(label,index,event,differences(a,b))
    assert want.snapshot()==got.snapshot(),(label,'final',differences(want.snapshot(),got.snapshot()))
def main():
    faulthandler.enable();p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path);p.add_argument('--cases',type=int,default=6000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-auxiliary.json')
    args=p.parse_args();assert args.cases>0
    exe=args.exe.read_bytes();native,lib=Native(exe),library()
    lib.bk_ending_gallery_auxiliary_initial.argtypes=[];lib.bk_ending_gallery_auxiliary_initial.restype=State
    lib.bk_ending_state_gallery_auxiliary_bindings.argtypes=[P,P,C.POINTER(Bindings)];lib.bk_ending_state_gallery_auxiliary_bindings.restype=I
    lib.bk_ending_gallery_auxiliary_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P];lib.bk_ending_gallery_auxiliary_step.restype=I
    initial=lib.bk_ending_gallery_auxiliary_initial()
    assert initial.fov==floating(native.integer(0x5545fc)) and initial.view==native.u.mem_read(0x6ddce1,1)[0]
    assert struct.unpack('<6I',native.u.mem_read(0x48bc7f,24))==(0x4886aa,0x48895c,0x488994,0x48924b,0x48a68f,0x48af42)
    rng=random.Random(0x488674);digest=hashlib.sha256();examples={};stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,live_mutations=0,failure_prefixes=0,write_failure_prefixes=0,bounded_rejections=0)
    def check(f,label,fail_at=0,write_fail=0):
        want,expected=native.run(f,fail_at,write_fail);ok,got,actual,error=portable(lib,f,fail_at,write_fail=write_fail)
        assert ok==(native.reason is None),(label,native.reason,error)
        compare(want,got,expected,actual,label)
        for event,_ in actual:examples.setdefault(event[0],f.clone())
        for blob in got.snapshot():
            if isinstance(blob,bytes):digest.update(blob)
        stats['calls']+=len(actual)
        if f.mutate_at and f.mutate_at<=len(actual) and fail_at!=f.mutate_at:stats['live_mutations']+=1
        if fail_at:stats['failure_prefixes']+=1
        elif write_fail:stats['write_failure_prefixes']+=1
        elif native.reason:stats['bounded_rejections']+=1
        return got,actual
    for case in range(args.cases):
        f=fixture(rng,case);got,events=check(f,('random',case));stats['random_frames']+=1
        if case<100 or case%79==0:
            for i in range(1,len(events)+1):check(f,('fail',case,i),i)
        if case and case%1000==0:print('gallery auxiliary: random',case,flush=True)
    def branch(group,state,opening,active=7,source=300):
        f=fixture(rng,1);f.scene.frame.group=group;f.final.byte_6d1c0d=state;f.final.byte_6dde58=opening
        f.active.value=active;f.state.view=0;f.state.fov=.2;f.camera_result=1
        f.present=[1]*48;f.playing=[0]*48;f.hr=[0]*48;f.loaded=[1]*48
        f.scene.control.toggles[7]=1;f.scene.auxiliary.pending=3
        for i in range(10):f.final.words_6dde24[i]=0
        for t in f.clips:t.source=source
        return f
    for g in range(5):
     for state in range(6):
      for opening in range(5):
       for source in [299.99997,300,300.00003,float('nan'),float('inf'),-float('inf')]:
        f=branch(g,state,opening,3 if state==2 else 5 if state==3 and opening==0 else 7,source)
        _,events=check(f,('boundary',g,state,opening,source));stats['boundary_frames']+=1
        if source==300:
            for i in range(1,len(events)+1):check(f,('boundary-failure',g,state,opening,i),i)
    #Every effect threshold and adjacent float values in the relevant pass.
    for g,state,opening,active,thresholds in [
        (2,2,0,3,[60,65]),(3,2,0,2,[32,56]),(4,2,0,2,[300]),
        (2,3,0,5,[105]),(3,3,0,5,[90,95]),(2,3,1,6,[120,125,140]),
        (3,3,1,6,[120,290]),(3,3,2,6,[120,290]),(0,3,3,7,[220,300]),
        (2,3,3,7,[160]),(3,3,3,7,[152,154,160,162]),
        (2,5,0,7,[160]),(3,5,0,7,[152,154,160,162]),(4,5,0,6,[133,140]),
        (4,4,0,6,[133,140,300]),(4,4,0,7,[160]),(4,4,0,8,[195,225]),
        (4,4,0,9,[36,257,264]),(4,4,1,9,[36,257,264])]:
     for threshold in thresholds:
      for source in [floating(bits(threshold)-1),threshold,floating(bits(threshold)+1),float('nan')]:
       for latch in [0,1,2,3,4]:
        f=branch(g,state,opening,active,source)
        for i in range(10):f.final.words_6dde24[i]=latch
        check(f,('threshold',g,state,opening,active,threshold,source,latch));stats['boundary_frames']+=1
    #Captured branch vs reread group/pending/volume and shared camera state.
    for g in range(5):
     for state,opening,active in [(0,1,2),(1,0,2),(2,0,3),(2,2,5),(3,0,5),(3,1,6),(3,2,6),(3,3,7),(4,0,6),(5,0,7)]:
        f=branch(g,state,opening,active);_,events=check(f,('mutation-base',g,state,opening))
        for i in range(1,len(events)+1):
            f.mutate_at=i;f.mutations={'scene.frame.group':(g+1)%5,'active.value':6,
                'scene.auxiliary.pending':2,'scene.control.toggles.7':0,'scene.control.toggles.1':37,
                'state.view':2,'state.fov':.2,'final.byte_6dde58':4,'final.byte_6d1c0d':3,
                'final.words_6dde24.0':1,'final.words_6dde24.1':0,
                'effect_volume.value':-801,'camera.pitch':31,'saved.toggle':79,'targets.1.0':bits(22)}
            check(f,('mutated',g,state,opening,i))
    #Full six-state dispatch with explicit synthetic actor/voice progression.
    #No asset, PCM, camera-math or measured framerate claims are made.
    for g in range(5):
     for rate in [30,60,120]:
        f=branch(g,0,0,0,0);f.animate=True;f.seconds=1/rate;f.state.fov=1
        f.final.word_6d1bcc=0;f.final.word_6c7f74=0;f.restart_count.value=0
        spans={0:(0,1),2:(0,40),3:(40,80),4:(80,100),5:(100,120),6:(120,150),7:(150,240),8:(240,250),9:(250,275)}
        for i,(a,z) in spans.items():f.clips[i]=Clip(a,z,a,0)
        seen=set();passes=set()
        for frame in range(rate*70):
            for i in range(48):
                if f.waits[i]>0:
                    f.waits[i]-=1
                    if not f.waits[i]:f.playing[i]=0
            state,opening=f.final.byte_6d1c0d,f.final.byte_6dde58
            seen.add(state)
            if state==5:passes.add(f.state.view)
            active=f.active.value;t=f.clips[active]
            if t.source>=t.end:
                next_clip=None
                if active==2 and state==2:next_clip=3
                elif active==5 and state==3 and g in [2,3]:next_clip=6
                elif active==7 and state==4:next_clip=8
                elif active==8 and state in [3,4]:next_clip=9
                if next_clip is not None:f.active.value=next_clip;t=f.clips[next_clip];t.source=t.start
            t.source=min(t.end,F(t.source+F(30/rate).value).value)
            f,_=check(f,('retained',g,rate,frame));stats['retained_frames']+=1
            if not f.scene.frame.state_721ee0:break
        assert not f.scene.frame.state_721ee0 and f.final.word_6c7f74==1,(g,rate,frame,f.final.byte_6d1c0d,f.final.byte_6dde58,f.active.value)
        assert {0,1,2,5}.issubset(seen) and (4 if g==4 else 3) in seen,(g,seen)
        assert passes=={0,1,2} and f.restart_count.value==2,(g,passes,f.restart_count.value)
        assert f.final.word_6d1bcc==0 and list(f.final.words_6dde24)==[0]*10
        print('gallery auxiliary: retained',g,rate,'complete',flush=True)
    #Store failures preserve the preceding source reset, never sampling.
    for n in [1,2]:
        f=branch(0,0,0);check(f,('store-failure',n),write_fail=n);assert native.reason
    #Array bounds stop the original at first unsafe access; the port rejects.
    for g in [5,128,255]:
     for state,opening in [(0,1),(2,2),(3,3),(5,0)]:
        f=branch(g,state,opening,9);f.state.view=0
        check(f,('group-bound',g,state,opening));assert native.reason
    for view in [3,127,128,254]:
        f=branch(0,5,0,5);f.state.view=view
        check(f,('view-bound',view));assert native.reason
    for active in [-1,128,0x7fffffff]:
        f=branch(0,5,0,active)
        check(f,('active-bound',active));assert native.reason
    for state,opening,active,capacity in [(0,0,2,2),(0,0,2,3),(2,0,3,3),(3,0,5,5),(3,1,6,6),(3,3,7,7),(4,0,8,8),(4,1,9,9)]:
        f=branch(2 if state==3 and opening==1 else 4,state,opening,active)
        f.clip_capacity=capacity
        if state==4:f.final.words_6dde24[0]=4
        check(f,('clip-bound',state,opening,capacity));assert native.reason
    f=branch(0,3,4,9);f.final.word_6d1bcc=I(bits(21)).value;f.final.word_6c7f74=0x7fffffff
    got,_=check(f,'cursor-wrap');assert got.final.word_6c7f74==-0x80000000
    missing_count=0
    for name,_ in Ops._fields_[1:]:
        if name in ['target','source','timing','active']:
            f=branch(0,0,1) if name=='target' else branch(0,0,0) if name=='source' else branch(0,5,0,5)
        else:f=examples['status' if name=='present' else name].clone()
        ok,_,_,error=portable(lib,f,missing=name)
        assert not ok and error.endswith('missing '+name+' service'),(name,error)
        missing_count+=1
    rejected=0
    for name in ['present','target','timing','active']:
        f=branch(0,0,1) if name=='target' else branch(0,0,2) if name=='present' else branch(0,5,0,5)
        ok,got,events,error=portable(lib,f,query_fail=name)
        assert not ok and not events and got.snapshot()==f.snapshot(),name
        rejected+=1
    for seconds in [-1,float('nan'),float('inf')]:
        f=branch(0,0,0);f.seconds=seconds
        ok,got,events,error=portable(lib,f);assert not ok and not events and got.snapshot()==f.snapshot();rejected+=1
    f=branch(0,0,0)
    for arg in range(3):
        v=Views(*([pointer(f.effect_volume)]*5));out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        inputs=[C.byref(f.scene),C.byref(v),C.byref(out)];inputs[arg]=None
        assert not lib.bk_ending_state_gallery_auxiliary_bindings(*inputs) and bytes(out)==before;rejected+=1
    for name,_ in Views._fields_:
        v=Views(*([pointer(f.effect_volume)]*5));setattr(v,name,None)
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        assert not lib.bk_ending_state_gallery_auxiliary_bindings(C.byref(f.scene),C.byref(v),C.byref(out)) and bytes(out)==before;rejected+=1
    assert set(examples)=={'voice','play','stop','expression','fov','camera','status','request','restart'},set(examples)
    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        missing_services=missing_count,input_query_rejections=rejected,operations=sorted(examples),
        state_sha256=digest.hexdigest(),max_error=0,full_gallery=False,real_assets=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS gallery auxiliary:',stats,digest.hexdigest(),flush=True)
if __name__=='__main__':main()
