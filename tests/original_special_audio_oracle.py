"""Original special loader/audio wrappers drive a second actual PCM mixer.

Run4e2c11..4e2d1e and4e3e50..4e4187, full50d4fa/50d858/50db23,
46435e,50d2a0 and direct Play. Only resource/DirectSound boundaries are
supplied. Native commands feed real Japanese WAV buffers and a separate
offline mixer; this validates transport/epochs/gains, not Windows DSP.
"""
import argparse,ctypes as C,hashlib,json,struct,tempfile
from bk3_assets import Archive
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
from original_prop_route_oracle import Native as Base
from original_special_event_oracle import AudioCall,Envelope
from playback_binding import library
from model_binding import ROOT
P,I,U,F=C.c_void_p,C.c_int32,C.c_uint32,C.c_float
Submit=C.CFUNCTYPE(C.c_int,P,C.POINTER(C.c_int16),C.c_size_t,P)
Poll=C.CFUNCTYPE(C.c_int,P,C.POINTER(C.c_uint64),P)
class Sink(C.Structure):_fields_=[('context',P),('rate',U),('block',U),('capacity',U),('submit',Submit),('poll',Poll)]
class Cursor(C.Structure):_fields_=[('pcm',P),('source_frame',C.c_size_t),('playing',C.c_int),('buffered',C.c_int),('pending',C.c_int)]
FIRST=8
def bind(lib):
    api=[('bk_resources_create',[P],P),('bk_resources_destroy',[P],None),('bk_resources_mount',[P,C.c_char_p,C.c_char_p,P],C.c_int),
        ('bk_resources_mount_directory',[P,C.c_char_p,C.c_char_p,C.c_size_t,P],C.c_int),
        ('bk_audio_create',[C.POINTER(Sink),P],P),('bk_audio_destroy',[P],None),
        ('bk_audio_clip_load',[P,C.c_char_p,C.c_char_p,P],P),('bk_audio_clip_release',[P],None),
        ('bk_audio_play',[P,U,P,C.c_int,I,I,P],C.c_int),('bk_audio_resume',[P,U,C.c_int,P],C.c_int),
        ('bk_audio_gain',[P,U,I,I,P],C.c_int),('bk_audio_get_gain',[P,U,C.POINTER(I),C.POINTER(I)],C.c_int),
        ('bk_audio_clear',[P,U,P],C.c_int),('bk_audio_playing',[P,U,C.POINTER(C.c_int)],C.c_int),
        ('bk_audio_poll',[P,P],C.c_int),('bk_audio_fill',[P,P],C.c_int),('bk_audio_cursor',[P,U,C.POINTER(Cursor)],C.c_int),
        ('bk_pcm_frames',[P],C.c_size_t),('bk_pcm_channels',[P],U),('bk_pcm_samples',[P],C.POINTER(C.c_int16)),
        ('bk_special_audio_create',[P,P,U,U,I,C.POINTER(C.c_uint8),P],P),('bk_special_audio_destroy',[P],None),
        ('bk_special_audio_stop',[P,P],C.c_int),('bk_special_audio_call',[P,C.POINTER(AudioCall),I,I,I,P],C.c_int),
        ('bk_special_audio_level',[P,U,C.POINTER(Envelope),F,C.POINTER(F),P],C.c_int),
        ('bk_special_audio_present',[P,U,C.POINTER(C.c_int)],C.c_int),('bk_special_audio_music_volume',[P,C.POINTER(I)],C.c_int)]
    for name,args,result in api:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
class Output:
    def __init__(self,lib,error):
        self.consumed=self.submitted=self.samples=self.nonzero=0;self.pcm=b'';self.digest=hashlib.sha256()
        @Submit
        def submit(_,pcm,frames,e):
            raw=C.string_at(pcm,frames*4);self.pcm+=raw;self.digest.update(raw)
            self.nonzero+=sum(x!=0 for x in pcm[:frames*2]);self.samples+=frames*2;self.submitted+=frames;return 1
        @Poll
        def poll(_,out,e):out[0]=self.consumed;return 1
        self.sink=Sink(None,48000,480,1920,submit,poll);self.mix=lib.bk_audio_create(C.byref(self.sink),error);assert self.mix,error.value
    def advance(self,lib,error,frame):
        self.consumed+=min(self.submitted-self.consumed,[0,240,480,960,1920][frame%5]);self.pcm=b''
        assert lib.bk_audio_poll(self.mix,error),error.value
