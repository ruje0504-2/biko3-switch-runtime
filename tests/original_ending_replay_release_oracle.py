"""Focused original instruction evidence for repeated selected replay release.
Parent48302B and dispatcher4D9575 execute, then each selected legacy destructor
executes against shared pointer slots. Only external IO/resource frees/COM are
observed leaves; this is not a rendered or fully allocated Windows session.
"""
import argparse, ctypes as C, hashlib, json, random, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'tests'),str(ROOT/'tools')]
import original_ending_gallery_control_oracle as parent
import original_ending_reload_oracle as reload
from original_matrix_oracle import machine
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path);parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    exe=args.exe.read_bytes()
    np=parent.Native(exe);nr=reload.Native(exe);nr.image_shapes=0
    rows=[]
    for variant,marker,release_kind in [(0,4,0),(1,1500,3),(1,1000,4)]:
        f=parent.fixture(random.Random(48302),1)
        f.scene.frame.phase=8;f.scene.frame.group=0;f.scene.frame.state_721ee0=0
        f.scene.frame.state_721ee4=marker;f.scene.frame.transition_action=0
        f.scene.frame.curtain_wanted=0;f.scene.auxiliary.variant=variant
        f.final.byte_6ddce0=5 if variant else 2;f.final.byte_6d1be1=f.final.byte_6ddce0
        f.final.word_6c7f74=0;f.final.workspace_6c7f80[0]=13
        f.present=[0,0];f.playing=[0,0];f.hr=[0,0];f.loaded=[0,0]
        f.mutate_at=0;f.mutations={}
        dispatch,trace=np.run(f);assert dispatch.scene.frame.state_721ee0==2
        next_state,trace2=np.run(dispatch)
        assert next_state.scene.frame.transition_action==7 and next_state.scene.frame.curtain_wanted==1
        s=reload.State()
        for name in ['frame','control','auxiliary']:
            dst=getattr(s,name);src=getattr(next_state.scene,name);assert C.sizeof(dst)==C.sizeof(src)
            C.memmove(C.addressof(dst),C.addressof(src),C.sizeof(dst))
        s.previous=24;s.selected=0;s.next_mode=1
        calls,after,early=nr.run(s,1,[0]*48,[0]*48,[0]*48,1.,0,[])
        frees=[event for event,_ in calls if event[0]=='release']
        assert frees==[('release',release_kind)],frees
        rows.append(dict(variant=variant,retained_marker=marker,parent_action=7,
                         requested_destructor=hex(reload.RELEASE[release_kind]),events=[x for x,_ in trace+trace2]))

    proof=[]
    for address in [0x4cfede,0x4d3753,0x4d43f5]:
        u=machine(exe);events=[]
        def word(a,v):u.mem_write(a,struct.pack('<I',v))
        def integer(a):return struct.unpack('<I',u.mem_read(a,4))[0]
        primary=0x3004000;track=0x3004100;sounds=[0x3004200,0x3004240]
        for a in [0x721b28,0x721b2c,0x721b30,0x719b38,0x719b3c,0x722334,0x722454]:word(a,0)
        word(0x721b28,primary);word(0x719b30,track);u.mem_write(0x721b3c,b'\0')
        for i,sound in enumerate(sounds):word(0x722334+i*0x120,sound);word(sound,0x3005000)
        word(0x3005024,0x3006100);word(0x3005048,0x3006200);word(0x3005008,0x3006300)
        word(0x53f108,0x3006000)
        leaves={0x521e94:'node-free',0x4b8a01:'camera-free',0x4aaacc:'bom-free',0x4a0ddc:'face-free',
                0x4014cd:'actor-free',0x50e7c1:'sprite-free',0x3006000:'critical-enter',
                0x3006100:'sound-status',0x3006200:'sound-stop',0x3006300:'sound-release'}
        def hook(_,a,size,context):
            sp=u.reg_read(UC_X86_REG_ESP);ret=integer(sp);arg=integer(sp+4);kind=leaves[a]
            events.append((kind,arg))
            cleanup=4
            if kind=='critical-enter':cleanup=8
            elif kind=='sound-status':word(integer(sp+8),0);cleanup=12
            elif kind.startswith('sound-'):cleanup=8
            u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+cleanup);u.reg_write(UC_X86_REG_EIP,ret)
        for a in leaves:u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
        stop=0x3007000;stack=0x200e000;word(stack,stop);u.reg_write(UC_X86_REG_ESP,stack)
        u.emu_start(address,stop,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==stop
        assert events.count(('actor-free',primary))==1 and integer(0x721b28)==0
        assert events.count(('node-free',track))==1 and integer(0x719b30)==0
        for i,sound in enumerate(sounds):
            assert events.count(('sound-release',sound))==1 and integer(0x722334+i*0x120)==0
        proof.append(dict(address=hex(address),events=events,primary_cleared=True,speech_cleared=True))
    report=dict(passed=True,scope=__doc__,exe_sha256=hashlib.sha256(exe).hexdigest(),
                source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                parent_dispatch_cases=rows,shared_release_cases=proof)
    out=args.report;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS native repeated release',len(rows),'dispatches',len(proof),'shared destructors',out)


if __name__ == "__main__":
    main()
