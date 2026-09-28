"""Fetch the verified latest-main Mesa SDK selected for this port (2026-09-27).

Does not replace global devkitPro portlibs. The original CI artifact omitted
Rust std; its .comment identifies rustc 1.100.0-nightly (6bb1652a0 2026-09-22).
"""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
LOCK = json.loads((ROOT/'config/dependencies.lock.json').read_text())
MESA_COMMIT = LOCK['mesa']['commit']
DEPS = [(LOCK[key]['archive'], LOCK[key]['url'], LOCK[key]['sha256'])
        for key in ('mesa', 'rust')]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def fetch(path, url, expected):
    if not path.exists():
        temporary = path.with_name(path.name + '.part')
        subprocess.run(['curl', '-fLsS', '--retry', '2', '--connect-timeout', '20',
                        '--max-time', '300', url, '-o', str(temporary)], check=True)
        if digest(temporary) != expected:
            raise ValueError(f'{path.name}: downloaded checksum mismatch')
        temporary.rename(path)
    if digest(path) != expected:
        raise ValueError(f'{path.name}: cached checksum mismatch')


def main():
    local = ROOT / 'local'
    local.mkdir(exist_ok=True)
    for name, url, sha256 in DEPS:
        print(f'Verifying {name}', flush=True)
        fetch(local / name, url, sha256)
    sdk = local / 'mesa-sdk'
    if not sdk.exists():
        with zipfile.ZipFile(local / 'mesa-sdk.zip') as archive:
            for info in archive.infolist():
                target = (sdk / info.filename).resolve()
                if not target.is_relative_to(sdk.resolve()):
                    raise ValueError('unsafe SDK archive path')
            archive.extractall(sdk)
    rust = local / 'rust-std-nightly-aarch64-unknown-linux-gnu'
    if not rust.exists():
        with tarfile.open(local / 'rust-std.tar.xz') as archive:
            archive.extractall(local, filter='data')
    prefix = sdk / 'opt/devkitpro/portlibs/switch'
    if not (prefix / 'lib/libvulkan.a').is_file():
        raise ValueError('SDK is incomplete')
    provenance = {'mesa_commit': MESA_COMMIT, 'mesa_version': LOCK['mesa']['version'],
                  'github_run': LOCK['mesa']['github_run'], 'github_artifact': LOCK['mesa']['github_artifact'],
                  'rust_nightly': LOCK['rust']['nightly'],
                  'archives': [{'file': n, 'url': u, 'sha256': h} for n, u, h in DEPS]}
    (local / 'sdk-provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(f'Mesa NVK SDK: {prefix}')


if __name__ == '__main__':
    main()
