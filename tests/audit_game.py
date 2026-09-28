"""Compare native indexes with independent Python and image pixels with Pillow."""
from pathlib import Path
import argparse
from collections import Counter
import ctypes
import os
import hashlib
import io
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive
from PIL import Image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('data', type=Path)
    p.add_argument('--output', type=Path, default=ROOT/'local/audit.json')
    args = p.parse_args()
    lib = ctypes.CDLL(str(Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build')) / ('libassets-test.dylib' if sys.platform == 'darwin' else 'libassets-test.so')))
    class CImage(ctypes.Structure):
        _fields_ = [('width', ctypes.c_uint32), ('height', ctypes.c_uint32), ('rgba', ctypes.c_void_p)]
    lib.bk_image_decode.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(CImage), ctypes.c_void_p]
    lib.bk_image_free.argtypes = [ctypes.POINTER(CImage)]
    results, total, images = [], 0, 0
    for path in sorted(args.data.glob('*.pp')):
        archive = Archive(path)
        native = subprocess.check_output([str(Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build'))/'asset-probe'), str(path)]).decode('cp932')
        expected = ''.join(f'{e.offset}\t{e.size}\t{e.name}\n' for e in archive.entries)
        if native != expected:
            raise ValueError(f'index mismatch: {path}')
        # Validate native PP payload decoding too, then compare every image pixel.
        subprocess.run([str(Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build'))/'asset-probe'), str(path), '--verify'], check=True)
        for entry in archive.entries:
            if Path(entry.name).suffix.lower() not in ('.bmp', '.tga'):
                continue
            data = archive.read(entry)
            im, error = CImage(), ctypes.create_string_buffer(256)
            if not lib.bk_image_decode(data, len(data), ctypes.byref(im), error):
                raise ValueError(f'{path}/{entry.name}: {error.value!r}')
            try:
                pixels = ctypes.string_at(im.rgba, im.width*im.height*4)
                with Image.open(io.BytesIO(data)) as reference:
                    if reference.size != (im.width, im.height) or reference.convert('RGBA').tobytes() != pixels:
                        raise ValueError(f'Pillow/native pixel mismatch: {path}/{entry.name}')
            finally:
                lib.bk_image_free(ctypes.byref(im))
            images += 1
        total += len(archive.entries)
        with path.open('rb') as stream:
            sha = hashlib.file_digest(stream, 'sha256').hexdigest()
        results.append({'file': path.name, 'size': path.stat().st_size, 'sha256': sha,
                        'entries': len(archive.entries),
                        'extensions': dict(Counter(Path(e.name).suffix.lower() for e in archive.entries))})
    result = {'archives': results, 'entries': total, 'images_pixel_exact': images,
              'checks': ['all C/Python indexes equal', 'all native image decodes pass', 'all image pixels equal Pillow']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(f'PASS: {len(results)} archives, {total} entries, {images} pixel-exact images')


if __name__ == '__main__':
    main()
