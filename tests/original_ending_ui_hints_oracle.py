"""Original4d4ed8..4d5e60 four hint branches, gauge and actual4971bd meter.
Original calls, float-motion fabs, line/circle/animation and geometry execute
unchanged; only device IO is substituted. Projected target arrays and primary
clip are explicit inputs, not a recovered projection or complete ending.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from original_ending_stage_ui_oracle import Native, Stage, bind, install, same
from original_detached_sprite_oracle import Frame as DrawFrame
from original_ending_frame_oracle import State as Frame, FIELDS, BYTES
from original_ending_control_oracle import State as Control
from original_ending_auxiliary_oracle import State as Auxiliary
import original_ending_ui_oracle as m
from original_ending_ui_toolbar_oracle import Bindings as ToolbarBindings
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from model_binding import ROOT, library

I=C.c_int32; F=C.c_float; Pair=I*2
class State(C.Structure):
    _fields_=[('length',F),('fixed_scroll',F),('variable_scroll',F),
              ('movement_ready',I),('once_flags',C.c_uint8),('meter_x',F),('meter_once_flags',C.c_uint8)]
class Timing(C.Structure):_fields_=[('start',F),('end',F),('source',F)]
class Bindings(C.Structure):
    _fields_=[('frame',C.POINTER(Frame)),('control',C.POINTER(Control)),
              ('auxiliary',C.POINTER(Auxiliary)),('stage3_state',C.POINTER(I)),
              ('active_clip',C.POINTER(I)),('points',C.POINTER(Pair)),
              ('choices',C.POINTER(I)),('targets',C.POINTER(Pair)),
              ('target_count',C.c_size_t),('alternate',C.POINTER(I)),
              ('gauge_y',C.POINTER(F)),('active_timing',C.POINTER(Timing))]
def bind_hints(lib):
    bind(lib)
    lib.bk_ending_ui_hints_start.argtypes=[C.POINTER(State),F,C.c_void_p]
    lib.bk_ending_ui_hints.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.POINTER(State),
                                    C.POINTER(Bindings),C.POINTER(F),C.POINTER(F),
                                    F,F,C.POINTER(DrawFrame),C.c_void_p]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--prefix',action='store_true')
    args=ap.parse_args();exe=args.exe.read_bytes();n=Native(exe);lib=library();bind_hints(lib)
    lib.bk_ending_ui_toolbar.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.POINTER(ToolbarBindings),
                                      C.POINTER(F),F,F,C.POINTER(m.Ops),C.POINTER(C.c_uint8),C.POINTER(DrawFrame),C.c_void_p]
    def native_key(u,address,size,_):
        sp=u.reg_read(UC_X86_REG_ESP);code,mode,extra=struct.unpack('<3I',u.mem_read(sp+4,12))
        assert mode==2 and code in [0,1] and extra==0
        n.keys.append((code,mode));u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,n.ri(sp))
    n.u.hook_add(UC_HOOK_CODE,native_key,begin=0x4b76c2,end=0x4b76c2)
    rng=random.Random(0x4d4ed8);e=C.create_string_buffer(256);frames=draws=vertices=0
    counts=[0]*4;ready=retained=skipped=meters=0
    keys=0
    for case in range(800 if args.prefix else 3200):
        kind=case%4+1;width=[320,640,961,1280][(case//4)%4];scale=F(width/1280).value
        base,stage=m.Ui(),Stage();flags=(C.c_uint8*6)();gauge=F(rng.uniform(-100,1000)*scale)
        unused=F();assert lib.bk_ending_ui_initialize(C.byref(base),width,flags,C.byref(unused),e)
        assert lib.bk_ending_stage_ui_initialize(C.byref(base),C.byref(stage),kind,case%5,case%2,width,e)
        install(n,base,stage)
        f,c,a=Frame(),Control(),Auxiliary()
        f.phase={1:2,2:5 if case%3 else 6,3:3,4:4}[kind]
        c.variant=1 if kind==1 else rng.choice([0,2,6,255])
        if case%19==0:f.phase=rng.choice([0,1,7,8,9,10])
        f.state_721ee4=3 if case%7 else 2;c.state_721eec=3 if case%7 else 2
        a.gate=3 if case%7 else 2;a.progress=rng.choice([-.1,0,.5,.99,1,1.2])
        gate=I(3 if case%7 else 2);clip=I(rng.choice([-1,0,1,2,3,6,7,9,10,11,17,18,20]))
        f.camera_event=rng.choice([0,1,1,2,2,3]);f.camera_cached=rng.choice([0,1,4,5,7])
        points=(Pair*3)(*[Pair(rng.randrange(100,700),rng.randrange(80,500)) for _ in range(3)])
        choices=(I*3)(*[rng.choice([-1,-1,0,1,2,3,4,5,6,255]) for _ in range(3)])
        if case%11==0:choices[:]=[-1]*3
        if case%13==0:points[1][:]=points[0][:];points[2][:]=points[0][:]
        targets=(Pair*8)(*[Pair(rng.randrange(-100,1000),rng.randrange(-100,900)) for _ in range(8)])
        alternate=Pair(rng.randrange(-100,1000),rng.randrange(-100,900))
        pointer=(F*2)(*points[0]);motion=(F*2)(rng.uniform(-300,300),rng.uniform(-300,300))
        if case%5==0:pointer[0]+=base.sprites[51].rect[2]/2
        s=State(rng.uniform(0,500)*scale,rng.choice([.001,.2,.998]),rng.choice([.001,.3,.998]),-33,rng.choice([0,1,128,129,254,255]))
        s.meter_x=rng.uniform(-100,1400);s.meter_once_flags=rng.choice([0,1,128,129,254,255])
        timing=Timing(rng.uniform(0,1000),1000,0);timing.source=timing.start+rng.choice([-10,0,.1,10,20,100])
        dt=F(rng.choice([0,1/60,.1,.5])).value
        for name,addr in FIELDS:n.wi(addr,getattr(f,name))
        for name,addr in BYTES:n.u.mem_write(addr,bytes([getattr(f,name)]))
        n.u.mem_write(0x721b3d,bytes([c.variant]));n.wi(0x721eec,c.state_721eec)
        n.wi(0x721ef0,a.gate);n.wi(0x721ee8,gate.value);n.wf(0x721e20,a.progress);n.wf(0x721e24,gauge.value)
        n.wi(0x721b28,n.actor);n.wi(n.actor+0x140,clip.value)
        if clip.value>=0:n.wf(n.actor+0x190+clip.value*156+0x54,timing.start);n.wf(n.actor+0x190+clip.value*156+0x60,timing.source)
        for addr,values in [(0x7220c8,points),(0x7220e4,choices),(0x721f90,targets),(0x6afd38,alternate)]:n.u.mem_write(addr,bytes(values))
        n.wf(0x719b18,s.length);n.wf(0x6bbe44,s.fixed_scroll);n.wf(0x6afd28,s.variable_scroll)
        n.wi(0x6afd14,s.movement_ready);n.u.mem_write(0x70c8cc,bytes([s.once_flags]))
        n.wf(0x6e9fa4,s.meter_x);n.u.mem_write(0x6ddea4,bytes([s.meter_once_flags]))
        n.wf(0x721ad0,scale);n.wf(0x733700,dt)
        # Do not cache this same start address with two different Unicorn
        # stop boundaries. The prefix run executes first-use initialization
        # once as part of the actual caller, then compares its final state.
        if not args.prefix:n.segment(0x4d49a4,0x4d49d3)
        assert lib.bk_ending_ui_hints_start(C.byref(s),scale,e)
        if not args.prefix:assert s.length==n.rf(0x719b18) and s.once_flags==n.u.mem_read(0x70c8cc,1)[0]
        n.u.mem_write(n.stack-4,bytes(4));n.wf(n.stack-0x124,pointer[0]);n.wf(n.stack-0x128,pointer[1])
        n.wf(n.stack-0x110,motion[0]);n.wf(n.stack-0x114,motion[1]);n.draws=[]
        opened=C.c_int32(0)
        if args.prefix:
            n.wi(0x70c8bc,c.hover);n.wi(0x721ec4,c.mode_721ec4);n.wi(0x72210c,opened.value)
            n.u.mem_write(0x7220f8,bytes(c.toggles))
            n.wi(0x709964,int(pointer[0]));n.wi(0x709968,int(pointer[1]))
            n.wi(0x709974,40);n.wi(0x709978,50)
            n.wi(0x70996c,0 if case%3==0 else int(motion[0]));n.wi(0x709970,0 if case%3==0 else int(motion[1]))
            n.keys=[];n.segment(0x4d49a4,0x4d5e60)
            pointer[:]=[n.rf(n.stack-0x124),n.rf(n.stack-0x128)]
            motion[:]=[n.rf(n.stack-0x110),n.rf(n.stack-0x114)]
        else:n.segment(0x4d4ed8,0x4d5e60)
        b=Bindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(gate),C.pointer(clip),points,choices,targets,8,alternate,C.pointer(gauge),C.pointer(timing))
        out=DrawFrame()
        if args.prefix:
            calls=[]
            @m.KEY
            def key(_,code,mode,value,err):calls.append((code,mode));value[0]=0;return 1
            tb=ToolbarBindings(C.pointer(f),C.pointer(c),C.pointer(a),C.pointer(opened));visible=C.c_uint8(99)
            assert lib.bk_ending_ui_toolbar(C.byref(base),C.byref(stage),C.byref(tb),pointer,scale,dt,
                                            C.byref(m.Ops(None,key)),C.byref(visible),C.byref(out),e),e.value
            assert calls==n.keys and visible.value==n.u.mem_read(n.stack-8,1)[0]
            assert opened.value==C.c_int32(n.ri(0x72210c)).value;keys+=len(calls)
        assert lib.bk_ending_ui_hints(C.byref(base),C.byref(stage),C.byref(s),C.byref(b),pointer,motion,scale,dt,C.byref(out),e), (case,e.value)
        assert (s.length,s.fixed_scroll,s.variable_scroll,s.movement_ready)==(n.rf(0x719b18),n.rf(0x6bbe44),n.rf(0x6afd28),I(n.ri(0x6afd14)).value),(case,list(motion),s.length,n.rf(0x719b18))
        assert s.once_flags==n.u.mem_read(0x70c8cc,1)[0]
        assert s.meter_x==n.rf(0x6e9fa4) and s.meter_once_flags==n.u.mem_read(0x6ddea4,1)[0]
        assert out.count==len(n.draws),(case,out.count,len(n.draws))
        for i,(slot,sp,raw) in enumerate(n.draws):
            d=out.draws[i];assert d.slot==slot and d.alpha==sp.alpha and tuple(d.uv)==tuple(sp.uv)
            for j,k in enumerate([0,1,2,3,0,2]):
                assert tuple(d.xy[2*k:2*k+2])==struct.unpack_from('<2f',raw,32*j),(case,slot,i,j)
                vertices+=1
        same(n,base,stage,('hints',case))
        meters+=sum(d.slot==71 for d in out.draws[:out.count])
        frames+=1;draws+=out.count;counts[kind-1]+=out.count>2
        ready+=s.movement_ready==1;retained+=s.movement_ready==-33;skipped+=all(v==-1 for v in choices)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,draws=draws,vertices=vertices,
                profiles_with_hint_draws=counts,movement_ready_frames=ready,retained_ready_frames=retained,
                all_points_skipped_frames=skipped,meter_draws=meters,key_calls=keys,whole_prefix=args.prefix,max_error=0,
                native_segment='0x4d49a4..0x4d5e60' if args.prefix else '0x4d4ed8..0x4d5e60',
                scope=__doc__+ (' Whole prefix includes actual pointer smoothing/capture and toolbar; port consumes captured float pointer/motion.' if args.prefix else ''))
    filename='original-ending-ui-prefix-oracle.json' if args.prefix else 'original-ending-ui-hints-oracle.json'
    (ROOT/'local'/filename).write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
