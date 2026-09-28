"""Original4d6404..4d677b cursor/notice/popup ordering.
Selection is explicit input from the preceding (still separate) selector.
No meaning is assigned to uninitialized caller stack values. The known native
null-device write policy is counted separately for unloaded popup images.
"""
import argparse,ctypes as C,hashlib,json,random
from pathlib import Path
from original_ending_stage_ui_oracle import Native,Stage,bind,install,same
from original_detached_sprite_oracle import Frame as DrawFrame,null_write
from original_ending_frame_oracle import State as Frame,FIELDS,BYTES
from original_ending_auxiliary_oracle import State as Auxiliary
import original_ending_ui_oracle as m
from model_binding import ROOT,library
class Notices(C.Structure):_fields_=[('notices',C.c_uint8*4),('popups',C.c_uint8*2)]
class Bindings(C.Structure):
    _fields_=[('frame',C.POINTER(Frame)),('auxiliary',C.POINTER(Auxiliary)),
              ('active_clip',C.POINTER(C.c_int32)),('normal_ready',C.POINTER(C.c_int32)),
              ('notices',C.POINTER(Notices))]
def main():
    import struct
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path)
    exe=ap.parse_args().exe.read_bytes();n=Native(exe);null_write(n);lib=library();bind(lib)
    lib.bk_ending_ui_cursor.argtypes=[C.POINTER(m.Ui),C.POINTER(Stage),C.POINTER(Bindings),
                                     C.c_int32,C.c_uint8,C.POINTER(C.c_float),C.c_float,C.POINTER(DrawFrame),C.c_void_p]
    rng=random.Random(0x4d6404);e=C.create_string_buffer(256)
    frames=draws=vertices=policy=absent=0
    for case in range(2400):
        base,stage=m.Ui(),Stage();flags=(C.c_uint8*6)();gauge=C.c_float()
        assert lib.bk_ending_ui_initialize(C.byref(base),rng.choice([640,961,1280]),flags,C.byref(gauge),e)
        kind=2 if case%5 in [1,3] else 3
        if case%5:
            assert lib.bk_ending_stage_ui_initialize(C.byref(base),C.byref(stage),kind,case%5,case%2,1280,e)
            if case%5>=3:assert lib.bk_ending_stage_ui_release(C.byref(base),C.byref(stage),kind,e)
        for slot in list(range(9))+[53,54,55,56,72,73]:
            p=base.sprites[slot] if slot<63 else stage.sprites[slot-63]
            p.transform.fade.stage=rng.randrange(6);p.transform.fade.alpha=rng.random()
            p.transform.fade.speed=rng.choice([0,.5,2])
        f,a=Frame(),Auxiliary();f.phase=case%12-1;f.state_721ee0=rng.choice([0,1,3,4])
        f.camera_cached=rng.choice([-1,0,4,11,12]);f.camera_event=rng.choice([0,1,2]);a.gate=rng.choice([0,1,2,3,4,5])
        clip=C.c_int32(rng.choice([-1,0,1,2,3,7]));ready=C.c_int32(rng.choice([0,1,-1]))
        notices=Notices((C.c_uint8*4)(*[rng.choice([0,1,2,255]) for _ in range(4)]),
                        (C.c_uint8*2)(*[rng.choice([0,1,2,255]) for _ in range(2)]))
        selected=rng.choice([-1,*range(9),10,99]);visible=rng.choice([0,0,1,2,255])
        point=(C.c_float*2)(rng.uniform(-100,1400),rng.uniform(-100,1000));dt=C.c_float(rng.choice([0,1/60,.1,1])).value
        install(n,base,stage)
        for name,addr in FIELDS:n.wi(addr,getattr(f,name))
        for name,addr in BYTES:n.u.mem_write(addr,bytes([getattr(f,name)]))
        n.wi(0x721ef0,a.gate);n.wi(0x719b0c,ready.value);n.wi(0x721b28,n.actor);n.wi(n.actor+0x140,clip.value)
        for j in range(4):n.u.mem_write(m.BASE+(53+j)*0x16c+0x167,bytes([notices.notices[j]]))
        for j in range(2):n.u.mem_write(m.BASE+(72+j)*0x16c+0x167,bytes([notices.popups[j]]))
        n.wf(0x733700,dt);n.wi(n.stack-0x11c,selected);n.u.mem_write(n.stack-8,bytes([visible]))
        n.wf(n.stack-0x124,point[0]);n.wf(n.stack-0x128,point[1]);n.draws=[];n.null_calls=[]
        n.segment(0x4d6404,0x4d677b)
        b=Bindings(C.pointer(f),C.pointer(a),C.pointer(clip),C.pointer(ready),C.pointer(notices));out=DrawFrame()
        assert lib.bk_ending_ui_cursor(C.byref(base),C.byref(stage),C.byref(b),selected,visible,point,dt,C.byref(out),e), (case,e.value)
        same(n,base,stage,('cursor',case));assert out.count==len(n.draws)
        for i,(slot,sp,raw) in enumerate(n.draws):
            d=out.draws[i];assert d.slot==slot and d.alpha==sp.alpha and tuple(d.uv)==tuple(sp.uv)
            for j,k in enumerate([0,1,2,3,0,2]):
                assert tuple(d.xy[2*k:2*k+2])==struct.unpack_from('<2f',raw,j*32)
                vertices+=1
        frames+=1;draws+=out.count;policy+=bool(n.null_calls);absent+=not bool(base.loaded&(1<<8))
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),frames=frames,draws=draws,vertices=vertices,
                null_device_policy_frames=policy,absent_cursor8_frames=absent,max_error=0,scope=__doc__)
    (ROOT/'local/original-ending-ui-cursor-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)
if __name__=='__main__':main()
