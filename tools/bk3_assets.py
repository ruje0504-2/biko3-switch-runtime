"""Read Biko3 resources without running Windows code or modifying inputs.

TBL: static analysis of Chinese EXE SHA256 a3c93603...51679e,
0x465190 (table loader), 0x497f4a/0x497f63/0x498711 (size/LZSS/XOR).
Inline PP: 32-byte negated names, uint32 sizes, negated payloads.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import struct

NEGATE = bytes((-x) & 255 for x in range(256))


def decode_tbl(data: bytes) -> bytes:
    if len(data) < 5:
        raise ValueError("truncated TBL")
    size = struct.unpack_from('<I', data)[0] ^ 0xa67f54cb
    if size < 2304 or size > 16 * 1024 * 1024 or (size - 2304) % 80:
        raise ValueError("invalid TBL allocation size")
    source = bytearray(data[4:])
    key = b'\x2f\xca\xd8\x35'
    # Native cipher changes complete dwords only; tail bytes remain as-is.
    for i in range(len(source) // 4 * 4):
        source[i] ^= key[i & 3]
    ring, out = bytearray(4096), bytearray()
    pos, cursor, flags = 0, 0xfee, 0
    # Files contain 256 bytes of writer scratch after the actual records.
    # Native lookup 0x4653d2 uses the 256 (offset,count) buckets, never scratch.
    required = size - 256
    while len(out) < required:
        flags >>= 1
        if not flags & 0x100:
            if pos >= len(source):
                raise ValueError("truncated TBL flags")
            flags = source[pos] | 0xff00
            pos += 1
        if flags & 1:
            if pos >= len(source):
                raise ValueError("truncated TBL literal")
            values = [source[pos]]
            pos += 1
            for value in values:
                out.append(value)
                ring[cursor] = value
                cursor = (cursor + 1) & 4095
        else:
            if pos + 2 > len(source):
                raise ValueError("truncated TBL match")
            a, b = source[pos:pos+2]
            pos += 2
            start, length = a | ((b & 0xf0) << 4), (b & 15) + 3
            if length > size - len(out):
                raise ValueError("TBL match exceeds output")
            for k in range(length):
                value = ring[(start + k) & 4095]
                out.append(value)
                ring[cursor] = value
                cursor = (cursor + 1) & 4095
    return bytes(out[:required])


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    size: int


class Archive:
    def __init__(self, path: Path):
        self.path = Path(path)
        self.size = self.path.stat().st_size
        self.entries: list[Entry] = []
        self.negated = False
        table = self.path.with_suffix('.tbl')
        if table.exists():
            decoded = decode_tbl(table.read_bytes())
            if len(decoded) < 2048 or (len(decoded) - 2048) % 80:
                raise ValueError(f"invalid TBL record size: {path}")
            self.table_bytes = decoded
            cursor = 0
            for bucket in range(256):
                start, count = struct.unpack_from('<II', decoded, bucket * 8)
                if start != cursor or start + count * 80 > len(decoded) - 2048:
                    raise ValueError("invalid TBL bucket range")
                for j in range(count):
                    first = decoded[2048 + start + j * 80 + 16]
                    if first != bucket:
                        raise ValueError("TBL bucket/name disagreement")
                cursor += count * 80
            if cursor != len(decoded) - 2048:
                raise ValueError("TBL size/bucket disagreement")
            for pos in range(2048, len(decoded), 80):
                stride, offset, length, flags = struct.unpack_from('<4I', decoded, pos)
                name = decoded[pos+16:pos+80].split(b'\0')[0].decode('cp932')
                if stride != 80 or flags != 0:
                    raise ValueError(f"unsupported TBL record: {stride}, {flags}")
                self.entries.append(Entry(name, offset, length))
        else:
            self.negated = True
            with self.path.open('rb') as stream:
                header = stream.read(8)
                if len(header) != 8:
                    raise ValueError("truncated PP header")
                count, total = struct.unpack('<II', header)
                if count > 100000 or 8 + count * 36 + total != self.size:
                    raise ValueError(f"invalid PP size: {path}")
                names = stream.read(count * 32)
                sizes = stream.read(count * 4)
                offset = 8 + count * 36
                for i in range(count):
                    name = names[i*32:(i+1)*32].translate(NEGATE).split(b'\0')[0].decode('cp932')
                    length = struct.unpack_from('<I', sizes, i*4)[0]
                    self.entries.append(Entry(name, offset, length))
                    offset += length
                if offset != self.size:
                    raise ValueError("PP entry lengths disagree with total")
        seen = set()
        for entry in self.entries:
            if not entry.name or '/' in entry.name or '\\' in entry.name or entry.name in ('.', '..'):
                raise ValueError("unsafe resource name")
            if entry.name.lower() in seen:
                raise ValueError("duplicate resource name")
            seen.add(entry.name.lower())
            if entry.offset < 0 or entry.offset + entry.size > self.size:
                raise ValueError("resource outside archive")

    def read(self, entry: Entry) -> bytes:
        with self.path.open('rb') as stream:
            stream.seek(entry.offset)
            data = stream.read(entry.size)
        if len(data) != entry.size:
            raise ValueError("short resource read")
        return data.translate(NEGATE) if self.negated else data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    results = []
    for path in sorted(args.directory.glob('*.pp')):
        archive = Archive(path)
        print(path.name, len(archive.entries), dict(Counter(Path(e.name).suffix.lower() for e in archive.entries)))
        results.append(dict(path=str(path), size=archive.size, negated=archive.negated,
                            entries=[asdict(e) for e in archive.entries]))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    main()
