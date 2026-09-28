import ctypes as C
import struct
import unittest
from model_binding import library, decode

def chunk(tag, data):
    return tag + struct.pack('<I',len(data)) + data

def named(name, id):
    return name.encode().ljust(64,b'\0') + struct.pack('<I',id)

def fixture(grouped=False, cycle=False, index=2, mid=10, texid=20, reverse_frames=False):
    material = named('stone',10) + struct.pack('<18f', *([1]*8+[0]*9+[1]))
    texture = named('wall',20) + b'wall.bmp'.ljust(64,b'\0') + bytes(72)
    frames = []
    for id, parent, mesh, x in [(100, 101 if cycle else 0,0,2), (101,100,30,3)]:
        f = bytearray(396); f[:68] = named('node',id)
        struct.pack_into('<16f',f,68,1,0,0,0,0,1,0,0,0,0,1,0,x,0,0,1)
        struct.pack_into('<II',f,172,parent,mesh); frames.append(f)
    sub=bytearray(332);struct.pack_into('<I',sub,4,mid);struct.pack_into('<I',sub,8,texid)
    struct.pack_into('<III',sub,64,3,3,1)
    vertex=b''.join(struct.pack('<15f',x,y,0,i,0,0,1,u,v,0,0,0,0,0,0)
                    for i,(x,y,u,v) in enumerate([(0,0,0,0),(1,0,1,0),(0,1,0,1)]))
    part=bytes(sub)+vertex+struct.pack('<3H',0,1,index)
    mesh=named('mesh',30)+struct.pack('<I',2 if grouped else 1)
    mesh += (named('child1',31)+struct.pack('<I',1)+part+
             named('child2',32)+struct.pack('<I',1)+part) if grouped else part
    if reverse_frames:frames.reverse()
    return b'OBJM'+bytes(8)+chunk(b'MATE',material)+chunk(b'TEXT',texture)+chunk(b'FRAM',b''.join(frames))+chunk(b'MESH',mesh)

class ModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.lib=library()

    def test_owned_data_and_references(self):
        source=bytearray(fixture()); buf=(C.c_ubyte*len(source)).from_buffer(source)
        result,out,error=decode(self.lib,buf)
        self.assertEqual(result,1,error)
        try:
            source[:]=bytes(len(source))
            m=out.contents
            self.assertEqual((m.mesh_count,m.submesh_count,m.vertex_count,m.triangle_count),(1,1,3,1))
            self.assertEqual(m.submeshes[0].material_index,0)
            self.assertEqual(m.submeshes[0].texture_indices[0],0)
            self.assertEqual(m.frames[1].parent_index,0)
            self.assertEqual(m.frames[1].mesh_index,0)
            self.assertEqual(m.frames[1].local[12],3)
            self.assertEqual(m.submeshes[0].vertices[1].position[0],1)
        finally:self.lib.bk_model_destroy(out)

    def test_grouped_mesh(self):
        result,out,error=decode(self.lib,fixture(grouped=True));self.assertEqual(result,1,error)
        try:
            self.assertEqual(out.contents.submesh_count,2)
            self.assertEqual(out.contents.submeshes[1].id,32)
        finally:self.lib.bk_model_destroy(out)

    def test_invalid_data_is_not_success(self):
        cases=[fixture(cycle=True),fixture(index=3),fixture(mid=999),fixture(texid=999)]
        valid=fixture()
        cases += [valid[:n] for n in range(len(valid))]
        cases += [valid+chunk(b'MESH',b''),valid+chunk(b'JUNK',b'x')[:-1]]
        for data in cases:
            result,out,error=decode(self.lib,data)
            if out:self.lib.bk_model_destroy(out)
            self.assertEqual(result,-1,error)
            self.assertFalse(out)
            self.assertTrue(error)

    def test_text_and_unknown_version_classified(self):
        for data in [b'xof 0302txt 0032\nFrame {}',b'OBJM'+struct.pack('<II',1,0)]:
            result,out,error=decode(self.lib,data)
            self.assertEqual(result,0,error);self.assertFalse(out)

    def test_world_matrix_order_and_overflow(self):
        result,out,error=decode(self.lib,fixture());self.assertEqual(result,1,error)
        try:
            world=(C.c_float*32)();message=C.create_string_buffer(256)
            self.assertEqual(self.lib.bk_model_world_matrices(out,world,32,message),1,message.value)
            self.assertEqual(world[28],5)
            out.contents.frames[0].local[0]=2
            self.assertEqual(self.lib.bk_model_world_matrices(out,world,32,message),1,message.value)
            self.assertEqual(world[28],8)
            out.contents.frames[0].local[0]=3e38
            out.contents.frames[1].local[0]=3e38
            self.assertEqual(self.lib.bk_model_world_matrices(out,world,32,message),0)
            self.assertIn(b'overflow',message.value)
        finally:self.lib.bk_model_destroy(out)

    def test_parent_can_follow_child(self):
        result,out,error=decode(self.lib,fixture(reverse_frames=True));self.assertEqual(result,1,error)
        try:
            world=(C.c_float*32)();message=C.create_string_buffer(256)
            self.assertEqual(out.contents.frames[0].parent_index,1)
            self.assertEqual(self.lib.bk_model_world_matrices(out,world,32,message),1,message.value)
            self.assertEqual(world[12],5)
        finally:self.lib.bk_model_destroy(out)

if __name__=='__main__':unittest.main()
