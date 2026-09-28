import ctypes as C
import unittest
from model_binding import library, decode
from static_binding import bind
from test_model import fixture

class StaticTests(unittest.TestCase):
    def setUp(self):
        self.lib = bind(library())
        result, self.model, message = decode(self.lib, fixture(grouped=True, reverse_frames=True))
        self.assertEqual(result, 1, message)
        self.error = C.create_string_buffer(256)
        self.alpha = (C.c_int*1)(0)
        self.static = None
    def tearDown(self):
        self.lib.bk_static_model_destroy(self.static)
        self.lib.bk_model_destroy(self.model)
    def create(self):
        self.static = self.lib.bk_static_model_create(self.model, self.alpha, self.error)
        self.assertTrue(self.static, self.error.value)
        return self.static.contents
    def test_instancing_and_parent_after_child(self):
        # Give both parent and child the same mesh; traversal must visit parent first.
        self.model.contents.frames[1].mesh_index = 0
        s = self.create()
        self.assertEqual((s.instance_count, s.opaque_count), (4, 4))
        self.assertEqual([(s.instances[i].frame, s.instances[i].submesh) for i in range(4)],
                         [(1,0), (1,1), (0,0), (0,1)])
        self.assertEqual((s.world[12], s.world[28]), (5,2))
        identity = (C.c_float*16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
        order = (C.c_uint32*4)(); distances = (C.c_float*4)()
        self.assertTrue(self.lib.bk_static_model_order(self.static, identity, order, distances, self.error))
        self.assertEqual(list(order), [0,1,2,3])
        self.assertEqual(list(distances), [2,2,5,5])
        identity[0] = float('nan')
        self.assertFalse(self.lib.bk_static_model_order(self.static, identity, order, distances, self.error))
    def test_alpha_texture_depth_policy(self):
        for alpha, texture, expected in [(1,0,(0,1)), (1,1,(1,1)), (.5,0,(1,0)),
                                           (1.5,0,(1,0)), (-1,0,(1,0)), (0,0,(1,0))]:
            self.model.contents.materials[0].diffuse[3] = alpha
            self.alpha[0] = texture
            s = self.create()
            self.assertEqual((s.parts[0].sorted, s.parts[0].depth_write), expected)
            self.lib.bk_static_model_destroy(self.static); self.static = None
        self.model.contents.submeshes[0].texture_count = 0
        s = self.create()
        self.assertEqual((s.parts[0].sorted, s.parts[0].depth_write), (0,1))
    def test_unsupported_state_is_rejected(self):
        m = self.model.contents
        for field in [0,56,60]:
            offset = m.submeshes[0].source_header_offset + field
            m.source[offset] = 3
            self.static = self.lib.bk_static_model_create(self.model, self.alpha, self.error)
            self.assertFalse(self.static)
            self.assertIn(b'unsupported', self.error.value)
            m.source[offset] = 0
