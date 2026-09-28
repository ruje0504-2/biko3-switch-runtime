"""All128 retained animation slots vs original XAN load/request/advance."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from original_clip_oracle import Native
from original_player_interaction_oracle import Timing
from clip_binding import library,Sample
from model_binding import ROOT
from bk3_assets import Archive

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('exe',type=Path);p.add_argument('data',type=Path);args=p.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();rng=random.Random(0x1f0)
    lib.bk_clip_timing.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Timing)]
    lib.bk_clip_player_create_authored_start.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p];lib.bk_clip_player_create_authored_start.restype=C.c_void_p
    arc=Archive(args.data/'bk3_01.pp');records=[];error=C.create_string_buffer(256);checks=0
    for entry in arc.entries:
        if not (entry.name.startswith('h0') and entry.name.endswith('.xan')):continue
        data=arc.read(entry);slots=[i for i in range(128) if struct.unpack_from('<2f',data,512+0x190+i*156+0x54)!=(0,0)]
        definition=lib.bk_clip_set_decode(data,len(data),error);assert definition,error.value
        player=lib.bk_clip_player_create_authored_start(definition,0,1,error);assert player,error.value
        try:
            native.bind(data,True);native.select(0,1)
            for step in range(96):
                if step%8==0:
                    slot=rng.choice(slots);native.select(slot,2);assert lib.bk_clip_request(player,slot,error),error.value
                seconds=C.c_float(rng.choice([0,.016,.1,1,5])).value
                native.step(seconds);sample=Sample();assert lib.bk_clip_advance(player,seconds,C.byref(sample),error),error.value
                for i in range(128):
                    t=Timing();assert lib.bk_clip_timing(player,i,C.byref(t))
                    address=native.obj+i*156
                    expected=bytes(native.uc.mem_read(address+0x1e4,8))+bytes(native.uc.mem_read(address+0x1f0,4))
                    assert bytes(t)==expected,(entry.name,step,i,list(struct.unpack('<3f',expected)),t.start,t.end,t.source)
                    checks+=1
            records.append(dict(file=entry.name,sha256=hashlib.sha256(data).hexdigest(),steps=96));print(entry.name,'PASS',flush=True)
        finally:lib.bk_clip_player_destroy(player);lib.bk_clip_set_destroy(definition)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=len(records),slot_snapshots=checks,exact_bytes=True,records=records,hooks=['SRT submission boundary only'],scope='Original authored retained slot times across requests/advances; accessor does not replace active clip with requested action.')
    (ROOT/'local/original-clip-timing-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
