"""Native 4f75f3/4f834a/4f8b61 and original resource tables vs 45 port profiles.
Only SetRect and the sound-loader service boundary are supplied by the fixture.
"""
import argparse, ctypes as C, hashlib, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from original_prop_route_oracle import Native as Base
from model_binding import ROOT, library

class Ambient(C.Structure):
    _fields_ = [('file', C.c_char_p), ('position', C.c_float*3),
                ('trigger', C.c_float), ('loop_mode', C.c_int32),
                ('uses_effect_volume', C.c_int)]
class Config(C.Structure):
    _fields_ = [('clip', C.c_char_p), ('atr', C.c_char_p), ('music', C.c_char_p),
                ('names', C.c_char_p*3), ('bounds', C.c_int32*4),
                ('ambient', Ambient*8)]
class Native(Base):
    rect = 0x300d000
    def __init__(self, exe):
        super().__init__(exe)
        self.word(0x53f29c, self.rect)
        for p in [self.rect, 0x50d858]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=p, end=p)
    def word(self, a, v): self.u.mem_write(a, struct.pack('<I', v & 0xffffffff))
    def string(self, a): return bytes(self.u.mem_read(a, 256)).split(b'\0')[0]
    def hook(self, u, a, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, *args = struct.unpack('<6I', u.mem_read(sp, 24))
        clean = 4
        if a == self.rect:
            u.mem_write(args[0], struct.pack('<4I', *args[1:]))
            clean = 24
        else:
            i = (args[0] - self.actor - 0x628) // 0x120
            assert 0 <= i < 8
            self.ambient[i] = (self.string(args[2]).split(b'\\')[-1],
                C.c_int32(args[3]).value, args[4],
                struct.unpack('<4f', u.mem_read(args[0]+0x100, 16)))
        u.reg_write(UC_X86_REG_EAX, 1)
        u.reg_write(UC_X86_REG_ESP, sp+clean)
        u.reg_write(UC_X86_REG_EIP, ret)
    def profile(self, g, a):
        self.u.mem_write(self.actor, bytes(0x1000))
        self.u.mem_write(0x5767c8, b'\1')
        self.word(0xbe9a10, -1234)
        self.ambient = {}
        for fn in [0x4f75f3, 0x4f834a, 0x4f8b61]:
            self.call(fn, struct.pack('<3I', self.actor, g, a))

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    args = ap.parse_args()
    exe = args.exe.read_bytes()
    n, lib = Native(exe), library()
    lib.bk_background_config.argtypes = [C.c_uint32, C.c_uint32]
    lib.bk_background_config.restype = C.POINTER(Config)
    slots = 0
    for g in range(5):
        for a in range(9):
            n.profile(g, a)
            c = lib.bk_background_config(g, a).contents
            for field, address in [('clip', 0x57ab18), ('atr', 0x57d818), ('music', 0x580518)]:
                assert getattr(c, field) == n.string(address+g*0x900+a*256).split(b'\\')[-1], (g,a,field)
            assert list(c.bounds) == list(struct.unpack('<4i', n.u.mem_read(n.actor+0x208,16)))
            assert list(c.names) == [n.string(n.actor+off) or None for off in [0x218,0x318,0x418]]
            for i, ambient in enumerate(c.ambient):
                if i not in n.ambient:
                    assert not ambient.file
                    continue
                name, volume, loop, source = n.ambient[i]
                assert ambient.file == name and ambient.loop_mode == loop
                assert list(ambient.position)+[ambient.trigger] == list(source)
                assert ambient.uses_effect_volume == (volume == -1234)
                assert volume in [-1234,-6000]
                slots += 1
    for g, a in [(5,0),(0,9),(0xffffffff,0),(0,0xffffffff)]:
        assert not lib.bk_background_config(g,a)
    result = dict(passed=True, profiles=45, ambient_slots=slots,
                  exe_sha256=hashlib.sha256(exe).hexdigest(), scope=__doc__)
    (ROOT/'local/original-background-config-oracle.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result), flush=True)
if __name__ == '__main__': main()
