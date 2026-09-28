"""Failure4eb0ee resource tables/order and4bf7db represented player reset.
Original x86 table construction and CPU stores execute; resource allocation,
path expansion, font binding and DirectSound are captured service boundaries.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_prop_route_oracle import Native as Base
from original_actor_entry_oracle import Player, PF, populate, transfer, compare
from model_binding import ROOT, library
class Resources(C.Structure):
    _fields_ = [('speech', C.c_char_p), ('message', C.c_char_p), ('label', C.c_int32)]
class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        for a in [0x4bfdd6, 0x4bf7db, 0x4ff69a, 0x4aaa29, 0x517ba9, 0x4ad8ec, 0x51765d, 0x4aa32e, 0x51876a, 0x4aa9f4]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=a, end=a)
    def string(self, address):
        return bytes(self.u.mem_read(address, 260)).split(b'\0')[0].decode('ascii') if address else ''
    def hook(self, u, a, size, _):
        sp=u.reg_read(UC_X86_REG_ESP);ret,*args=struct.unpack('<9I',u.mem_read(sp,36))
        if a == 0x4ad8ec:
            value=self.string(args[1])+self.string(args[2]);u.mem_write(args[0],value.encode()+b'\0')
        elif a == 0x4ff69a: self.events.append(('speech', args[0], self.string(args[1])))
        elif a == 0x51765d: self.events.append(('message', self.string(args[0]), self.string(args[1]), args[2]))
        elif a == 0x4aa32e: self.events.append(('font', self.string(args[0]), *args[1:]))
        elif a == 0x51876a: self.events.append(('lookup', args[0], args[1]))
        elif a == 0x4aa9f4: self.events.append(('bind', args[0]))
        elif a == 0x517ba9: self.events.append(('close', args[0]))
        elif a == 0x4aaa29: self.events.append(('clear-font',))
        else:
            self.events.append(('release-player' if a == 0x4bfdd6 else 'load-player',args[0]))
            if a == 0x4bf7db: u.mem_write(0x71bcd8,b'\0')
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def run(self, group, outcome):
        self.events=[];u=self.u
        u.mem_write(0x7219a8,struct.pack('<I',group));u.mem_write(0x71bcd8,bytes([outcome]));u.mem_write(0x5767c8,b'\1')
        for a in [0x71b840,0x729118]: u.mem_write(a,b'\xff'*8)
        u.mem_write(0xbef71c,struct.pack('<2I',99,99))
        self.call(0x4eb0ee,b'')
        assert u.mem_read(0x71bcd8,1)[0]==outcome
        for a in [0x71b840,0x729118]: assert bytes(u.mem_read(a,8))==bytes(8)
        return self.events

def run_range(n,start,end):
    u=n.u;u.reg_write(UC_X86_REG_ESP,n.stack-0x4000);u.reg_write(UC_X86_REG_EBP,n.stack);u.reg_write(UC_X86_REG_FPCW,0x037f)
    u.emu_start(start,end,count=200000);assert u.reg_read(UC_X86_REG_EIP)==end

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();digest=hashlib.sha256(exe).hexdigest()
    assert digest=='a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    n,lib,rng=Native(exe),library(),random.Random(0x4eb0ee)
    lib.bk_failure_resources.argtypes=[C.c_uint32,C.c_uint8,C.POINTER(Resources)]
    lib.bk_player_failure_reset.argtypes=[C.POINTER(Player),C.POINTER(C.c_int32),C.c_float]
    for group in range(5):
        for outcome in range(1,7):
            out=Resources();assert lib.bk_failure_resources(group,outcome,C.byref(out))
            events=n.run(group,outcome)
            expected=[('release-player',0x71b510),('load-player',0x71b510),('speech',0x728de8,out.speech.decode()),('clear-font',),('close',0xb54760),('close',0xbe8f48),('message','\\bk3_05.pp',out.message.decode(),0xb54760),('font','\\Type_S.FTT',104,380,432,72,16,18,0xffffffff),('lookup',out.label,0xb54760),('bind',0xb54878)]
            assert events==expected,(group,outcome,events,expected)
    cpu=Base(exe);u=cpu.u;head=0x300d000;player=0x71b510;worst=0
    for case in range(2400):
        s=Player();populate(s,rng);expected=Player.from_buffer_copy(s)
        slots=(C.c_int32*21)(*(rng.randrange(-100,100) for _ in range(21)))
        transfer(u,player,s,PF,True);u.mem_write(player+0x14,bytes(slots));u.mem_write(cpu.stack+8,struct.pack('<I',player))
        outcome=s.completion_requested
        # Constructor resource/shadow stages don't write represented fields.
        run_range(cpu,0x4bf80f,0x4bf9e3)
        height=C.c_float(rng.uniform(-100,100)).value;u.mem_write(player+8,struct.pack('<I',head));u.mem_write(head+0xf4,struct.pack('<f',height))
        run_range(cpu,0x4bfbbf,0x4bfbef)
        transfer(u,player,expected,PF,False);expected.completion_requested=outcome
        assert lib.bk_player_failure_reset(C.byref(s),slots,height)
        worst=max(worst,compare(s,expected));assert bytes(slots)==bytes(u.mem_read(player+0x14,84))
    old=bytes(s),bytes(slots)
    assert not lib.bk_player_failure_reset(C.byref(s),slots,float('nan'))
    assert old==(bytes(s),bytes(slots))
    report=dict(passed=True,exe_sha256=digest,resource_profiles=30,player_resets=2400,max_error=worst,scope=__doc__)
    (ROOT/'local/original-failure-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
