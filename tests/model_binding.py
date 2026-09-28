"""ctypes view of the public CPU model API, used only by host tests."""
import ctypes as C
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
u32 = C.c_uint32

class Chunk(C.Structure):
    _fields_ = [('tag', C.c_char*5), ('offset', u32), ('size', u32)]
class Vertex(C.Structure):
    _fields_ = [('position', C.c_float*3), ('beta', C.c_float),
                ('normal', C.c_float*3), ('uv', (C.c_float*2)*4)]
class Mesh(C.Structure):
    _fields_ = [('name', C.c_char*65), ('id', u32), ('first_submesh', u32), ('submesh_count', u32)]
class Submesh(C.Structure):
    _fields_ = [('name', C.c_char*65), ('id', u32), ('mesh_index', u32),
                ('vertex_count', u32), ('index_count', u32), ('material_id', u32),
                ('texture_ids', u32*4), ('texture_count', u32), ('material_index', u32),
                ('texture_indices', u32*4), ('source_header_offset', u32),
                ('vertices', C.POINTER(Vertex)), ('indices', C.POINTER(C.c_uint16))]
class Material(C.Structure):
    _fields_ = [('name', C.c_char*65), ('id', u32), ('diffuse', C.c_float*4),
                ('ambient', C.c_float*4), ('specular', C.c_float*4), ('emissive', C.c_float*4),
                ('power', C.c_float), ('unknown', C.c_float)]
class Texture(C.Structure):
    _fields_ = [('name', C.c_char*65), ('filename', C.c_char*65), ('id', u32)]
class Frame(C.Structure):
    _fields_ = [('name', C.c_char*65), ('id', u32), ('parent_id', u32), ('mesh_id', u32),
                ('parent_index', u32), ('mesh_index', u32), ('local', C.c_float*16)]
class Model(C.Structure):
    _fields_ = [('source', C.POINTER(C.c_ubyte)), ('source_size', C.c_size_t),
                ('chunks', C.POINTER(Chunk)), ('chunk_count', u32),
                ('meshes', C.POINTER(Mesh)), ('mesh_count', u32),
                ('submeshes', C.POINTER(Submesh)), ('submesh_count', u32),
                ('materials', C.POINTER(Material)), ('material_count', u32),
                ('textures', C.POINTER(Texture)), ('texture_count', u32),
                ('frames', C.POINTER(Frame)), ('frame_count', u32),
                ('vertex_count', C.c_uint64), ('triangle_count', C.c_uint64)]

def library():
    path = Path(os.environ.get('BK3_BUILD_DIR', ROOT/'build'))
    lib = C.CDLL(str(path/('libmodel-test.dylib' if sys.platform=='darwin' else 'libmodel-test.so')))
    lib.bk_model_decode.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(C.POINTER(Model)), C.c_void_p]
    lib.bk_model_decode.restype = C.c_int
    lib.bk_model_destroy.argtypes = [C.POINTER(Model)]
    lib.bk_model_world_matrices.argtypes = [C.POINTER(Model), C.POINTER(C.c_float), C.c_size_t, C.c_void_p]
    lib.bk_model_world_matrices.restype = C.c_int
    return lib

def decode(lib, data):
    out = C.POINTER(Model)()
    error = C.create_string_buffer(256)
    result = lib.bk_model_decode(data, len(data), C.byref(out), error)
    return result, out, error.value.decode()
