"""Run original clip selection/advance with only animation submission hooked.

The original code receives fresh runtime counters (not stale XAN pointers).
All clip definitions are original bytes. The native scheduler and auto-chain
remain unhooked. SRT submission is independently checked by the blend oracle.
"""
import argparse, ctypes as C, hashlib, json, random, struct, sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_matrix_oracle import machine
from clip_binding import library,State,Sample
from model_binding import ROOT
sys.path.insert(0,str(ROOT/'tools'))
from bk3_assets import Archive
class Native:
    obj=0x3000000; model=0x3006000; root=0x3007000
    stack=0x2008000; stop=0x300f000
    def __init__(self,exe):
        self.uc=machine(exe)
        for address in [0x4097d6,0x409a94]:
            self.uc.hook_add(UC_HOOK_CODE,self.submit,begin=address,end=address)
    def submit(self,u,addr,size,user):
        esp=u.reg_read(UC_X86_REG_ESP)
        ret=struct.unpack('<I',u.mem_read(esp,4))[0]
        if addr==0x4097d6:
            t=struct.unpack('<f',u.mem_read(esp+8,4))[0];self.calls.append((0,t,t,0))
        else:
            a,b,t=struct.unpack('<3f',u.mem_read(esp+8,12));self.calls.append((1,a,b,t))
        u.reg_write(UC_X86_REG_ESP,esp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def bind(self,data,authored=False):
        payload=bytearray(data[512:])
        if not authored:
            payload[:0x190]=bytes(0x190)
            struct.pack_into('<i',payload,0x148,-1)
            for i in range(128):
                c=0x190+i*0x9c
                for off in [0x44,0x5c,0x60,0x64,0x6c]:payload[c+off:c+off+4]=bytes(4)
        # The original loader sets enabled and replaces all model pointers.
        struct.pack_into('<I',payload,0x18c,0)
        self.uc.mem_write(self.obj,bytes(payload))
        self.uc.mem_write(self.model,bytes(0x1000));self.uc.mem_write(self.root,bytes(0x1000))
        self.word(self.obj+0x160,self.model);self.word(self.model+0x14,self.root)
        self.word(self.model+0x148,0x123456)
    def word(self,p,v):self.uc.mem_write(p,struct.pack('<I',v))
    def call(self,addr,args):
        self.calls=[];self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
        self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
        self.uc.emu_start(addr,self.stop,count=100000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==self.stop
    def select(self,slot,instant):self.call([0x401f71,0x401d24,0x401b0a,0x4018c8][instant],struct.pack('<II',self.obj,slot))
    def step(self,seconds):self.call(0x4026fe,struct.pack('<If',self.obj,seconds));assert len(self.calls)==1;return self.calls[0]
    def state(self):
        b=self.uc.mem_read(self.obj,0x4f90)
        def i(off):return struct.unpack_from('<i',b,off)[0]
        def f(off):return struct.unpack_from('<f',b,off)[0]
        slot=i(0x140);c=0x190+slot*0x9c
        return [slot,i(0x148),i(0x184),i(0x188),i(0x164),i(c+0x64),f(c+0x44),f(c+0x60),f(c+0x5c),f(0x168),f(0x16c),f(0x170)]
def run(native,lib,data,slots,steps,instant=1,switches=None,authored=False,repeat=False,repeat_mode=2):
    err=C.create_string_buffer(256);definition=lib.bk_clip_set_decode(data,len(data),err)
    assert definition,err.value
    count=0;worst=0
    try:
        for slot in slots:
            create=lib.bk_clip_player_create_authored if authored else lib.bk_clip_player_create
            p=create(definition,err);assert p,err.value
            try:
                def select(target,mode):
                    native.select(target,mode)
                    ok=lib.bk_clip_request_mode(p,target,1,err) if mode==3 else lib.bk_clip_request(p,target,err) if mode==2 else lib.bk_clip_select(p,target,mode,err)
                    assert ok,err.value
                native.bind(data,authored)
                if instant is not None:select(slot,instant)
                state=State();sample=Sample()
                for index,dt in enumerate([None]+steps):
                    if switches and index in switches:
                        target,mode=switches[index];select(target,mode)
                    if repeat:select(slot,repeat_mode)
                    if dt is not None:
                        dt=C.c_float(dt).value;expected=native.step(dt)
                        assert lib.bk_clip_advance(p,dt,C.byref(sample),err),err.value
                        actual=[getattr(sample,n) for n,_ in Sample._fields_]
                        for a,b in zip(actual,expected):
                            e=abs(a-b)/max(1,abs(b));worst=max(worst,e)
                            assert e<=1e-6,('sample',slot,index,dt,actual,expected)
                    assert lib.bk_clip_state(p,C.byref(state))
                    actual=[getattr(state,n) for n,_ in State._fields_];expected=native.state()
                    for a,b in zip(actual,expected):
                        e=abs(a-b)/max(1,abs(b));worst=max(worst,e)
                        assert e<=1e-6,('state',slot,index,dt,actual,expected)
                    count+=1
            finally:lib.bk_clip_player_destroy(p)
    finally:lib.bk_clip_set_destroy(definition)
    return count,worst

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();native=Native(exe);lib=library();arc=Archive(a.data/'bk3_04.pp');records=[]
    for e in arc.entries:
        if not(e.name.startswith('cam') and e.name.endswith('.xan')):continue
        data=arc.read(e);slots=[i for i in range(128) if struct.unpack_from('<2f',data,512+0x190+i*156+0x54)!=(0,0)]
        count,worst=run(native,lib,data,slots,[1/120]*2100+[0,.001,.1,1,10,100]*4)
        # Reselection after partially consuming a clip, including chain targets.
        extra,error=run(native,lib,data,slots,[1/120]*40,switches={5:(slots[-1],0),20:(slots[0],1)})
        records.append({'entry':e.name,'sha256':hashlib.sha256(data).hexdigest(),'slots':slots,'checks':count+extra,'max_normalized_error':max(worst,error)})
        print(e.name,count+extra,'PASS',max(worst,error),flush=True)
    synthetic=[];rng=random.Random(340)
    for index in range(60):
        data=bytearray(0x5190);data[:6]=b'test.x';data[256:262]=b'test.x'
        for i in range(3):
            off=512+0x190+i*156;loop=(index+i)%2
            struct.pack_into('<i',data,off,loop);struct.pack_into('<i',data,off+0x48,(index+i)%3)
            struct.pack_into('<i',data,off+0x50,8+index%8)
            start,end=(5.,80.) if index%3 else (80.,5.)
            struct.pack_into('<2f',data,off+0x54,start,end)
            if index%2:struct.pack_into('<3i',data,off+0x70,1,(i+1)%3,1+index%3)
            struct.pack_into('<f',data,off+0x7c,[0,1e-7,1e-6,2e-6,.5,2,8][index%7])
        count,worst=run(native,lib,bytes(data),[0,1,2],[0,0,1/120]+[rng.choice([0,1/120,.01,.1,.5,2]) for _ in range(200)],instant=index%2,switches={50:(1,0),100:(2,1)})
        synthetic.append({'checks':count,'max_normalized_error':worst})
    report={'passed':True,'exe_sha256':hashlib.sha256(exe).hexdigest(),'native_functions':['0x401d24','0x401f71','0x4025b9','0x4026fe'],'scope':'Fresh camera clip playback; SRT dispatch boundary hooked, actual scheduler unhooked. Disk pointers/counters deliberately not restored. Does not test gameplay dispatch.','assets':len(records),'checks':sum(r['checks'] for r in records),'synthetic_checks':sum(r['checks'] for r in synthetic),'max_normalized_error':max(r['max_normalized_error'] for r in records+synthetic),'records':records}
    (ROOT/'local/original-clip-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('TOTAL',report['checks'],report['synthetic_checks'],report['max_normalized_error'],flush=True)
if __name__=='__main__':main()
