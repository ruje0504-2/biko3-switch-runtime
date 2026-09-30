"""Check production loader scalar results against the pinned original EXE.

Reads SELECTED_LOAD rows from the real Japanese resource/session probe. Runs
4D152C..4D15BF,4D194D..4D19B9,4D1A77..4D1A81,4D1C07..4D1CC1. The native
memset/strcpy execute unchanged; only4D460B background IO is observed/stubbed.
Resource identity/lifetime, opening and PCM are checked by the C probe, not
claimed by this isolated scalar replay. Parent721EE0/721EE4 must stay intact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_prop_route_oracle import Native as Base


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.u.hook_add(UC_HOOK_CODE, self.background, begin=0x4D460B, end=0x4D460B)

    def word(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def read(self, address):
        return struct.unpack('<i', self.u.mem_read(address, 4))[0]

    def background(self, u, address, size, context):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, name = struct.unpack('<2I', u.mem_read(sp, 8))
        assert bytes(u.mem_read(name, 32)).split(b'\0')[0] == b'\\m02_90.xan'
        self.replaced += 1
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, ret)

    def segment(self, start, end):
        self.u.reg_write(UC_X86_REG_EBP, self.stack)
        self.u.reg_write(UC_X86_REG_ESP, self.stack - 0x900)
        self.u.emu_start(start, end, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == end

    def run(self, before):
        group, variant, arg, selected, gate, main, secondary, event, target, action, oa, ob = before
        self.replaced = 0
        for address, value in [(self.stack + 8, arg), (0x721E04, variant),
                               (0x721ED8, selected), (0x721EF0, gate),
                               (0x721EE0, main), (0x721EE4, secondary),
                               (0x7220E0, event), (0x721ECC, target),
                               (0x721E00, -123), (0x722100, 73), (0x721E08, 94)]:
            self.word(address, value)
        for address, value in [(0x721B3C, group), (0x721B3D, action),
                               (0x71BCDD, oa), (0x71BCDE, ob)]:
            self.u.mem_write(address, bytes([value & 255]))
        self.segment(0x4D152C, 0x4D15BF)
        self.segment(0x4D194D, 0x4D19B9)
        self.segment(0x4D1A77, 0x4D1A81)
        self.segment(0x4D1C07, 0x4D1CC1)
        words = [self.read(a) for a in [0x721E00, 0x721ED8, 0x721EF0,
                 0x721EE0, 0x721EE4, 0x7220E0, 0x721ECC]]
        words.append(self.u.mem_read(0x721B3D, 1)[0])
        words.extend(self.read(a) for a in [0x7220F4, 0x722100, 0x721E08])
        words.extend(self.u.mem_read(0x7220FB, 4))
        words.append(self.replaced)
        return words


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('logs', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native = Native(exe)
    checks = []
    for path in args.logs:
        raw = path.read_bytes()
        text = raw.decode()
        assert 'PASS selected session entries70 ' in text, 'incomplete real-asset session run'
        cases = [list(map(int, line.split()[1:])) for line in text.splitlines()
                 if line.startswith('SELECTED_LOAD ')]
        assert len(cases) == 60
        groups, variants, arguments, choices = set(), set(), set(), set()
        for row in cases:
            assert len(row) == 28
            before, actual = row[:12], row[12:]
            wanted = native.run(before)
            assert actual == wanted, (path, before, actual, wanted)
            groups.add(before[0]); variants.add(before[1])
            arguments.add(before[2]); choices.add(before[3])
        assert groups == set(range(5)) and variants == {0, 1}
        assert arguments == {0, 1, 2} and choices == {0, 2, 3, 4, 6}
        checks.append({'log': str(path), 'sha256': hashlib.sha256(raw).hexdigest(),
                       'loads': len(cases), 'scalar_words': len(cases) * 16})
    report = {'passed': True, 'exe_sha256': hashlib.sha256(exe).hexdigest(),
              'checks': checks, 'scope': __doc__, 'hooks': ['4D460B background IO']}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
