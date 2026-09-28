"""Compare bounded horizontal route movement with original x86 (no hooks)."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_matrix_oracle import machine
from model_binding import ROOT, library

class Motion(C.Structure):
    _fields_ = [('position', C.c_float*3), ('yaw_degrees', C.c_float), ('cursor', C.c_uint32), ('run_remaining', C.c_int32), ('segment_start', C.c_uint32), ('segment_end', C.c_uint32), ('last_crossed', C.c_uint32), ('crossed', C.c_uint8)]

class Native:
    actor, stack, stop = 0x3000000, 0x2008000, 0x300f000
    def __init__(self, exe):
        self.u = machine(exe)
    def route(self, raw):
        self.u.mem_write(0xbe9a18, raw + bytes(20480-len(raw)))
    def run(self, state, distance):
        u = self.u
        u.mem_write(self.actor+0x29c, struct.pack('<3f', *state.position))
        u.mem_write(self.actor+0x2ac, struct.pack('<f', state.yaw_degrees))
        u.mem_write(self.actor+0x830, struct.pack('<I', state.cursor))
        u.mem_write(0xbf3c6c, struct.pack('<i', state.run_remaining))
        u.mem_write(0xbf3c80, struct.pack('<II', state.segment_start, state.segment_end))
        u.mem_write(0xbf3c74, struct.pack('<I', state.last_crossed))
        u.mem_write(0xbf3c78, bytes([state.crossed]))
        u.mem_write(self.stack, struct.pack('<IIf', self.stop, self.actor, distance))
        u.reg_write(UC_X86_REG_ESP, self.stack)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(0x50132f, self.stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == self.stop
        return Motion((C.c_float*3)(*struct.unpack('<3f', u.mem_read(self.actor+0x29c, 12))), struct.unpack('<f', u.mem_read(self.actor+0x2ac, 4))[0], struct.unpack('<I',u.mem_read(self.actor+0x830,4))[0], struct.unpack('<i',u.mem_read(0xbf3c6c,4))[0], *struct.unpack('<II', u.mem_read(0xbf3c80,8)), struct.unpack('<I',u.mem_read(0xbf3c74,4))[0], u.mem_read(0xbf3c78,1)[0])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path);parser.add_argument('data',type=Path)
    args=parser.parse_args();exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x50132f)
    lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p];lib.bk_route_decode.restype=C.c_void_p
    lib.bk_route_destroy.argtypes=[C.c_void_p]
    lib.bk_route_motion_step.argtypes=[C.POINTER(Motion),C.c_void_p,C.c_float,C.c_char_p];lib.bk_route_motion_step.restype=C.c_int
    count=crossings=failures=0;worst=0;files=[]
    def check(route, state, distance, label):
        nonlocal count,crossings,worst,failures
        original=Motion.from_buffer_copy(state);error=C.create_string_buffer(256)
        ok=lib.bk_route_motion_step(C.byref(state),route,distance,error)
        if not ok:
            assert bytes(original)==bytes(state)
            assert b'past decoded route sentinel' in error.value,(label,error.value)
            failures+=1
            return
        wanted=native.run(original,distance)
        for name in ['cursor','run_remaining','segment_start','segment_end','last_crossed','crossed']:
            assert getattr(state,name)==getattr(wanted,name),(label,name,getattr(state,name),getattr(wanted,name),list(original.position),distance)
        for a,b in zip([*state.position,state.yaw_degrees],[*wanted.position,wanted.yaw_degrees]):
            err=abs(a-b)/max(1,abs(b));worst=max(worst,err)
            assert math.isfinite(a) and err<2e-5,(label, list(original.position),original.cursor,distance,a,b,err)
        count+=1;crossings+=state.crossed==1
    def loaded(raw):
        error=C.create_string_buffer(256);buf=C.create_string_buffer(raw)
        route=lib.bk_route_decode(buf,len(raw),error);assert route,error.value
        native.route(raw)
        return route
    for file in sorted(args.data.glob('*.ckp')):
        raw=file.read_bytes();points=[]
        for offset in range(0,len(raw),20):
            values=struct.unpack_from('<4fB',raw,offset)
            if not any(values):break
            points.append(values)
        if not points:continue
        route=loaded(raw)
        try:
            all_points=points+[(0,0,0,0,0)]
            for cursor in range(1,len(points)+1):
                p,q=all_points[cursor-1],all_points[cursor]
                length=C.c_float(math.hypot(p[0]-q[0],p[2]-q[2])).value
                for fraction in [0,.37,1,1.25]:
                    distance=C.c_float(length*fraction).value
                    state=Motion((C.c_float*3)(p[0],-7.5,p[2]),rng.uniform(-360,360),cursor,rng.choice([-1,0,1,2,3]),7,8,9,1)
                    check(route,state,distance,(file.name,cursor,fraction))
            files.append(file.name)
        finally:lib.bk_route_destroy(route)
    for case in range(1000):
        points=[]
        x,z=rng.uniform(1,200),rng.uniform(1,200)
        for i in range(12):
            if rng.randrange(4):x+=rng.uniform(-20,20);z+=rng.uniform(-20,20)
            points.append((x,3,z,0,rng.choice([0,0,0,1,3,5,6,8,255])))
        raw=b''.join(struct.pack('<4fB3x',*p) for p in points)+bytes(20)
        route=loaded(raw)
        try:
            cursor=rng.randrange(0,10);p=points[max(0,cursor-1)]
            for distance in [0,.125,8,16,32,200]:
                state=Motion((C.c_float*3)(p[0]+rng.uniform(-1,1),-7.5,p[2]+rng.uniform(-1,1)),rng.uniform(-720,720),cursor,rng.choice([-2,0,1,2,3]),3,4,5,2)
                check(route,state,C.c_float(distance).value,('synthetic',case,cursor,distance))
        finally:lib.bk_route_destroy(route)
    report=dict(x87_control_word="0x037f",passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=count,crossing_cases=crossings,bounds_rejections=failures,route_files=files,max_normalized_error=worst,native_functions=['0x50132f','0x50164b','0x4ad870','0x4aeba9','0x4adcd9'],hooks=[],scope='Native horizontal route interpolation, strict first-point crossing versus inclusive later crossing, run countdown and facing. Bounded decoder rejects movement past sentinel. No point-action effects, collision or complete AI.')
    (ROOT/'local/original-route-motion-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',count,'route steps,',crossings,'crossings,',failures,'bounds rejections; max error',worst)
if __name__=='__main__':main()
