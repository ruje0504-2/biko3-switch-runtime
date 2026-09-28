"""Check 0x4018c8 configured-duration requests, including repeated/aliased chains."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from original_clip_oracle import Native,run
from clip_binding import library
from model_binding import ROOT
from bk3_assets import Archive

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();arc=Archive(args.data/'bk3_01.pp');records=[]
    for group in range(6):
        entry=f'h0{group}_80.xan';data=arc.read(next(e for e in arc.entries if e.name==entry));slots=[i for i in range(128) if struct.unpack_from('<2f',data,512+0x190+i*156+0x54)!=(0,0)]
        count,worst=run(native,lib,data,slots,[0,1/60,.1,.25,2]*30,instant=3,authored=True,repeat=True,repeat_mode=3,switches={10:(slots[-1],2),40:(slots[0],1),70:(slots[-1],3),90:(slots[0],2)})
        records.append(dict(entry=entry,checks=count,max_normalized_error=worst));print(entry,count,'PASS',worst,flush=True)
    synthetic=0;error=0
    for case in range(24):
        data=bytearray(0x5190);data[:6]=b'test.x';data[256:262]=b'test.x'
        for i in range(3):
            p=512+0x190+i*156
            struct.pack_into('<i',data,p,case%2);struct.pack_into('<i',data,p+0x48,case%3);struct.pack_into('<i',data,p+0x50,8)
            struct.pack_into('<2f',data,p+0x54,5,80)
            struct.pack_into('<3i',data,p+0x70,1,(i+1)%3,case%3)
            struct.pack_into('<f',data,p+0x7c,[0,1e-7,2e-6,.125,5,15][case%6])
        n,e=run(native,lib,bytes(data),[0,1,2],[0,1/60,.1,.25,2]*40,instant=3,repeat=True,repeat_mode=3,switches={20:(1,2),60:(2,1),100:(0,3),140:(1,2)})
        synthetic+=n;error=max(error,e)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),records=records,asset_checks=sum(r['checks'] for r in records),synthetic_checks=synthetic,max_normalized_error=max([error]+[r['max_normalized_error'] for r in records]),native_functions=['0x4018c8','0x401b0a','0x401d24','0x4026fe'],hooks=['0x4097d6 / 0x409a94 SRT submission capture'],scope='Configured request compares requested, preserves mutable per-slot blend duration, and combines with ten-tick/instant requests and native chain scheduling. No actor policy implied.',x87_control_word='0x037f')
    (ROOT/'local/original-configured-request-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report['asset_checks'],synthetic,'checks; max error',report['max_normalized_error'])
if __name__=='__main__':main()
