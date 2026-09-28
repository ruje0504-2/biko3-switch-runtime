"""Actual actor adapter checks, layered on the independent native BOM loader.
No substitution of original control/math routines; only retained globals seeded.
"""
import ctypes as C
import struct
from unicorn.x86_const import UC_X86_REG_EAX
from original_bom_motion_oracle import Native, Manual, Return, MANUAL, Fp, bits


def bind(lib):
    for name,args in [
        ('manual',[C.c_void_p,C.POINTER(Manual),C.c_int,C.c_uint,C.c_int32,C.c_int32,C.c_float,C.c_float,C.c_int32,C.c_void_p]),
        ('return_single',[C.c_void_p,C.POINTER(Return),C.c_uint,Fp,C.c_float,C.c_int32,C.c_uint32,C.c_int32,C.POINTER(C.c_int),C.c_void_p]),
        ('return_multiple',[C.c_void_p,C.POINTER(Return),C.c_uint,Fp,C.c_float,C.c_int32,C.c_uint32,C.POINTER(C.c_int),C.c_void_p]),
        ('follow_references',[C.c_void_p,C.c_void_p])]:
        getattr(lib,'bk_bom_assets_'+name).argtypes=args
    lib.bk_bom_return_init.argtypes=[C.POINTER(Return)]


class Check:
    def __init__(self,lib,vm,assets,config,e,equal):
        self.lib,self.vm,self.assets,self.config,self.e,self.equal=lib,vm,assets,config,e,equal
        self.state=Native.__new__(Native);self.state.u=vm.uc
        self.manual=[Manual(),Manual()];self.returns=[Return(),Return()]
        self.calls=self.completed=self.followed=0
        for p in [0x704de0,0x704d20,0x704d98,0x704d58,0x704d48]:vm.word(p,3)
        vm.word(0x645600,vm.context)
        for i,s in enumerate(self.manual):
            _,pos,angle=MANUAL[i];self.state.vf(pos,s.offset);self.state.vf(angle,s.angles)
        for i,s in enumerate(self.returns):
            lib.bk_bom_return_init(C.byref(s));self.state.returning(s,i,True)

    def call(self,address,words):
        self.vm.call(address,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]));self.calls+=1

    def step(self,step):
        vm,lib,a,e=self.vm,self.lib,self.assets,self.e
        phase=step%30;count=min(2,self.config.count);flip=step%2
        scene=(C.c_float*16)(*vm.floats(vm.context+0xc0,16))
        if phase<10:
            for i in range(count):
                dx=(step*13+i*7)%17-8;dy=(step*11+i*3)%19-9
                words=[0]*11;words[6]=dx;words[7]=dy
                addr,pos,ang=MANUAL[i]
                self.call(addr,words+[i,vm.output,bits(.09),bits(7.5),flip])
                assert lib.bk_bom_assets_manual(a,C.byref(self.manual[i]),i,i,dx,dy,.09,7.5,flip,e),e.value
                self.equal(self.manual[i].offset,vm.floats(pos,2),('manual',step,i,'offset'))
                self.equal(self.manual[i].angles,vm.floats(ang,2),('manual',step,i,'angles'))
        else:
            which=1 if phase<20 else 0;s=self.returns[which];done=C.c_int()
            ms=[0,16,33,60,333][step%5];reset=int(phase==29)
            self.call(0x49b900 if which else 0x49b28f,[count-1 if which else 0,vm.output,bits(.09),flip,ms,reset])
            expected_done=vm.uc.reg_read(UC_X86_REG_EAX)
            if which:ok=lib.bk_bom_assets_return_multiple(a,C.byref(s),count,scene,.09,flip,ms,C.byref(done),e)
            else:ok=lib.bk_bom_assets_return_single(a,C.byref(s),0,scene,.09,flip,ms,reset,C.byref(done),e)
            assert ok,e.value
            assert done.value==expected_done,('complete',step,done.value,expected_done)
            self.completed+=done.value
            expected=Return.from_buffer_copy(bytes(s));self.state.returning(expected,which,False)
            assert bytes(s)==bytes(expected),('return state',step)
        # The per-frame loop follows PRIMARY references, unlike loader parents.
        for i in range(self.config.count):
            child=vm.read(vm.output+0xa4+i*4);ref=vm.read(vm.output+0x104+i*4)
            assert child and ref,('fixture missing follow node',i)
            self.call(0x422c49,[child,ref,0,0,0])
            self.call(0x4230bd,[child,ref,0,0,bits(1),0,bits(1),0]);self.followed+=1
        assert lib.bk_bom_assets_follow_references(a,e),e.value
