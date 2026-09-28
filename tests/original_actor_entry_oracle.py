"""Native actor CPU initialization with nonzero retained state.

Executes full4befc0 and initializer instruction ranges, including real CKP
loader and action setters. No substitute reset/placement policy. Model/face/
shadow resource stages are separate, previously verified adapters.
"""
import argparse, ctypes as C, hashlib, json, math, random, struct
from pathlib import Path
from original_entry_oracle import Native, Request, Selection
from original_player_control_oracle import State as Player
from original_player_movement_oracle import FIELDS
from original_npc_spatial_oracle import State as Npc
from original_footsteps_oracle import Actions
from original_route_oracle import Point
from original_placement_oracle import Placement
from model_binding import ROOT, library

class Metadata(C.Structure):
    _fields_ = [('start', C.c_uint32), ('end', C.c_uint32)]

def populate(s, rng):
    for name, typ in s._fields_:
        if issubclass(typ, C.Structure): populate(getattr(s, name), rng)
        elif issubclass(typ, C.Array):
            if typ._type_ is C.c_char: setattr(s, name, b'retained')
            else:
                v = getattr(s, name)
                for j in range(len(v)):
                    v[j] = rng.uniform(-30, 30) if typ._type_ is C.c_float else rng.randrange(1, 90)
        else: setattr(s, name, rng.uniform(-30, 30) if typ is C.c_float else rng.randrange(1, 90))

def obj(s, path):
    names = path.split('.')
    for n in names[:-1]: s = getattr(s, n)
    return s, names[-1]

def transfer(u, base, s, fields, write):
    for path, offset, fmt in fields:
        target, name = obj(s, path)
        value = getattr(target, name)
        if write:
            values = list(value) if isinstance(value, C.Array) else [value]
            u.mem_write(base + offset, struct.pack('<' + fmt, *values))
        else:
            values = struct.unpack('<' + fmt, u.mem_read(base + offset, struct.calcsize(fmt)))
            if isinstance(value, C.Array): value[:] = values
            else: setattr(target, name, values[0])

def compare(a, b, path=''):
    worst = 0
    for name, typ in a._fields_:
        x, y = getattr(a, name), getattr(b, name)
        if issubclass(typ, C.Structure): worst = max(worst, compare(x, y, path+'.'+name))
        else:
            xs, ys = (list(x), list(y)) if isinstance(x, C.Array) else ([x], [y])
            for xv, yv in zip(xs, ys):
                if isinstance(xv, float):
                    assert math.isfinite(xv) and math.isfinite(yv)
                    delta = abs(xv-yv); worst = max(worst, delta)
                    assert delta <= 1e-4, (path, name, xv, yv)
                else: assert xv == yv, (path, name, xv, yv)
    return worst

PF = [('spatial.movement.'+name, offset, fmt) for name, offset, fmt in FIELDS]
PF += [('spatial.vertical_position',0x2a8,'f'),('spatial.scene.wall_heading',0x7d4,'f'),
       ('spatial.scene.surface_name',0x5c0,'260s'),('spatial.scene.wall_name',0x6c4,'260s'),
       ('interaction.trigger.origin',0x7d8,'4f'),('interaction.trigger.target',0x7e8,'4f'),
       ('interaction.trigger.prop_kind',0x800,'i'),('interaction.script_phase',0x7f8,'b'),
       ('return_yaw',0x7fc,'f'),('completion_requested',0x7c8,'b'),
       ('completion_mode',0x7cb,'b'),('wall_available',0x57a,'b')]
NF = [('path.position',0x29c,'3f'),('path.yaw_degrees',0x2ac,'f'),('path.cursor',0x830,'I'),
      ('ai.point.motion.action',0x10,'i'),('ai.point.motion.behavior',0x850,'i'),
      ('ai.point.motion.hidden',0x328,'B'),('ai.point.motion.mode',0x578,'b'),
      ('ai.point.motion.route_flag',0x84c,'b'),
      ('ai.point.motion.wait.duration',0x840,'I'),('ai.point.motion.wait.deadline',0x844,'I'),
      ('ai.point.motion.wait.armed',0x848,'B'),('ai.point.action_wait.duration',0x854,'I'),
      ('ai.point.action_wait.deadline',0x858,'I'),('ai.point.action_wait.armed',0x85c,'B'),
      ('ai.point.background_wait',0x330,'B'),('ai.point.fade_out',0x331,'B'),
      ('ai.point.gate_state',0x870,'B'),('ai.stimulus',0x319,'b'),
      ('alpha',0x32c,'f'),('visible',0x318,'B'),('surface_name',0x5c0,'260s')]
