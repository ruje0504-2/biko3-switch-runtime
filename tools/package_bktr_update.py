#!/usr/bin/env python3
"""Convert the prepared full-resource update to a BKTR update of a given base.

Requires hactool and hacPack with tools/hacpack-bktr.patch applied. Inputs are
read-only. The work directory contains private tool output and game resources.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

from fetch_sdk import ROOT, digest

NODE = 0x4000
CHUNK = 4 * 1024 * 1024
END = 0xffffffff


def copy_region(source, destination, offset, size):
    source.seek(offset)
    while size:
        block = source.read(min(size, CHUNK))
        if not block:
            raise ValueError('Truncated input region')
        destination.write(block)
        size -= len(block)


def unpack_nsp(path, directory):
    directory.mkdir()
    with path.open('rb') as stream:
        magic, count, names_size, _ = struct.unpack('<4sIII', stream.read(16))
        if magic != b'PFS0' or count != 3:
            raise ValueError('Expected the three-NCA Biko3 package')
        entries = [struct.unpack('<QQI4x', stream.read(24)) for _ in range(count)]
        names = stream.read(names_size)
        data_start = stream.tell()
        for offset, size, name_offset in entries:
            name = names[name_offset:names.index(0, name_offset)].decode()
            if Path(name).name != name or not name.endswith('.nca'):
                raise ValueError('Unexpected NSP member')
            target = directory/name
            with target.open('wb') as output:
                copy_region(stream, output, data_start + offset, size)
            if digest(target)[:32] != name[:32]:
                raise ValueError('NCA content ID mismatch')


def romfs_files(section, header):
    # IVFC level 5 is the RomFS. Directory/file offsets are relative to its
    # metadata tables; file data offsets are relative to its data partition.
    fs = header[0x600:0x800]
    if fs[:5] != bytes((2, 0, 0, 3, 3)):
        raise ValueError('Expected a complete CTR RomFS in section 1')
    root = struct.unpack_from('<Q', fs, 8 + 16 + 5 * 24)[0]
    with section.open('rb') as stream:
        stream.seek(root)
        fields = struct.unpack('<10Q', stream.read(80))
        if fields[0] != 80:
            raise ValueError('Invalid RomFS header')
        stream.seek(root + fields[3])
        directories = stream.read(fields[4])
        stream.seek(root + fields[7])
        files = stream.read(fields[8])
    result = {}
    def walk(offset, prefix):
        _, _, child, file, _, length = struct.unpack_from('<6I', directories, offset)
        name = directories[offset+24:offset+24+length].decode()
        folder = prefix + (name+'/' if name else '')
        while file != END:
            _, sibling, data, size, _, length = struct.unpack_from('<IIQQII', files, file)
            name = files[file+32:file+32+length].decode()
            result[folder+name] = (root + fields[9] + data, size)
            file = sibling
        while child != END:
            walk(child, folder)
            child = struct.unpack_from('<I', directories, child+4)[0]
    walk(0, '')
    return result


def regions_equal(left, right, left_offset, right_offset, size):
    left.seek(left_offset)
    right.seek(right_offset)
    while size:
        count = min(size, CHUNK)
        a, b = left.read(count), right.read(count)
        if len(a) != count or a != b:
            return False
        size -= count
    return True


def bucket(entries, size, format):
    if not entries or len(entries) > (NODE-16)//struct.calcsize(format):
        raise ValueError('This builder supports a single BKTR entry bucket')
    root = struct.pack('<IIQQ', 0, 1, size, entries[0][0]).ljust(NODE, b'\0')
    leaf = struct.pack('<IIQ', 0, len(entries), size)
    leaf += b''.join(struct.pack(format, *entry) for entry in entries)
    return root + leaf.ljust(NODE, b'\0')


def build_bktr(base, target, base_header, target_header, work):
    original = romfs_files(base, base_header)
    updated = romfs_files(target, target_header)
    if set(updated) != set(original) | {'patch.pp'} or 'patch.pp' in original:
        raise ValueError('Expected unchanged base files plus patch.pp')
    ranges = []
    with base.open('rb') as left, target.open('rb') as right:
        for name, (offset, size) in sorted(updated.items(), key=lambda item: item[1][0]):
            if name == 'patch.pp':
                continue
            old_offset, old_size = original[name]
            if size != old_size or not regions_equal(left, right, old_offset, offset, size):
                raise ValueError(f'Original data changed: {name}')
            # Keep every mapping boundary AES-block aligned. Padding can be
            # reused only when its actual bytes agree in both input images.
            length = (size + 15) & ~15
            if not regions_equal(left, right, old_offset, offset, length):
                length = size & ~15
            if length:
                ranges.append((offset, old_offset, length))
    entries, records = [], []
    cursor, reused = 0, 0
    section = work/'bktr.section'
    with target.open('rb') as source, section.open('wb') as output:
        def add(virtual, physical, storage, length):
            if not length:
                return
            if (records and records[-1]['storage'] == storage and
                    records[-1]['virtual'] + records[-1]['bytes'] == virtual and
                    records[-1]['physical'] + records[-1]['bytes'] == physical):
                records[-1]['bytes'] += length
                return
            entries.append((virtual, physical, storage))
            records.append({'virtual': virtual, 'physical': physical,
                            'storage': storage, 'bytes': length})
        for offset, old_offset, length in ranges:
            if offset < cursor:
                raise ValueError('Overlapping file ranges')
            gap = offset - cursor
            add(cursor, output.tell(), 1, gap)
            copy_region(source, output, cursor, gap)
            add(offset, old_offset, 0, length)
            reused += length
            cursor = offset + length
        gap = target.stat().st_size - cursor
        add(cursor, output.tell(), 1, gap)
        copy_region(source, output, cursor, gap)
        payload = output.tell()
        output.write(bytes((-payload) % NODE))
        indirect_offset = output.tell()
        output.write(bucket(entries, target.stat().st_size, '<QQI'))
        aes_offset = output.tell()
        # A single generation-zero AES-CTR-Ex range uses hacPack's normal
        # CTR encryption for both patch data and relocation metadata.
        output.write(bucket([(0, 0, 0)], aes_offset, '<QII'))
    fs = bytearray(target_header[0x600:0x800])
    fs[4] = 4  # NcaEncryptionType_AesCtrEx / CRYPT_BKTR.
    struct.pack_into('<QQ4sIII', fs, 0x100, indirect_offset, 2*NODE, b'BKTR', 1, len(entries), 0)
    struct.pack_into('<QQ4sIII', fs, 0x120, aes_offset, 2*NODE, b'BKTR', 1, 1, 0)
    fs[0x140:0x148] = bytes(8)
    (work/'bktr.fsheader').write_bytes(fs)
    plan = {'virtual_bytes': target.stat().st_size, 'base_files': len(original),
            'base_bytes_reused': reused, 'patch_payload_bytes': payload,
            'section_bytes': section.stat().st_size, 'indirect_entries': len(entries),
            'indirect_offset': indirect_offset, 'aes_ctr_ex_offset': aes_offset,
            'aes_ctr_ex_entries': 1, 'ranges': records}
    (work/'bktr-plan.json').write_text(json.dumps(plan, indent=2)+'\n')
    return plan


def patch_cnmt(base_cnmt, base_meta, contents, content_version):
    base_id, version, kind = struct.unpack_from('<QIB', base_cnmt)
    ext_size, count = struct.unpack_from('<HH', base_cnmt, 14)
    if kind != 0x80 or ext_size != 16:
        raise ValueError('Expected an Application base CNMT')
    update_id, required_system, _ = struct.unpack_from('<QII', base_cnmt, 32)
    if update_id != base_id + 0x800:
        raise ValueError('Unexpected base update ID')
    infos = [base_cnmt[32+ext_size+i*56+32:32+ext_size+(i+1)*56] for i in range(count)]
    infos.append(bytes.fromhex(digest(base_meta)[:32]) + base_meta.stat().st_size.to_bytes(6, 'little') + bytes((0, 0)))
    history = struct.pack('<7I', 1, 0, 0, 0, len(infos), 0, 0)
    history += struct.pack('<QIB3x', base_id, version, kind) + base_cnmt[-32:]
    history += struct.pack('<H6x', len(infos)) + b''.join(infos)
    header = struct.pack('<QIBBHHHBBBBI4x', update_id, content_version, 0x81, 0, 24, len(contents), 0, 0, 0, 0, 0, 0)
    data = header + struct.pack('<QII8x', base_id, required_system, len(history))
    for content_type, path in contents:
        checksum = bytes.fromhex(digest(path))
        data += checksum + checksum[:16] + path.stat().st_size.to_bytes(6, 'little') + bytes((content_type, 0))
    return data + history + bytes(32), base_id, update_id


def write_nsp(paths, output):
    names, entries, cursor = b'', [], 0
    for path in paths:
        entries.append(struct.pack('<QQI4x', cursor, path.stat().st_size, len(names)))
        names += path.name.encode() + b'\0'
        cursor += path.stat().st_size
    names += bytes((-(16 + 24*len(paths) + len(names))) % 32)
    with output.open('wb') as stream:
        stream.write(b'PFS0' + struct.pack('<III', len(paths), len(names), 0))
        stream.write(b''.join(entries) + names)
        for path in paths:
            with path.open('rb') as source:
                shutil.copyfileobj(source, stream, CHUNK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base_nsp', type=Path)
    parser.add_argument('full_update_nsp', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--workdir', type=Path, required=True)
    parser.add_argument('--display-version', default='1.0.79cn')
    parser.add_argument('--content-version', type=int, default=196608)
    parser.add_argument('--hacpack', type=Path, default=ROOT/'local/hacpack/hacpack')
    parser.add_argument('--hactool', type=Path, default=Path('/opt/devkitpro/tools/bin/hactool'))
    parser.add_argument('--keyset', type=Path, default=Path.home()/'.switch/prod.keys')
    args = parser.parse_args()
    args.output = args.output.resolve()
    if args.output.exists() or not 0 < len(args.display_version.encode()) < 16:
        raise ValueError('Output exists or display version is invalid')
    work = args.workdir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    def run(command, env=None):
        with (work/'tools.log').open('ab') as log:
            subprocess.run(list(map(str, command)), cwd=work, env=env,
                           stdout=log, stderr=log, check=True)
    hac = [args.hactool.resolve(), '--suppresskeys', '--disablekeywarns', '-k', args.keyset.resolve(), '-x']
    inputs = {}
    for label, path in [('base', args.base_nsp), ('target', args.full_update_nsp)]:
        print(f'Reading {label} NSP', flush=True)
        directory = work/label
        unpack_nsp(path.resolve(), directory)
        kinds = {}
        for nca in directory.glob('*.nca'):
            header = nca.with_suffix('.header')
            run(hac + ['--header='+str(header), nca])
            data = header.read_bytes()
            kinds[data[0x205]] = (nca, data)
        if set(kinds) != {0, 1, 2}:
            raise ValueError('Missing Program/Meta/Control NCA')
        program, header = kinds[0]
        run(hac + ['-r', '--section1='+str(directory/'romfs.section'), program])
        inputs[label] = kinds
    print('Mapping unchanged game resources back to the base', flush=True)
    plan = build_bktr(work/'base/romfs.section', work/'target/romfs.section',
                      inputs['base'][0][1], inputs['target'][0][1], work)
    exefs, control, meta = work/'exefs', work/'control', work/'base-meta'
    for p in [exefs, control, meta]: p.mkdir()
    run(hac + ['--exefsdir='+str(exefs), inputs['target'][0][0]])
    run(hac + ['--romfsdir='+str(control), inputs['base'][2][0]])
    run(hac + ['--section0dir='+str(meta), inputs['base'][1][0]])
    base_cnmt = next(meta.glob('*.cnmt')).read_bytes()
    base_id = struct.unpack_from('<Q', base_cnmt)[0]
    configured = int(json.loads((ROOT/'config/switch-title.json').read_text())['title_id'], 16)
    if base_id != configured or any(struct.unpack_from('<Q', inputs[k][0][1], 0x210)[0] != base_id for k in inputs):
        raise ValueError('Inputs do not belong to the configured application')
    nacp_path = control/'control.nacp'
    nacp = bytearray(nacp_path.read_bytes())
    nacp[0x3060:0x3070] = args.display_version.encode().ljust(16, b'\0')
    nacp_path.write_bytes(nacp)
    ncas, empty = work/'ncas', work/'empty-romfs'
    ncas.mkdir(); empty.mkdir()
    common = [args.hacpack.resolve(), '-k', args.keyset.resolve(), '--type', 'nca',
              '--titleid', f'{base_id:016X}', '--tempdir', work/'hacpack-temp',
              '--backupdir', work/'backup', '-o', ncas]
    env = dict(os.environ, HACPACK_BKTR_DATA=str(work/'bktr.section'), HACPACK_BKTR_HEADER=str(work/'bktr.fsheader'))
    print(f'Building BKTR Program: {plan["section_bytes"]:,} bytes; display {args.display_version}', flush=True)
    run(common + ['--ncatype', 'program', '--exefsdir', exefs, '--romfsdir', empty], env)
    program = next(ncas.glob('*.nca'))
    run(hac + ['--basenca='+str(inputs['base'][0][0]), '--header='+str(work/'built.header'), program])
    if (work/'built.header').read_bytes()[0x600:0x800] != (work/'bktr.fsheader').read_bytes():
        raise ValueError('hacPack needs tools/hacpack-bktr.patch applied and rebuilt')
    run(common + ['--ncatype', 'control', '--romfsdir', control])
    control_nca = next(p for p in ncas.glob('*.nca') if p != program)
    data, _, update_id = patch_cnmt(base_cnmt, inputs['base'][1][0], [(1, program), (3, control_nca)], args.content_version)
    cnmt = work/'patch.cnmt'; cnmt.write_bytes(data)
    run([args.hacpack.resolve(), '-k', args.keyset.resolve(), '--type', 'nca', '--ncatype', 'meta',
         '--titletype', 'patch', '--titleid', f'{update_id:016X}', '--cnmt', cnmt,
         '--tempdir', work/'hacpack-temp', '--backupdir', work/'backup', '-o', ncas])
    meta_nca = next(p for p in ncas.glob('*.nca') if p not in (program, control_nca))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_nsp([meta_nca, program, control_nca], args.output)
    result = {'type': 'bktr-update', 'application_id': f'{base_id:016X}', 'title_id': f'{update_id:016X}',
              'display_version': args.display_version, 'content_version': args.content_version,
              'base_nsp_sha256': digest(args.base_nsp), 'source_update_sha256': digest(args.full_update_nsp),
              'nsp': args.output.name, 'nsp_bytes': args.output.stat().st_size, 'nsp_sha256': digest(args.output),
              'bktr': plan, 'patch_history_bytes': struct.unpack_from('<I', data, 0x2c)[0],
              'contents': [{'type': k, 'name': p.name, 'sha256': digest(p), 'bytes': p.stat().st_size}
                           for k, p in [(0, meta_nca), (1, program), (3, control_nca)]]}
    args.output.with_suffix('.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'Built {args.output.name}: {result["nsp_bytes"]:,} bytes; SHA256 {result["nsp_sha256"]}', flush=True)


if __name__ == '__main__':
    main()
