"""Unmodified authored XAN plus initial actor request/instant selection."""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from original_clip_oracle import Native, Archive
from clip_binding import library, State, Sample
from model_binding import ROOT

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe);lib=library();error=C.create_string_buffer(256)
    lib.bk_clip_player_create_authored_start.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p];lib.bk_clip_player_create_authored_start.restype=C.c_void_p
    records=[];states=0;samples=0;recovered=0
    for pack in ['bk3_01','bk3_14']:
        ar=Archive(a.data/(pack+'.pp'))
        for entry in ar.entries:
            if not(entry.name.startswith('h0') and entry.name.endswith('.xan') and entry.name[4:6] in ['55','60','61','70','80']):continue
            data=ar.read(entry);clips=lib.bk_clip_set_decode(data,len(data),error);assert clips,(entry.name,error.value)
            slots=[i for i in range(128) if lib.bk_clip_definition(clips,i).contents.active]
            assert slots
            old=struct.unpack_from('<i',data,512+0x140)[0];old_request=struct.unpack_from('<i',data,512+0x148)[0]
            unsupported=lib.bk_clip_player_create_authored(clips,error);was_valid=bool(unsupported);lib.bk_clip_player_destroy(unsupported)
            if not was_valid:recovered+=1
            before=states
            try:
                for slot in slots:
                    for instant in [0,1]:
                        p=lib.bk_clip_player_create_authored_start(clips,slot,instant,error)
                        if not p:
                            assert not instant and old_request==slot and not was_valid,(entry.name,slot,instant,error.value)
                            continue
                        try:
                            n.bind(data,True);n.select(slot,1 if instant else 2)
                            for step in range(41):
                                if step:
                                    dt=C.c_float([0,1/120,.02,.1,1,10,100][step%7]).value
                                    if step%9==0:
                                        target=slots[(step//9)%len(slots)];n.select(target,2);assert lib.bk_clip_request(p,target,error)
                                    expected=n.step(dt);sample=Sample();assert lib.bk_clip_advance(p,dt,C.byref(sample),error),error.value
                                    actual=[getattr(sample,k) for k,_ in Sample._fields_]
                                    assert actual==list(expected),(entry.name,slot,instant,step,actual,expected);samples+=1
                                state=State();assert lib.bk_clip_state(p,C.byref(state))
                                actual=[getattr(state,k) for k,_ in State._fields_];expected=n.state()
                                assert actual==expected,(entry.name,slot,instant,step,actual,expected);states+=1
                        finally:lib.bk_clip_player_destroy(p)
                records.append(dict(archive=pack,entry=entry.name,sha256=hashlib.sha256(data).hexdigest(),original_current=old,original_requested=old_request,strict_authored_valid=was_valid,active_slots=slots,state_comparisons=states-before))
                print(pack,entry.name,'PASS',states-before,'states',flush=True)
            finally:lib.bk_clip_set_destroy(clips)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),files=len(records),previously_rejected=recovered,states=states,samples=samples,records=records,native_functions=['0x401b0a','0x401d24','0x4026fe'],scope='Unmodified actual XAN header/counters, load then initial request/instant select, subsequent scheduler/repeated requests. Only SRT submissions substituted. Inactive old source is retained for the transition; same-request invalid seeds still reject.')
    (ROOT/'local/original-actor-start-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(records),'files;',states,'states;',samples,'samples;',recovered,'previously rejected authored files')
if __name__=='__main__':main()
