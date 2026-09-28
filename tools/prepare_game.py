"""Extract the user's Wise split installer without executing the Windows app.

Both biko3.EXE and biko3.W02 must remain together. Original files are read-only.
The macOS ARM64 extractor is pinned; other hosts can pass --unpacker.
"""
import argparse
from pathlib import Path
import platform
import subprocess
import zipfile
from bk3_assets import Archive
from fetch_sdk import fetch, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'local/game')
    parser.add_argument('--unpacker', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Output already exists; pass a new --output directory to preserve it.')
    if not args.installer.is_file() or not args.installer.with_suffix('.W02').is_file():
        raise SystemExit('Both installer EXE and W02 are required.')
    unpacker = args.unpacker
    if unpacker is None:
        if platform.system() != 'Darwin' or platform.machine() != 'arm64':
            raise SystemExit('Pass --unpacker for this host platform.')
        local = ROOT / 'local'
        local.mkdir(exist_ok=True)
        archive = local / 'wise.zip'
        fetch(archive,
              'https://github.com/mnadareski/WiseUnpacker/releases/download/3.0.0/WiseUnpacker_3.0.0_net10.0_osx-arm64_release.zip',
              'fce16dee6f45436e14fcd706563b6e9f59ac30c1a9e9fc53a89fbd1c5a4689f5')
        unpacker = local / 'wise/WiseUnpacker'
        if not unpacker.exists():
            unpacker.parent.mkdir(exist_ok=True)
            with zipfile.ZipFile(archive) as z:
                unpacker.write_bytes(z.read('WiseUnpacker'))
            unpacker.chmod(0o755)
    subprocess.run([str(unpacker.resolve()), '-x', f'-o={args.output.resolve()}',
                    str(args.installer.resolve())], check=True)
    data = args.output / 'MAINDIR/Data'
    paths = sorted(data.glob('*.pp'))
    if len(paths) != 22:
        raise ValueError(f'Expected 22 archives, got {len(paths)}; extraction incomplete')
    count = sum(len(Archive(p).entries) for p in paths)
    print(f'Extracted and checked {len(paths)} archives, {count} entries in {data}')


if __name__ == '__main__':
    main()