class Native(Base):
    buffers,table,stubs=0x3004000,0x3005000,0x3006000
    def __init__(self,exe,lib,store,mix,error):
        super().__init__(exe);self.lib,self.store,self.mix,self.error=lib,store,mix,error
        self.clips=[None]*5;self.submitted=[False]*5;self.volume=[0]*5;self.pan=[0]*5;self.rewind=[False]*5
        self.names=[None]*5;self.commands=self.loads=0
        for offset in [8,0x24,0x30,0x34,0x3c,0x40]:
            self.word(self.table+offset,self.stubs+offset);self.u.hook_add(UC_HOOK_CODE,self.hook,begin=self.stubs+offset,end=self.stubs+offset)
        for i in range(5):self.word(self.buffers+i*32,self.table)
        for addr in [0x50d4fa,0x50d858,0x4ad8ec,0x51ceee,0x4ad050,0x463311,0x520538,0x50de05]:
            self.u.hook_add(UC_HOOK_CODE,self.hook,begin=addr,end=addr)
    def word(self,p,v):self.u.mem_write(p,struct.pack('<I',v&0xffffffff))
    def read(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
    def string(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0] if p else b''
    def hook(self,u,addr,size,ctx):
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);a=struct.unpack('<5I',u.mem_read(sp+4,20));pop=4;result=0
        if addr in [0x50d4fa,0x50d858]:
            self.slot=4 if addr==0x50d4fa else (a[0]-0x722464)//0x120;assert 0<=self.slot<5;return
        if addr==0x4ad8ec:u.mem_write(a[0],self.string(a[1])+self.string(a[2])+b'\0')
        elif addr==0x51ceee:
            assert self.string(a[0]).lstrip(b'\\')==b'bk3_02.pp'
            self.pending=self.string(a[1]).lstrip(b'\\');self.word(a[2],0x300e000);self.word(a[3],100)
        elif addr in [0x4ad050,0x463311]:
            if addr==0x463311:self.pending=self.string(a[0]).split(b'\\')[-1]
            s=self.slot;assert self.lib.bk_audio_clear(self.mix,FIRST+s,self.error)
            self.lib.bk_audio_clip_release(self.clips[s]);self.clips[s]=self.lib.bk_audio_clip_load(self.store,b'bk3_02',self.pending,self.error)
            assert self.clips[s],self.error.value
            self.names[s]=self.pending.decode();self.volume[s]=self.pan[s]=0;self.submitted[s]=self.rewind[s]=False
            self.loads+=1;result=self.buffers+s*32
        elif addr==0x520538:pass
        elif addr==0x50de05:result=65536 #cached buffer-size field unused by these wrappers
        else:
            s=(a[0]-self.buffers)//32;assert 0<=s<5;offset=addr-self.stubs;self.commands+=1
            if offset==8:
                assert self.lib.bk_audio_clear(self.mix,FIRST+s,self.error)
                self.lib.bk_audio_clip_release(self.clips[s]);self.clips[s]=None;self.submitted[s]=False;pop=8
            elif offset==0x24:
                playing=C.c_int();assert self.lib.bk_audio_playing(self.mix,FIRST+s,C.byref(playing));self.word(a[1],playing.value);pop=12
            elif offset==0x34:assert a[1]==0;self.rewind[s]=True;pop=12
            elif offset==0x30:
                assert a[1]==a[2]==0;loop=a[3]&1
                if self.submitted[s] and not self.rewind[s]:assert self.lib.bk_audio_resume(self.mix,FIRST+s,loop,self.error)
                else:assert self.lib.bk_audio_play(self.mix,FIRST+s,self.clips[s],loop,self.volume[s],self.pan[s],self.error)
                self.submitted[s]=True;self.rewind[s]=False;pop=20
            else:
                assert offset in [0x3c,0x40];value=I(a[1]).value
                (self.volume if offset==0x3c else self.pan)[s]=value
                if self.submitted[s]:assert self.lib.bk_audio_gain(self.mix,FIRST+s,self.volume[s],self.pan[s],self.error),self.error.value
                pop=12
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+pop);u.reg_write(UC_X86_REG_EIP,ret)
    def fragment(self,start,end):
        self.u.reg_write(UC_X86_REG_EBP,self.stack);self.u.reg_write(UC_X86_REG_ESP,self.stack-0x3000);self.u.reg_write(UC_X86_REG_FPCW,0x37f)
        self.u.emu_start(start,end,count=1000000);assert self.u.reg_read(UC_X86_REG_EIP)==end
    def entry(self,g,volume,loops):
        self.word(0x7219a8,g);self.word(0xbe9a10,volume);self.u.mem_write(0x5767c8,b'\1')
        for i in range(4):self.u.mem_write(0x72257d+i*0x120,bytes([loops[i]]))
        self.fragment(0x4e2c11,0x4e2d1e);self.fragment(0x4e3e50,0x4e4187)
    def apply(self,c,masters):
        for p,v in zip([0xbe9a0c,0xbe9a08,0xbe9a10],masters):self.word(p,v)
        slot=c.slot;ptr=self.buffers+slot*32
        if c.operation==0:self.call(0x50db23,struct.pack('<IfI',0x722114,c.amount,c.flags))
        elif c.operation==1:
            self.u.mem_write(0x300a000,c.name+b'\0');self.u.mem_write(0x300a200,(c.pack or b'')+b'\0')
            self.call(0x50d858,struct.pack('<IIIiI',0x722464+slot*0x120,0x300a200 if c.pack else 0,0x300a000,c.volume,c.flags))
        elif c.operation==2:self.call(0x46435e,struct.pack('<II',ptr,c.flags))
        elif c.operation==3:
            # Direct COM Play is a callback boundary, with no rewind call.
            if self.submitted[slot]:assert self.lib.bk_audio_resume(self.mix,FIRST+slot,c.flags&1,self.error)
            else:assert self.lib.bk_audio_play(self.mix,FIRST+slot,self.clips[slot],c.flags&1,self.volume[slot],self.pan[slot],self.error)
            self.submitted[slot]=True;self.commands+=1
        else:self.call(0x50d2a0,struct.pack('<I8fIf',ptr,*c.source,*c.listener,c.flags,c.amount))
    def close(self):
        for clip in self.clips:self.lib.bk_audio_clip_release(clip)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--output',type=Path,default=ROOT/'local/original-special-audio.json');a=ap.parse_args()
    lib=library();bind(lib);e=C.create_string_buffer(256);store=lib.bk_resources_create(e);assert store
    assert lib.bk_resources_mount(store,b'bk3_02',str(a.data/'bk3_02.pp').encode(),e),e.value
    outputs=[Output(lib,e),Output(lib,e)];native=Native(a.exe.read_bytes(),lib,store,outputs[1].mix,e);owner=None
    loops=(C.c_uint8*4)(9,8,7,6);frames=levels=retirements=rejections=0;profiles=[];envelope=Envelope();reference=Envelope()
    def compare():
        for slot in range(5):
            x,y=Cursor(),Cursor();assert lib.bk_audio_cursor(outputs[0].mix,FIRST+slot,C.byref(x));assert lib.bk_audio_cursor(outputs[1].mix,FIRST+slot,C.byref(y))
            assert (x.source_frame,x.playing,x.buffered,x.pending,bool(x.pcm))==(y.source_frame,y.playing,y.buffered,y.pending,bool(y.pcm)),(group,frames,slot,'cursor')
            playing=[]
            for output in outputs:
                value=C.c_int();assert lib.bk_audio_playing(output.mix,FIRST+slot,C.byref(value));playing.append(value.value)
            assert playing[0]==playing[1],(group,frames,slot,'status')
        for output in outputs:assert lib.bk_audio_fill(output.mix,e),e.value
        assert outputs[0].pcm==outputs[1].pcm,(group,frames,'PCM')
    def command(c,masters):
        native.apply(c,masters)
        assert lib.bk_special_audio_call(owner,C.byref(c),*masters,e),(group,frames,c.operation,e.value)
    try:
      for group in range(5):
        volume=-1700-group*100
        native.entry(group,volume,loops)
        owner=lib.bk_special_audio_create(store,outputs[0].mix,FIRST,group,volume,loops,e);assert owner,e.value
        for slot in range(4):assert loops[slot]==native.u.mem_read(0x72257d+slot*0x120,1)[0]
        profiles.append(dict(group=group,music=native.names[4],effect0=native.names[0] if group!=2 else None))
        for t in range(180):
            for output in outputs:output.advance(lib,e,t)
            masters=[[-900,-8000,-10000][t//60],-1500,volume]
            command(AudioCall(operation=0,flags=[0,1,255][t%3],amount=[0,.9,1.1,100,4000][t%5]),masters)
            v=I();assert lib.bk_special_audio_music_volume(owner,C.byref(v));assert v.value==I(native.read(0x722218)).value
            if t%30==0:
                name=f'se{400+group*10}.wav'.encode()
                command(AudioCall(operation=1,slot=1,pack=b'bk3_02.pp' if t%60==0 else None,name=name if t%60==0 else b'\\wav\\'+name,volume=volume,flags=(t//30)%3),masters)
                if t%60==0:command(AudioCall(operation=2,slot=1,flags=0),masters)
            if group!=2 and t%10==0:command(AudioCall(operation=3,flags=int(t<90)),masters)
            for slot in range(4):
                present=C.c_int();assert lib.bk_special_audio_present(owner,slot,C.byref(present));assert present.value==bool(native.clips[slot])
                if present.value:
                    c=AudioCall(operation=4,slot=slot,flags=t%3,amount=4);c.source[:]=[3,0,6,99];c.listener[:]=[t*.5,2,-4,t*17]
                    command(c,masters)
            level=F();dt=F([0,1/60,.1][t%3]).value
            assert lib.bk_special_audio_level(owner,1,C.byref(envelope),dt,C.byref(level),e)
            cursor=Cursor();assert lib.bk_audio_cursor(outputs[1].mix,FIRST+1,C.byref(cursor))
            wanted=0
            if cursor.playing and cursor.buffered:
                count=lib.bk_pcm_frames(cursor.pcm)*lib.bk_pcm_channels(cursor.pcm)
                if count>=221:
                    samples=lib.bk_pcm_samples(cursor.pcm);start=cursor.source_frame*lib.bk_pcm_channels(cursor.pcm)
                    target=F((sum(abs(samples[(start+i)%count]) for i in range(220))//110)/512).value
                    reference.target=target;previous=reference.smoothed
                    reference.smoothed=min(target,F(previous+10*dt).value) if previous<target else max(target,F(previous-10*dt).value)
                    reference.smoothed=min(reference.smoothed,9);wanted=reference.smoothed
            assert level.value==wanted and bytes(envelope)==bytes(reference),(group,t,level.value,wanted)
            compare();frames+=1;levels+=1
        assert lib.bk_special_audio_stop(owner,e)
        for slot in range(4):native.call(0x50dac1,struct.pack('<I',0x722464+slot*0x120))
        native.call(0x50d9fd,struct.pack('<I',0x722114))
        # Retired owner's repeated stop and delayed destruction must not
        # clear the next owner's newly issued mixer command.
        guard=lib.bk_audio_clip_load(store,b'bk3_02',b'se002.wav',e);assert guard
        for output in outputs:assert lib.bk_audio_play(output.mix,FIRST,guard,1,-2500,0,e)
        assert lib.bk_special_audio_stop(owner,e);lib.bk_special_audio_destroy(owner);owner=None
        if group==0:
            # Initial music succeeds before a late effect error. Failure must
            # preserve the already active replacement owner and borrowed loops.
            arc=Archive(a.data/'bk3_02.pp')
            with tempfile.TemporaryDirectory(prefix='special-audio-failure-') as folder:
                folder=Path(folder)
                for case in range(3):
                    if case==1:(folder/'bg036.wav').write_bytes(arc.read(next(x for x in arc.entries if x.name=='bg036.wav')))
                    if case==2:(folder/'se120.wav').write_bytes(b'RIFF-corrupt-fixture')
                    bad=lib.bk_resources_create(e);assert bad
                    try:
                        assert lib.bk_resources_mount_directory(bad,b'bk3_02',str(folder).encode(),64*1024*1024,e)
                        before=bytes(loops)
                        assert not lib.bk_special_audio_create(bad,outputs[0].mix,FIRST,0,-500,loops,e)
                        assert bytes(loops)==before
                        v,p=I(),I();assert lib.bk_audio_get_gain(outputs[0].mix,FIRST,C.byref(v),C.byref(p))
                        assert (v.value,p.value)==(-2500,0);rejections+=1
                    finally:lib.bk_resources_destroy(bad)
        compare();lib.bk_audio_clip_release(guard)
        for output in outputs:assert lib.bk_audio_clear(output.mix,FIRST,e)
        retirements+=1
        print('PASS special audio group',group,flush=True)
    finally:
        lib.bk_special_audio_destroy(owner);native.close()
        for output in outputs:lib.bk_audio_destroy(output.mix)
        lib.bk_resources_destroy(store)
    assert outputs[0].nonzero>0
    result=dict(passed=True,exe_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(),frames=frames,levels=levels,native_commands=native.commands,loads=native.loads,retirements=retirements,construction_rejections=rejections,pcm_samples=outputs[0].samples,pcm_sha256=outputs[0].digest.hexdigest(),profiles=profiles,scope=__doc__)
    a.output.write_text(json.dumps(result,indent=2)+'\n');print('PASS special audio:',json.dumps(result),flush=True)
if __name__=='__main__':main()
