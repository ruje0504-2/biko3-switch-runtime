import ctypes as C
import struct
import unittest
from model_binding import library,decode
from environment_binding import bind
from test_model import fixture,chunk,named

def light_record(kind,id):
    return named('light',id)+struct.pack('<I25f',kind,*([.2,.15,.1,0]+[1,1,1,0]+[0,0,0,0]+[99,98,97]+[0,0,1]+[100,1,0,.1,0,.8,1]))
def environment_fixture():
    data=bytearray(fixture())
    fram=data.index(b'FRAM')+8
    struct.pack_into('<I',data,fram+180,40)
    struct.pack_into('<I',data,fram+396+180,41)
    return bytes(data)+chunk(b'LIGH',light_record(0xffffffff,40)+light_record(1,41))+chunk(b'FOG ',struct.pack('<5I3f',0,3,0,0,0xffffffff,0,10000,0))

class EnvironmentTests(unittest.TestCase):
    def setUp(self): self.lib=bind(library())
    def check_data(self,data,valid=True,enabled=0,spot=False):
        result,model,message=decode(self.lib,bytes(data));self.assertEqual(result,1,message)
        error=C.create_string_buffer(256);world=(C.c_float*(model.contents.frame_count*16))();env=None
        try:
            self.assertTrue(self.lib.bk_model_world_matrices(model,world,len(world),error))
            env=self.lib.bk_model_environment_create(model,world,error)
            self.assertEqual(bool(env),valid,error.value)
            if valid:
                s=env.contents
                self.assertEqual((s.light_count,s.ambient_count,s.lighting.point_count),(2,1,int(not spot)))
                self.assertEqual(s.lighting.spot_count,int(spot))
                point=s.lighting.spots[0].point if spot else s.lighting.points[0]
                if spot:self.assertEqual(list(s.lighting.spots[0].direction),[0,0,1])
                self.assertEqual([round(x*255) for x in s.lighting.ambient],[51,38,25])
                self.assertEqual(list(point.position),[5,0,0])
                self.assertEqual(list(point.specular),[1,1,1])
                self.assertEqual((s.lights[1].frame_index,s.fog.enabled,s.fog.mode,s.fog.end),(1,enabled,3,10000))
        finally:
            self.lib.bk_model_environment_destroy(env);self.lib.bk_model_destroy(model)
    def test_light_layout_frame_binding_ambient_quantization(self): self.check_data(environment_fixture())
    def test_reject_unsupported_and_malformed_environment(self):
        base=environment_fixture();lp=base.index(b'LIGH')+8;fp=base.index(b'FRAM')+8;fog=base.index(b'FOG ')+8
        for offset,fmt,value in [(lp+68,'I',3),(lp+172+64,'I',40),(lp+172+72,'f',float('nan')),
                                 (lp+172+156,'f',-1),(fp+180,'I',999),(fp+396+180,'I',0),
                                 (fog,'I',2),(fog+24,'f',float('inf'))]:
            data=bytearray(base);struct.pack_into('<'+fmt,data,offset,value)
            self.check_data(data,False)
        self.check_data(fixture(),False)
    def test_enabled_fog_and_specular_materials(self):
        base=bytearray(environment_fixture());fog=base.index(b'FOG ')+8
        struct.pack_into('<I',base,fog,1)
        struct.pack_into('<f',base,base.index(b'MATE')+8+68+16*4,32)
        self.check_data(base,enabled=1)
        for off,fmt,v in [(4,'I',4),(8,'I',2),(12,'I',2),(28,'f',-1),(28,'f',2),(24,'f',0)]:
            bad=bytearray(base);struct.pack_into('<'+fmt,bad,fog+off,v);self.check_data(bad,False)
    def test_reject_all_light_record_truncations(self):
        base=environment_fixture();lp=base.index(b'LIGH');block=base[lp+8:lp+8+344]
        for size in range(344):
            # 172 is structurally complete but leaves a dangling frame reference.
            self.check_data(base[:lp]+chunk(b'LIGH',block[:size]),False)

    def test_spot_cone_validation_and_frame_direction(self):
        raw=bytearray(environment_fixture());offset=raw.index(b'LIGH')+8+172
        struct.pack_into('<I',raw,offset+68,2)
        self.check_data(raw,spot=True)
        for off,v in [(148,-1),(148,float('inf')),(164,-.1),(164,1.1),(168,4),(168,float('nan'))]:
            bad=bytearray(raw);struct.pack_into('<f',bad,offset+off,v);self.check_data(bad,False)
