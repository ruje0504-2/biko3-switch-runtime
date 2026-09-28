"""Compare every original model's decoded CPU records with its source bytes."""
import argparse
import ctypes as C
import json
from pathlib import Path
import struct
import sys
from model_binding import library, decode, Material

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive

def check(data,m):
    assert C.string_at(m.source,m.source_size)==data
    source_chunks={};pos=12
    while pos<len(data):
        tag=data[pos:pos+4];size=struct.unpack_from('<I',data,pos+4)[0]
        source_chunks[tag]=(pos+8,size);pos+=size+8
    assert len(source_chunks)==m.chunk_count
    for c in m.chunks[:m.chunk_count]:
        assert source_chunks[c.tag]==(c.offset,c.size)
    pos,size=source_chunks[b'MESH'];end=pos+size;mi=si=nvtotal=nitotal=0
    while pos<end:
        name=data[pos:pos+64].split(b'\0')[0];id,count=struct.unpack_from('<II',data,pos+64);pos+=72
        mesh=m.meshes[mi];assert (mesh.name,mesh.id,mesh.first_submesh,mesh.submesh_count)==(name,id,si,count)
        for _ in range(count):
            child_name,child_id=name,id
            if count!=1:
                child_name=data[pos:pos+64].split(b'\0')[0];child_id=struct.unpack_from('<I',data,pos+64)[0];pos+=72
            sub=m.submeshes[si]
            assert (sub.name,sub.id,sub.mesh_index,sub.source_header_offset)==(child_name,child_id,mi,pos)
            nv,ni,nt=struct.unpack_from('<III',data,pos+64)
            assert (sub.vertex_count,sub.index_count,sub.texture_count)==(nv,ni,nt)
            assert sub.material_id==struct.unpack_from('<I',data,pos+4)[0]
            assert list(sub.texture_ids)==list(struct.unpack_from('<4I',data,pos+8))
            if sub.material_id:assert m.materials[sub.material_index].id==sub.material_id
            for k in range(nt):
                if sub.texture_ids[k]:assert m.textures[sub.texture_indices[k]].id==sub.texture_ids[k]
            pos+=332
            assert C.string_at(sub.vertices,nv*60)==data[pos:pos+nv*60]
            pos+=nv*60
            assert C.string_at(sub.indices,ni*2)==data[pos:pos+ni*2]
            pos+=ni*2;si+=1;nvtotal+=nv;nitotal+=ni
        mi+=1
    assert (mi,si,nvtotal,nitotal//3)==(m.mesh_count,m.submesh_count,m.vertex_count,m.triangle_count)
    for tag,stride,records,count in [(b'MATE',140,m.materials,m.material_count),(b'TEXT',204,m.textures,m.texture_count),(b'FRAM',396,m.frames,m.frame_count)]:
        pos,size=source_chunks.get(tag,(0,0));assert size//stride==count
        for i in range(count):
            p=pos+i*stride;v=records[i]
            assert v.name==data[p:p+64].split(b'\0')[0]
            assert v.id==struct.unpack_from('<I',data,p+64)[0]
            if tag==b'MATE':assert C.string_at(C.addressof(v)+Material.diffuse.offset,72)==data[p+68:p+140]
            if tag==b'TEXT':assert v.filename==data[p+68:p+132].split(b'\0')[0]
            if tag==b'FRAM':
                assert C.string_at(v.local,64)==data[p+68:p+132]
                assert (v.parent_id,v.mesh_id)==struct.unpack_from('<II',data,p+172)
                if v.parent_id:assert m.frames[v.parent_index].id==v.parent_id
                if v.mesh_id:assert m.meshes[v.mesh_index].id==v.mesh_id
    return {'meshes':mi,'submeshes':si,'vertices':nvtotal,'triangles':nitotal//3,
            'materials':m.material_count,'textures':m.texture_count,'frames':m.frame_count}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('data',type=Path)
    p.add_argument('--output',type=Path,default=ROOT/'local/cpu-model-audit.json');args=p.parse_args()
    lib=library();results=[];unsupported=[];failures=[]
    for path in sorted(args.data.glob('*.pp')):
        archive=Archive(path)
        for e in archive.entries:
            if not e.name.lower().endswith('.x'):continue
            data=archive.read(e);result,out,error=decode(lib,data)
            try:
                if data.startswith(b'xof 0302txt 0032'):
                    assert result==0 and not out
                    unsupported.append({'archive':path.name,'name':e.name,'format':'DirectX text'})
                else:
                    assert result==1,error
                    results.append({'archive':path.name,'name':e.name,**check(data,out.contents)})
                    world=(C.c_float*(out.contents.frame_count*16))();message=C.create_string_buffer(256)
                    assert lib.bk_model_world_matrices(out,world,len(world),message),message.value
            except AssertionError as ex:failures.append({'archive':path.name,'name':e.name,'error':str(ex)})
            finally:lib.bk_model_destroy(out)
        print(f'{path.name}: {len(results)} OBJM checked so far',flush=True)
    keys=('meshes','submeshes','vertices','triangles','materials','textures','frames')
    summary={'objm_models':len(results),'unsupported_text_models':unsupported,'failures':failures,
             'totals':{k:sum(x[k] for x in results) for k in keys},
             'checks':['owned source bytes','all vertex/index bytes','material values','texture names/IDs','frame matrices/IDs/links','all world matrices computed without overflow'],
             'results':results}
    args.output.write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='results'},indent=2))
    if failures:raise SystemExit(1)

if __name__=='__main__':main()
