"""Compare4D39E6 targets with original4D4167..4D4203, without hooks.

The original copies published nodes5/13/0, independently of optional719B40.
Synthetic samples exercise distinct inputs and float bit preservation. Optional
AUX_TARGET rows verify the actual Japanese resource probe's captured inputs.
This segment comparison is not a full loader, frame or hardware acceptance.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct

from unicorn.x86_const import UC_X86_REG_EIP
from original_prop_route_oracle import Native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('library', type=Path)
    parser.add_argument('--logs', nargs='*', type=Path, default=[])
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == 'a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e'
    native = Native(exe)
    lib = C.CDLL(str(args.library.resolve()))
    lib.bk_ending_4d39e6_targets.argtypes = [C.c_void_p] * 5
    error = C.create_string_buffer(256)

    def original(raw):
        for i, address in enumerate([0x721F08, 0x721F28, 0x721EF4]):
            node = 0x3000000 + i * 0x400
            native.u.mem_write(address, struct.pack('<I', node))
            native.u.mem_write(node + 0xF0, raw[i * 12:(i + 1) * 12])
        native.u.emu_start(0x4D4167, 0x4D4203, count=1000)
        assert native.u.reg_read(UC_X86_REG_EIP) == 0x4D4203
        return bytes(native.u.mem_read(0x70C8D8, 36))

    rng = random.Random(0x4D4167)
    for sample in range(4096):
        values = [rng.uniform(-10000, 10000) for _ in range(9)]
        if sample % 4 == 0:
            values[sample % 9] = -0.0
        raw = struct.pack('<9f', *values)
        inputs = [C.create_string_buffer(raw[i * 12:(i + 1) * 12]) for i in range(3)]
        output = C.create_string_buffer(36)
        assert lib.bk_ending_4d39e6_targets(output, *inputs, error), error.value
        assert output.raw == original(raw), sample

    logs = []
    for path in args.logs:
        contents = path.read_bytes()
        rows = [line.split()[1:] for line in contents.decode().splitlines()
                if line.startswith('AUX_TARGET ')]
        assert len(rows) == 10 and b'PASS 4D39E6 resources:' in contents
        identities = set()
        for row in rows:
            assert len(row) == 20
            identities.add(tuple(map(int, row[:2])))
            inputs = struct.pack('<9I', *[int(x, 16) for x in row[2:11]])
            actual = struct.pack('<9I', *[int(x, 16) for x in row[11:]])
            assert actual == original(inputs), row
        assert identities == {(g, v) for g in range(5) for v in range(2)}
        logs.append({'path': str(path), 'sha256': hashlib.sha256(contents).hexdigest(),
                     'loads': 10, 'target_components': 90})
    report = {'passed': True, 'scope': __doc__, 'exe_sha256': digest,
              'library_sha256': hashlib.sha256(args.library.read_bytes()).hexdigest(),
              'synthetic_samples': 4096, 'synthetic_components': 4096 * 9,
              'real_asset_logs': logs, 'hooks': [], 'max_error': 0}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
