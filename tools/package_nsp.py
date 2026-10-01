#!/usr/bin/env python3
"""Build an installed base title with original Japanese Data and HOS saves.

Requires the existing Switch ELF, devkitPro tools, hacbrewpack and local keys.
Game assets, keys, intermediate NCAs and the resulting NSP stay out of Git.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

from bk3_assets import Archive
from fetch_sdk import ROOT, LOCK, digest

LANGUAGES = ('AmericanEnglish', 'BritishEnglish', 'Japanese', 'French',
             'German', 'LatAmSpanish', 'Spanish', 'Italian', 'Dutch',
             'CanFrench', 'Portuguese', 'Russian', 'Korean', 'TradChinese',
             'SimpChinese', 'Reserved')
DATA_EXTENSIONS = {'.pp', '.tbl', '.ckp', '.atr', '.fam', '.ftt', '.ix',
                   '.b3f', '.mpg', '.cfg', '.tbf'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path, help='unmodified Japanese game root')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--keyset', type=Path, default=Path.home()/'.switch/prod.keys')
    parser.add_argument('--hacbrewpack', type=Path, default=Path.home()/'bin/hacbrewpack')
    parser.add_argument('--devkitpro', type=Path,
                        default=Path(os.environ.get('DEVKITPRO', '/opt/devkitpro')))
    args = parser.parse_args()
    title = json.loads((ROOT/'config/switch-title.json').read_text())
    version = LOCK['project_version']
    output = (args.output or ROOT/f'交付/biko3-{title["title_id"]}.nsp').resolve()
    game = args.game.resolve()
    data = game/'Data'
    if output.is_relative_to(game):
        raise ValueError('Output must be separate from original game data')
    if output.exists():
        raise ValueError(f'Output already exists: {output}')
    elf = ROOT/'build-switch/biko3-runtime.elf'
    nro = ROOT/'build-switch/biko3-runtime.nro'
    build = json.loads((ROOT/'build-switch/build-manifest.json').read_text())
    if build['version'] != version or build['mesa_commit'] != LOCK['mesa']['commit'] or build['sha256'] != digest(nro):
        raise ValueError('Switch build/lock mismatch; run build-switch.sh first')
    for path in (elf, ROOT/'icon.jpg', args.keyset, args.hacbrewpack):
        if not path.is_file():
            raise ValueError(f'Missing required local file: {path}')
    files = sorted(p for p in data.iterdir()
                   if p.is_file() and not p.name.startswith('.')
                   and p.suffix.lower() in DATA_EXTENSIONS)
    required = {f'bk3_{i:02d}.pp' for i in range(21)} | {'fambom.pp'}
    if not required.issubset({p.name for p in files}):
        raise ValueError('Original Data directory is missing game archives')
    for path in files:
        if path.suffix.lower() == '.pp':
            Archive(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    work_root = ROOT/'local/nsp'
    work_root.mkdir(parents=True, exist_ok=True)
    manifest = {'title_id': title['title_id'], 'name': title['name'],
                'author': title['author'], 'version': version,
                'type': 'base-application', 'patch_included': False,
                'game_root': 'romfs:', 'save_root': 'save:/biko3',
                'user_save_bytes': title['user_save_bytes'],
                'user_save_journal_bytes': title['user_save_journal_bytes'],
                'mesa_commit': build['mesa_commit'], 'elf_sha256': digest(elf),
                'data_files': []}
    tools = args.devkitpro/'tools/bin'
    with tempfile.TemporaryDirectory(prefix='base-', dir=work_root) as folder:
        work = Path(folder)
        log_path = work_root/'package.log'
        def run(command):
            with log_path.open('ab') as log:
                subprocess.run([str(x) for x in command], cwd=work,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
        log_path.write_bytes(b'')
        exefs, control, romfs = work/'exefs', work/'control', work/'romfs'
        for directory in (exefs, control, romfs/'Data'):
            directory.mkdir(parents=True)
        print('Preparing native ExeFS and title metadata', flush=True)
        run([args.devkitpro/'devkitA64/bin/aarch64-none-elf-strip', elf,
             '-o', work/'main.elf'])
        run([tools/'elf2nso', work/'main.elf', exefs/'main'])
        program_id = '0x'+title['title_id']
        npdm = {
            'name': 'biko3', 'title_id': program_id, 'program_id': program_id,
            'title_id_range_min': program_id, 'title_id_range_max': program_id,
            'program_id_range_min': program_id, 'program_id_range_max': program_id,
            'main_thread_stack_size': '0x100000', 'main_thread_priority': 44,
            'default_cpu_id': 0, 'process_category': 0, 'pool_partition': 0,
            'is_64_bit': True, 'address_space_type': 1, 'is_retail': True,
            'filesystem_access': {'permissions': '0xFFFFFFFFFFFFFFFF'},
            'service_host': [], 'service_access': ['*'],
            'kernel_capabilities': [
                {'type': 'kernel_flags', 'value': {
                    'highest_thread_priority': 59, 'lowest_thread_priority': 28,
                    'highest_cpu_id': 2, 'lowest_cpu_id': 0}},
                {'type': 'syscalls', 'value': {
                    f'svc{i:02X}': f'0x{i:02X}'
                    for i in (*range(0x53), *range(0x60, 0x6e), *range(0x72, 0x79))}},
                {'type': 'application_type', 'value': 1},
                {'type': 'min_kernel_version', 'value': '0x30'},
                {'type': 'handle_table_size', 'value': 512},
                {'type': 'debug_flags', 'value': {}}]}
        (work/'npdm.json').write_text(json.dumps(npdm))
        run([tools/'npdmtool', work/'npdm.json', exefs/'main.npdm'])
        nacp_path = control/'control.nacp'
        run([tools/'nacptool', '--create', title['name'], title['author'],
             version, nacp_path, '--titleid='+title['title_id']])
        nacp = bytearray(nacp_path.read_bytes())
        assert len(nacp) == 0x4000
        for i in range(16):
            nacp[i*0x300:i*0x300+0x200] = title['name'].encode().ljust(0x200, b'\0')
            nacp[i*0x300+0x200:(i+1)*0x300] = title['author'].encode().ljust(0x100, b'\0')
        nacp[0x3025] = 1  # Require the HOME-selected account for this title's save.
        struct.pack_into('<I', nacp, 0x302c, 0xffff)
        for offset in (0x3080, 0x3148):
            struct.pack_into('<QQ', nacp, offset, title['user_save_bytes'],
                             title['user_save_journal_bytes'])
        nacp_path.write_bytes(nacp)
        run([sys.executable, ROOT/'tools/prepare_nro_icon.py', ROOT/'icon.jpg', work/'icon.jpg'])
        for language in LANGUAGES:
            shutil.copyfile(work/'icon.jpg', control/f'icon_{language}.dat')
        manifest['icon_sha256'] = digest(work/'icon.jpg')
        print(f'Copying {len(files)} original Data files (no patch.pp)', flush=True)
        for source in files:
            destination = romfs/'Data'/source.name
            shutil.copyfile(source, destination)
            checksum = digest(source)
            if checksum != digest(destination):
                raise ValueError(f'Copy checksum mismatch: {source.name}')
            manifest['data_files'].append({'path': 'Data/'+source.name,
                                           'bytes': source.stat().st_size,
                                           'sha256': checksum})
        print('Building Program, Control and Application Meta NCAs', flush=True)
        run([args.hacbrewpack.resolve(), '--keyset', args.keyset.resolve(),
             '--titleid', title['title_id'], '--titlename', title['name'],
             '--titlepublisher', title['author'], '--nologo'])
        results = list((work/'hacbrewpack_nsp').glob('*.nsp'))
        if len(results) != 1:
            raise ValueError('Packager did not produce exactly one NSP')
        shutil.move(results[0], output)
    manifest['nsp'] = output.name
    manifest['nsp_bytes'] = output.stat().st_size
    manifest['nsp_sha256'] = digest(output)
    output.with_suffix('.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    print(f'Built {output.name}: {manifest["nsp_bytes"]:,} bytes', flush=True)
    print('SHA256 '+manifest['nsp_sha256'])


if __name__ == '__main__':
    main()
