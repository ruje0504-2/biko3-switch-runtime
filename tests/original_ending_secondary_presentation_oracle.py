"""Complete original47d3cb orchestration and retained scalar/RNG state.

Animation/face/material/PCM leaves are observing services. The native caller,
timing comparisons, mouth ramp, counters and CRT random generator execute
unchanged. This is not a natural phase2 playthrough or an asset-render test.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Base
from original_ending_frame_oracle import State as Frame, Input
from original_ending_auxiliary_oracle import State as Auxiliary
from original_ending_presentation_oracle import (
    Clock, Advance, Find, Hide, Material, Simple, Face, Level, bits,
)
from model_binding import ROOT, library

I, U, F, B, P = C.c_int32, C.c_uint32, C.c_float, C.c_uint8, C.c_void_p
PRIMARY, BACKGROUND, MODEL, ROOT_NODE, BACK_MODEL = (
    0x3000000, 0x3007000, 0x3006000, 0x3006400, 0x3007800)
HIDDEN = 0x300a000
class State(C.Structure):
    _fields_ = [('rate', F), ('remaining', I), ('active_clip', I),
                ('slow_phase', I), ('mouth_level', F), ('mouth_descending', B)]
class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('auxiliary', C.POINTER(Auxiliary)),
                ('automatic', C.POINTER(I)), ('toggles', C.POINTER(B)),
                ('primary', C.POINTER(U)), ('background', C.POINTER(U)),
                ('hidden', C.POINTER(U)), ('random', C.POINTER(U))]
class Timing(C.Structure):
    _fields_ = [(n, F) for n in ('end', 'source', 'rate')]
Active = C.CFUNCTYPE(I, P, C.POINTER(I), P)
Prediction = C.CFUNCTYPE(I, P, U, C.POINTER(Timing), P)
class Ops(C.Structure):
    _fields_ = [('context', P), ('clock', Clock), ('advance', Advance),
                ('find', Find), ('hide', Hide), ('material', Material),
                ('publish', Simple), ('face', Face), ('level', Level),
                ('active', Active), ('timing', Prediction)]
STATE = dict(rate=0x54cd04, remaining=0x54e290, active_clip=0x54e294,
             slow_phase=0x54e298, mouth_level=0x6bbe40, mouth_descending=0x6bbe3c)
LEAVES = [0x4026fe, 0x425904, 0x423a99, 0x4a7d10, 0x423be2,
          0x411de1, 0x410fd8, 0x4110ef, 0x4ad5a4, 0x411985, 0x300e000]

def packed_state(s):
    return tuple(bits(getattr(s, n)) if t is F else int(getattr(s, n))
                 for n, t in State._fields_)

class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f358, 0x300e000)
        self.u.mem_write(0x300e100, b'\xd9\x05\x00\xe2\x00\x03\xc3')
        for address in LEAVES:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=address, end=address)
    def word(self, p, v):
        self.u.mem_write(p, struct.pack('<I', v & 0xffffffff))
    def read(self, p):
        return struct.unpack('<I', self.u.mem_read(p, 4))[0]
    def state(self):
        out = State()
        for n, t in State._fields_:
            value = bytes(self.u.mem_read(STATE[n], 1 if t is B else 4))
            setattr(out, n, struct.unpack('<' + ('f' if t is F else 'B' if t is B else 'i'), value)[0])
        return out
    def write_state(self, s):
        for n, t in State._fields_:
            self.u.mem_write(STATE[n], struct.pack('<' + ('f' if t is F else 'B' if t is B else 'i'), getattr(s, n)))
    def change(self, changes):
        for n, v in changes.items():
            if n == 'group': self.u.mem_write(0x721b3c, bytes([v]))
            elif n == 'toggles': self.u.mem_write(0x7220f8, bytes(v))
            elif n == 'hidden': self.u.mem_write(HIDDEN, struct.pack('<3I', *v))
            elif n == 'rate': self.word(STATE[n], bits(v))
            elif n in STATE: self.word(STATE[n], v)
            else: self.word(dict(automatic=0x6afd40, stage=0x721ee4,
                                 variant=0x721e04, active=PRIMARY+0x140,
                                 expression=0x721df0, eye_range=0x721df4)[n], v)
    def hook(self, u, address, size, ctx):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret = self.read(sp)
        a = struct.unpack('<6I', u.mem_read(sp+4, 24))
        result = 0
        if address == 0x300e000:
            event = ('clock', self.data['now']); result = self.data['now']
        elif address == 0x4026fe:
            actor = {PRIMARY: 0, BACKGROUND: 2}[a[0]]
            event = ('advance', actor, a[1])
            if actor == 0: self.word(PRIMARY+0x140, self.data['next_active'])
        elif address == 0x425904:
            name = bytes(u.mem_read(a[1], 32)).split(b'\0')[0].decode('ascii')
            assert name in ('OYU', 'Null_del2')
            event = ('find', a[0], name)
            self.word(a[2], self.data['oyu' if name == 'OYU' else 'excluded'])
        elif address == 0x423a99: event = ('hide', *a[:2])
        elif address == 0x4a7d10:
            name = bytes(u.mem_read(a[0],260)).split(b'\0')[0].decode('ascii')
            event = ('material', name, a[1], a[2])
        elif address == 0x423be2: event = ('publish',)
        elif address == 0x411de1:
            assert a[:2] == (900, 0); event = ('face', 0, a[2], 0, 0)
        elif address == 0x410fd8:
            assert a[0] == 900; event = ('face', 1, 0, a[1], 0)
        elif address == 0x4110ef:
            assert a[0] == 900; event = ('face', 2, 0, 0, a[1])
        elif address == 0x4ad5a4:
            assert a[0] == 800
            event = ('level', bits(self.data['level']))
            self.word(0x300e200, bits(self.data['level']))
        elif address == 0x411985:
            assert a[0] == 900; event = ('face', 3, a[1], 0, a[2])
        self.trace.append(event)
        index = len(self.trace)-1
        if index == self.mutation: self.change(self.changes)
        if index == self.failure:
            self.failed = True; u.reg_write(UC_X86_REG_EIP, self.stop); return
        if address == 0x4ad5a4: u.reg_write(UC_X86_REG_EIP,0x300e100)
        else:
            u.reg_write(UC_X86_REG_EAX,result)
            u.reg_write(UC_X86_REG_ESP,sp+4); u.reg_write(UC_X86_REG_EIP,ret)
    def run(self, data, state, dt, mutation, changes, failure):
        self.data,self.mutation,self.changes,self.failure = data,mutation,changes,failure
        self.trace,self.failed = [],False
        self.write_state(state)
        self.change({n:data[n] for n in ('group','toggles','hidden','automatic',
                    'stage','variant','active','expression','eye_range')})
        for p,v in [(0x721b28,PRIMARY),(0x721b34,BACKGROUND),
                    (PRIMARY+0x160,MODEL),(MODEL+0x14,ROOT_NODE),
                    (MODEL+0x180,900),(BACKGROUND+0x160,BACK_MODEL),
                    (BACK_MODEL+0x14,102),(0x722334,800),
                    (0x733700,bits(dt)),(0x58edd8,data['seed'])]: self.word(p,v)
        for slot in range(128):
            p=PRIMARY+0x190+slot*156
            self.word(p+0x58,bits(data['end']));self.word(p+0x5c,bits(data['clip_rate']))
            self.word(p+0x60,bits(data['source']))
        self.call(0x47d3cb, bytes(Input())+struct.pack('<3I',0x70d370,HIDDEN,0x719b44))
        return self.trace, self.failed, packed_state(self.state()), self.read(0x58edd8)

def portable(lib, d, state, dt, mutation, changes, failure):
    s=State.from_buffer_copy(state);frame=Frame();aux=Auxiliary()
    frame.group,frame.state_721ee4=d['group'],d['stage']
    aux.variant,aux.expression_a,aux.expression_b=d['variant'],d['eye_range'],d['expression']
    automatic,active=I(d['automatic']),I(d['active'])
    toggles,hidden=(B*8)(*d['toggles']),(U*3)(*d['hidden'])
    primary,background,seed=U(ROOT_NODE),U(102),U(d['seed'])
    b=Bindings(C.pointer(frame),C.pointer(aux),C.pointer(automatic),toggles,
               C.pointer(primary),C.pointer(background),hidden,C.pointer(seed))
    trace=[]
    def emit(*event):
        trace.append(event);index=len(trace)-1
        if index==mutation:
            for n,v in changes.items():
                if n=='group':frame.group=v
                elif n=='stage':frame.state_721ee4=v
                elif n=='toggles':toggles[:]=v
                elif n=='hidden':hidden[:]=v
                elif n in ('variant','expression','eye_range'):
                    setattr(aux,dict(variant='variant',expression='expression_b',eye_range='expression_a')[n],v)
                elif n in ('active','automatic'):{'active':active,'automatic':automatic}[n].value=v
                else:setattr(s,n,v)
        return index!=failure
    @Clock
    def clock(_,out,e):out[0]=d['now'];return emit('clock',out[0])
    @Advance
    def advance(_,actor,t,e):
        if actor==0:active.value=d['next_active']
        return emit('advance',actor,bits(t))
    @Find
    def find(_,root,name,out,e):
        out[0]=d['oyu' if name==b'OYU' else 'excluded']
        return emit('find',root,name.decode())
    @Hide
    def hide(_,node,value,e):return emit('hide',node,value)
    @Material
    def material(_,name,hide,alpha,e):return emit('material',name.decode(),hide,bits(alpha))
    @Simple
    def publish(_,e):return emit('publish')
    @Face
    def face(_,kind,value,expression,now,e):return emit('face',kind,bits(value),expression,now)
    @Level
    def level(_,out,e):out[0]=d['level'];return emit('level',bits(out[0]))
    @Active
    def get_active(_,out,e):out[0]=active.value;return 1
    @Prediction
    def timing(_,slot,out,e):out[0]=Timing(d['end'],d['source'],d['clip_rate']);return 1
    ops=Ops(None,clock,advance,find,hide,material,publish,face,level,get_active,timing)
    error=C.create_string_buffer(256)
    ok=lib.bk_ending_secondary_presentation_step(C.byref(s),C.byref(b),dt,C.byref(ops),error)
    return trace,not bool(ok),packed_state(s),seed.value

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe',type=Path)
    ap.add_argument('--output',type=Path,default=ROOT/'local/original-ending-secondary-presentation.json')
    args=ap.parse_args();exe=args.exe.read_bytes();native=Native(exe);lib=library()
    lib.bk_ending_secondary_presentation_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),F,C.POINTER(Ops),P]
    lib.bk_ending_secondary_presentation_initial.restype=State
    assert packed_state(lib.bk_ending_secondary_presentation_initial())==packed_state(native.state())
    lib.bk_ending_presentation_material.argtypes=[U,U];lib.bk_ending_presentation_material.restype=C.c_char_p
    for g in range(5):
        for i in range(4):
            wanted=bytes(native.u.mem_read(0x54cd08+(g*4+i)*260,260)).split(b'\0')[0]
            assert lib.bk_ending_presentation_material(g,i)==wanted
    rng=random.Random(0x47d3cb);events=failures=mutations=cycles=0
    for case in range(9000):
        dt=F(rng.choice([0,1/60,.1,.5,1,2.25])).value
        s=State(F(rng.choice([.3,.9,.02,1.])).value,
                rng.choice([-2147483648,-1,0,1,2,8,15]),
                rng.choice([1,2,3,9,14,15,16,127]),rng.choice([0,1,2]),
                F(rng.choice([0,.0001,1,8.999,9])).value,rng.choice([0,1,255]))
        d=dict(group=case%5,stage=rng.choice([-1,0,1,1,1,2,4,5,6,7]),
               automatic=rng.choice([0,1,-1]),variant=rng.randrange(2),
               active=rng.choice([1,2,3,4,9,14,15,16,127]),
               next_active=rng.choice([1,2,3,4,9,14,15,16,127]),
               end=F(80).value,source=F(rng.choice([0,79,80,81])).value,
               clip_rate=F(rng.choice([0,.1,1,10])).value,
               toggles=[rng.choice([0,0,1,255]) for _ in range(8)],
               hidden=[rng.choice([0,300+i]) for i in range(3)],
               now=rng.getrandbits(32),seed=rng.getrandbits(32),
               level=F(rng.uniform(0,10)).value,oyu=rng.choice([0,600]),
               excluded=rng.choice([0,700]),expression=rng.randrange(10),eye_range=rng.randrange(10))
        if case%3==0:s.active_clip=d['active']
        if case%29==0:
            d.update(stage=1,automatic=1,source=80);s.active_clip=d['active'];s.remaining=1
        if case%37==0:d['source']=float('nan')
        mutation=-1 if case%2 else rng.randrange(20)
        failure=-1 if case%4 else rng.randrange(20)
        changes=rng.choice([dict(group=rng.randrange(5)),dict(stage=rng.choice([1,5,6]),automatic=1),
                            dict(active=rng.choice([1,3,9,16])),dict(toggles=[0]*8),
                            dict(hidden=[0,302,303]),dict(rate=.9),dict(expression=3,eye_range=6)])
        want=native.run(d,s,dt,mutation,changes,failure)
        got=portable(lib,d,s,dt,mutation,changes,failure)
        assert got==want,(case,dt,d,packed_state(s),mutation,changes,failure,got,want)
        events+=len(want[0]);failures+=want[1];mutations+=0<=mutation<len(want[0]);cycles+=want[3]!=d['seed']
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=9000,
                observed_calls=events,failure_prefixes=failures,live_mutations=mutations,
                random_cycles=cycles,max_error=0,scope=__doc__)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS secondary presentation',report,flush=True)
if __name__=='__main__':main()
