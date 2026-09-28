"""Actual4d4a06..4d4ed8 toolbar scheduling with live UI/control owners.
Original hover, requests, animation and geometry execute unchanged. Only
key and device IO are substituted; real key callbacks may mutate live mode
fields. This is the common prefix, not full4d499b or a complete ending.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_ending_stage_ui_oracle import Native, Stage, bind, install, same
from original_detached_sprite_oracle import Frame as DrawFrame
from original_ending_frame_oracle import State as Frame, FIELDS, BYTES
from original_ending_control_oracle import State as Control, FLAG_ADDR
from original_ending_auxiliary_oracle import State as Auxiliary
import original_ending_ui_oracle as m
from model_binding import ROOT, library

class Bindings(C.Structure):
    _fields_ = [('frame', C.POINTER(Frame)), ('control', C.POINTER(Control)),
                ('auxiliary', C.POINTER(Auxiliary)), ('open', C.POINTER(C.c_int32))]

def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('exe', type=Path)
    exe=ap.parse_args().exe.read_bytes(); n=Native(exe); lib=library(); bind(lib)
    lib.bk_ending_ui_toolbar.argtypes=[C.POINTER(m.Ui), C.POINTER(Stage), C.POINTER(Bindings),
                                      C.POINTER(C.c_float), C.c_float, C.c_float,
                                      C.POINTER(m.Ops), C.POINTER(C.c_uint8),
                                      C.POINTER(DrawFrame), C.c_void_p]
    e=C.create_string_buffer(256); rng=random.Random(0x4d4a06)
    base,stage=m.Ui(),Stage();flags=(C.c_uint8*6)();gauge=C.c_float()
    frames=draws=vertices=key_calls=mutations=repeated=0
    def mutate_native():
        n.wi(0x721e08,2);n.wi(0x721ec4,1);n.wi(0x721ec8,3)
    def key_hook(u,a,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);code,mode,extra=struct.unpack('<3I',u.mem_read(sp+4,12))
        assert code in [0,1] and mode==2 and extra==0
        n.trace.append((code,mode));ret=n.keys[code]
        if n.mutate:mutate_native()
        u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,n.ri(sp))
    n.u.hook_add(UC_HOOK_CODE,key_hook,begin=0x4b76c2,end=0x4b76c2)
    for case in range(2400):
        width=[320,640,961,1280][case%4];scale=C.c_float(width/1280).value
        if case%120==0:
            assert lib.bk_ending_ui_initialize(C.byref(base),width,flags,C.byref(gauge),e)
        f,c,a=Frame(),Control(),Auxiliary()
        f.phase=case%12-1;f.camera_manual=rng.choice([0,0,0,1,255]);f.state_721ee0=rng.choice([0,1,3,7])
        f.curtain_wanted=rng.choice([0,0,1,255]);f.camera_request=rng.choice([0,1,2,-1])
        f.camera_clip=rng.randrange(-1,5);f.camera_mode=rng.choice([0,1,2,-1]);f.auxiliary_mode=rng.randrange(-1,6)
        c.mode_721ec4=rng.randrange(-1,5);c.hover=rng.choice([-1,0,12,29,38,21,17,19,45,47,59,61])
        c.toggles[:]=[rng.choice([0,0,1,255]) for _ in range(8)]
        c.pause_flags[:]=[rng.choice([0,1,2,255]) for _ in range(6)]
        a.gate=rng.choice([0,1,3,7]);opened=C.c_int32(rng.choice([0,1,-1,2]))
        ptr=(C.c_float*2)(rng.choice([0,1096,1200,1280,1300])*scale,100*scale)
        dt=C.c_float(rng.choice([0,1/60,.001,.1,.5])).value
        install(n,base,stage)
        for name,addr in FIELDS:n.wi(addr,getattr(f,name))
        for name,addr in BYTES:n.u.mem_write(addr,bytes([getattr(f,name)]))
        n.wi(0x721ec4,c.mode_721ec4);n.wi(0x70c8bc,c.hover);n.wi(0x721ef0,a.gate);n.wi(0x72210c,opened.value)
        n.u.mem_write(0x7220f8,bytes(c.toggles))
        for addr,val in zip(FLAG_ADDR,c.pause_flags):n.u.mem_write(addr,bytes([val]))
        n.wf(0x721ad0,scale);n.wf(0x733700,dt)
        n.wf(n.stack-0x124,ptr[0]);n.wf(n.stack-0x128,ptr[1]);n.u.mem_write(n.stack-8,b'\x7f')
        n.keys=[rng.choice([0,1,255,256,0xffffffff]) for _ in range(2)];n.mutate=case%3==0
        n.trace=[];n.draws=[];n.segment(0x4d4a06,0x4d4ed8)
        trace=[]
        @m.KEY
        def key(_,code,mode,out,err):
            trace.append((code,mode));out[0]=n.keys[code]
            if n.mutate:f.camera_clip=2;c.mode_721ec4=1;f.auxiliary_mode=3
            return 1
        frame=DrawFrame();visible=C.c_uint8(127);b=Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(opened))
        assert lib.bk_ending_ui_toolbar(C.byref(base),C.byref(stage),C.byref(b),ptr,scale,dt,
                                        C.byref(m.Ops(None,key)),C.byref(visible),C.byref(frame),e), e.value
        assert trace==n.trace and visible.value==n.u.mem_read(n.stack-8,1)[0]
        assert opened.value==C.c_int32(n.ri(0x72210c)).value
        assert frame.count==len(n.draws)
        for i,(slot,sp,raw) in enumerate(n.draws):
            d=frame.draws[i];assert d.slot==slot and d.alpha==sp.alpha and d.rgb==sp.rgb and tuple(d.uv)==tuple(sp.uv)
            for j,k in enumerate([0,1,2,3,0,2]):
                assert tuple(d.xy[2*k:2*k+2])==struct.unpack_from('<2f',raw,32*j),(case,i,slot,j)
                vertices+=1
        same(n,base,stage,('toolbar',case))
        frames+=1;draws+=frame.count;key_calls+=len(trace);mutations+=bool(trace) and n.mutate
        repeated+=sum(d.slot==17 for d in frame.draws[:frame.count])==2
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,draws=draws,
                vertices=vertices,key_calls=key_calls,mutated_key_frames=mutations,
                repeated_button_frames=repeated,max_error=0,scope=__doc__)
    (ROOT/'local/original-ending-ui-toolbar-oracle.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report,flush=True)
if __name__=='__main__':main()