GF = [('path.run_remaining',0xbf3c6c,'i'),('path.segment_start',0xbf3c80,'I'),
      ('path.segment_end',0xbf3c84,'I'),('path.last_crossed',0xbf3c74,'I'),('path.crossed',0xbf3c78,'B')]


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',type=Path);ap.add_argument('data',type=Path);a=ap.parse_args()
    exe=a.exe.read_bytes();n=Native(exe,a.data);u=n.uc;lib=library();rng=random.Random(0x4fad20)
    lib.bk_game_entry_select.argtypes=[C.POINTER(Selection),C.POINTER(Request)]
    lib.bk_player_entry_reset.argtypes=[C.POINTER(Player),C.POINTER(C.c_int32),C.POINTER(C.c_uint8),C.POINTER(Request),C.c_float]
    lib.bk_player_entry_placement.argtypes=[C.POINTER(Placement),C.POINTER(Request)]
    lib.bk_npc_entry_reset.argtypes=[C.POINTER(Npc),C.POINTER(Actions),C.POINTER(Metadata),C.c_void_p,C.POINTER(Request),C.c_uint32]
    lib.bk_route_decode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.bk_route_decode.restype=C.c_void_p
    lib.bk_route_count.argtypes=[C.c_void_p];lib.bk_route_point.argtypes=[C.c_void_p,C.c_uint32];lib.bk_route_point.restype=C.POINTER(Point)
    lib.bk_route_destroy.argtypes=[C.c_void_p];lib.bk_route_set_flags.argtypes=[C.c_void_p,C.c_uint32,C.c_uint8]
    counts={'player':0,'npc':0,'invalid_override':0,'same_start_override':0,'rejections':0};worst=0
    player=0x71b510;actor=0x728de8;head=0x300d000;error=C.create_string_buffer(256)
    for g in range(5):
      for area in range(9):
        for flow in range(256):
          req=Request(g,area,0,flow);sel=Selection();assert lib.bk_game_entry_select(C.byref(sel),C.byref(req))
          state=Player();populate(state,rng);old=Player.from_buffer_copy(state)
          slots=(C.c_int32*21)(*[rng.randrange(30,90) for _ in range(21)]);inventory=(C.c_uint8*5)(*[rng.randrange(256) for _ in range(5)])
          transfer(u,player,state,PF,True);u.mem_write(player+0x14,bytes(slots));u.mem_write(player+0x7cc,bytes(inventory));n.word(0x7219a8,g);u.mem_write(0x721ad4,bytes([flow]))
          n.call(0x4befc0,struct.pack('<I4f',player,*sel.player_position,sel.player_yaw))
          n.word(n.stack+8,player);n.run(0x4bf1d4,0x4bf3e8)
          height=C.c_float(rng.uniform(-100,100)).value;n.word(player+8,head);u.mem_write(head+0xf4,struct.pack('<f',height));n.run(0x4bf5c4,0x4bf5f4)
          transfer(u,player,old,PF,False);old.spatial.scene.wall.position[:]=old.spatial.movement.position[:]
          assert lib.bk_player_entry_reset(C.byref(state),slots,inventory,C.byref(req),height)
          worst=max(worst,compare(state,old));assert bytes(slots)==bytes(u.mem_read(player+0x14,84));assert bytes(inventory)==bytes(u.mem_read(player+0x7cc,5))
          placement=Placement();assert lib.bk_player_entry_placement(C.byref(placement),C.byref(req));assert list(placement.position)==list(old.spatial.movement.position);assert placement.yaw_degrees==old.spatial.movement.yaw
          counts['player']+=1
        raw=(a.data/sel.route_file.decode()).read_bytes();route=lib.bk_route_decode(raw,len(raw),error);assert route,error.value;count=lib.bk_route_count(route);lib.bk_route_destroy(route)
        for case in range(64):
          flow=[8,0x38,0x48,1,255,0,2,16][case%8];cursor=rng.randrange(count);req=Request(g,area,cursor,flow);assert lib.bk_game_entry_select(C.byref(sel),C.byref(req))
          state=Npc();populate(state,rng);expected=Npc.from_buffer_copy(state);actions=Actions();populate(actions,rng);meta=Metadata(999,777)
          route=lib.bk_route_decode(raw,len(raw),error);assert route,error.value
          try:
            if sel.route_cursor>=count:
              before=(bytes(state),bytes(actions),bytes(meta));assert not lib.bk_npc_entry_reset(C.byref(state),C.byref(actions),C.byref(meta),route,C.byref(req),0);assert before==(bytes(state),bytes(actions),bytes(meta));counts['invalid_override']+=1;continue
            start=sel.route_cursor if flow==0x48 else rng.randrange(count+1)
            if flow==0x48: counts['same_start_override']+=1
            transfer(u,actor,state,NF,True);transfer(u,0,state,GF,True)
            for name,offset in [('walk',0x18),('run',0x20),('special',0x68),('extra',0x70)]:u.mem_write(actor+offset,bytes(getattr(actions,name)))
            n.word(0x7219a8,g);u.mem_write(0x721ad4,bytes([flow]));u.mem_write(0x5767c8,b'\1');n.word(0xbf3c8c+g*108+area*12,cursor);n.word(0xbf3ea8+g*108+area*12,start)
            u.mem_write(n.stack+8,struct.pack('<3I',actor,g,area));n.run(0x4fb5f0,0x4fba03)
            transfer(u,actor,expected,NF,False);transfer(u,0,expected,GF,False)
            assert lib.bk_npc_entry_reset(C.byref(state),C.byref(actions),C.byref(meta),route,C.byref(req),start)
            worst=max(worst,compare(state,expected));assert bytes(meta)==bytes(u.mem_read(actor+0x834,8))
            for name,offset in [('walk',0x18),('run',0x20),('special',0x68),('extra',0x70)]:assert bytes(getattr(actions,name))==bytes(u.mem_read(actor+offset,8))
            for i in range(count+1):assert lib.bk_route_point(route,i).contents.flags==u.mem_read(0xbe9a28+i*20,1)[0]
            before=(bytes(state),bytes(actions),bytes(meta));assert not lib.bk_npc_entry_reset(C.byref(state),C.byref(actions),C.byref(meta),route,C.byref(req),count+1);assert before==(bytes(state),bytes(actions),bytes(meta));assert not lib.bk_route_set_flags(route,count,1);counts['rejections']+=2
            counts['npc']+=1
          finally:lib.bk_route_destroy(route)
    # Nonfinite head and invalid profile reject without modifying any output.
    for height,req in [(float('nan'),Request()),(float('inf'),Request()),(1,Request(5,0,0,8))]:
      before=(bytes(state) if isinstance(state,Player) else b'',bytes(slots),bytes(inventory));p=Player();populate(p,rng);saved=bytes(p)
      assert not lib.bk_player_entry_reset(C.byref(p),slots,inventory,C.byref(req),height);assert bytes(p)==saved and bytes(slots)==before[1] and bytes(inventory)==before[2];counts['rejections']+=1
    report=dict(passed=True,exe_sha256=hashlib.sha256(exe).hexdigest(),cases=counts,max_error=worst,native_ranges=['4befc0 full','4bf1d4..4bf3e8','4bf5c4..4bf5f4','4fb5f0..4fba03 including4ff78e'],scope='Represented actor CPU resets, retained fields, all256 entry flow bytes for player, all45 real routes and profile endpoints; model/face/shadow stages and enclosing loader excluded. Flow48 out-of-decoded-route requests rejected; no equivalence claimed to padded original reads.')
    (ROOT/'local/original-actor-entry-oracle.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',counts,'max_error',worst)
if __name__=='__main__':main()
