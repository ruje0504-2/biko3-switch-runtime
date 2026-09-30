"""Original4C59C0/4C7916/4C8768/51B9B3 gallery UI.
Texture, audio, pointer and allocation are service boundaries. Original sprite
constructors, hit tests, input order, transitions, name formatting, resource
replacement, vertex generation and flow scheduling execute in Unicorn.
This is a CPU/service oracle, not a real asset or Switch acceptance test.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_zoom_sprite_oracle import Native as Base, Zoom
from original_pause_oracle import Cursor, Sprite, ss
from original_common_hud_oracle import State as Common, Flow, Timer
from original_item_notice_oracle import Fade
from model_binding import ROOT, library

class State(C.Structure):
    _fields_ = [('sprites', Sprite*49), ('pointer', C.c_float*2),
                ('group', C.c_int16), ('requested_action', C.c_int16),
                ('transition', C.c_int16), ('picture', C.c_int16),
                ('view_gate', C.c_int32), ('image_view', C.c_int32), ('music_volume', C.c_int32),
                ('actions', (C.c_int32*9)*5), ('pictures', (C.c_int32*5)*5),
                ('hover', C.c_int32*20), ('loaded', C.c_uint64)]
class Bindings(C.Structure):
    _fields_ = [('common', C.POINTER(Common)), ('cursor', C.POINTER(Cursor))]
Sound = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint, C.c_void_p)
Image = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint, C.c_char_p, C.c_void_p)
Position = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(C.c_float), C.c_void_p)
Release = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint8, C.c_void_p)
class Ops(C.Structure):
    _fields_ = [('context', C.c_void_p), ('sound', Sound), ('image', Image), ('position', Position)]
class Input(C.Structure):
    _fields_ = [('buttons', C.c_uint32), ('seconds', C.c_float), ('scale', C.c_float), ('height', C.c_uint)]
class Draw(C.Structure):
    _fields_ = [('slot', C.c_uint), ('corners', C.c_float*4), ('alpha', C.c_float)]
class Frame(C.Structure):
    _fields_ = [('count', C.c_uint), ('draws', Draw*36), ('action', C.c_int32)]
class Selection(C.Structure):
    _fields_ = [('group', C.c_int32), ('variant', C.c_int32), ('selection', C.c_int32)]
class DispatchOps(C.Structure):
    _fields_ = [('context', C.c_void_p), ('release', Release), ('story', Sound)]
Unlock = (C.c_uint8*8)*5
ADDR = [0x734058+i*0x16c for i in range(49)] + [0xb537e8, 0xbeea18]
SHORTS = [('group', 0x709b10), ('requested_action', 0x709b1c), ('transition', 0x709b1e), ('picture', 0x709b28)]
INTS = [('view_gate', 0x709b34), ('image_view', 0x709b38), ('music_volume', 0x709b04)]
def snapshot(s, c, cur):
    return dict(sprites=[ss(p) for p in s.sprites], pointer=tuple(s.pointer),
                **{name: getattr(s, name) for name, _ in SHORTS+INTS},
                actions=[tuple(row) for row in s.actions], pictures=[tuple(row) for row in s.pictures],
                hover=tuple(s.hover), loaded=s.loaded,
                common=(ss(Sprite(c.curtain, (C.c_float*4)())), bytes(c.wait), c.gate, c.action, c.blocked),
                cursor=(ss(cur.sprite), bytes(cur.idle), cur.wanted))

class Native(Base):
    def __init__(self, exe):
        super().__init__(exe)
        self.events = []; self.draw_meta = []; self.buttons = 0; self.point = [0, 0]
        self.height = 960; self.dispatch = None
        self.wi(0x53f2f0, 0x300e100)
        self.wi(0x41f0000, 0x41f0100); self.wi(0x41f013c, 0x300e200)
        self.wi(0x7099f8, 0x41f1000)
        for slot in range(1, 9): self.wi(0xbeee10+(slot-1)*0x120, 100+slot)
        for a in [0x524e54,0x4ad8ec,0x43e583,0x466805,0x466814,0x43e91b,
                  0x4a06df,0x4ad2bf,0x46d9b4,0x300e100,0x300e200,0x4af97a,
                  0x4b75aa,0x4b76c2,0x4c8768,0x4e77bf,0x4f75dd]:
            self.u.hook_add(UC_HOOK_CODE, self.hook, begin=a, end=a)
    def hook(self, u, a, size, data):
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<10I', u.mem_read(sp+4, 40)); pop = 4; result = 0
        if a in [0x443bb8, 0x443be5, 0x43e96a]:
            if a == 0x43e96a:
                i = (args[0]-0x4000000)//0x400
                raw = bytes(u.mem_read(self.verts(i), 192))
                self.draw_meta.append((i, (*struct.unpack_from('<2f', raw, 0), *struct.unpack_from('<2f', raw, 64)), self.rf(ADDR[i]+0x12c)))
            return super().hook(u, a, size, data)
        if a == 0x524e54: pop = 24
        elif a == 0x4ad8ec: u.mem_write(args[0], b'assets\0')
        elif a == 0x43e583:
            i = ADDR.index(args[0]-0x100)
            self.blank(i); self.wi(args[0], self.handle(i))
            self.events.append(('image', i, self.text(args[1]).split('\\')[-1]))
        elif a == 0x43e91b:
            self.events.append(('image', (args[0]-0x4000000)//0x400, None))
        elif a == 0x4a06df: result = 0x41f0000
        elif a == 0x46d9b4:
            assert args[0] == 81
            result = 0x41f1000; u.mem_write(result, bytes(81))
        elif a == 0x300e100:
            fmt = self.text(args[1]); values = args[2:4] if fmt == '0%d_%02d' else args[2:3]
            raw = (fmt % tuple(values)).encode()+b'\0'; u.mem_write(args[0], raw); result = len(raw)-1
        elif a == 0x300e200: pop = 12
        elif a == 0x4ad2bf:
            if args[0] != 0x41f0000:
                assert args[1:3] == (0, (-600)&0xffffffff)
                self.events.append(('sound', args[0]-100))
        elif a == 0x4af97a: result = self.height
        elif a == 0x4b75aa:
            self.events.append(('position',))
            for p, value in zip(args, self.point): self.wf(p, value)
        elif a == 0x4b76c2:
            assert args[1:3] == (1, 0)
            mask = 32 if args[0] == 1 else 1
            assert args[0] in [0, 1, 0x5a, 0x33450]
            result = bool(self.buttons & mask)
        elif a == 0x4c8768:
            if self.dispatch is None: return
            result = self.dispatch & 0xffffffff
        elif a == 0x4e77bf: self.events.append(('release', args[0]&255))
        elif a == 0x4f75dd:
            assert args[0] == 0x7219a8 and args[2] == 0
            self.events.append(('story', args[1]))
        u.reg_write(UC_X86_REG_EAX, result); u.reg_write(UC_X86_REG_EIP, self.ri(sp)); u.reg_write(UC_X86_REG_ESP, sp+pop)
    def install(self, s, c, cur, inp):
        self.height = inp.height; self.buttons = inp.buttons
        for i, p in enumerate(list(s.sprites)+[cur.sprite, Sprite(c.curtain, (C.c_float*4)(0, 0, 1280*inp.scale, 960*inp.scale))]):
            z = Zoom(p.fade, (C.c_float*2)(1, 1), (C.c_float*2)(0, 0))
            self.install_zoom(ADDR[i], i, z, p.rect, 1)
            self.u.mem_write(ADDR[i]+0x149, b'\1')
            if i < 49 and not s.loaded & (1 << i): self.wi(ADDR[i]+0x100, 0)
        for name, a in SHORTS: self.u.mem_write(a, struct.pack('<h', getattr(s, name)))
        for name, a in INTS: self.wi(a, getattr(s, name))
        self.u.mem_write(0x709b20, bytes(s.pointer)); self.u.mem_write(0x709b3c, bytes(s.actions)); self.u.mem_write(0x709bf0, bytes(s.pictures)); self.u.mem_write(0x41f1000, bytes(s.hover))
        for a, v in [(0xbeeb7c,c.gate),(0xbeeb7e,c.action),(0xbeeb7f,c.blocked),(0xb5394f,cur.wanted)]: self.u.mem_write(a,bytes([v]))
        self.u.mem_write(0xbeeb54,bytes(c.wait));self.u.mem_write(0xb53924,bytes(cur.idle))
        self.wf(0x721ad0,inp.scale);self.wf(0x733700,inp.seconds);self.wi(0xbe9a10,-600)
        self.events=[];self.draw_meta=[]
    def read(self):
        s=State()
        for i,a in enumerate(ADDR[:49]):
            s.sprites[i]=Sprite(self.read_zoom(a).fade,(C.c_float*4)(*[self.rf(a+o) for o in [0x114,0x118,0x10c,0x110]]))
            if self.ri(a+0x100):s.loaded|=1<<i
        for name,a in SHORTS:setattr(s,name,struct.unpack('<h',self.u.mem_read(a,2))[0])
        for name,a in INTS:setattr(s,name,C.c_int32(self.ri(a)).value)
        s.pointer[:]=struct.unpack('<2f',self.u.mem_read(0x709b20,8));s.actions=type(s.actions).from_buffer_copy(self.u.mem_read(0x709b3c,180));s.pictures=type(s.pictures).from_buffer_copy(self.u.mem_read(0x709bf0,100));s.hover=type(s.hover).from_buffer_copy(self.u.mem_read(0x41f1000,80))
        c=Common();c.curtain=self.read_zoom(ADDR[50]).fade;c.wait=Timer.from_buffer_copy(self.u.mem_read(0xbeeb54,C.sizeof(Timer)));c.gate,c.action,c.blocked=[self.u.mem_read(a,1)[0] for a in [0xbeeb7c,0xbeeb7e,0xbeeb7f]]
        a=ADDR[49];cur=Cursor(Sprite(self.read_zoom(a).fade,(C.c_float*4)(*[self.rf(a+o) for o in [0x114,0x118,0x10c,0x110]])),Timer.from_buffer_copy(self.u.mem_read(0xb53924,C.sizeof(Timer))),self.u.mem_read(0xb5394f,1)[0])
        return s,c,cur

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('--cases',type=int,default=3000);ap.add_argument('--output',type=Path,default=ROOT/'local/original-gallery-menu-oracle.json');args=ap.parse_args()
    exe=args.exe.read_bytes();n=Native(exe);lib=library();e=C.create_string_buffer(256)
    lib.bk_gallery_menu_initialize.argtypes=[C.POINTER(State),C.c_uint,C.c_uint8,C.c_uint,C.c_uint,C.POINTER(Unlock),C.c_int32,C.POINTER(Ops),C.c_void_p]
    lib.bk_gallery_menu_step.argtypes=[C.POINTER(State),C.POINTER(Bindings),C.POINTER(Input),C.POINTER(Ops),C.POINTER(Frame),C.c_void_p]
    lib.bk_gallery_menu_dispatch.argtypes=[C.POINTER(State),C.c_int32,C.POINTER(Selection),C.POINTER(Flow),C.POINTER(DispatchOps),C.c_void_p]
    lib.bk_gallery_menu_name.argtypes=[C.c_void_p,C.c_uint,C.c_uint,C.c_uint]
    lib.bk_menu_cursor_initialize.argtypes=[C.POINTER(Cursor),C.c_uint,C.c_uint]
    rng=random.Random(0x4c8768);counts=dict(constructors=0,frames=0,draws=0,events=0,dispatches=0,names=0,rejections=0)
    trace=[];point=[0,0]
    @Image
    def image(_,slot,name,error):trace.append(('image',slot,name.decode() if name else None));return 1
    @Sound
    def sound(_,slot,error):trace.append(('sound',slot));return 1
    @Position
    def position(_,out,error):trace.append(('position',));out[0],out[1]=point;return 1
    ops=Ops(None,sound,image,position)
    def init(width,group,unlocks):
        s=State();c=Common();cur=Cursor();assert lib.bk_menu_cursor_initialize(C.byref(cur),width,width*3//4)
        assert lib.bk_gallery_menu_initialize(C.byref(s),width,0x10,group,0,C.byref(unlocks),-900,C.byref(ops),e),e.value
        return s,c,cur
    # All formatted names run through the original lookup tables and CRT call.
    for g in range(5):
        n.u.mem_write(0x709b10,struct.pack('<h',g))
        for mode,size in [(0,8),(1,5),(2,1),(3,1)]:
            for index in range(size):
                n.call(0x4c77b1,struct.pack('<3I',index,0x3001000,mode));out=C.create_string_buffer(32)
                assert lib.bk_gallery_menu_name(out,g,index,mode)
                assert out.value.decode()==n.text(0x3001000);counts['names']+=1
    # Reenter from title, story and endings; arbitrary byte unlock values persist.
    for width in [320,640,960,1001,1280,1920]:
        for previous in [1,0x10,0x48]:
            for group in range(5):
                u=Unlock(*[(C.c_uint8*8)(*[rng.randrange(256) for _ in range(8)]) for _ in range(5)])
                s,c,cur=init(width,group,u)
                s.pointer[:]=[12.5,45.5];s.requested_action=7;s.picture=3;s.view_gate=25;s.image_view=0
                inp=Input(0,0,width/1280,width*3//4);n.install(s,c,cur,inp)
                n.u.mem_write(0x721ad4,bytes([previous]));n.u.mem_write(0x721b3c,bytes([group]));n.wi(0x7219a8,group);n.u.mem_write(0xb54738,bytes(u));n.wi(0xbe9a0c,-900)
                n.call(0x4c59c0,b'');want=snapshot(*n.read());expected=n.events.copy();trace.clear()
                assert lib.bk_gallery_menu_initialize(C.byref(s),width,previous,group,group,C.byref(u),-900,C.byref(ops),e),e.value
                assert snapshot(s,c,cur)==want,('constructor',width,previous,group)
                assert trace==expected,('constructor events',trace,expected);counts['constructors']+=1
    def check(s,c,cur,inp,p):
        nonlocal point
        n.install(s,c,cur,inp);n.point=list(p);n.call(0x4c8768,b'')
        wanted=snapshot(*n.read());wd=n.draw_meta.copy();wt=n.events.copy();action=C.c_int32(n.u.reg_read(UC_X86_REG_EAX)).value
        trace.clear();point=list(p);f=Frame();b=Bindings(C.pointer(c),C.pointer(cur))
        assert lib.bk_gallery_menu_step(C.byref(s),C.byref(b),C.byref(inp),C.byref(ops),C.byref(f),e),(counts,e.value)
        actual=snapshot(s,c,cur)
        assert actual==wanted,('state',counts,[(k,actual[k],wanted[k]) for k in actual if actual[k]!=wanted[k]])
        d=[(x.slot,tuple(x.corners),x.alpha) for x in f.draws[:f.count]]
        assert d==wd,('draw',counts,d,wd)
        assert trace==wt,('trace',counts,trace,wt)
        assert f.action==action,('action',f.action,action)
        counts['frames']+=1;counts['draws']+=f.count;counts['events']+=len(trace)
        return f
    for case in range(args.cases):
        width=[320,640,960,1001,1280,1920][case%6]
        u=Unlock(*[(C.c_uint8*8)(*[rng.choice([0,0,1,128,255]) for _ in range(8)]) for _ in range(5)])
        s,c,cur=init(width,case%5,u);c.curtain=Fade(rng.random(),rng.choice([0,2,4]),case%6);c.blocked=case%3
        s.transition=rng.choice([-1,0,1,2,3,4]);s.requested_action=case%11;s.picture=case%5;s.view_gate=case%3
        s.image_view=case%4==0;s.loaded|=1<<48
        s.sprites[48]=Sprite(Fade(1,2,0),(C.c_float*4)(0,0,width,width*3//4))
        for sprite in s.sprites:
            sprite.fade=Fade(rng.random(),rng.choice([0,.5,2,4]),rng.randrange(6))
        for i in range(20):s.hover[i]=rng.choice([0,0,1,7])
        slot=rng.randrange(1,48);r=s.sprites[slot].rect
        s.pointer[:]=[r[0]+r[2]*rng.choice([0,.5,1,1.01]),r[1]+r[3]*rng.choice([0,.5,1,1.01])]
        check(s,c,cur,Input(rng.choice([0,1,32,33]),rng.choice([0,1/60,1/30,.0001,.5]),width/1280,width*3//4),[rng.uniform(0,width),rng.uniform(0,width*3//4)])
    # Continuous page switching, each action and each picture with actual fade waits.
    enabled=Unlock(*[(C.c_uint8*8)(*([1]*8)) for _ in range(5)])
    sequences=0
    for width in [640,960,1280]:
        for group in range(5):
            for slot in [12,14,17,20,23,26,29,33,36,38,41,42,43,44,45]:
                s,c,cur=init(width,0,enabled);lib.bk_fade_sprite_initialize(C.byref(c.curtain))
                s.pointer[:]=s.sprites[2+group*2].rect[:2]
                check(s,c,cur,Input(1,1/60,width/1280,width*3//4),s.pointer)
                r=s.sprites[slot].rect;s.pointer[:]=[r[0]+r[2]*.5,r[1]+r[3]*.5]
                f=check(s,c,cur,Input(1,1/60,width/1280,width*3//4),s.pointer)
                for tick in range(240):
                    buttons=32 if slot>=41 and tick==70 else 0
                    f=check(s,c,cur,Input(buttons,1/60,width/1280,width*3//4),s.pointer)
                    if f.action!=99:break
                if slot<41:assert f.action==(10 if slot==12 else (slot-14)//3)
                else:assert s.image_view==0 and not s.loaded&(1<<48),(width,group,slot,s.image_view,s.transition,c.curtain.stage,c.curtain.alpha)
                sequences+=1
    # Dispatcher preserves selection for9/10/99; both replay lanes remain distinct.
    for group in range(5):
        for action in [-1,*range(12),99,0x7fffffff]:
            s=State();s.group=group;selection=Selection(21,22,23);flow=Flow(0x18,1,0,2)
            n.u.mem_write(0x709b10,struct.pack('<h',group));n.wi(0x728de0,21);n.wi(0x729788,22);n.wi(0x729784,23)
            for a,v in zip([0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c],bytes(flow)):n.u.mem_write(a,bytes([v]))
            n.dispatch=action;n.events=[];n.call(0x51b9b3,b'');n.dispatch=None
            wt=n.events.copy();trace.clear()
            @Release
            def release(_,value,err):trace.append(('release',value));return 1
            @Sound
            def story(_,value,err):trace.append(('story',value));return 1
            dispatchops=DispatchOps(None,release,story)
            assert lib.bk_gallery_menu_dispatch(C.byref(s),action,C.byref(selection),C.byref(flow),C.byref(dispatchops),e),e.value
            assert tuple([selection.group,selection.variant,selection.selection])==tuple(C.c_int32(n.ri(a)).value for a in [0x728de0,0x729788,0x729784])
            assert bytes(flow)==b''.join(n.u.mem_read(a,1) for a in [0xbeeb84,0x721ad4,0xbfbbb9,0xbfbb9c])
            assert trace==wt;counts['dispatches']+=1
    # Reject impossible indices/extents before services or state mutation.
    for badgroup in [5,65535]:
        out=C.create_string_buffer(b'unchanged',32)
        assert not lib.bk_gallery_menu_name(out,badgroup,0,0) and out.value==b'unchanged';counts['rejections']+=1
    # A failed release must not publish the scheduled flow, but its preceding
    # selection writes remain visible. Failed story setup must not release.
    @Release
    def reject_release(_, value, err):trace.append(('release',value));return 0
    @Sound
    def reject_story(_, value, err):trace.append(('story',value));return 0
    for action in [0,6,7,8,10]:
        s=State();s.group=3;selection=Selection(21,22,23);flow=Flow(0x18,1,0,2);before=bytes(flow)
        trace.clear();dispatchops=DispatchOps(None,reject_release,reject_story)
        assert not lib.bk_gallery_menu_dispatch(C.byref(s),action,C.byref(selection),C.byref(flow),C.byref(dispatchops),e)
        assert bytes(flow)==before
        if action<8:assert (selection.group,selection.variant,selection.selection)==(3,int(action==7),6 if action>=6 else action)
        else:assert (selection.group,selection.variant,selection.selection)==(21,22,23)
        assert trace==[('story',3)] if action==8 else trace==[('release',0x18)]
        counts['rejections']+=1
    # Disabled actions/photos retain old hover latches and cannot activate.
    s,c,cur=init(960,2,Unlock());s.hover[:]=[1]*20
    for slot in [14,17,20,23,26,29,33,36,38,41,42,43,44,45]:
        r=s.sprites[slot].rect;s.pointer[:]=[r[0]+r[2]/2,r[1]+r[3]/2]
        check(s,c,cur,Input(1,1/60,.75,720),s.pointer)
        assert s.transition==-1 and c.blocked==0 and list(s.hover)[5:19]==[1]*14
    # Required callbacks are never replaced by a successful no-op backend.
    for missing in ['sound','image','position']:
        s,c,cur=init(960,0,enabled);b=Bindings(C.pointer(c),C.pointer(cur));f=Frame();inp=Input(0,1/60,.75,720)
        incomplete=Ops.from_buffer_copy(ops);setattr(incomplete,missing,dict(sound=Sound,image=Image,position=Position)[missing]())
        before=snapshot(s,c,cur);trace.clear()
        assert not lib.bk_gallery_menu_step(C.byref(s),C.byref(b),C.byref(inp),C.byref(incomplete),C.byref(f),e)
        assert snapshot(s,c,cur)==before and not trace;counts['rejections']+=1
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),**counts,continuous_sequences=sequences,max_error=0,scope=__doc__)
    args.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
