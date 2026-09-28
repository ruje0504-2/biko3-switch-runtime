import ctypes as C
from model_binding import Model, library
from test_material import State

class Part(C.Structure):
    _fields_ = [('material', State), ('priority', C.c_uint32), ('sort_bias', C.c_uint32),
                ('sorted', C.c_int), ('depth_write', C.c_int)]
class Instance(C.Structure):
    _fields_ = [('frame', C.c_uint32), ('submesh', C.c_uint32)]
class Static(C.Structure):
    _fields_ = [('model', C.POINTER(Model)), ('parts', C.POINTER(Part)),
                ('world', C.POINTER(C.c_float)), ('instances', C.POINTER(Instance)),
                ('instance_count', C.c_uint32), ('opaque_count', C.c_uint32)]
def bind(lib):
    lib.bk_static_model_create.argtypes = [C.POINTER(Model), C.POINTER(C.c_int), C.c_void_p]
    lib.bk_static_model_create.restype = C.POINTER(Static)
    lib.bk_static_model_destroy.argtypes = [C.POINTER(Static)]
    lib.bk_static_model_order.argtypes = [C.POINTER(Static), C.POINTER(C.c_float),
                                         C.POINTER(C.c_uint32), C.POINTER(C.c_float), C.c_void_p]
    lib.bk_static_model_order.restype = C.c_int
    lib.bk_static_sort_keys.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_float),
                                       C.POINTER(C.c_uint32), C.c_uint32]
    return lib
