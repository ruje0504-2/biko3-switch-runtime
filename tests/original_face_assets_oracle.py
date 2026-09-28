"""Actual FAM face retargets plus continuous native controller/MORP vertices."""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
import sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from original_morph_pose_oracle import Native as MorphNative, Sample, bind as morph_bind
from original_face_controller_oracle import Native as ControllerNative, State, Commands, FIELDS
from original_face_config_oracle import Config, bind as config_bind
from model_binding import ROOT, Model, Vertex, library, decode
sys.path.insert(0, str(ROOT/'tools'))
from bk3_assets import Archive

def bind(lib):
    morph_bind(lib);config_bind(lib)
    for name,args,result in [
        ('bk_resources_create',[C.c_void_p],C.c_void_p),
        ('bk_resources_mount',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_void_p],C.c_int),
        ('bk_resources_mount_directory',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t,C.c_void_p],C.c_int),
        ('bk_resources_destroy',[C.c_void_p],None),
        ('bk_face_assets_create',[C.c_void_p,C.c_char_p,C.c_char_p,C.c_char_p,C.POINTER(Model),C.c_char_p,C.c_void_p],C.c_void_p),
        ('bk_face_assets_destroy',[C.c_void_p],None),
        ('bk_face_assets_mesh',[C.c_void_p,C.c_uint32],C.c_void_p),
        ('bk_face_assets_apply',[C.c_void_p,C.POINTER(Commands),C.c_void_p],C.c_int),
        ('bk_face_assets_initialize',[C.c_void_p,C.POINTER(State),C.POINTER(C.c_uint32),C.POINTER(C.c_uint32),C.c_void_p],C.c_int),
        ('bk_face_assets_step',[C.c_void_p,C.POINTER(State),C.c_int32,C.c_float,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint32,C.POINTER(C.c_uint32),C.c_void_p],C.c_int),
        ('bk_face_init',[C.POINTER(State),C.c_uint,C.c_uint,C.c_void_p],C.c_int),
        ('bk_model_submesh_name',[C.POINTER(Model),C.c_uint32,C.c_char_p,C.c_void_p,C.c_void_p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result

def bits(n):return struct.unpack('<I',struct.pack('<f',n))[0]
class Native(MorphNative):
    source_mesh, clip, object, group, registry = 0x3002000,0x3003000,0x3004000,0x3005000,0x3008000
    def __init__(self,exe):
        super().__init__(exe)
        for address in [0x4ad8ec,0x401074,0x4014cd,0x42a0cd,0x4a71de,0x42d217,0x42d270,0x46da06]:
            self.u.hook_add(UC_HOOK_CODE,self.boundary,begin=address,end=address)
    def read(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
    def string(self,p):return bytes(self.u.mem_read(p,256)).split(b'\0')[0]
    def call(self,addr,*args):
        self.u.mem_write(self.stack,struct.pack('<'+'I'*(len(args)+1),self.stop,*args))
        self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.u.emu_start(addr,self.stop,count=3000000);assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
        return self.u.reg_read(UC_X86_REG_EAX)
    def name(self,parent,ordinal,filename):
        u=self.u;base=0x2006000;source=0x300a000
        u.mem_write(source,parent+b'\0');u.mem_write(0x5d2604,filename+b'\0')
        self.call(0x534a52,0x5d2604)
        if ordinal is None:
            self.word(base-0x1ac,source);start,end=0x418043,0x41806b
        else:
            self.word(base-0x1ac,source);self.word(base-0x1b0,ordinal);start,end=0x4183f6,0x41843d
        u.reg_write(UC_X86_REG_EBP,base);u.reg_write(UC_X86_REG_ESP,base-0x1000)
        u.emu_start(start,end,count=100000);assert u.reg_read(UC_X86_REG_EIP)==end
        return self.string(source if ordinal is None else base-0x114)
    def boundary(self,u,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP);ret=self.read(sp);args=[self.read(sp+4+i*4) for i in range(3)];result=0
        if address==0x4ad8ec:u.mem_write(args[0],(self.string(args[2]) if args[2] else b'')+b'\0')
        elif address==0x401074:
            assert self.string(args[0])==self.source_clip;result=self.clip
        elif address==0x4a71de:
            assert self.string(args[0])==self.vix_name
            if self.vix is not None:
                if self.vix:u.mem_write(0x300d000,self.vix)
                self.word(args[1],0x300d000);result=len(self.vix)
        elif address==0x42d217:assert args[0]<=0x1000;result=0x300c000
        u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def bind_slot(self,lib,morph,track,target,slot,source_clip,vix):
        self.seed(lib,morph,track,target);self.word(self.track+0x70,self.source_mesh)
        self.word(self.clip+0x160,self.object);self.word(self.object+0x168,self.group)
        self.word(self.group+0x78,1);self.word(self.group+0x80,self.group+0x100);self.word(self.group+0x100,self.track)
        self.u.mem_write(self.registry,bytes(0x110));self.word(0x645614,self.registry);self.word(0x645618,2)
        for i,(name,ptr) in enumerate([(slot.target,self.mesh),(slot.source,self.source_mesh)]):
            p=self.registry+i*0x88;self.u.mem_write(p,name+b'\0');self.word(p+0x80,ptr);self.word(p+0x84,0x3ea)
        self.u.mem_write(0x5767c8,b'\1');self.word(0x645664,0)
        self.source_clip=source_clip;self.vix_name=slot.selection;self.vix=vix
        for p,s in [(0x300a000,slot.target),(0x300a100,source_clip),(0x300a200,slot.source),(0x300a300,slot.selection)]:self.u.mem_write(p,s+b'\0')
        assert self.call(0x4f37cf,0,0x300a000,0x300a100,0x300a200,0x300a300)==self.track
        assert self.read(self.track+0x70)==self.mesh and self.read(self.mesh+0xe4)==self.track
        selected=self.read(self.track+0x90);count=self.read(self.track+0x94);indices=[]
        if count:indices=list(struct.unpack('<'+'H'*count,self.u.mem_read(self.read(self.track+0x98),count*2)))
        return selected,indices

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);ap.add_argument('--ending',action='store_true');args=ap.parse_args()
    exe=args.exe.read_bytes();lib=library();bind(lib);n=Native(exe);controller=ControllerNative(exe)
    if args.ending:
        from original_ending_face_init import EndingController
        controller=EndingController(exe)
        lib.bk_face_assets_create_ending.argtypes=lib.bk_face_assets_create.argtypes
        lib.bk_face_assets_create_ending.restype=C.c_void_p
    error=C.create_string_buffer(256);store=lib.bk_resources_create(error)
    assert lib.bk_resources_mount(store,b'bk3_01',str(args.data/'bk3_01.pp').encode(),error),error.value
    assert lib.bk_resources_mount_directory(store,b'faces',str(args.data).encode(),20480,error),error.value
    archive=Archive(args.data/'bk3_01.pp');entries={e.name.encode():e for e in archive.entries};records=[]
    files=[(p,p.read_bytes()) for p in sorted(args.data.glob('*.fam'))]
    ending_packs={}
    if args.ending:
        fam=Archive(args.data/'fambom.pp');files=[(Path(e.name),fam.read(e)) for e in fam.entries if e.name.endswith('.fam')]
        assert lib.bk_resources_mount(store,b'ending-faces',str(args.data/'fambom.pp').encode(),error),error.value
        for i in range(8,15):
            pack=f'bk3_{i:02}';a=Archive(args.data/(pack+'.pp'));ending_packs[pack]=(a,{e.name.encode():e for e in a.entries})
            assert lib.bk_resources_mount(store,pack.encode(),str(args.data/(pack+'.pp')).encode(),error),error.value
    samples=vertices=bindings=names=rejects=steps=0;worst=0;rng=random.Random(0x4f2971)
    try:
        for path,raw in files:
            config=Config();assert lib.bk_face_config_decode(raw,len(raw),C.byref(config),error)
            pack='bk3_01'
            if args.ending:
                matches=[(p,a,en) for p,(a,en) in ending_packs.items() if config.actor_clip in en]
                assert len(matches)==1,(path.name,len(matches))
                pack,archive,entries=matches[0]
            if config.actor_clip not in entries:continue
            target_name=archive.read(entries[config.actor_clip])[:256].split(b'\0')[0]
            source_name=archive.read(entries[config.source_clip])[:256].split(b'\0')[0]
            target_bytes=archive.read(entries[target_name]);source_bytes=archive.read(entries[source_name])
            ok,target,msg=decode(lib,target_bytes);assert ok==1,msg
            ok,source,msg=decode(lib,source_bytes);assert ok==1,msg
            morph=lib.bk_model_morph_create(source,error);assert morph,error.value
            factory=lib.bk_face_assets_create_ending if args.ending else lib.bk_face_assets_create
            face=factory(store,b'ending-faces' if args.ending else b'faces',path.name.encode(),pack.encode(),target,target_name,error);assert face,(path.name,error.value)
            try:
                model_names=[]
                for model,filename in [(target,target_name),(source,source_name)]:
                    lookup={}
                    for i in range(model.contents.submesh_count):
                        sub=model.contents.submeshes[i];parent=model.contents.meshes[sub.mesh_index]
                        name=n.name(parent.name,None if parent.submesh_count==1 else i-parent.first_submesh,filename)
                        portable=C.create_string_buffer(256);assert lib.bk_model_submesh_name(model,i,filename,portable,error),error.value
                        assert name==portable.value,(path.name,i,name,portable.value);lookup[name]=i;names+=1
                    model_names.append(lookup)
                bound={};expected={};vix_records=[]
                for g in range(2):
                    for i in range(config.counts[g]):
                        slot=config.slots[g][i];ti=model_names[0][slot.target];si=model_names[1][slot.source]
                        track=next(j for j in range(lib.bk_model_morph_count(morph)) if lib.bk_model_morph_track(morph,j).contents.submesh==si)
                        vix=archive.read(entries[slot.selection]) if slot.selection in entries else None
                        selection=n.bind_slot(lib,morph,track,target.contents.submeshes[ti],slot,config.source_clip,vix)
                        if vix is not None:vix_records.append(dict(name=slot.selection.decode(),sha256=hashlib.sha256(vix).hexdigest(),count=len(selection[1])))
                        bound[g,i]=(ti,track,selection);sub=target.contents.submeshes[ti];expected[ti]=C.string_at(sub.vertices,sub.vertex_count*60);bindings+=1
                state=State();assert lib.bk_face_init(C.byref(state),*config.counts,error)
                random_state=C.c_uint32(rng.getrandbits(32));controller.seed(state,random_state.value)
                # Original post-construction setup; clocks differ deliberately.
                clock=[0,0,1,2] if len(records)%2==0 else [15000,15001,15002,15003]
                if args.ending:
                    commands=controller.initialize(state,random_state.value,clock)
                else:
                    controller.call(0x411de1,controller.face,bits(0),bits(9));controller.call(0x411ea7,controller.face,bits(0),bits(9));controller.call(0x411f5d,controller.face,bits(.5))
                    controller.now=clock[0];controller.call(0x410fd8,controller.face,0)
                    controller.now=clock[1];controller.call(0x4110ef,controller.face,1000);commands=list(controller.commands)
                    controller.word(controller.face+0xbc,bits(1));controller.now=clock[2];controller.call(0x411985,controller.face,bits(100),1000);commands+=controller.commands
                    controller.now=clock[3];controller.call(0x411985,controller.face,bits(0),2000);commands+=controller.commands
                assert lib.bk_face_assets_initialize(face,C.byref(state),(C.c_uint32*4)(*clock),C.byref(random_state),error),(path.name,error.value)
                for step in range(41):
                    if step:
                        now=clock[-1]+step*137;requested=(step//3)%12 if args.ending else (step//11)%4;level=C.c_float([0,9,2.2,6.75,1,10][step%6]).value
                        controller.now=now;controller.call(0x410fd8,controller.face,requested)
                        controller.now=now+1;controller.call(0x411985,controller.face,bits(level),now-1);commands=list(controller.commands)
                        controller.now=now+2;controller.call(0x4110ef,controller.face,now-1);commands+=controller.commands
                        assert lib.bk_face_assets_step(face,C.byref(state),requested,level,now-1,now,now+1,now+2,C.byref(random_state),error),(path.name,step,error.value)
                    assert bytes(state)==bytes(controller.state()),(path.name,step,'controller state')
                    assert random_state.value==struct.unpack('<I',controller.u.mem_read(0x58edd8,4))[0]
                    for g,i,blend,a,b,w in commands:
                        ti,track,(enabled,indices)=bound[g,i];sub=target.contents.submeshes[ti]
                        n.seed(lib,morph,track,sub);n.u.mem_write(n.destination,expected[ti]);n.selection(enabled,indices)
                        expected[ti]=n.apply(Sample(blend,a,b,w));samples+=1
                    for ti,wanted in expected.items():
                        mesh=lib.bk_face_assets_mesh(face,ti);assert mesh
                        actual=C.string_at(lib.bk_morph_mesh_vertices(mesh),len(wanted));vertices+=len(wanted)//60
                        if actual!=wanted:
                            for v in range(len(wanted)//60):
                                av=struct.unpack_from('<9f',actual,v*60);bv=struct.unpack_from('<9f',wanted,v*60)
                                for j,(a,b) in enumerate(zip(av,bv)):
                                    delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta);assert math.isfinite(delta) and delta<3e-6,(path.name,step,ti,v,j,a,b)
                                assert actual[v*60+36:v*60+60]==wanted[v*60+36:v*60+60]
                    if step%10==0:
                        old_state=bytes(state);old_random=random_state.value
                        assert not lib.bk_face_assets_step(face,C.byref(state),2**31-1,float('nan'),0,0,0,0,C.byref(random_state),error)
                        assert bytes(state)==old_state and random_state.value==old_random
                        bad=Commands();bad.count=2;bad.commands[0].group=0;bad.commands[0].index=0;bad.commands[0].from_=3;bad.commands[1].group=1;bad.commands[1].index=0;bad.commands[1].from_=float('nan')
                        assert not lib.bk_face_assets_apply(face,C.byref(bad),error)
                        for ti,wanted in expected.items():assert C.string_at(lib.bk_morph_mesh_vertices(lib.bk_face_assets_mesh(face,ti)),len(wanted))==wanted
                        rejects+=2
                    steps+=1
                records.append(dict(pack=pack,config=path.name,config_sha256=hashlib.sha256(raw).hexdigest(),target=target_name.decode(),target_sha256=hashlib.sha256(target_bytes).hexdigest(),source=source_name.decode(),source_sha256=hashlib.sha256(source_bytes).hexdigest(),counts=list(config.counts),vix=vix_records));print(path.name,'PASS',flush=True)
            finally:
                lib.bk_face_assets_destroy(face);lib.bk_model_morph_destroy(morph);lib.bk_model_destroy(source);lib.bk_model_destroy(target)
    finally:lib.bk_resources_destroy(store)
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),profiles=len(records),bindings=bindings,native_names=names,steps=steps,morph_submissions=samples,vertex_comparisons=vertices,atomic_rejections=rejects,max_normalized_error=worst,records=records,native_functions=['0x418043..0x41806b','0x4183f6..0x41843d','0x4f37cf','0x429c6b','0x433c55','0x430f70','0x430f8a','0x431028','0x410fd8','0x411985','0x4110ef','0x4316be','0x432642'],scope='20 existing bk3_01 actor configurations. Native name creation, registry lookup, track retarget and VIX attachment; source loading/path/allocation/destruction boundaries substituted. Full native controller setup and consecutive request/mouth/blink commands, then real native MORP evaluation over shared CPU face meshes. Controller MORP calls captured and replayed; no eye texture animation, skinning, GPU or full app frame.')
    if args.ending:
        assert len(records)==50
        report['scope']='50 packaged ending FAM bindings including fourth eye slot. Original4f2dae setup/warm-up with eye IO boundary, real controller steps and MORP shared vertices. Primary/secondary asset placement and full ending frame remain separate.'
        report['native_functions'].append('0x4f33ae..0x4f34cb')
    (ROOT/('local/original-ending-face-assets-oracle.json' if args.ending else 'local/original-face-assets-oracle.json')).write_text(json.dumps(report,indent=2)+'\n');print('PASS',steps,'steps;',samples,'submissions;',vertices,'vertices; max error',worst)
if __name__=='__main__':main()
