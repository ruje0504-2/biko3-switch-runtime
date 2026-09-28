"""Inventory camera-named assets without claiming their scene/time bindings."""
import argparse
import hashlib
import json
from pathlib import Path
from bk3_assets import Archive
from inspect_objm import inspect

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive',type=Path)
    p.add_argument('--output',type=Path,default=Path('reports/camera-inventory.json'))
    args=p.parse_args();archive=Archive(args.archive);records=[]
    for entry in archive.entries:
        if not entry.name.lower().startswith('cam') or not entry.name.lower().endswith('.x'):continue
        data=archive.read(entry)
        item={'entry':entry.name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
        if data[:4]==b'OBJM':
            info=inspect(data);item['format']='OBJM'
            item['chunks']=[{'tag':c['tag'],'size':c['size']} for c in info['chunks']]
            frames=next(c for c in info['chunks'] if c['tag']=='FRAM')
            if frames['size']%396:raise ValueError('unsupported FRAM layout')
            names=[data[frames['offset']+8+i*396:frames['offset']+8+i*396+64].split(b'\0')[0].decode('ascii') for i in range(frames['size']//396)]
            item['frame_names']=names
            item['candidate_nodes_by_name_only']=[n for n in names if n.lower().startswith('cam')]
        elif data.startswith(b'xof 0302txt 0032'):
            item['format']='DirectX text';item['parsed']=False
        else:raise ValueError(f'unknown camera format: {entry.name}')
        records.append(item)
    report={'archive':args.archive.name,'assets':records,'asset_count':len(records),
            'objm_count':sum(r['format']=='OBJM' for r in records),
            'text_count':sum(r['format']=='DirectX text' for r in records),
            'office_scene_binding_verified':False,'animation_time_mapping_verified':False,
            'scope':'asset structure and node names only; no playable camera or original pose claimed'}
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(f"Camera inventory: {report['objm_count']} OBJM, {report['text_count']} unparsed DirectX text; bindings unverified")
if __name__=='__main__':main()
