"""Convert the supplied Chinese/3DH patches into one read-only Switch overlay.

Never runs the Windows installers or writes to the original game. Patch.3dh
stores an offset/length directory at EOF. Blocks longer than 31 bytes negate
their first 32 bytes, followed by an uncompressed size and a zlib stream.
The original installer routines are 49b26c/49b378 (BK3_MSC.exe, unpacked).
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path
import struct
import zlib

from bk3_assets import Archive, NEGATE

SOURCE_HASHES = {
    '去码/Patch.3dh': 'c6cd21d57f81bf6f121534b99dd7aa3f4d47f5850f0b9711250c92fbd779bdf2',
    '汉化/BK3_00.PP': '7200a30d1a6dc8b039bcf5d89de880213b6a063d801c25da72e2b381c03fd69d',
    '汉化/BK3_05.PP': '5ea60942de3dc487eb73c370abe96065b1dba59f8c9e3d037a077a9d2d7a6930',
    '汉化/Dest.ftt': '0b151d41811793d0870de32f3c93bb8f4b4be681df502b30cf40054472d7d89c',
}


def block(raw: bytes) -> bytes:
    if len(raw) > 31:
        raw = raw[:32].translate(NEGATE) + raw[32:]
    size, = struct.unpack_from('<I', raw)
    result = zlib.decompress(raw[4:])
    if len(result) != size:
        raise ValueError('3DH block length mismatch')
    return result


def font_overlay(original: bytes, chinese: bytes) -> bytes:
    # The original fixed-width renderer indexes the two input bytes directly.
    # Retain Japanese glyphs for the EXE's built-in dates/menu labels, then add
    # the patch's GBK bitmap slots. Chinese dialogue itself remains unmodified.
    start = 0x60004
    if original[:4] != chinese[:4] or len(chinese) < start:
        raise ValueError('font cell/header mismatch')
    result = bytearray(original)
    shift = len(original) - start
    for code in range(32768):
        pos = 4 + code * 12
        offset, pitch, width, height, reserved = struct.unpack_from('<I4H', chinese, pos)
        if pitch and height:
            if offset + pitch * height > len(chinese) - start:
                raise ValueError('Chinese glyph outside bitmap payload')
            struct.pack_into('<I4H', result, pos, shift + offset, pitch, width, height, reserved)
    result += chinese[start:]
    return bytes(result)


def write_pp(path: Path, entries: dict[str, bytes]) -> None:
    names = list(entries)
    if any(len(n.encode('ascii')) > 31 for n in names):
        raise ValueError('patch entry name exceeds inline PP capacity')
    total = sum(map(len, entries.values()))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('wb') as f:
        f.write(struct.pack('<II', len(names), total))
        for name in names:
            f.write(name.encode('ascii').ljust(32, b'\0').translate(NEGATE))
        for data in entries.values():
            f.write(struct.pack('<I', len(data)))
        for data in entries.values():
            f.write(data.translate(NEGATE))


def build(source: Path, game: Path, output: Path) -> dict:
    for name, expected in SOURCE_HASHES.items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f'unsupported source patch version: {name}')
    base: dict[str, Archive] = {}
    records: dict[str, tuple[int, int, list[tuple[int, bytes]]]] = {}
    evidence: dict[str, dict] = {}

    def archive(pack: str) -> Archive:
        if pack not in base:
            base[pack] = Archive(game / 'Data' / (pack + '.pp'))
        return base[pack]

    def replacement(pack: str, name: str, before: bytes, after: bytes) -> None:
        key = f'{pack}.{name}'.lower()
        records[key] = (len(before), len(after), [(0, after)])
        evidence[key] = {'base_sha256': hashlib.sha256(before).hexdigest(),
                         'result_sha256': hashlib.sha256(after).hexdigest()}

    for pack in ['bk3_00', 'bk3_05']:
        translated = Archive(source / '汉化' / (pack.upper() + '.PP'))
        original = archive(pack)
        by_name = {e.name.lower(): e for e in original.entries}
        for e in translated.entries:
            replacement(pack, e.name, original.read(by_name[e.name.lower()]), translated.read(e))

    font = (source / '汉化' / 'Dest.ftt').read_bytes()
    for name in ['Type_S.FTT', 'Type_G.FTT']:
        path = next(p for p in (game / 'Data').iterdir() if p.name.lower() == name.lower())
        before = path.read_bytes()
        replacement('fonts', name, before, font_overlay(before, font))

    raw = (source / '去码' / 'Patch.3dh').read_bytes()
    tail, = struct.unpack_from('<I', raw)
    index = block(raw[-tail:])

    def payload(slot: int) -> bytes:
        pos, length = struct.unpack_from('<II', index, slot * 8)
        if not length or pos + length > len(raw) - tail:
            raise ValueError('3DH directory entry outside data')
        return block(raw[pos:pos + length])

    config = configparser.ConfigParser(interpolation=None)
    config.read_string(payload(0).decode('gbk'))
    exe_operations = []
    operations = 0
    for key, command in config['Start'].items():
        if not key.isdigit() or key == '0':
            continue
        kind, args = command.split(' ', 1)
        value, offset, size, target = args.rsplit(',', 3)
        offset, size = int(offset), int(size)
        if target.lower().endswith('.exe'):
            exe_operations.append({'offset': offset, 'value': int(value) & 0xffffffff})
            continue
        pack = Path(target.rsplit('\\', 1)[-1]).stem.lower()
        a = archive(pack)
        found = [e for e in a.entries if e.offset <= offset and offset + size <= e.offset + e.size]
        if len(found) != 1:
            raise ValueError(f'3DH operation does not map to one resource: {command}')
        e = found[0]
        if kind == 'pth':
            data = payload(int(key) + 8)
        elif kind == 'wrt' and size == 4:
            data = struct.pack('<I', int(value) & 0xffffffff)
        else:
            raise ValueError(f'unsupported 3DH operation: {kind}')
        if len(data) != size:
            raise ValueError('3DH replacement size mismatch')
        if a.negated:
            data = data.translate(NEGATE)
        record = records.setdefault(f'{pack}.{e.name}'.lower(), (e.size, e.size, []))
        record[2].append((offset - e.offset, data))
        operations += 1

    # These are Windows-only resource verification, five movie bindings, and
    # the shared mosaic AVI loader. The native render adapters honor flag2.
    expected_exe = {649908: 12970249, 851237: 68585, 855081: 68585,
                    858850: 68585, 865783: 68585, 869007: 68585,
                    942024: (-1017256565) & 0xffffffff}
    if len(exe_operations) != 21 or any(expected_exe.get(x['offset']) != x['value'] for x in exe_operations):
        raise ValueError('unexpected executable patch operation')

    entries = {'patch.cfg': b'BKPT' + struct.pack('<II', 2, 3)}
    for key, (old_size, new_size, segments) in sorted(records.items()):
        record = struct.pack('<III', old_size, new_size, len(segments)) + b''.join(
            struct.pack('<II', offset, len(data)) + data for offset, data in segments)
        # Independent streams preserve lazy loading; text does not require
        # reading or decompressing the images/models elsewhere in the archive.
        entries[key] = struct.pack('<I', len(record)) + zlib.compress(record, 1)
        if key not in evidence:
            pack, name = key.split('.', 1)
            a = archive(pack)
            e = next(e for e in a.entries if e.name.lower() == name)
            before = a.read(e)
            after = bytearray(before)
            for offset, data in segments:
                after[offset:offset + len(data)] = data
            evidence[key] = {'base_sha256': hashlib.sha256(before).hexdigest(),
                             'result_sha256': hashlib.sha256(after).hexdigest()}
    write_pp(output, entries)
    result = {'format': 'BKPT2 within inline PP', 'compression': 'zlib level 1 per resource', 'flags': 3,
              'source_hashes': SOURCE_HASHES, 'resources': len(records),
              'uncensored_resource_operations': operations,
              'native_executable_equivalents': expected_exe,
              'output_bytes': output.stat().st_size,
              'output_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'entries': evidence}
    output.with_suffix('.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('game', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    result = build(a.source, a.game, a.output)
    print(f"Created {a.output}: {result['resources']} resources, {result['output_bytes']} bytes, SHA256 {result['output_sha256']}")


if __name__ == '__main__':
    main()
