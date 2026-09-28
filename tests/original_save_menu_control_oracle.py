"""Original507540 full save/load list and confirmation input dispatcher.
No rendering is invoked by this component. File, detail-resource, text-reset,
pause-reentry and release services are traced boundaries; original bank copying
into gameplay fields and flow scheduler run natively. Codec has separate oracle.
"""
import argparse,ctypes as C,hashlib,json,random,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_pause_oracle import Native as Base,Bindings,Ops,Input,Sound,Warp,Pointer,Release,Remove
from original_common_hud_oracle import State as Common,Flow
from original_item_notice_oracle import Fade
from model_binding import ROOT,library
class State(C.Structure):
    _fields_=[('cursor',C.c_float*2)]+[(x,C.c_int32) for x in ['tab','skip_draw','last_hover','hover','confirm_column','confirm_hover','page','column','row']]+[(x,C.c_float*4) for x in ['back','yes','no']]
class SaveInput(C.Structure):_fields_=[('ui',Input),('current_group',C.c_uint32),('occupied',(C.c_uint8*10)*5)]
Slot=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p)
Group=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint,C.c_void_p)
Action=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p)
class SaveOps(C.Structure):_fields_=[('menu',Ops),('load',Slot),('store',Slot),('refresh',Group),('clear',Action),('details',Group),('reset',Action),('resume',Action)]
FIELDS=dict(tab=0x589308,skip_draw=0xbf410c,last_hover=0xbf4110,hover=0xbf411c,confirm_column=0xbf4b28,confirm_hover=0xbf4b2c,page=0xbf4b30,column=0xbf4b34,row=0xbf4b38)
def snap(s,c,f,latch,game):return (bytes(s),c.action,c.blocked,bytes(f),latch,tuple(game))
class Native(Base):
    def __init__(self,exe):
        super().__init__(exe)
        for a in [0x509fd2,0x509c25,0x50a2ae,0x50c2b6,0x50ace3,0x4ec9c8,0x4ec87c]:self.u.hook_add(UC_HOOK_CODE,self.hook,begin=a,end=a)
    def read_control(self):
        s=State();s.cursor[:]=[self.rf(0xbf4b20),self.rf(0xbf4b24)]
        for name,a in FIELDS.items():setattr(s,name,C.c_int32(self.ri(a)).value)
        for name,slot in [('back',1),('yes',10),('no',12)]:getattr(s,name)[:]=list(self.read_sprite(slot).rect)
        c=Common();c.action=self.u.mem_read(0xbeeb7e,1)[0];c.blocked=self.u.mem_read(0xbeeb7f,1)[0]
        f=Flow(*(self.u.mem_read(a,1)[0] for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c]))
        game=[self.ri(0x7219a8),self.ri(0x7219ac),*self.u.mem_read(0x71bcdc,8),self.ri(0x734050)]
        return snap(s,c,f,self.u.mem_read(0xbef178,1)[0],game)
    def trace_event(self,*event):self.trace.append((*event,self.read_control()))
    def hook(self,u,a,size,data):
        events={0x509fd2:('load',2),0x509c25:('store',2),0x50a2ae:('refresh',1),0x50c2b6:('clear',0),0x50ace3:('details',1),0x4ec9c8:('reset',0),0x4ec87c:('resume',0),0x4e77bf:('release',1)}
        if a in events:
            name,n=events[a];sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4)) if n else ()
            self.trace_event(name,*args);u.reg_write(UC_X86_REG_EIP,self.ri(sp));u.reg_write(UC_X86_REG_ESP,sp+4);return
        super().hook(u,a,size,data)
    def run(self,s,c,f,latch,inp,point,game,records):
        self.trace=[];self.buttons=inp.ui.buttons;self.point=list(point);self.motion=[0.,0.]
        for name,a in FIELDS.items():self.wi(a,getattr(s,name)&0xffffffff)
        for i,p in enumerate(s.cursor):self.wf(0xbf4b20+i*4,p)
        for name,slot in [('back',1),('yes',10),('no',12)]:
            for off,v in zip([0x114,0x118,0x10c,0x110],getattr(s,name)):self.wf(0x734058+slot*0x16c+off,v)
        for a,v in [(0xbeeb84,f.current),(0x721ad4,f.previous),(0xbfbbb9,f.target),(0xbfbb9c,f.mode),(0xbeeb7e,c.action),(0xbeeb7f,c.blocked),(0xbeeb4c,c.curtain.stage),(0xbef178,latch)]:self.u.mem_write(a,bytes([v]))
        self.wf(0x721ad0,inp.ui.scale);self.wi(0x7219a8,game[0]);self.wi(0x7219ac,game[1]);self.u.mem_write(0x71bcdc,bytes(game[2:10]));self.wi(0x734050,game[10]);self.u.mem_write(0x721b14,struct.pack('<5I',*range(777,782)))
        for g in range(5):
            for k in range(10):
                base=0xb53c40+g*560+k*56;self.wi(base+4,records[g][k][0]);self.u.mem_write(base+8,bytes(records[g][k][1:]));self.u.mem_write(base+24,bytes([inp.occupied[g][k]]))
        self.call(0x507540,struct.pack('<I',int(f.previous==0x20)))
        return self.read_control(),self.trace,self.point

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);a=ap.parse_args();exe=a.exe.read_bytes();n=Native(exe);lib=library();rng=random.Random(0x507540);e=C.create_string_buffer(256)
    lib.bk_save_menu_control.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(SaveInput),C.POINTER(SaveOps),C.c_void_p]
    steps=events=loads=stores=0
    for case in range(12000):
        scale=C.c_float(rng.choice([320,640,1001,1280,1920])/1280).value
        s=State();s.tab=rng.randrange(3,8);s.skip_draw=rng.randrange(3);s.last_hover=rng.choice([1,21,30,99]);s.hover=rng.randrange(21,31);s.confirm_column=rng.randrange(2);s.confirm_hover=rng.choice([10,12,99]);s.page=rng.choice([0,0,1,1,7]);s.column=rng.randrange(5);s.row=rng.randrange(12);s.cursor[:]=[rng.random(),rng.random()]
        for name,r in [('back',[976,880,256,56]),('yes',[368,520,256,56]),('no',[656,520,256,56])]:getattr(s,name)[:]=[C.c_float(x*scale).value for x in r]
        c=Common();c.curtain=Fade(1,2,rng.randrange(6));c.action=rng.choice([0,1,1,2]);c.blocked=rng.choice([0,1,1,2]);f=Flow(0x28,rng.choice([1,4,0x20]),rng.randrange(256),rng.randrange(4));latch=rng.choice([0,1,255]);game=[rng.randrange(5),rng.randrange(9),*rng.randbytes(8),666]
        inp=SaveInput(Input(rng.randrange(32),0,0,scale,0),game[0]);records=[]
        for g in range(5):
            inp.occupied[g][:]=[rng.choice([0,1,255]) for _ in range(10)];records.append([[rng.randrange(9),*rng.randbytes(8)] for _ in range(10)])
        if case%8==0:
            s.page=1;f.previous=0x20 if case%16==0 else 1;inp.ui.buttons=1
        choices=[(0,0),(1050,910),(495,548),(789,548),(430+168*rng.randrange(6),170),(750,228+54*rng.randrange(10)),(750,282+54*rng.randrange(9)),(1062,770)]
        x,y=rng.choice(choices)
        if case%8==0:x,y=495,548
        point=[C.c_float((x+rng.choice([0,.001,-.001]))*scale).value,C.c_float(y*scale).value]
        want,wt,wp=n.run(s,c,f,latch,inp,point,game,records);trace=[];current=list(point);la=C.c_uint8(latch)
        def state():return snap(s,c,f,la.value,game)
        def event(*v):trace.append((*v,state()));return 1
        @Sound
        def sound(_,slot,e):return event('sound',slot)
        @Warp
        def warp(_,x,y,e):current[:]=[x,y];return event('warp',x,y)
        @Pointer
        def pointer(_,p,m,e):p[0],p[1]=current;m[0]=m[1]=0;return 1
        @Release
        def release(_,flow,e):return event('release',flow)
        @Slot
        def load(_,g,k,e):
            event('load',g,k);game[:]=[g,*records[g][k],777+g];return 1
        @Slot
        def store(_,g,k,e):return event('store',g,k)
        @Group
        def refresh(_,g,e):return event('refresh',g)
        @Group
        def details(_,g,e):return event('details',g)
        @Action
        def clear(_,e):return event('clear')
        @Action
        def reset(_,e):return event('reset')
        @Action
        def resume(_,e):return event('resume')
        ops=SaveOps(Ops(None,sound,warp,pointer,release,Remove()),load,store,refresh,clear,details,reset,resume)
        bindings=Bindings(C.pointer(c),C.pointer(f),None,None,C.pointer(la))
        assert lib.bk_save_menu_control(C.byref(s),C.byref(bindings),C.byref(inp),C.byref(ops),e),(case,e.value)
        assert state()==want,(case,'state',[(i,x,y) for i,(x,y) in enumerate(zip(state(),want)) if x!=y])
        assert trace==wt,(case,'trace',trace,wt)
        assert current==wp,(case,'pointer',current,wp)
        steps+=1;events+=len(trace);loads+=sum(v[0]=='load' for v in trace);stores+=sum(v[0]=='store' for v in trace)
    assert loads>100 and stores>100,(loads,stores)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),steps=steps,events=events,loads=loads,stores=stores,scope=__doc__)
    (ROOT/'local/original-save-menu-control-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS save-menu-control',report)
if __name__=='__main__':main()
