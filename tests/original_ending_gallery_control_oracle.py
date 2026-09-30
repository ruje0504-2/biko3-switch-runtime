"""Full48302B parent and483A36 classification against native instructions.

Native state tables, CRT record copies/clears, threshold arithmetic and
classification run unchanged. Camera/audio/expression and the five child
controllers are observing service boundaries. Real BkEndingState aliases
are bound by the production binding helper, with no duplicated workspace.
This does not implement those children or validate a playable gallery.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from original_ending_state_oracle import State as OldScene, REGIONS
from original_ending_tertiary_control_oracle import Records, Record
from model_binding import ROOT, library

I, U, F, B, S, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_int8, C.c_void_p
SOUNDS = [0x300a000, 0x300a020]
CHILDREN = [0x483af0, 0x4843aa, 0x4855c9, 0x48758c, 0x488674]


class Scene(C.Structure):
    #The existing oracle's prefix layout predates the state6 saved camera.
    _fields_ = OldScene._fields_ + [('auxiliary_saved_target', U*3), ('auxiliary_saved_orbit', F*4)]


class State(C.Structure):
    _fields_ = [('fov', F)]


class Bindings(C.Structure):
    _fields_ = [(n,P) for n in ['frame', 'control', 'auxiliary', 'records',
        'action', 'previous', 'opening', 'normal', 'secondary', 'tertiary',
        'special', 'transition_latch', 'selected', 'cursor', 'workspace']]+[
        ('workspace_capacity', U)]+[(n,P) for n in ['counters', 'opening_counter',
        'step_counter', 'next_mode', 'speech_name', 'voice_volume']]


Expression = C.CFUNCTYPE(I, P, I, I, I, P)
Fov = C.CFUNCTYPE(I, P, F, P)
Camera = C.CFUNCTYPE(I, P, I, I, C.POINTER(U), U, C.POINTER(U), P)
Target = C.CFUNCTYPE(I, P, C.POINTER(U), P)
Status = C.CFUNCTYPE(I, P, U, C.POINTER(I), P)
Load = C.CFUNCTYPE(I, P, U, C.c_char_p, P)
Play = C.CFUNCTYPE(I, P, U, I, P)
Name = C.CFUNCTYPE(I, P, P, P)
Voice = C.CFUNCTYPE(I, P, I, U, I, I, P)
Cue = C.CFUNCTYPE(I, P, I, I, U, I, P)
Child = C.CFUNCTYPE(I, P, U, P)


class Ops(C.Structure):
    _fields_ = [('context',P), ('expression',Expression), ('fov',Fov),
        ('camera',Camera), ('target',Target), ('present',Status), ('status',Status),
        ('load',Load), ('play',Play), ('voice_name',Name), ('voice',Voice),
        ('cue',Cue), ('child',Child)]


def bits(f): return struct.unpack('<I',struct.pack('<f',f))[0]
def floating(n): return struct.unpack('<f',struct.pack('<I',n))[0]


class Fixture:
    @property
    def final(self): return self.scene.retained.final

    def clone(self):
        f = Fixture()
        for name in ['scene','state','records','target','volume','camera_fov']:
            value = getattr(self,name)
            setattr(f,name,type(value).from_buffer_copy(value))
        for name in ['present','playing','hr','loaded']:
            setattr(f,name,list(getattr(self,name)))
        for name in ['seconds','camera_result','voice_name','mutate_at','mutations','capacity']:
            setattr(f,name,getattr(self,name))
        return f

    def snapshot(self):
        return (bytes(self.scene),bytes(self.state),bytes(self.target),bytes(self.volume),
                bytes(self.camera_fov),tuple(self.present),tuple(self.playing))

    def effect(self,event,index):
        kind=event[0]
        if kind == 'fov': self.camera_fov.value=floating(event[1])
        elif kind == 'expression':
            self.scene.auxiliary.expression_a, self.scene.auxiliary.expression_b = event[1:3]
            self.scene.face_mode=event[3]
        elif kind in ['load','voice']:
            slot=event[1] if kind=='load' else event[2]
            self.present[slot],self.playing[slot]=self.loaded[slot],0
        elif kind == 'play': self.playing[event[1]]=self.present[event[1]]
        elif kind == 'cue':
            self.present[event[3]]=self.loaded[event[3]]
            self.playing[event[3]]=self.present[event[3]]
        elif kind == 'child':
            self.final.word_6dde4c += 1
            self.scene.frame.state_721ee0=0
        if index == self.mutate_at:
            for path,value in self.mutations.items():
                obj=self
                keys=path.split('.')
                for key in keys[:-1]: obj=getattr(obj,key)
                key=keys[-1]
                if key.isdigit():obj[int(key)]=value
                else:setattr(obj,key,value)


class Native:
    def __init__(self,exe):
        self.u=machine(exe)
        self.stack,self.stop=0x2008000,0x300f000
        self.word(0x53f2f0,0x300d010)
        self.word(0x53f214,0x300d020)
        self.word(0x300b024,0x300d100)
        self.word(0x721f08,0x3002000)
        for sound in SOUNDS:self.word(sound,0x300b000)
        for address in [0x42cf0e,0x4dfb96,0x4e0b7e,0x4e0ecb,0x4e0818,
                        0x4e0956,0x4ad2bf,0x479739,0x49490a,
                        0x300d010,0x300d020,0x300d100,*CHILDREN]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=address,end=address)

    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def integer(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
    def string(self,a):return bytes(self.u.mem_read(a,260)).split(b'\0')[0]

    def sync(self,read):
        f=self.f
        for offset,address,size,_ in REGIONS:
            pointer=C.addressof(f.scene)+offset
            if read:C.memmove(pointer,bytes(self.u.mem_read(address,size)),size)
            else:self.u.mem_write(address,C.string_at(pointer,size))
        for owner,address in [(f.state,0x5545e0),(f.target,0x30020f0),(f.volume,0xbe9a08)]:
            if read:C.memmove(C.addressof(owner),bytes(self.u.mem_read(address,C.sizeof(owner))),C.sizeof(owner))
            else:self.u.mem_write(address,bytes(owner))
        if not read:
            for i,sound in enumerate(SOUNDS):self.word(0x722334+i*0x120,sound if f.present[i] else 0)

    def hook(self,u,address,_size,_context):
        sp=u.reg_read(UC_X86_REG_ESP)
        ret=self.integer(sp)
        args=struct.unpack('<6I',u.mem_read(sp+4,24))
        self.sync(True)
        f=self.f
        result,cleanup,event=0,4,None
        if address==0x300d010:
            fmt=self.string(args[1])
            assert fmt in [b'PH%d0001.wav',b'PH%d3101.wav',b'PH%d0261.wav',
                           b'PH%d0260.wav',b'PH%d3213.wav',b'PH%d3311.wav'],fmt
            name=fmt%I(args[2]).value
            u.mem_write(args[0],name+b'\0');result=len(name)
        elif address==0x300d020:
            assert args[0]==0x722224
            u.mem_write(args[0],self.string(args[1])+b'\0')
            self.sync(True)
            result,cleanup=args[0],12
        elif address==0x42cf0e:event=('fov',args[0])
        elif address==0x4dfb96:event=('expression',*[I(x).value for x in args[:3]])
        elif address in [0x4e0b7e,0x4e0ecb]:
            assert args[0]==0x71af38
            kind=int(address==0x4e0ecb)
            event=('camera',kind,I(args[1]).value,tuple(args[2:5]),args[5]) if kind else ('camera',0,0,(0,0,0),0)
            result=f.camera_result
        elif address==0x4e0818:
            assert args[1:4]==(0,0,0)
            event=('voice_name',)
            u.mem_write(args[0],f.voice_name+b'\0')
        elif address==0x4e0956:
            self.last_slot=args[1]
            event=('load',args[1],self.string(args[0]))
        elif address==0x4ad2bf:
            slot=SOUNDS.index(args[0]) if args[0] else self.last_slot
            assert args[1]==0 and slot in [0,1]
            event=('play',slot,I(args[2]).value)
        elif address==0x479739:
            self.last_slot=args[1]
            event=('voice',I(args[0]).value,args[1],I(args[2]).value,I(args[3]).value)
        elif address==0x49490a:event=('cue',I(args[0]).value,I(args[1]).value,args[2],I(args[3]).value)
        elif address==0x300d100:
            slot=SOUNDS.index(args[0])
            assert f.present[slot]
            event=('status',slot)
            self.word(args[1],f.playing[slot])
            result,cleanup=f.hr[slot],12
        elif address in CHILDREN:event=('child',CHILDREN.index(address)+4)
        else:raise AssertionError(hex(address))
        if event:
            self.trace.append((event,f.snapshot()))
            if self.fail_at==len(self.trace):
                self.failed=True;u.emu_stop();return
            f.effect(event,len(self.trace))
        self.sync(False)
        u.reg_write(UC_X86_REG_EAX,result&0xffffffff)
        u.reg_write(UC_X86_REG_ESP,sp+cleanup)
        u.reg_write(UC_X86_REG_EIP,ret)

    def run(self,original,fail_at=0):
        self.f,self.trace=original.clone(),[]
        self.fail_at,self.failed,self.last_slot=fail_at,False,0
        self.sync(False)
        self.u.mem_write(0xb550b0,bytes(self.f.records))
        self.u.mem_write(0x733700,struct.pack('<f',self.f.seconds))
        self.u.mem_write(self.stack-512,b'\xcd'*512)
        self.word(self.stack,self.stop)
        self.u.reg_write(UC_X86_REG_ESP,self.stack)
        self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(0x48302b,self.stop,count=1000000)
        assert self.failed or self.u.reg_read(UC_X86_REG_EIP)==self.stop,hex(self.u.reg_read(UC_X86_REG_EIP))
        self.sync(True)
        assert self.u.mem_read(0xb550b0,C.sizeof(Records))==bytes(original.records)
        return self.f,self.trace


def portable(lib,original,fail_at=0,missing=None,query_fail=None):
    f,trace,errors,callbacks=original.clone(),[],[],[]
    def emit(event):
        trace.append((event,f.snapshot()))
        if fail_at==len(trace):return 0
        f.effect(event,len(trace))
        return 1
    def decorate(typ):
        def wrapper(fn):
            def checked(*args):
                try:return fn(*args)
                except Exception as exc:errors.append(repr(exc));return 0
            cb=typ(checked);callbacks.append(cb);return cb
        return wrapper
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
        for index in range(3):out[index]=f.target[index]
        return 1
    @decorate(Status)
    def present(_,slot,out,_e):
        if query_fail=='present':return 0
        out[0]=f.present[slot]
        return 1
    @decorate(Status)
    def status(_,slot,out,_e):
        out[0]=int(f.hr[slot]==0 and bool(f.playing[slot]&1))
        return emit(('status',slot))
    @decorate(Load)
    def load(_,slot,name,_e):return emit(('load',slot,name))
    @decorate(Play)
    def play(_,slot,volume,_e):return emit(('play',slot,volume))
    @decorate(Name)
    def voice_name(_,out,_e):
        C.memmove(out,f.voice_name+b'\0',len(f.voice_name)+1)
        return emit(('voice_name',))
    @decorate(Voice)
    def voice(_,cue,slot,bank,select,_e):return emit(('voice',cue,slot,bank,select))
    @decorate(Cue)
    def cue(_,cue,bank,slot,flags,_e):return emit(('cue',cue,bank,slot,flags))
    @decorate(Child)
    def child(_,state,_e):return emit(('child',state))
    b=Bindings()
    assert lib.bk_ending_state_gallery_bindings(C.byref(f.scene),C.byref(f.records),C.byref(f.volume),C.byref(b))
    b.workspace_capacity=f.capacity
    ops=Ops(None,expression,fov,camera,target,present,status,load,play,voice_name,voice,cue,child)
    if missing:setattr(ops,missing,dict(Ops._fields_)[missing]())
    error=C.create_string_buffer(256)
    ok=lib.bk_ending_gallery_control_step(C.byref(f.state),C.byref(b),f.seconds,C.byref(ops),error)
    assert not errors,errors
    assert bytes(f.records)==bytes(original.records),'record owner was modified'
    return bool(ok),f,trace,error.value.decode()


RECORD_BYTES=random.Random(0xb550b0).randbytes(C.sizeof(Records))
def fixture(rng,case):
    f=Fixture()
    f.scene=Scene.from_buffer_copy(rng.randbytes(C.sizeof(Scene)))
    f.state=State(rng.choice([0,.19999,.2,.20001,.5,1,2,float('nan')]))
    f.records=Records.from_buffer_copy(RECORD_BYTES)
    f.scene.frame.group=case%5
    f.scene.frame.state_721ee0=case%12-2
    f.scene.frame.camera_clip=rng.randrange(-1,4)
    f.scene.auxiliary.variant=rng.choice([-1,0,1,2])
    f.scene.auxiliary.selection=rng.choice([-1,0,1,2,0x7fffffff])
    f.final.byte_6dde58=rng.choice([0,1,2,3,4,255])
    f.final.byte_6ddce0=rng.choice([0,1,2,3,4,5,7,255])
    f.final.byte_6d1be1=rng.randrange(256)
    f.final.word_6c7f74=rng.randrange(10000)
    f.final.word_6dde4c=41
    f.final.workspace_6c7f80[f.final.word_6c7f74]=rng.choice(list(range(-1,23))+[0x7fffffff,0x1000c,0x80,0x108])
    f.scene.control.toggles[7]=rng.choice([0,1,128,255])
    f.volume=I(rng.randrange(-10000,1))
    f.camera_fov=F(.7)
    f.target=(U*3)(*[rng.randrange(0x100000000) for _ in range(3)])
    f.present=[rng.randrange(2) for _ in range(2)]
    f.playing=[rng.randrange(4) for _ in range(2)]
    f.hr=[rng.choice([0,0,0,-1]) for _ in range(2)]
    f.loaded=[rng.randrange(2) for _ in range(2)]
    f.seconds=F(rng.choice([0,1/120,1/60,1/30,.99,1,1.1])).value
    f.camera_result=rng.choice([0,1,0x100,0xa580,0xffffffff])
    f.voice_name=b'PH11101.wav'
    f.capacity,f.mutate_at,f.mutations=10000,0,{}
    if case%7==0:
        f.mutate_at=case//7%7+1
        f.mutations={'scene.frame.group':(f.scene.frame.group+1)%5,
            'scene.auxiliary.variant':1-int(bool(f.scene.auxiliary.variant)),
            'scene.auxiliary.selection':2,'scene.frame.camera_clip':2,
            'volume.value':-437,'state.fov':.41,'scene.control.toggles.7':128,
            'target.0':bits(19.5),'final.byte_6dde50':77}
    return f


def compare(want,got,expected,actual,label):
    assert [x for x,_ in actual]==[x for x,_ in expected],(label,'calls',[x for x,_ in actual],[x for x,_ in expected])
    for index,((event,a),(_,b)) in enumerate(zip(actual,expected)):
        assert a==b,(label,index,event,'snapshot',[(j,len(x),len(y)) for j,(x,y) in enumerate(zip(a,b)) if x!=y])
    if got.snapshot()!=want.snapshot():
        a,b=bytes(got.scene),bytes(want.scene)
        mismatches=[(name,a[off:off+size],b[off:off+size]) for off,_,size,name in REGIONS
                    if a[off:off+size]!=b[off:off+size]]
        raise AssertionError((label,'final',[(x,y[:24],z[:24]) for x,y,z in mismatches],
                              bytes(got.state),bytes(want.state)))


def main():
    faulthandler.enable()
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe',type=Path)
    p.add_argument('--cases',type=int,default=12000)
    p.add_argument('--output',type=Path,default=ROOT/'local/original-ending-gallery-control.json')
    args=p.parse_args()
    if args.cases<1:p.error('cases must be positive')
    exe=args.exe.read_bytes()
    native,lib=Native(exe),library()
    lib.bk_ending_gallery_control_initial.argtypes=[]
    lib.bk_ending_gallery_control_initial.restype=State
    lib.bk_ending_state_gallery_bindings.argtypes=[P,P,P,C.POINTER(Bindings)]
    lib.bk_ending_state_gallery_bindings.restype=I
    lib.bk_ending_gallery_control_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P]
    lib.bk_ending_gallery_control_step.restype=I
    assert bytes(lib.bk_ending_gallery_control_initial())==native.u.mem_read(0x5545e0,4)
    assert struct.unpack('<9I',native.u.mem_read(0x483a02,36))==(0x4834cd,0x4839fe,0x483870,0x48307c,0x48383e,0x483848,0x483852,0x48385c,0x483866)
    rng,digest=random.Random(0x48302b),hashlib.sha256()
    stats=dict(random_frames=0,boundary_frames=0,retained_frames=0,calls=0,
               live_mutations=0,failure_prefixes=0,record_copies=0)
    operations,children,names=set(),set(),set()
    def check(f,label,failure=False):
        want,expected=native.run(f)
        ok,got,trace,error=portable(lib,f)
        assert ok,(label,error)
        compare(want,got,expected,trace,label)
        stats['calls']+=len(trace)
        stats['live_mutations']+=0<f.mutate_at<=len(trace)
        stats['record_copies']+=bytes(f.final.workspace_6c7f80)!=bytes(got.final.workspace_6c7f80)
        for event,_ in trace:
            operations.add(event[0])
            if event[0]=='child':children.add(event[1])
            if event[0]=='load':names.add(event[2].decode())
        digest.update(bytes(got.scene)+bytes(got.state))
        if failure:
            for index in range(1,len(trace)+1):
                want,expected=native.run(f,index)
                ok,got_failed,prefix,error=portable(lib,f,index)
                assert not ok
                compare(want,got_failed,expected,prefix,(label,'failure',index))
                stats['failure_prefixes']+=1
        return got
    for case in range(args.cases):
        check(fixture(rng,case),('random',case),case%11==0)
        stats['random_frames']+=1
    #All signed low-byte classes; full workspace word still selects12/13/14
    #only, so0x1000c must classify as12 without selecting its same subcase.
    for variant in [0,1]:
        for high in [0,0x10000]:
            for byte in range(256):
                f=fixture(rng,1)
                f.scene.frame.state_721ee0=0
                f.scene.auxiliary.variant=variant
                f.final.workspace_6c7f80[f.final.word_6c7f74]=high+byte
                check(f,('byte',variant,high,byte))
                stats['boundary_frames']+=1
    for group in range(5):
        for variant in [0,1]:
            for mutation in [1,2,3]:
                f=fixture(rng,0)
                f.scene.frame.group,f.scene.auxiliary.variant=group,variant
                f.scene.frame.state_721ee0=3;f.final.byte_6dde58=1
                f.camera_result=1;f.mutate_at=mutation
                f.mutations={'scene.frame.group':(group+1)%5,'scene.auxiliary.variant':1-variant,
                             'target.0':bits(31.25),'volume.value':-371}
                check(f,('captured-record',group,variant,mutation),True)
                stats['boundary_frames']+=1
            f=fixture(rng,1)
            f.scene.frame.group,f.scene.auxiliary.variant=group,variant
            f.scene.frame.state_721ee0=3
            f.final.byte_6dde58=0
            f.scene.control.toggles[7]=1
            f.present,f.playing,f.hr,f.loaded=[0,0],[0,0],[0,0],[1,1]
            f.camera_result=1
            f.seconds=F(1/60).value
            for step in range(10):
                if step>=4:f.playing=[0,0]
                f=check(f,('retained',group,variant,step))
                stats['retained_frames']+=1
    rejected=0
    for seconds in [-1,float('nan'),float('inf')]:
        f=fixture(rng,1);f.seconds=seconds
        ok,got,trace,error=portable(lib,f)
        assert not ok and not trace and got.snapshot()==f.snapshot(),error
        rejected+=1
    for main in [0,2]:
        for cursor in [-1,10000,0x7fffffff]:
            f=fixture(rng,1);f.scene.frame.state_721ee0=main
            f.final.byte_6ddce0=2;f.final.word_6c7f74=cursor
            ok,got,trace,error=portable(lib,f)
            assert not ok and error=='gallery control: workspace cursor outside capacity' and not trace
            assert got.scene.frame.camera_request==1
            if main==2:assert got.scene.frame.transition_action==7
            rejected+=1
    for group,capacity,message in [(5,10000,'captured record group outside table'),
                                  (0,9999,'workspace cannot hold original record lane')]:
        f=fixture(rng,1);f.scene.frame.state_721ee0=3;f.scene.frame.group=group
        f.final.byte_6dde58=1;f.capacity=capacity;f.camera_result=1
        ok,got,trace,error=portable(lib,f)
        assert not ok and error=='gallery control: '+message
        assert got.final.byte_6dde58==2 and got.state.fov==1
        assert bytes(got.final.workspace_6c7f80)==bytes(f.final.workspace_6c7f80)
        rejected+=1
    f=fixture(rng,1);f.scene.frame.state_721ee0=3;f.scene.frame.group=5;f.final.byte_6dde58=2
    ok,_,trace,error=portable(lib,f)
    assert not ok and not trace and error=='gallery control: camera group outside table'
    rejected+=1
    f=fixture(rng,1);f.scene.frame.state_721ee0=3;f.final.byte_6dde58=1
    f.scene.auxiliary.variant=1;f.camera_result=1
    ok,got,trace,_=portable(lib,f,query_fail='target')
    assert not ok and [event[0] for event,_ in trace]==['fov','camera','fov']
    assert got.final.byte_6dde58==2 and got.final.word_6c7f74==0
    assert got.scene.auxiliary.index==54
    assert bytes(got.final.workspace_6c7f80)==bytes(f.records.groups[f.scene.frame.group].retained[1])
    assert bytes(got.scene.frame.camera_values)==bytes(f.scene.frame.camera_values)
    rejected+=1
    f=fixture(rng,1);f.scene.frame.state_721ee0=2;f.final.byte_6ddce0=0
    ok,got,trace,_=portable(lib,f,query_fail='present')
    assert not ok and not trace and got.scene.next_mode==1
    assert got.scene.frame.transition_action==6 and got.final.byte_6c7f70==0
    assert got.scene.frame.curtain_wanted==f.scene.frame.curtain_wanted
    rejected+=1
    missing_count=0
    for member,_ in Ops._fields_[1:]:
        f=fixture(rng,1);f.scene.frame.state_721ee0=3;f.final.byte_6dde58=0
        f.camera_result=1;f.mutate_at=0
        if member in ['fov','camera','target']:f.final.byte_6dde58=1
        elif member in ['load','play']:f.final.byte_6dde58=2
        elif member in ['present','status','voice_name','voice']:
            f.final.byte_6dde58=3
            f.present=[int(member=='status'),0];f.playing=[0,0];f.hr=[0,0]
            f.scene.control.toggles[7]=1
            f.scene.auxiliary.variant=int(member=='voice')
        elif member=='cue':
            f.scene.frame.state_721ee0=0;f.final.byte_6ddce0=2
            f.scene.auxiliary.variant=0
            f.final.workspace_6c7f80[f.final.word_6c7f74]=12
        elif member=='child':f.scene.frame.state_721ee0=4
        ok,got,trace,error=portable(lib,f,missing=member)
        assert not ok and error=='gallery control: missing '+member+' service',(member,error)
        missing_count+=1
    #Invalid alias bindings preserve the entire output structure.
    f=fixture(rng,1)
    for arg in range(4):
        out=Bindings.from_buffer_copy(b'\xa5'*C.sizeof(Bindings));before=bytes(out)
        inputs=[C.byref(f.scene),C.byref(f.records),C.byref(f.volume),C.byref(out)]
        inputs[arg]=None
        assert not lib.bk_ending_state_gallery_bindings(*inputs) and bytes(out)==before
        rejected+=1
    if args.cases>=12000:
        assert children==set(range(4,9))
        assert operations=={'fov','expression','camera','load','play','status','voice_name','voice','cue','child'},operations
        assert stats['record_copies']>0 and stats['live_mutations']>0
    result=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),**stats,
        bounded_rejections=rejected,missing_services=missing_count,operations=sorted(operations),
        children=sorted(children),loaded_names=sorted(names),state_sha256=digest.hexdigest(),
        max_error=0,full_gallery=False,gpu_or_device_validation=False)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS gallery control:',stats,digest.hexdigest(),flush=True)


if __name__=='__main__':main()
