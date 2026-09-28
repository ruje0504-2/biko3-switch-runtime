"""Execute native main-flow camera dispatch; controller bodies are boundaries."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from original_matrix_oracle import machine
from model_binding import ROOT,library
class State(C.Structure):_fields_=[('phase',C.c_uint8),('transition',C.c_uint8),('stage',C.c_int32)]
class Native:
    routes={0x4bdc12:1,0x4b8a89:2,0x4bdfb0:3,0x4be290:4}
    boundaries=[0x4b26b8,0x4b2f41,0x4f737b,0x4c009b,0x4c1313,0x4bfea9,0x4fc8f9,0x4fc36d,0x5121ce,0x512102,0x4ef141,0x4ef095,0x4f4306,0x4f3c80,0x4f3d34,0x4f5c6a,0x4cc320]
    def __init__(self,exe):
        self.uc=machine(exe)
        for a in self.boundaries+list(self.routes):self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def hook(self,u,address,size,user):
        if address in self.routes:self.calls.append(self.routes[address])
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_EAX,self.complete if address in [0x4bdfb0,0x4be290] else 0)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,state,complete):
        self.complete=complete;self.calls=[]
        self.uc.mem_write(0x71ba88,bytes([state.phase]));self.uc.mem_write(0x729780,bytes([state.transition]));self.uc.mem_write(0xbef784,struct.pack('<i',state.stage))
        self.uc.mem_write(0x2008000,struct.pack('<I',0x300f000));self.uc.reg_write(UC_X86_REG_ESP,0x2008000)
        self.uc.emu_start(0x51a682,0x300f000,count=100000);assert self.uc.reg_read(UC_X86_REG_EIP)==0x300f000
        assert len(self.calls)<=1
        return self.calls[0] if self.calls else 0,struct.unpack('<i',self.uc.mem_read(0xbef784,4))[0]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args();exe=args.exe.read_bytes();native=Native(exe);lib=library()
    lib.bk_game_camera_route.argtypes=[C.POINTER(State)];lib.bk_game_camera_route.restype=C.c_int
    lib.bk_game_camera_finish.argtypes=[C.POINTER(State),C.c_int];lib.bk_game_camera_finish.restype=C.c_int
    checks=0;counts=[0]*5
    for phase in range(256):
        for stage in [-1,0,1,2,3,2147483647]:
            for transition in [0,1,2,255]:
                for complete in [0,1]:
                    state=State(phase,transition,stage);route,wanted=native.run(state,complete)
                    assert lib.bk_game_camera_route(C.byref(state))==route,(phase,stage,transition)
                    assert lib.bk_game_camera_finish(C.byref(state),complete) and state.stage==wanted
                    counts[route]+=1;checks+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=checks,branch_counts=counts,native_functions=['0x51a682','0x4ec78d'],hooks=[hex(a) for a in native.boundaries+list(native.routes)],scope='Original branch selection and camera-stage completion write. World/AI/UI/controller bodies replaced at boundaries; pose math independently checked by controller oracles.')
    (ROOT/'local/original-camera-policy-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',checks,'camera dispatches',counts)
if __name__=='__main__':main()
