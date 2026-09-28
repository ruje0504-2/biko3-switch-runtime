"""Structural audit only: OBJM chunks and the 0x14c/60-byte mesh layout.

Native evidence: 0x417df2 dispatch, 0x41853a mesh header, 0x418565 submesh
header, 0x4185a1 vertices, 0x4188ad index-byte advancement. This does not
execute animations, apply envelopes, load materials or run gameplay.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import struct
from bk3_assets import Archive


def mesh_info(data):
    pos = 0
    meshes = submeshes = vertices = indices = 0
    while pos < len(data):
        if pos + 72 > len(data):
            raise ValueError('truncated mesh header')
        count = struct.unpack_from('<I', data, pos+68)[0]
        if count > 4096:
            raise ValueError('unsupported mesh count/layout')
        meshes += 1
        pos += 72
        for _ in range(count):
            if count != 1:
                # Grouped meshes carry an extra named child header (0x41893e).
                if pos + 72 > len(data) or struct.unpack_from('<I', data, pos+68)[0] != 1:
                    raise ValueError('unsupported grouped child header')
                pos += 72
            if pos + 332 > len(data):
                raise ValueError('truncated submesh header')
            nv, ni = struct.unpack_from('<II', data, pos+64)
            pos += 332
            if ni % 3 or pos + nv*60 + ni*2 > len(data):
                raise ValueError('unsupported vertex/index layout or bounds')
            for v in range(nv):
                fields = struct.unpack_from('<9f', data, pos+v*60)
                if not all(math.isfinite(f) for f in fields):
                    raise ValueError('nonfinite vertex data')
            pos += nv*60
            for i in range(ni):
                if struct.unpack_from('<H',data,pos+i*2)[0] >= nv:
                    raise ValueError('index outside vertex array')
            pos += ni*2
            vertices += nv
            indices += ni
            submeshes += 1
    return {'meshes': meshes, 'submeshes': submeshes, 'vertices': vertices, 'triangles': indices//3}


def inspect(data):
    if len(data) < 12 or data[:4] != b'OBJM':
        raise ValueError('unsupported model signature')
    pos, chunks, geometry = 12, [], []
    while pos < len(data):
        if pos+8 > len(data):
            raise ValueError('truncated chunk header')
        tag = data[pos:pos+4].decode('ascii')
        size = struct.unpack_from('<I',data,pos+4)[0]
        if pos+8+size > len(data):
            raise ValueError('chunk outside model')
        chunks.append({'tag':tag, 'offset':pos, 'size':size})
        if tag == 'MESH':
            geometry.append(mesh_info(data[pos+8:pos+8+size]))
        pos += 8+size
    return {'chunks': chunks, 'geometry': geometry}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data',type=Path)
    parser.add_argument('--output',type=Path,default=Path('local/model-audit.json'))
    args = parser.parse_args()
    results, failures, text_models = [], [], []
    for p in sorted(args.data.glob('*.pp')):
        archive=Archive(p)
        for entry in archive.entries:
            if not entry.name.lower().endswith('.x'):
                continue
            try:
                data = archive.read(entry)
                if data.startswith(b'xof 0302txt 0032'):
                    text_models.append({'archive':p.name, 'name':entry.name,
                                        'format':'DirectX text 0302', 'parsed':False})
                    continue
                results.append({'archive':p.name, 'name':entry.name, **inspect(data)})
            except (ValueError,UnicodeDecodeError,struct.error) as error:
                failures.append({'archive':p.name, 'name':entry.name,'error':str(error)})
    summary={'objm_models_structurally_checked':len(results), 'text_models_not_parsed':text_models, 'failures':failures,
             'chunks':dict(Counter(c['tag'] for m in results for c in m['chunks'])),
             'geometry':{key:sum(g[key] for m in results for g in m['geometry']) for key in ('meshes','submeshes','vertices','triangles')},
             'runtime_model_support':False,'results':results}
    args.output.write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='results'},indent=2))
    if failures:
        raise SystemExit(1)


if __name__=='__main__':
    main()
