import ctypes as C
from pathlib import Path
import struct
import tempfile
import unittest
from model_binding import Model, Material, library, decode
from test_model import fixture
from test_assets import make_pp

class State(C.Structure):
    _fields_ = [('diffuse', C.c_float*4), ('encoded_alpha', C.c_float),
                ('blend', C.c_int), ('visible', C.c_int), ('alpha_hint', C.c_int),
                ('specular_enabled', C.c_int)]
class Image(C.Structure):
    _fields_ = [('width', C.c_uint32), ('height', C.c_uint32), ('rgba', C.c_void_p)]
class TextureImage(C.Structure):
    _fields_ = [('image', Image), ('alpha_hint', C.c_int)]

class MaterialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib = lib = library()
        lib.bk_material_state.argtypes = [C.POINTER(Material), C.c_int, C.POINTER(State), C.c_void_p]
        lib.bk_resources_create.argtypes = [C.c_void_p]; lib.bk_resources_create.restype = C.c_void_p
        lib.bk_resources_mount.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p, C.c_void_p]
        lib.bk_resources_destroy.argtypes = [C.c_void_p]
        lib.bk_model_texture_load.argtypes = [C.POINTER(Model), C.c_uint32, C.c_void_p,
                                             C.c_char_p, C.POINTER(TextureImage), C.c_void_p]
        lib.bk_image_free.argtypes = [C.POINTER(Image)]

    def test_native_confirmed_alpha_cases(self):
        # Expected values pinned by the independent original x86 oracle.
        for encoded, blend, effective, visible in [(1,0,1,1),(.5,0,.5,1),(1.5,1,.5,1),
                                                  (-1,2,1,1),(3,1,1,1),(-3,2,1,1),(0,0,0,0)]:
            m = Material(); m.diffuse[:] = [1,.5,.25,encoded]
            out = State(); error = C.create_string_buffer(256)
            self.assertTrue(self.lib.bk_material_state(C.byref(m),0,C.byref(out),error),error.value)
            self.assertEqual((out.blend,out.visible,out.diffuse[3]),(blend,visible,effective))
        m.diffuse[3] = -0.0
        self.assertTrue(self.lib.bk_material_state(C.byref(m),0,C.byref(out),error))
        self.assertEqual(bytes(out.diffuse)[12:16],struct.pack('<f',-0.0))

    def test_nonfinite_rejected(self):
        error = C.create_string_buffer(256)
        for name in ('diffuse','ambient','specular','emissive','power'):
            for value in (float('nan'),float('inf'),-float('inf')):
                m = Material(); m.diffuse[3] = 1
                if name=='power':m.power=value
                else:getattr(m,name)[0]=value
                out=State()
                self.assertFalse(self.lib.bk_material_state(C.byref(m),0,C.byref(out),error))

    def test_pack_binding_alpha_flag_missing_and_corruption(self):
        result, model, error = decode(self.lib,fixture()); self.assertEqual(result,1,error)
        message=C.create_string_buffer(256);store=self.lib.bk_resources_create(message)
        self.assertTrue(store,message.value)
        try:
            model.contents.textures[0].filename=b'WALL.TGA'
            header=bytearray(18);header[2]=2;header[12]=header[14]=1;header[16]=32;header[17]=40
            # Fully opaque TGA still receives the original loader's alpha flag.
            with tempfile.TemporaryDirectory() as d:
                path=Path(d)/'texture.pp';path.write_bytes(make_pp([('wall.tga',bytes(header)+b'\0\0\xff\xff')]))
                self.assertTrue(self.lib.bk_resources_mount(store,b'scene',str(path).encode(),message))
                out=TextureImage()
                self.assertTrue(self.lib.bk_model_texture_load(model,0,store,b'scene',C.byref(out),message),message.value)
                self.assertEqual((out.image.width,out.image.height,out.alpha_hint),(1,1,1))
                self.assertEqual(C.string_at(out.image.rgba,4),b'\xff\0\0\xff')
                self.lib.bk_image_free(C.byref(out.image))
                self.assertFalse(self.lib.bk_model_texture_load(model,0,store,b'missing',C.byref(out),message))
                self.assertFalse(out.image.rgba)
                overlay=Path(d)/'bad.pp';overlay.write_bytes(make_pp([('wall.tga',b'broken')]))
                self.assertTrue(self.lib.bk_resources_mount(store,b'scene',str(overlay).encode(),message))
                self.assertFalse(self.lib.bk_model_texture_load(model,0,store,b'scene',C.byref(out),message))
                self.assertFalse(out.image.rgba)
                model.contents.textures[0].filename=b'wall.dds'
                self.assertFalse(self.lib.bk_model_texture_load(model,0,store,b'scene',C.byref(out),message))
                self.assertIn(b'unsupported',message.value)
        finally:
            self.lib.bk_resources_destroy(store);self.lib.bk_model_destroy(model)

if __name__=='__main__':unittest.main()
