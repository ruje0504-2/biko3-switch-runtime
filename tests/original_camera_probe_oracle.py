"""Coupled original 4b9175 orbit -> 4b4f31 wall queries retain camera+0x524.
The wall code reads the preceding controller's globals; its probe is never
injected. This covers the application binding missed by isolated controllers.
Synthetic walls deliberately avoid native singular intersection geometry.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_player_view_oracle import Native, State, Input, Flags, Pose, Effects, Vec, I
from original_player_wall_oracle import Wall, Input as WallInput, Triangle
from model_binding import ROOT, library

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    raw=args.exe.read_bytes();vm=Native(raw);lib=library();error=C.create_string_buffer(256)
    lib.bk_player_view_step.argtypes=[C.POINTER(State),C.c_int,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
    lib.bk_player_view_collision_query.argtypes=[C.POINTER(State),C.POINTER(WallInput)]
    lib.bk_player_wall_triangle.argtypes=[C.POINTER(Wall),C.POINTER(C.c_int),C.POINTER(WallInput),Triangle,C.c_void_p]
    rng=random.Random(0x71b45c);actions=(C.c_int32*21)(*range(21));records=[];worst=0.;frames=0
    def view_step(s,inp):
        out=Effects();actual=State.from_buffer_copy(s)
        expected,*_=vm.step(s,inp,Flags(0,0),0,0,0,actions,rng)
        assert lib.bk_player_view_step(C.byref(actual),1,C.byref(inp),C.byref(out),error),error.value
        return actual,expected
    def walls(s,inp,triangles,wrong=False):
        wall=Wall(position=inp.position,camera_distance=s.target_distance)
        wi=WallInput(previous=inp.position,height=18)
        assert lib.bk_player_view_collision_query(C.byref(s),C.byref(wi))
        assert list(wi.camera)==list(s.probe)
        if wrong:wi.camera[:]=s.pose.world[12:15]
        near=C.c_int()
        for tri in triangles:
            assert lib.bk_player_wall_triangle(C.byref(wall),C.byref(near),C.byref(wi),tri,error),error.value
        assert not wall.singular_camera
        return wall.camera_distance
    def native_walls(inp,triangles):
        u=vm.u;dest=0x300c000
        u.mem_write(dest,bytes(inp.position)+bytes(4));u.mem_write(0x71b7d8,bytes(inp.position));u.mem_write(0x71b7c4,bytes(16))
        # Do NOT write 0x71b45c: use actual original orbit's retained output.
        for tri in triangles:
            payload=struct.pack('<II',vm.stop,dest)+b''.join(bytes(v)+bytes(4) for v in tri)+struct.pack('<f',18)
            u.mem_write(vm.stack-8192,bytes(8192));u.mem_write(vm.stack,payload)
            u.reg_write(UC_X86_REG_ESP,vm.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
            u.emu_start(0x4b4f31,vm.stop,count=300000);assert u.reg_read(UC_X86_REG_EIP)==vm.stop
        return struct.unpack('<f',u.mem_read(0x71b374,4))[0]
    for hz in [15,30,53,60]:
      for pitch in [-60,20,60]:
        inp=Input(position=Vec(5,0,0),vertical=18,yaw=25,pitch=pitch,npc_position=Vec(5,0,50),npc_height=18,npc_vertical=18,seconds=2/hz)
        seed=State(pose=Pose((C.c_float*16)(*I),Vec(5,18,-40)),target_distance=40,distance=40)
        triangles=[Triangle(Vec(-100,0,-10),Vec(-100,40,-10),Vec(100,0,-10)),Triangle(Vec(-100,40,-10),Vec(100,40,-10),Vec(100,0,-10))]
        actual,expected=view_step(seed,inp);bad=State.from_buffer_copy(actual)
        fixed_z=[];bad_z=[]
        for frame in range(400):
            desired=walls(actual,inp,triangles);native=native_walls(inp,triangles)
            assert abs(desired-native)<2e-6,(hz,pitch,frame,desired,native)
            actual.target_distance=desired;expected.target_distance=native
            # Native state is independent across frames, port state independently sampled.
            next_native,*_=vm.step(expected,inp,Flags(0,0),0,0,0,actions,rng)
            effects=Effects();assert lib.bk_player_view_step(C.byref(actual),1,C.byref(inp),C.byref(effects),error)
            for a,b in zip([*actual.pose.world,actual.distance,*actual.probe],[*next_native.pose.world,next_native.distance,*next_native.probe]):
                d=abs(a-b)/max(1,abs(b));worst=max(worst,d);assert d<2e-6,(hz,pitch,frame,a,b)
            expected=next_native
            bad.target_distance=walls(bad,inp,triangles,True)
            assert lib.bk_player_view_step(C.byref(bad),1,C.byref(inp),C.byref(effects),error)
            if frame>=200:fixed_z.append(actual.pose.world[14]);bad_z.append(bad.pose.world[14])
            frames+=1
        fixed_span=max(fixed_z)-min(fixed_z);bad_span=max(bad_z)-min(bad_z)
        assert fixed_span<1e-5 and bad_span>.001,(hz,pitch,fixed_span,bad_span)
        records.append(dict(hz=hz,pitch=pitch,fixed_z_peak_to_peak=fixed_span,old_wrong_binding_z_peak_to_peak=bad_span))
    report=dict(passed=True,exe_sha256=hashlib.sha256(raw).hexdigest(),frames=frames,wall_calls=frames*2,maximum_relative_error=worst,records=records,scope=__doc__,native_probe_address='0x71af38+0x524=0x71b45c')
    (ROOT/'local/original-camera-probe-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
