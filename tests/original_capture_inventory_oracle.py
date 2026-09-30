"""Compare4af5e1 name classification/counts to real flat-directory scans.

Only Windows string and directory-enumeration APIs are fixtures. CRT prefix
comparisons and all original count/cap instructions execute in the pinned EXE.
Missing directories have an explicit zero policy, not equivalence to the
original FindFirstFile failure's uninitialized filename. DOS8.3 wildcard
aliases and non-ASCII filesystem equivalence are outside this check.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
import tempfile

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from model_binding import ROOT, library
from original_prop_route_oracle import Native as Base


class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        for index, imp in enumerate([0x53f214, 0x53f21c, 0x53f104, 0x53f100]):
            function = 0x300d000 + index * 0x100
            self.u.mem_write(imp, struct.pack('<I', function))
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=function, end=function)

    def string(self, address):
        return bytes(self.u.mem_read(address, 260)).split(b'\0')[0]

    def hook(self, u, address, size, _):
        sp = u.reg_read(UC_X86_REG_ESP)
        ret, first, second = struct.unpack('<3I', u.mem_read(sp, 12))
        if address == 0x300d000:
            u.mem_write(first, self.string(second) + b'\0')
            result = first
        elif address == 0x300d100:
            u.mem_write(first, self.string(first) + self.string(second) + b'\0')
            result = first
        else:
            if address == 0x300d200:
                assert self.string(first) == b'album\\*.bmp'
                self.cursor = 0
            else:
                assert first == 123
                self.cursor += 1
            # A successful first enumeration with an ignored name represents
            # zero matching prefixes without emulating an uninitialized stack.
            name = self.names[self.cursor] if self.cursor < len(self.names) else None
            u.mem_write(second, bytes(320))
            if name is not None:
                u.mem_write(second, struct.pack('<I', 0x10 if name.endswith('_dir.bmp') else 0x20))
                u.mem_write(second + 44, name.encode('ascii') + b'\0')
            result = (123 if address == 0x300d200 else 1) if name is not None else 0
        u.reg_write(UC_X86_REG_EAX, result)
        u.reg_write(UC_X86_REG_ESP, sp + 12)
        u.reg_write(UC_X86_REG_EIP, ret)

    def run(self, names):
        self.names = [name for name in names if name.lower().endswith('.bmp')] or ['ignored.bmp']
        self.u.mem_write(0x3001000, b'album\0')
        self.u.mem_write(0x3001200, b'*.bmp\0')
        self.u.mem_write(0x3001400, bytes([0xa5]) * 20)
        self.call(0x4af5e1, struct.pack('<3I', 0x3001000, 0x3001200, 0x3001400))
        return tuple(struct.unpack('<5i', self.u.mem_read(0x3001400, 20)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    native = Native(exe)
    lib = library()
    lib.bk_capture_files_create.argtypes = [C.c_char_p, C.c_void_p]
    lib.bk_capture_files_create.restype = C.c_void_p
    lib.bk_capture_files_destroy.argtypes = [C.c_void_p]
    lib.bk_capture_files_count_photos.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.c_void_p]
    error = C.create_string_buffer(256)
    rng = random.Random(0x4af5e1)
    prefixes = ['ri_', 're_', 'cr_', 'ma_', 'mi_']
    cases = [[], ['ignored.bmp'], ['ri_.bmp'],
             ['ri_lower.bmp', 'RI_upper.BMP', 're_upper.BMP', 'Re_mixed.bmp',
              'cr_mixed.bMp', 'ma_wrong.png', 'mi_pending.bmp.part', 'ri_dir.bmp',
              'ri_nested', 'mi_multi.part.bmp', 'mi.bmp', 'ri_last.bmpx', '.ri_hidden.bmp']]
    cases += [[f'{prefix}{i}.bmp' for i in range(n)]
              for prefix in prefixes for n in [1, 99, 100, 101, 137]]
    cases += [[f'{prefix}{i}.bmp' for prefix in prefixes for i in range(110)]]
    for _ in range(80):
        names = []
        for i in range(rng.randrange(1, 241)):
            prefix = rng.choice(prefixes + [p.upper() for p in prefixes] + ['xx_', '_'])
            ext = rng.choice(['.bmp', '.BMP', '.bMp', '.bmp.part', '.png'])
            names.append(f'{prefix}{i:04d}{ext}')
        rng.shuffle(names)
        cases.append(names)
    state = hashlib.sha256()
    names_checked = rescans = 0
    parent = ROOT / 'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    for index, names in enumerate(cases):
        with tempfile.TemporaryDirectory(prefix='album-inventory-', dir=parent) as folder:
            root = Path(folder)
            owner = lib.bk_capture_files_create(str(root).encode(), error)
            assert owner, error.value
            try:
                for name in names:
                    path = root / 'album' / name
                    if name in ['ri_dir.bmp', 'ri_nested']:
                        path.mkdir()
                        (path / 'mi_child.bmp').write_bytes(b'not-a-bitmap')
                    else:
                        path.write_bytes(b'not-a-bitmap')
                result = (C.c_int32 * 5)(-91, -92, -93, -94, -95)
                assert lib.bk_capture_files_count_photos(owner, result, error), error.value
                want = native.run(names)
                assert tuple(result) == want, (index, tuple(result), want)
                state.update(bytes(result))
                names_checked += len(names)
                # An added/deleted external photo must appear/disappear on a
                # subsequent scan; no cached inventory or stale previous output.
                added = 'mi_added.bmp'
                (root / 'album' / added).write_bytes(b'')
                assert lib.bk_capture_files_count_photos(owner, result, error), error.value
                assert tuple(result) == native.run(names + [added])
                (root / 'album' / added).unlink()
                assert lib.bk_capture_files_count_photos(owner, result, error), error.value
                assert tuple(result) == want
                rescans += 2
            finally:
                lib.bk_capture_files_destroy(owner)
    with tempfile.TemporaryDirectory(prefix='album-errors-', dir=parent) as folder:
        root = Path(folder)
        owner = lib.bk_capture_files_create(str(root).encode(), error)
        assert owner, error.value
        try:
            album = root / 'album'
            album.rmdir()
            result = (C.c_int32 * 5)(9, 8, 7, 6, 5)
            assert lib.bk_capture_files_count_photos(owner, result, error)
            assert tuple(result) == (0,) * 5
            album.write_bytes(b'not-a-directory')
            result[:] = [9, 8, 7, 6, 5]
            assert not lib.bk_capture_files_count_photos(owner, result, error)
            assert tuple(result) == (9, 8, 7, 6, 5)
            assert not lib.bk_capture_files_count_photos(None, result, error)
            assert tuple(result) == (9, 8, 7, 6, 5)
            assert not lib.bk_capture_files_count_photos(owner, None, error)
        finally:
            lib.bk_capture_files_destroy(owner)
    report = dict(passed=True, exe_sha256=hashlib.sha256(exe).hexdigest(),
                  cases=len(cases), names=names_checked, rescans=rescans,
                  failures=3, missing_directory_policy_checks=1,
                  state_sha256=state.hexdigest(), scope=__doc__)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS capture inventory: ' + json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
