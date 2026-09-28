"""Original4c009b dispatch, whole ordinary/script movement, on all real ATRs.

Menu/UI hotkeys are absent from this fixture. Only device input, cached head
screen projection and collision construction boundaries are substituted.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from model_binding import ROOT,library,decode
from original_player_scene_oracle import Native as SceneNative,State as Scene,Input as SceneInput
from original_player_spatial_oracle import State as Spatial,Input as SpatialInput
from original_player_interaction_oracle import State as Interaction,Input as InteractionInput,ALIASES as INTERACTION_KEYS,compare_movement
from original_player_trigger_oracle import compare as compare_trigger
from original_player_script_oracle import Placement
from original_player_movement_oracle import State as Movement,Input as MovementInput,FIELDS,ALIASES
from original_player_wall_oracle import Wall,Input as WallInput,Vec
from original_collision_oracle import bind
from bk3_assets import Archive

class State(C.Structure):
    _fields_=[('spatial',Spatial),('interaction',Interaction),('return_yaw',C.c_float),('completion_mode',C.c_int8),('completion_requested',C.c_int8),('wall_available',C.c_int8)]
class Input(C.Structure):
    _fields_=[('spatial',SpatialInput),('interaction',InteractionInput),('has_shadow',C.c_int)]
class Effects(C.Structure):
    _fields_=[('placements',C.c_uint),('roots',Placement*2),('shadow_place',C.c_int),('shadow',Placement)]
class Native(SceneNative):
    scene=0x726640
    model,root_group,root,head=0x3001000,0x3009000,0x300a000,0x300c000
    shadow_model,shadow_group,shadow_root=0x3008000,0x300b000,0x300d000
    stack=0x200e000
    def __init__(self,exe):
        super().__init__(exe)
        for address in [0x4b757e,0x4b76c2,0x42407a]:self.u.hook_add(UC_HOOK_CODE,self.input_hook,begin=address,end=address)
    def input_hook(self,u,address,size,data):
        sp=u.reg_read(UC_X86_REG_ESP)
        if address==0x42407a:
            root,matrix=struct.unpack('<2I',u.mem_read(sp+4,8))
            assert root in [self.root,self.shadow_root]
            self.roots.append((root,struct.unpack('<16f',u.mem_read(matrix,64))))
            return
        ret=struct.unpack('<I',u.mem_read(sp,4))[0];result=0
        if address==0x4b757e:
            a,b=struct.unpack('<2I',u.mem_read(sp+4,8))
            u.mem_write(a,struct.pack('<f',self.control_input.spatial.movement.look[0]));u.mem_write(b,struct.pack('<f',self.control_input.spatial.movement.look[1]))
        else: result=struct.unpack('<3I',u.mem_read(sp+4,12)) in self.keys
        u.reg_write(UC_X86_REG_EAX,int(result));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    def step_control(self,state,inp,rng):
        self.prepare(state.spatial.scene,inp.spatial.scene)
        self.control_input=inp;self.roots=[]
        self.keys={rng.choice(aliases) for bit,aliases in enumerate(ALIASES) if inp.spatial.movement.buttons&(1<<bit)}
        self.keys.update(rng.choice(aliases) for bit,aliases in enumerate(INTERACTION_KEYS) if inp.interaction.buttons&(1<<bit))
        u,p=self.u,self.player
        u.mem_write(p,struct.pack('<3I',self.model,self.shadow_model if inp.has_shadow else 0,self.head))
        u.mem_write(p+0x14,bytes(inp.spatial.movement.actions))
        u.mem_write(p+0x298,struct.pack('<f',inp.spatial.base_head_height))
        u.mem_write(p+0x2a8,struct.pack('<f',state.spatial.vertical_position))
        for name,offset,fmt in FIELDS:
            v=getattr(state.spatial.movement,name);vs=list(v) if isinstance(v,C.Array) else [v]
            u.mem_write(p+offset,struct.pack('<'+fmt,*vs))
        u.mem_write(p+0x7d8,bytes(state.interaction.trigger.origin)+bytes(state.interaction.trigger.target))
        u.mem_write(p+0x800,struct.pack('<i',state.interaction.trigger.prop_kind))
        for offset,value in [(0x7f8,state.interaction.script_phase),(0x7cb,state.completion_mode),(0x7c8,state.completion_requested),(0x57a,state.wall_available)]:u.mem_write(p+offset,struct.pack('<b',value))
        u.mem_write(p+0x7fc,struct.pack('<f',state.return_yaw))
        u.mem_write(0x7099ec,struct.pack('<i',state.spatial.movement.move_latch))
        u.mem_write(0x733700,struct.pack('<f',inp.spatial.movement.seconds))
        u.mem_write(0x7219a8,struct.pack('<2i',inp.interaction.trigger.group,inp.interaction.trigger.area))
        for i,prop in enumerate(inp.interaction.trigger.props):
            b=0x729d80+i*0x998
            u.mem_write(b,struct.pack('<i',prop.active));u.mem_write(b+12,struct.pack('<i',prop.kind));u.mem_write(b+0x29c,bytes(prop.position))
        for i,t in enumerate(inp.interaction.clips):
            u.mem_write(self.model+i*156+0x1e4,struct.pack('<2f',t.start,t.end));u.mem_write(self.model+i*156+0x1f0,struct.pack('<f',t.source))
        u.mem_write(self.model+0x140,struct.pack('<i',inp.spatial.movement.active_clip))
        for model,group,root in [(self.model,self.root_group,self.root),(self.shadow_model,self.shadow_group,self.shadow_root)]:
            u.mem_write(model+0x160,struct.pack('<I',group));u.mem_write(group+0x14,struct.pack('<I',root))
            u.mem_write(root,bytes(0x300));u.mem_write(root+0x100,struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
        u.mem_write(self.head+0xf4,struct.pack('<f',inp.spatial.cached_head_height))
        self.call(0x4c009b,struct.pack('<2I',0x3000000,p),limit=30000000)
        result=State.from_buffer_copy(state);result.spatial.scene=self.read(state.spatial.scene)
        for name,offset,fmt in FIELDS:
            vs=struct.unpack('<'+fmt,u.mem_read(p+offset,struct.calcsize(fmt)));v=getattr(result.spatial.movement,name)
            if isinstance(v,C.Array):v[:]=vs
            else:setattr(result.spatial.movement,name,vs[0])
        result.spatial.movement.move_latch=struct.unpack('<i',u.mem_read(0x7099ec,4))[0]
        result.spatial.vertical_position=struct.unpack('<f',u.mem_read(p+0x2a8,4))[0]
        result.interaction.trigger.origin[:]=struct.unpack('<4f',u.mem_read(p+0x7d8,16));result.interaction.trigger.target[:]=struct.unpack('<4f',u.mem_read(p+0x7e8,16))
        result.interaction.trigger.prop_kind=struct.unpack('<i',u.mem_read(p+0x800,4))[0]
        result.interaction.script_phase=struct.unpack('<b',u.mem_read(p+0x7f8,1))[0]
        result.completion_requested=struct.unpack('<b',u.mem_read(p+0x7c8,1))[0];result.wall_available=struct.unpack('<b',u.mem_read(p+0x57a,1))[0]
        return result,self.roots

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);args=ap.parse_args()
    exe=args.exe.read_bytes();native=Native(exe);lib=library();bind(lib);rng=random.Random(0x4c009b)
    lib.bk_player_control_step.argtypes=[C.POINTER(State),C.c_void_p,C.POINTER(Input),C.POINTER(Effects),C.c_void_p]
    lib.bk_player_actions_initialize.argtypes=[C.POINTER(C.c_int32)]
    archive=Archive(args.data/'bk3_03.pp');entries={e.name.lower():e for e in archive.entries}
    error=C.create_string_buffer(256);worst=0;cases=0;defined=0;undefined=0;phases={};records=[]
    def scalar(a,b,context):
        nonlocal worst
        delta=abs(a-b)/max(1,abs(b));worst=max(worst,delta)
        assert math.isfinite(delta) and delta<4e-5,(context,a,b,delta)
    for file in sorted(args.data.glob('*.atr')):
        filename=file.with_suffix('.x').name;data=archive.read(entries[filename.lower()]);atr=file.read_bytes()
        ok,model,msg=decode(lib,data);assert ok,msg
        world=(C.c_float*(model.contents.frame_count*16))();assert lib.bk_model_world_matrices(model,world,len(world),error)
        collision=lib.bk_collision_create(model,world,len(world),filename.upper().encode(),atr,len(atr),error);assert collision,error.value
        try:
            native.create(model.contents,world,filename.upper().encode(),atr)
            meshes=[lib.bk_collision_mesh(collision,i).contents for i in range(lib.bk_collision_count(collision))]
            points=[Vec(*m.vertices[m.indices[i]]) for m in meshes for i in range(0,m.index_count,3)]
            for case in range(72):
                point=Vec(*rng.choice(points));point[0]+=rng.uniform(-5,5);point[2]+=rng.uniform(-5,5)
                camera=Vec(point[0]+7,point[1]+18,point[2]+23)
                state=State(spatial=Spatial(movement=Movement(position=point,previous=Vec(2,3,4),velocity=Vec(1,2,3),yaw=rng.uniform(-30,390),pitch=10,interaction_mode=rng.choice([0,1,2,5])),scene=Scene(wall=Wall(position=point,camera_distance=200,normal=Vec(1,0,0),near_wall=1),wall_heading=77,wall_name=rng.choice([b'NULL',b'Mesh_KABE_1@M00_10.X',b'Mesh_Atari_Hantei@M00_10.X',b'old wall']),surface_name=b'old ground',any_wall=1),vertical_position=57),return_yaw=-35,completion_mode=case%3,completion_requested=-1,wall_available=1)
                inp=Input(spatial=SpatialInput(movement=MovementInput(seconds=rng.choice([0,.016,.033,.5]),look=(C.c_float*2)(3,-2),buttons=rng.randrange(64),active_clip=rng.choice([0,4,18,22])),scene=SceneInput(wall=WallInput(camera=camera,rays=(Vec*7)(*(camera for _ in range(7)))),head_distance=99,projected_depth=.9,screen_scale=(C.c_float*2)(.625,.625),screen_position=(C.c_int32*2)(320,240),excluded_surface=b''),base_head_height=18,cached_head_height=point[1]+12),has_shadow=case%2)
                assert lib.bk_player_actions_initialize(inp.spatial.movement.actions)
                state.spatial.movement.action=rng.choice(list(inp.spatial.movement.actions))
                phase=[0,0,0,1,2,3,-1,2,1][case%9];state.interaction.script_phase=phase;phases[phase]=phases.get(phase,0)+1
                state.interaction.trigger.origin[:]=[point[0]-5,point[1],point[2]+7,-90]
                state.interaction.trigger.target[:]=[point[0]+10,point[1]+10,point[2]-7,120]
                inp.interaction.trigger.group=(case//9)%5;inp.interaction.trigger.area=case%9;inp.interaction.buttons=(case//4)%4
                for i,prop in enumerate(inp.interaction.trigger.props):
                    prop.active=int(i<2 and case%3==0);prop.kind=10 if i==0 else 11;prop.position[:]=[point[0]+i*3+10,point[1]+50,point[2]]
                for timing in inp.interaction.clips:timing.start=300;timing.end=600;timing.source=rng.choice([369,380,400,453,500,534,600,601])
                expected,roots=native.step_control(state,inp,rng)
                actual=State.from_buffer_copy(state);effect=Effects()
                assert lib.bk_player_control_step(C.byref(actual),collision,C.byref(inp),C.byref(effect),error),(filename,case,error.value)
                cases+=1
                if actual.spatial.scene.wall.missing_projection:
                    undefined+=1;continue
                defined+=1
                worst=max(worst,compare_movement(actual.spatial.movement,expected.spatial.movement,(filename,case)),compare_trigger(actual.interaction.trigger,expected.interaction.trigger,(filename,case)))
                assert actual.interaction.script_phase==expected.interaction.script_phase
                assert actual.completion_requested==expected.completion_requested
                assert actual.wall_available==expected.wall_available,(filename,case,'available')
                scalar(actual.spatial.vertical_position,expected.spatial.vertical_position,'vertical')
                a,b=actual.spatial.scene,expected.spatial.scene
                assert a.wall_name==b.wall_name and a.surface_name==b.surface_name and a.npc_in_view==b.npc_in_view and a.any_wall==b.any_wall
                assert a.wall.near_wall==b.wall.near_wall and tuple(a.wall.rays_blocked)==tuple(b.wall.rays_blocked)
                for av,bv in zip(a.wall.normal,b.wall.normal):scalar(av,bv,'normal')
                scalar(a.wall_heading,b.wall_heading,'heading')
                if not a.wall.singular_camera:scalar(a.wall.camera_distance,b.wall.camera_distance,'camera')
                commands=[(native.root,effect.roots[i]) for i in range(effect.placements)]
                if effect.shadow_place:commands.append((native.shadow_root,effect.shadow))
                assert len(commands)==len(roots),(filename,case,len(commands),len(roots))
                for (aroot,placement),(broot,matrix) in zip(commands,roots):
                    assert aroot==broot
                    for av,bv in zip(placement.world,matrix):scalar(av,bv,'root')
            records.append(dict(file=file.name,cases=72));print(filename,'PASS',flush=True)
        finally:lib.bk_collision_destroy(collision);lib.bk_model_destroy(model)
    result=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),samples=cases,defined=defined,undefined_projection_cases=undefined,phases=phases,max_relative_error=worst,scenes=records,hooks=['device input4b757e/4b76c2','cached head screen projection42d56c','collision constructor allocation/frame lookup'],scope=__doc__)
    (ROOT/'local/original-player-control-oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
