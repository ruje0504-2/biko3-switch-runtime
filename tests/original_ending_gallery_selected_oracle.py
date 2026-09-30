"""Complete4855C9 against original instructions with actual BkEndingState aliases.

State dispatch, camera/record tables, descriptor writes and both clock positions
execute natively. Clock/RNG/actor requests,4946B4 voice, media, fade, expression
and camera leaves are deterministic fixtures. This is a CPU controller check,
not a real asset, PCM, production gallery or Switch validation.
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
from original_ending_gallery_control_oracle import (Scene,REGIONS,I,U,F,B,P,S,
    Expression,Fov,Camera as CameraCall,Status,Load,bits,floating,fixture as parent_fixture)
from original_ending_gallery_secondary_oracle import (Saved,Clock,Voice,Active,Request,
    Random,Camera)
from original_ending_gallery_normal_oracle import Native as Base,Play,PRIMARY

SOUNDS=[0x300a000+i*32 for i in range(6)]
TARGETS=[0x3008000+i*256 for i in range(3)]
NODES=[0,5,13]
class State(C.Structure):
    _fields_=[('fov',F),('base',I),('counter',I),('reset',I),('alternate',I),
              ('view',I),('countdown',B),('view_kind',S)]
class Presets(C.Structure):
    _fields_=[('active',(F*3)*4),('authored',(F*3)*4)]
class Clip(C.Structure):
    _fields_=[('start',F),('end',F),('source',F),('chain',I)]
class Extra(C.Structure):
    _fields_=[('expression_override',I),('fade_stage',B),('flash_wanted',B),
              ('action',B),('curtain_wanted',B)]
class Bindings(C.Structure):
    _fields_=[(n,P) for n in ['frame','control','auxiliary','camera','presets','saved',
        'substate','opening','cursor','workspace']]+[('workspace_capacity',U)]+[(n,P) for n in [
        'counters','camera_words','previous_clock','current_clock','elapsed','open',
        'expression_override','face_mode','fade_stage','flash_wanted','action','curtain_wanted',
        'speech_name','voice_volume','effect_volume']]
class Views(C.Structure):
    _fields_=[(n,P) for n in ['camera','presets','saved','expression_override','fade_stage',
        'flash_wanted','action','curtain_wanted','voice_volume','effect_volume']]
Target=C.CFUNCTYPE(I,P,U,C.POINTER(U),P)
ReadClip=C.CFUNCTYPE(I,P,I,C.POINTER(Clip),P)
Source=C.CFUNCTYPE(I,P,I,F,P)
Chain=C.CFUNCTYPE(I,P,I,I,P)
Stop=C.CFUNCTYPE(I,P,U,P)
Fade=C.CFUNCTYPE(I,P,B,P)
class Ops(C.Structure):
    _fields_=[('context',P),('clock',Clock),('random',Random),('present',Status),
        ('status',Status),('voice',Voice),('load',Load),('play',Play),('stop',Stop),
        ('expression',Expression),('fov',Fov),('camera',CameraCall),('target',Target),
        ('active',Active),('clip',ReadClip),('source',Source),('chain',Chain),
        ('request',Request),('restart',Request),('fade',Fade)]
STATE_ADDR=[('fov',0x5545f4,4),('base',0x6d1bd0,4),('counter',0x6dde5c,4),
    ('reset',0x6dde60,4),('alternate',0x6dde64,4),('view',0x6d1be4,4),
    ('countdown',0x5545f8,1),('view_kind',0x6d1be2,1)]
EXTRA_ADDR=[('expression_override',0x6c7f78,4),('fade_stage',0x738b7c,1),
    ('flash_wanted',0x738baf,1),('action',0xbeeb7e,1),('curtain_wanted',0xbeeb7f,1)]

class Fixture:
    owners=['scene','state','saved','camera','presets','extra','targets','volume','effect_volume',
        'active','clips','randoms','random_index','clocks','clock_index','restart_count']
    @property
    def final(self):return self.scene.retained.final
    def clone(self):
        f=Fixture()
        for name in self.owners:
            v=getattr(self,name);setattr(f,name,type(v).from_buffer_copy(v))
        for name in ['present','playing','hr','loaded']:setattr(f,name,list(getattr(self,name)))
        for name in ['seconds','camera_result','mutate_at','mutations','clip_capacity','capacity']:
            setattr(f,name,getattr(self,name))
        return f
    def snapshot(self):
        return tuple(bytes(getattr(self,n)) for n in self.owners)+(tuple(self.present),tuple(self.playing))
    def effect(self,event,index):
        kind=event[0]
        if kind=='clock':self.clock_index.value+=1
        elif kind=='random':self.random_index.value+=1
        elif kind=='fov':self.camera.fov=floating(event[1])
        elif kind=='camera':self.camera.yaw=F(self.camera.yaw+3.25).value
        elif kind=='expression':
            self.scene.auxiliary.expression_a,self.scene.auxiliary.expression_b=event[1:3]
            self.scene.face_mode=event[3]
        elif kind=='voice':
            #4946B4 is a fixture boundary; its asset mapping is covered by
            #the existing auxiliary/audio oracle, not asserted here.
            self.present[event[2]]=self.loaded[event[2]];self.playing[event[2]]=self.present[event[2]]
        elif kind=='load':self.present[event[1]],self.playing[event[1]]=self.loaded[event[1]],0
        elif kind=='play':self.playing[event[1]]=self.present[event[1]]
        elif kind=='stop':self.playing[event[1]]=0
        elif kind=='fade':self.extra.fade_stage=4 if self.extra.fade_stage==2 else self.extra.fade_stage
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
    word=Base.word
    integer=Base.integer
    string=Base.string
    def __init__(self,exe):
        self.u=machine(exe);self.stack,self.stop=0x2008000,0x300f000
        for a,v in [(0x53f2f0,0x300d010),(0x53f214,0x300d020),(0x53f358,0x300d030),
            (0x300b024,0x300d100),(0x300b048,0x300d110),(0x721b28,PRIMARY)]:self.word(a,v)
        for node,t in zip(NODES,TARGETS):self.word(0x721ef4+node*4,t)
        for sound in SOUNDS:self.word(sound,0x300b000)
        #Guard invalid domains immediately before the original first access.
        self.write_addresses = [
            0x485732,0x48574a,0x485762,0x48577a,0x485792,0x4857aa,
            0x485bad,0x485bbd,0x485bcd,0x485bdc,0x485bec,0x485bfc,
            0x485c0b,0x485c1b,0x485c2b,0x485c3a,0x485c4a,
            0x485d8b,0x485da3,0x485dbb,0x485dd3,0x485eb6,0x485ecd,
            0x485f65,0x485f7c,0x4865db,0x486e14,0x486e24,0x486e34,
            0x486e43,0x486e53,0x486e63,0x486e72,0x486e82,0x486e92,0x486ea1]
        self.bounds={0x4857b5:'preset',0x48581d:'target',0x4858db:'group',
            0x485d19:'next_record',0x4860fa:'record',0x48614b:'row',
            0x4868c3:'row',0x486b0c:'row',0x486d39:'row',
            0x4861a3:'camera_row',0x48649a:'camera_row',0x487032:'camera_row',
            0x4874b5:'camera_row',0x48729c:'repeat',0x48737f:'finish',
            0x486993:'finish0',0x486bdc:'finish0',
            0x48655b:'active_clip',0x486cb7:'active_clip',0x486f1a:'active_clip'}
        for a in [0x534a34,0x4018c8,0x401f71,0x42cf0e,0x4dfb96,0x4e0ecb,
                  0x4946b4,0x4e0956,0x4ad2bf,0x50e633,0x300d010,0x300d020,
                  0x300d030,0x300d100,0x300d110,*self.bounds,*self.write_addresses]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def sync(self,read):
        f=self.f
        views=[(a,C.addressof(f.scene)+off,n) for off,a,n,_ in REGIONS
               if a not in [0xbeeb7e,0xbeeb7f]]
        for obj,fields in [(f.state,STATE_ADDR),(f.extra,EXTRA_ADDR),
            (f.saved,[('orbit',0x6dde14,16),('toggle',0x6c7f7c,1),('target',0x6d1bc0,12)]),
            (f.camera,[('yaw',0x71b364,16)]),(f.presets,[('active',0x71b37c,48)])]:
            views.extend((a,C.addressof(obj)+getattr(type(obj),name).offset,n) for name,a,n in fields)
        views += [(t+0xf0,C.addressof(f.targets)+i*12,12) for i,t in enumerate(TARGETS)]
        views += [(0xbe9a08,C.addressof(f.volume),4),(0xbe9a10,C.addressof(f.effect_volume),4),
                  (PRIMARY+0x140,C.addressof(f.active),4)]
        for a,p,n in views:
            if read:C.memmove(p,bytes(self.u.mem_read(a,n)),n)
            else:self.u.mem_write(a,C.string_at(p,n))
        if read:
            raw=self.u.mem_read(PRIMARY+0x190,128*156)
            out=bytearray(128*16)
            for i in range(128):
                for off,k in [(0x54,0),(0x58,4),(0x60,8),(0x70,12)]:
                    out[i*16+k:i*16+k+4]=raw[i*156+off:i*156+off+4]
            C.memmove(C.addressof(f.clips),bytes(out),len(out))
        else:
            raw=bytes(f.clips);out=bytearray(128*156)
            for i in range(128):
                for off,k in [(0x54,0),(0x58,4),(0x60,8),(0x70,12)]:
                    out[i*156+off:i*156+off+4]=raw[i*16+k:i*16+k+4]
            self.u.mem_write(PRIMARY+0x190,bytes(out))
            for i,sound in enumerate(SOUNDS):self.word(0x722334+i*0x120,sound if f.present[i] else 0)
    def halt(self,why):self.reason=why;self.u.emu_stop()
    def hook(self,u,address,_size,_context):
        self.sync(True);f=self.f
        if address in self.write_addresses:
            self.writes+=1
            if self.write_fail==self.writes:self.halt('injected descriptor failure')
            return
        if address in self.bounds:
            kind=self.bounds[address];g=f.scene.frame.group;v=f.scene.auxiliary.variant;q=f.scene.auxiliary.selection
            valid={'preset':0<=f.scene.frame.camera_clip<3,'target':0<=f.scene.control.target_choice<3,
                'group':g<5,'row':g<5 and 0<=v<2 and 0<=q<3,
                'camera_row':g<5 and 0<=q<3,
                'record':0<=f.final.word_6c7f74<f.capacity,
                'next_record':U(f.final.word_6c7f74+1).value<f.capacity,
                'repeat':g<5 and 0<=v<2 and 0<=q<3 and 0<=f.state.view<2,
                'finish':g<5 and 0<=v<2 and 0<=q<3 and 0<=f.state.view<3,
                'finish0':g<5 and 0<=v<2 and 0<=q<3,
                'active_clip':0<=f.active.value<f.clip_capacity}[kind]
            if not valid:self.halt(kind+' bound')
            return
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.integer(sp);args=struct.unpack('<6I',u.mem_read(sp+4,24))
        result,cleanup,event=0,4,None
        if address==0x300d010:
            assert self.string(args[1])==b'PH%d0103.wav'
            name=b'PH%d0103.wav'%I(args[2]).value;u.mem_write(args[0],name+b'\0');result=len(name)
        elif address==0x300d020:
            assert args[0]==0x722224;u.mem_write(args[0],self.string(args[1])+b'\0')
            self.sync(True);result,cleanup=args[0],12
        elif address==0x534a34:result=f.randoms[f.random_index.value%16];event=('random',result)
        elif address==0x300d030:result=f.clocks[f.clock_index.value%8];event=('clock',result)
        elif address in [0x4018c8,0x401f71]:
            assert args[0]==PRIMARY;event=('request' if address==0x4018c8 else 'restart',I(args[1]).value)
        elif address==0x42cf0e:event=('fov',args[0])
        elif address==0x4dfb96:event=('expression',*[I(x).value for x in args[:3]])
        elif address==0x4e0ecb:
            assert args[0]==0x71af38
            event=('camera',1,I(args[1]).value,tuple(args[2:5]),args[5]);result=f.camera_result
        elif address==0x4946b4:event=('voice',args[0],args[1],I(args[2]).value)
        elif address==0x4e0956:event=('load',args[1],self.string(args[0]))
        elif address==0x4ad2bf:
            slot=5 if ret==0x485c68 else 0
            assert args[0]==(SOUNDS[slot] if f.present[slot] else 0)
            event=('play',slot,I(args[1]).value,I(args[2]).value)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0]);assert f.present[slot]
            self.word(args[1],f.playing[slot]);result,cleanup=f.hr[slot],12;event=('status',slot)
        elif address==0x300d110:
            slot=SOUNDS.index(args[0]);event=('stop',slot);cleanup=8
        elif address==0x50e633:
            assert args[0]==0x738a48;event=('fade',args[1])
        else:raise AssertionError(hex(address))
        if event:
            self.trace.append((event,f.snapshot()))
            if self.fail_at==len(self.trace):self.halt('injected failure');return
            f.effect(event,len(self.trace))
        self.sync(False)
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,original,fail_at=0,write_fail=0):
        self.f,self.trace=original.clone(),[];self.fail_at,self.reason=fail_at,None;self.write_fail,self.writes=write_fail,0
        self.sync(False);self.u.mem_write(0x733700,struct.pack('<f',self.f.seconds))
        self.u.mem_write(self.stack-2048,b'\xcd'*2048);self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(0x4855c9,self.stop,count=1000000)
        assert self.reason or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True);return self.f,self.trace

def pointer(obj,name=None):return C.addressof(obj)+(getattr(type(obj),name).offset if name else 0)
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
    @decorate(Clock)
    def clock(_,out,_e):out[0]=f.clocks[f.clock_index.value%8];return emit(('clock',out[0]))
    @decorate(Random)
    def rand(_,out,_e):out[0]=f.randoms[f.random_index.value%16];return emit(('random',out[0]))
    @decorate(Voice)
    def voice(_,cue,slot,flags,_e):return emit(('voice',cue,slot,flags))
    @decorate(Load)
    def load(_,slot,name,_e):return emit(('load',slot,name))
    @decorate(Play)
    def play(_,slot,flags,volume,_e):return emit(('play',slot,flags,volume))
    @decorate(Stop)
    def stop(_,slot,_e):return emit(('stop',slot))
    @decorate(Expression)
    def expression(_,a,b,m,_e):return emit(('expression',a,b,m))
    @decorate(Fov)
    def fov(_,v,_e):return emit(('fov',bits(v)))
    @decorate(CameraCall)
    def camera(_,kind,choice,offset,extra,out,_e):
        out[0]=f.camera_result;return emit(('camera',kind,choice,tuple(offset[:3]),extra))
    @decorate(Target)
    def target(_,node,out,_e):
        if query_fail=='target' or node not in NODES:return 0
        for i in range(3):out[i]=f.targets[NODES.index(node)][i]
        return 1
    @decorate(Active)
    def active(_,out,_e):
        if query_fail=='active':return 0
        out[0]=f.active.value;return 1
    @decorate(ReadClip)
    def clip(_,index,out,_e):
        if query_fail=='clip' or not 0<=index<f.clip_capacity:return 0
        out[0]=f.clips[index];return 1
    @decorate(Source)
    def source(_,index,value,_e):
        nonlocal writes
        writes+=1
        if write_fail==writes or not 0<=index<f.clip_capacity:return 0
        f.clips[index].source=value;return 1
    @decorate(Chain)
    def chain(_,index,value,_e):
        nonlocal writes
        writes+=1
        if write_fail==writes or not 0<=index<f.clip_capacity:return 0
        f.clips[index].chain=value;return 1
    @decorate(Request)
    def request(_,v,_e):return emit(('request',v))
    @decorate(Request)
    def restart(_,v,_e):return emit(('restart',v))
    @decorate(Fade)
    def fade(_,v,_e):return emit(('fade',v))
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot];return 1
    @decorate(Status)
    def status(_,slot,out,_e):
        out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1));return emit(('status',slot))
    b=Bindings();v=Views(pointer(f.camera),pointer(f.presets),pointer(f.saved),
        *[pointer(f.extra,n) for n in ['expression_override','fade_stage','flash_wanted','action','curtain_wanted']],
        pointer(f.volume),pointer(f.effect_volume))
    assert lib.bk_ending_state_gallery_selected_bindings(C.byref(f.scene),C.byref(v),C.byref(b))
    assert b.workspace==pointer(f.final,'workspace_6c7f80') and b.substate==pointer(f.scene,'final_state')
    b.workspace_capacity=f.capacity
    o=Ops(None,clock,rand,present,status,voice,load,play,stop,expression,fov,camera,
          target,active,clip,source,chain,request,restart,fade)
    if missing:setattr(o,missing,dict(Ops._fields_)[missing]())
    e=C.create_string_buffer(256)
    ok=lib.bk_ending_gallery_selected_step(C.byref(f.state),C.byref(b),f.seconds,C.byref(o),e)
    assert not errors,errors
    return bool(ok),f,trace,e.value.decode()

def fixture(rng,case):
    f=Fixture();old=parent_fixture(rng,case)
    f.scene=old.scene;f.scene.frame.state_721ee0=6;f.scene.final_state=case%13
    f.scene.auxiliary.variant=case//13%2;f.scene.auxiliary.selection=case//26%3
    f.scene.frame.camera_clip=rng.randrange(3);f.scene.control.target_choice=rng.randrange(3)
    f.final.byte_6dde58=rng.choice([0,1,2,3,255]);f.final.word_6c7f74=0
    for i in range(3):f.final.workspace_6c7f80[i]=rng.choice([12,13,14,21,-1,0,256])
    f.final.words_6dde24[0]=rng.randrange(2)
    f.final.word_6dde54=I(rng.choice([0,3000,10000,15000,0xffffffff,0x7fffffff])).value
    f.final.word_6d1bd8=I(rng.choice([0,5000,0xfffffffe,0x7fffffff])).value
    f.state=State(rng.choice([.2,.21,.5,1,float('nan')]),0,rng.choice([0,1,2,-1]),19,
                  rng.randrange(2),0,5,case//3%2)
    f.state.view=rng.choice([-1,0,1] if not f.state.view_kind else [0,1,2])
    f.extra=Extra(2,2,0,0,0)
    f.saved=Saved((F*4)(-7,11,52,3),rng.randrange(256),(U*3)(1,2,3))
    f.camera=Camera.from_buffer_copy(rng.randbytes(C.sizeof(Camera)))
    f.camera.yaw=19.25;f.camera.pitch=-13.75;f.camera.radius=51.5;f.camera.height=3.25
    f.presets=Presets()
    for i in range(4):
        for j in range(3):f.presets.active[i][j]=i*20+j;f.presets.authored[i][j]=i*30+j
    f.targets=((U*3)*3)(*[(U*3)(bits(1+i),bits(3+i),bits(5+i)) for i in range(3)])
    f.volume=I(-351);f.effect_volume=I(-173)
    f.active=I(rng.choice([2,3,4,5,7,11,15,17,18,20,21,22,23,24,127]))
    f.clips=(Clip*128)(*[Clip(10,19,rng.choice([10,18,19,20,float('nan')]),rng.choice([0,0,1,-1])) for _ in range(128)])
    f.randoms=(I*16)(*[rng.choice([0,25,26,999,-1,-6,-10,-11,0x7fffffff,-0x80000000,rng.randrange(32768)]) for _ in range(16)])
    f.random_index=I(0);f.clocks=(U*8)(*[rng.randrange(0x100000000) for _ in range(8)])
    f.clock_index=I(0);f.restart_count=I(0)
    f.seconds=rng.choice([0,1/120,1/60,1/30,1,2]);f.camera_result=rng.choice([0,1,255,256,0xffffffff])
    f.present=[rng.randrange(2) for _ in range(6)];f.playing=[rng.randrange(4) for _ in range(6)]
    f.hr=[rng.choice([0,0,0x80004005]) for _ in range(6)];f.loaded=[rng.randrange(2) for _ in range(6)]
    f.clip_capacity=128;f.capacity=10000;f.mutate_at=0;f.mutations={}
    if case%7==0:
        f.mutate_at=case//7%10+1
        f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,'volume.value':-437,
            'effect_volume.value':-591,'state.fov':.41,'scene.control.toggles.7':128,
            'scene.control.toggles.1':37,'camera.pitch':19.5,'saved.orbit.2':31.25,
            'saved.toggle':51,'active.value':22,'scene.auxiliary.pending':3,
            'scene.final_state':6,'final.words_6dde24.0':0,'state.alternate':0,
            'final.word_6d1bd8':73,'targets.0.0':bits(19.5)}
    return f

def compare(want,got,expected,actual,label):
    assert [e for e,_ in expected]==[e for e,_ in actual],(label,'calls',[e for e,_ in expected],[e for e,_ in actual])
    names=Fixture.owners+['present','playing']
    for index,((event,a),(_,b)) in enumerate(zip(expected,actual)):
        if a!=b:
            diffs=[(names[j],[(k,x[k],y[k]) for k in range(len(x)) if x[k]!=y[k]][:30]) for j,(x,y) in enumerate(zip(a,b)) if x!=y]
            raise AssertionError((label,index,event,'snapshot',diffs))
    assert want.snapshot()==got.snapshot(),(label,'final',[(n,[(k,a,b) for k,(a,b) in enumerate(zip(bytes(getattr(want,n)),bytes(getattr(got,n)))) if a!=b][:25]) for n in Fixture.owners if bytes(getattr(want,n))!=bytes(getattr(got,n))])

def main():
    faulthandler.enable();p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path);p.add_argument('--cases',type=int,default=6000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-selected.json')
    args=p.parse_args();exe=args.exe.read_bytes();native,lib=Native(exe),library()
    lib.bk_ending_gallery_selected_initial.argtypes=[];lib.bk_ending_gallery_selected_initial.restype=State
    lib.bk_ending_state_gallery_selected_bindings.argtypes=[P,P,C.POINTER(Bindings)]
    lib.bk_ending_state_gallery_selected_bindings.restype=I
    lib.bk_ending_gallery_selected_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P]
    lib.bk_ending_gallery_selected_step.restype=I
    s=lib.bk_ending_gallery_selected_initial()
    for name,a,n in STATE_ADDR:assert C.string_at(pointer(s,name),n)==native.u.mem_read(a,n),(name,a)
    assert struct.unpack('<12I',native.u.mem_read(0x48755c,48))==(0x48566e,0x4859d7,0x485a18,0x48754c,0x485aaf,0x485b24,0x485c83,0x485f91,0x48600d,0x486408,0x48754c,0x486f06)
    rng=random.Random(0x4855c9);digest=hashlib.sha256();operations=set();examples={}
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,live_mutations=0,
               failure_prefixes=0,write_failure_prefixes=0,guarded_bounds=0,early_returns=0)
    def check(f,label,fail_at=0,write_fail=0):
        want,expected=native.run(f,fail_at,write_fail)
        ok,got,actual,error=portable(lib,f,fail_at,write_fail=write_fail)
        assert ok==(native.reason is None),(label,native.reason,error)
        compare(want,got,expected,actual,label)
        for event,_ in actual:operations.add(event[0]);examples.setdefault(event[0],f.clone())
        for blob in got.snapshot():
            if isinstance(blob,bytes):digest.update(blob)
        stats['calls']+=len(actual)
        if f.mutate_at and f.mutate_at<=len(actual) and fail_at!=f.mutate_at:stats['live_mutations']+=1
        if write_fail:stats['write_failure_prefixes']+=1
        elif fail_at:stats['failure_prefixes']+=1
        elif native.reason:stats['guarded_bounds']+=1
        elif sum(event[0]=='clock' for event,_ in actual)==1:stats['early_returns']+=1
        return got,actual
    for case in range(args.cases):
        f=fixture(rng,case);got,events=check(f,('random',case));stats['random_frames']+=1
        if case<100 or (case%71==0):
            for i in range(1,len(events)+1):check(f,('failure',case,i),i)
        if case and case%1000==0:print('gallery selected: random',case,flush=True)
    #Every camera row/branch at equal, past and unordered clip boundaries,
    #including both saved-view paths and all three opening substates.
    for group in range(5):
      for variant in range(2):
       for selection in range(3):
        for state,opening in [(0,0),(0,1),(5,0),(6,0),(7,0),(8,0),(9,0),(9,1),(9,2),(11,0)]:
         for source in [10,19,20,float('nan')]:
          f=fixture(rng,1);f.mutate_at=0;f.scene.frame.group=group
          f.scene.auxiliary.variant=variant;f.scene.auxiliary.selection=selection
          f.scene.final_state=state;f.final.byte_6dde58=opening;f.scene.frame.camera_mode=4
          f.final.word_6dde54=16000;f.final.word_6d1bd8=0;f.state.fov=.2;f.camera_result=1
          f.state.counter=0;f.state.view_kind=variant;f.state.view=0
          f.present=[1]*6;f.playing=[0]*6;f.hr=[0]*6;f.loaded=[1]*6
          f.active.value=4 if state in [5,7] else (21 if group in [0,4] and variant==0 and selection<2 else 22)
          for t in f.clips:t.source=source;t.chain=0
          got,events=check(f,('boundary',group,variant,selection,state,opening,source));stats['boundary_frames']+=1
          if state in [8,9,11] and source==19:
           for i in range(1,len(events)+1):check(f,('boundary-fail',group,variant,selection,state,opening,i),i)
    #Integer delta, strict threshold, zero previous time, x87 FOV <1 guard.
    for state,threshold in [(1,3000),(6,10000),(8,15000)]:
      for elapsed in [threshold-1,threshold,threshold+1,0x7fffffff,0xffffffff]:
       for previous,now in [(0,20000),(0xfffffffe,1),(100,99),(100,100)]:
        f=fixture(rng,1);f.scene.final_state=state;f.final.word_6dde54=I(elapsed).value
        f.final.word_6d1bd8=I(previous).value;f.clocks=(U*8)(*[now+i for i in range(8)])
        check(f,('clock-boundary',state,elapsed,previous,now));stats['boundary_frames']+=1
    for dt in [0,1/120,1/60,1/30,.99999994,1,1.00000012,2]:
      for fov_value in [.19999999,.2,.20000002,1,float('nan')]:
        f=fixture(rng,1);f.scene.final_state=0;f.final.byte_6dde58=1;f.seconds=dt;f.state.fov=fov_value
        check(f,('fov-boundary',dt,fov_value));stats['boundary_frames']+=1
    #Descriptor chain toggles are separate from ended flags. Every slot is
    #compared, including deliberate slot9 preservation on exit.
    for state in [5,9]:
        f=fixture(rng,1);f.scene.final_state=state;f.final.byte_6dde58=2;f.active.value=4
        f.playing=[0]*6;f.present=[0]*6;f.scene.frame.camera_mode=0;f.state.counter=2
        got,_=check(f,('chains',state));stats['boundary_frames']+=1
        changed=[i for i in range(128) if bytes(f.clips[i])!=bytes(got.clips[i])]
        assert set(changed)<=set([5,6,7,9,10,11,13,14,15,17,18])
    #A failed write must retain all preceding writes and must not invoke a
    #later leaf or tail clock. Stop native execution before each actual store.
    for state,opening,alternate,counter,next_record in [(0,0,0,0,21),(5,0,0,0,21),
            (6,0,0,1,21),(6,0,0,0,21),(6,0,1,0,21),(9,0,0,2,21),(9,2,0,2,21)]:
        f=fixture(rng,1);f.scene.final_state=state;f.final.byte_6dde58=opening
        f.state.alternate=alternate;f.state.counter=counter;f.final.word_6dde54=16000
        f.final.word_6d1bd8=0;f.final.workspace_6c7f80[1]=next_record;f.active.value=4
        f.present=[0]*6;f.playing=[0]*6;f.scene.frame.camera_mode=0
        for t in f.clips:t.source=19
        check(f,('write-base',state,opening,alternate,counter));count=native.writes
        assert count>0
        for index in range(1,count+1):check(f,('write-fail',state,opening,alternate,counter,index),write_fail=index)
    #Complete retained-state CPU sequences. Actor/media progression is an
    #explicit fixture, with six-second platform stalls between controller
    #calls; FOV uses30/60/120Hz deltas. Not an animation/PCM integration test.
    terminals=struct.unpack('<30i',native.u.mem_read(0x54f9e4,120))
    for group in range(5):
      for variant in range(2):
       for selection in range(3):
        for rate in [30,60,120]:
          f=fixture(rng,1);f.scene.frame.group=group;f.scene.auxiliary.variant=variant
          f.scene.auxiliary.selection=selection;f.scene.final_state=0
          f.final.byte_6dde58=0;f.final.word_6d1bd8=0;f.final.word_6dde54=0
          f.final.word_6c7f74=0;f.final.workspace_6c7f80[0]=12+selection
          f.final.workspace_6c7f80[1]=21;f.final.words_6dde24[0]=0
          f.state=lib.bk_ending_gallery_selected_initial();f.extra=Extra(0,2,0,0,0)
          f.camera_result=1;f.seconds=F(1/rate).value;f.active.value=0
          f.present=[1]*6;f.hr=[0]*6;f.loaded=[1]*6
          terminal=terminals[group*6+variant*3+selection]
          for t in f.clips:t.chain=0
          seen=set();seen_open=set()
          for step in range(rate*6+100):
            state=f.scene.final_state;seen.add(state)
            if state==9:seen_open.add(f.final.byte_6dde58)
            #Model leaf outcomes explicitly, never replace controller writes.
            if state in [4,5] and f.active.value==3:f.active.value=4
            if state==5 and f.active.value==5:f.active.value=7
            if state==6:f.active.value=11 if f.state.alternate else 7
            if state==8:f.active.value=17
            for t in f.clips:t.source=t.end
            f.clips[17].source=f.clips[17].start
            f.clips[terminal].chain=1
            f.playing=[0]*6;now=100+step*6000;f.clocks=(U*8)(*[now+i for i in range(8)])
            f.clock_index.value=0
            f,_=check(f,('retained',group,variant,selection,rate,step));stats['retained_frames']+=1
            if f.extra.action==0x31:break
          assert f.extra.action==0x31 and f.extra.curtain_wanted==1,(group,variant,selection,rate,step,f.scene.final_state,f.final.byte_6dde58,f.active.value)
          assert {0,1,2,4,5,6,8,9}<=seen and 11 in seen,(group,variant,selection,seen)
          assert {0,1,2}<=seen_open,seen_open
          assert (f.final.word_6d1bd8,f.final.word_6d1bdc,f.final.word_6dde54)==(0,0,0)
      print('gallery selected: retained group',group,'complete',flush=True)
    #Native no-op states and cursor wrapping still retain their clock effects.
    for state in [3,10,12,127,-1,-128]:
        f=fixture(rng,1);f.scene.final_state=state
        got,events=check(f,('unknown',state));assert [e[0] for e,_ in events]==['clock','clock']
    f=fixture(rng,1);f.scene.final_state=7;f.active.value=4;f.present=[0]*6
    f.final.word_6c7f74=0x7fffffff
    got,_=check(f,'cursor-wrap');assert got.final.word_6c7f74==-0x80000000
    for state,opening,field,values in [(0,1,'preset',[-1,3]),(0,1,'target',[-1,3]),
        (0,1,'group',[5,255]),(9,1,'variant',[-1,2]),(9,1,'selection',[-1,3]),
        (11,0,'active',[-1,128]),(11,0,'view',[2,3,0x7fffffff]),
        (6,0,'record',[-2,9999]),(8,0,'record',[-1,10000])]:
      for value in values:
        f=fixture(rng,1);f.scene.final_state=state;f.final.byte_6dde58=opening
        f.scene.frame.camera_mode=4;f.state.counter=1 if state==6 else 2
        f.state.alternate=0;f.state.view_kind=0;f.state.view=0
        f.final.word_6dde54=16000;f.final.word_6d1bd8=0;f.present=[0]*6
        f.active.value=23
        for t in f.clips:t.source=19;t.chain=0
        f.clips[17].source=10
        if field=='preset':f.scene.frame.camera_clip=value
        elif field=='target':f.scene.control.target_choice=value
        elif field=='group':f.scene.frame.group=value
        elif field=='variant':f.scene.auxiliary.variant=value
        elif field=='selection':f.scene.auxiliary.selection=value
        elif field=='active':f.active.value=value
        elif field=='view':f.state.view=value
        elif field=='record':f.final.word_6c7f74=value
        check(f,('bounds',state,field,value));assert native.reason
    missing_count=0
    for member,_ in Ops._fields_[1:]:
        event=member
        if member=='present':event='status'
        if member in ['target','clip','active','source','chain']:
            f=fixture(rng,1);f.present=[0]*6
            f.scene.final_state=0 if member in ['source','target'] else (5 if member=='chain' else 11)
            f.final.byte_6dde58=int(member=='target');f.active.value=4
        else:
            assert event in examples,event
            f=examples[event].clone()
        ok,got,trace,error=portable(lib,f,missing=member)
        assert not ok and error.endswith('missing '+member+' service'),(member,error)
        missing_count+=1
    query_rejections=0
    for query in ['target','active','clip','present']:
        f=fixture(rng,1);f.scene.final_state=0 if query=='target' else 11
        f.final.byte_6dde58=1;f.clips[f.active.value].source=19
        ok,got,trace,error=portable(lib,f,query_fail=query)
        assert not ok;query_rejections+=1
    for dt in [-1,float('nan'),float('inf')]:
        f=fixture(rng,1);f.seconds=dt
        ok,got,trace,error=portable(lib,f)
        assert not ok and not trace and f.snapshot()==got.snapshot();query_rejections+=1
    #Complete parent binding inputs reject atomically; valid construction is
    #checked on every portable call against actual retained addresses.
    f=fixture(rng,1)
    for arg in range(3):
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        v=Views(*([pointer(f.volume)]*10));inputs=[C.byref(f.scene),C.byref(v),C.byref(out)];inputs[arg]=None
        assert not lib.bk_ending_state_gallery_selected_bindings(*inputs) and bytes(out)==before
        query_rejections+=1
    for field,_ in Views._fields_:
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        v=Views(*([pointer(f.volume)]*10));setattr(v,field,None)
        assert not lib.bk_ending_state_gallery_selected_bindings(C.byref(f.scene),C.byref(v),C.byref(out)) and bytes(out)==before
        query_rejections+=1
    assert operations=={'clock','random','status','voice','load','play','stop','expression','fov','camera','request','restart','fade'},operations

    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        missing_services=missing_count,input_query_rejections=query_rejections,
        operations=sorted(operations),state_sha256=digest.hexdigest(),max_error=0,
        full_gallery=False,real_assets=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS gallery selected:',stats,digest.hexdigest(),flush=True)
if __name__=='__main__':main()
