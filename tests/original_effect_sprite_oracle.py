"""Original50e633/50e6ba modes enter1..3/exit1..3/idle0,3,5 and43ed45.
Only device/vertex-buffer boundaries are substituted; original rotation,
transition, hit extents, alpha and geometry functions execute unmodified.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from original_zoom_sprite_oracle import Native as Base, Zoom
from original_item_notice_oracle import Fade
from model_binding import ROOT, library

class Effect(C.Structure):
    _fields_ = [('fade', Fade), ('scale', C.c_float*2), ('pivot', C.c_float*2),
                ('degrees', C.c_float), ('radians', C.c_float), ('motion', C.c_float*2),
                ('half_extent', C.c_int32*2), ('enter', C.c_uint8), ('exit', C.c_uint8),
                ('idle', C.c_uint8), ('direction', C.c_uint8)]

def snapshot(s):
    return (s.fade.alpha, s.fade.speed, s.fade.stage, *s.scale, *s.pivot,
            s.degrees, s.radians, *s.motion, *s.half_extent, s.enter,s.exit,s.idle,s.direction)

class Native(Base):
    def install(self, a, i, s, r):
        self.install_zoom(a, i, Zoom(s.fade, s.scale, s.pivot), r, s.enter)
        self.u.mem_write(a+0x148, bytes([s.enter,s.exit,s.idle]))
        self.u.mem_write(a+0x160, bytes([s.direction]))
        for off,v in [(0x130,s.degrees),(0x150,s.motion[0]),(0x154,s.motion[1])]:self.wf(a+off,v)
        self.u.mem_write(a+0x158, bytes(s.half_extent))
        self.call(0x43f230, struct.pack('<If',self.handle(i),s.radians))
    def read(self, a, i):
        z=self.read_zoom(a)
        s=Effect(z.fade,z.scale,z.pivot,self.rf(a+0x130),self.rf(self.handle(i)+0x98),
                 (C.c_float*2)(self.rf(a+0x150),self.rf(a+0x154)),
                 (C.c_int32*2).from_buffer_copy(self.u.mem_read(a+0x158,8)),
                 *self.u.mem_read(a+0x148,3),self.u.mem_read(a+0x160,1)[0])
        return s

def bind(lib):
    lib.bk_effect_sprite_initialize.argtypes=[C.POINTER(Effect),C.POINTER(C.c_float),C.c_uint8,C.c_uint8,C.c_uint8]
    lib.bk_effect_sprite_advance.argtypes=[C.POINTER(Effect),C.POINTER(C.c_float),C.c_float]
    lib.bk_effect_sprite_quad.argtypes=[C.POINTER(Effect),C.POINTER(C.c_float),C.POINTER(C.c_float)]
    lib.bk_fade_sprite_request.argtypes=[C.POINTER(Fade),C.c_uint8]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();bind(lib);rng=random.Random(0x50e6ba)
    count=vertices=0;worst=0
    def check(s,r,dt,wanted):
        nonlocal count,vertices,worst
        old=snapshot(s);n.install(n.base,0,s,r);n.draws=[];n.wf(0x733700,dt)
        if wanted is not None:
            n.call(0x50e633,struct.pack('<II',n.base,wanted));assert lib.bk_fade_sprite_request(C.byref(s.fade),wanted)
        n.call(0x50e6ba,struct.pack('<II',n.base,0))
        assert lib.bk_effect_sprite_advance(C.byref(s),r,dt)
        assert snapshot(s)==snapshot(n.read(n.base,0)),(count,old,dt,wanted,snapshot(s),snapshot(n.read(n.base,0)))
        q=(C.c_float*8)();assert lib.bk_effect_sprite_quad(C.byref(s),r,q);assert len(n.draws)==1
        for j,i in enumerate([0,1,2,3,0,2]):
            want=struct.unpack_from('<2f',n.draws[0],32*j);got=tuple(q[2*i:2*i+2])
            err=max(abs(v-w) for v,w in zip(got,want));worst=max(worst,err)
            assert got==want,(count,j,'xy',got,want,snapshot(s))
            assert struct.unpack_from('<I',n.draws[0],32*j+16)[0]==(int(s.fade.alpha*255)<<24)|0xffffff
            vertices+=1
        count+=1
    for case in range(8100):
        s=Effect(Fade(rng.random(),rng.choice([0,.01,.5,2,100]),case%6),
                 (C.c_float*2)(rng.uniform(-1,6),rng.uniform(-1,6)),
                 (C.c_float*2)(rng.uniform(-1,2),rng.uniform(-1,2)),rng.uniform(-400,400),rng.uniform(-3,3),
                 (C.c_float*2)(rng.uniform(-2,2),rng.uniform(-2,2)),(C.c_int32*2)(-1,987),
                 1+(case//6)%3,1+(case//18)%3,[0,3,5][(case//54)%3],rng.choice([0,1,2,255]))
        r=(C.c_float*4)(rng.uniform(-200,1300),rng.uniform(-200,1000),rng.uniform(0,1300),rng.uniform(0,1000))
        check(s,r,C.c_float(rng.choice([0,1/60,.00001,.5,1,10])).value,rng.choice([None,0,1,2,255]))
    for mode in [(1,1,0),(2,3,5),(3,2,5),(3,2,3)]:
        for direction in [0,1,2,255]:
            s=Effect();s.direction=direction;r=(C.c_float*4)(500,320,256,256)
            assert lib.bk_effect_sprite_initialize(C.byref(s),r,*mode)
            for tick in range(600):
                request={2:1,140:0,149:1,230:1,400:0,420:1,490:1}.get(tick)
                check(s,r,C.c_float(1/60).value,request)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=count,vertices=vertices,max_error=worst,scope=__doc__)
    (ROOT/'local/original-effect-sprite-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
