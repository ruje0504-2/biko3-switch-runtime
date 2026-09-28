"""Native entry flow precedence, cursor overrides, routes and spawn table.

Full 0x4ebfd0 runs with UI/dialogue loaders and clip reset at boundaries.
Cursor/spawn selection runs original instruction ranges with explicit state.
"""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_route_oracle import Native as RouteNative
from model_binding import ROOT,library
class Request(C.Structure):_fields_=[('group',C.c_uint32),('area',C.c_uint32),('route_cursor',C.c_uint32),('previous_flow',C.c_uint8)]
class Selection(C.Structure):
    _fields_=[('route_file',C.c_char_p),('actor_clip',C.c_char_p),('head_node',C.c_char_p),('camera_clip',C.c_char_p),('player_position',C.c_float*3),('player_yaw',C.c_float),('route_cursor',C.c_uint32),('phase',C.c_uint8),('dialogue',C.c_uint8),('actor_fade_out',C.c_uint8)]
class Native(RouteNative):
    hooks=[0x401d24,0x517640,0x51765d,0x4aa32e,0x517c8e,0x4aa9f4,0x50e633]
    def __init__(self,exe,directory):
        super().__init__(exe,directory)
        for address in self.hooks:self.uc.hook_add(UC_HOOK_CODE,self.service,begin=address,end=address)
    def service(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        if address==0x401d24:assert struct.unpack('<I',u.mem_read(sp+8,4))[0]==0
        u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self,start,end):
        self.uc.reg_write(UC_X86_REG_ESP,self.stack-0x4000);self.uc.reg_write(UC_X86_REG_EBP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(start,end,count=200000);assert self.uc.reg_read(UC_X86_REG_EIP)==end
    def flow(self,group,area,previous):
        self.word(0x7219a8,group);self.word(0x7219ac,area);self.uc.mem_write(0x721ad4,bytes([previous]))
        for a in [0x71ba88,0x729360,0xbeecef]:self.uc.mem_write(a,b'\x7f')
        self.call(0x4ebfd0,b'')
        phase=self.uc.mem_read(0x71ba88,1)[0];assert phase==self.uc.mem_read(0x729360,1)[0]
        return phase,self.uc.mem_read(0xbeecef,1)[0]
    def cursor(self,group,area,previous,cursor):
        actor=0x3000000;self.uc.mem_write(actor,bytes(0x900));self.word(0x7219a8,group);self.uc.mem_write(0x721ad4,bytes([previous]))
        self.word(0xbf3c8c+group*108+area*12,cursor)
        self.uc.mem_write(self.stack+8,struct.pack('<3I',actor,group,area));self.run(0x4fb73e,0x4fb822)
        return struct.unpack('<I',self.uc.mem_read(actor+0x830,4))[0],self.uc.mem_read(actor+0x331,1)[0]
    def spawn(self,group,area):
        self.word(0x7219a8,group);self.word(0x7219ac,area);self.run(0x4e9538,0x4e95b4)
        return [struct.unpack('<f',self.uc.mem_read(self.stack+o,4))[0] for o in [-8,-16,-4,-12]]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe,args.data);lib=library();lib.bk_game_entry_select.argtypes=[C.POINTER(Selection),C.POINTER(Request)];lib.bk_game_entry_select.restype=C.c_int
    count=0;rows=[]
    for group in range(5):
        head_pointer=struct.unpack('<I',native.uc.mem_read(0x5848d0+group*4,4))[0];head=native.string(head_pointer)
        for area in range(9):
            route=native.select(group,area,1)[0];spawn=native.spawn(group,area)
            for previous in range(256):
                cursor=(previous*7+area*13+group)%1024;r=Request(group,area,cursor,previous);selection=Selection()
                assert lib.bk_game_entry_select(C.byref(selection),C.byref(r))
                assert (selection.phase,selection.dialogue)==native.flow(group,area,previous),(group,area,previous)
                assert (selection.route_cursor,selection.actor_fade_out)==native.cursor(group,area,previous,cursor)
                assert selection.route_file.decode()==route and selection.head_node.decode()==head
                native.call(0x4bef54,b'');variant=native.uc.reg_read(UC_X86_REG_EAX)
                assert selection.camera_clip.decode()==native.string(0x55a448+variant*16)
                assert list(selection.player_position)+[selection.player_yaw]==spawn
                count+=1
            rows.append(dict(group=group,area=area,route=route,head=head,player_spawn=spawn))
    from original_aim_oracle import Pose,I
    lib.bk_game_entry_camera_pose.argtypes=[C.POINTER(Pose),C.POINTER(C.c_float)];lib.bk_game_entry_camera_pose.restype=C.c_int
    def vector(a,v):native.uc.mem_write(a,struct.pack('<'+'f'*len(v),*v))
    for case in range(256):
        controller,frame,context=0x3004000,0x3005000,0x3006000
        for node in [frame,context]:
            native.uc.mem_write(node,bytes(0x400))
            for off in [0x80,0xc0,0x100]:vector(node+off,I)
        native.word(0x645600,context);native.word(0x645604,frame);native.word(native.stack+8,controller)
        native.run(0x4b7d50,0x4b7e38)
        origin=(C.c_float*3)(case*.37,-case*.11,case-100);vector(0x729084,origin)
        native.run(0x4ec25d,0x4ec27f)
        pose=Pose();assert lib.bk_game_entry_camera_pose(C.byref(pose),origin)
        assert list(pose.world)==list(struct.unpack('<16f',native.uc.mem_read(frame+0xc0,64)))
        # This instruction range writes the real global controller at 71af38.
        assert list(pose.position)==list(struct.unpack('<3f',native.uc.mem_read(0x71b358,12)))
    selection=Selection();C.memset(C.byref(selection),0x55,C.sizeof(selection));saved=bytes(selection)
    for r in [Request(5,0,0,0),Request(0,9,0,0),Request(0,0,1024,0),Request(0xffffffff,0,0,0)]:
        assert not lib.bk_game_entry_select(C.byref(selection),C.byref(r));assert bytes(selection)==saved
    assert not lib.bk_game_entry_select(C.byref(selection),None)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),entry_cases=count,camera_initializations=256,profiles=rows,hooks=[hex(a) for a in native.hooks]+['0x4ad8ec path prefix','Windows file APIs'],native_functions=['0x4ebfd0','0x4fb73e..0x4fb822','0x4e9538..0x4e95b4','0x4ff78e'],scope='Full entry phase precedence for every byte value, explicit cursor overrides, route names/head bindings/player spawn metadata. Does not decode saves, execute dialogue/UI or prove initial global cursor contents.')
    (ROOT/'local/original-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',count,'entry selections')
if __name__=='__main__':main()
