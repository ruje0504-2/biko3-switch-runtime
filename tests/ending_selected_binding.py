"""ctypes declarations for the independent4D1025 CPU asset owner."""
import ctypes as C
from original_ending_tertiary_assets_oracle import (
    bind as shared_bind, CameraState, Presets, Background)
from original_ending_selected_config_oracle import Config, bind as config_bind
from original_face_controller_oracle import State as FaceState
from model_binding import Material


class Load(C.Structure):
    _fields_ = [('group', C.c_uint), ('variant', C.c_uint), ('selection', C.c_uint),
                ('selected', C.c_int32), ('primary_path', C.c_char_p),
                ('background', C.c_void_p)]


def bind(lib):
    shared_bind(lib)
    config_bind(lib)
    p, u, n, f = C.c_void_p, C.c_uint, C.c_int, C.c_float
    fp = C.POINTER(C.c_float)
    # The selected oracle hides the live forest, not an isolated actor.
    # Without this declaration ctypes narrows its 64-bit owner to a C int.
    lib.bk_actor_forest_visibility.argtypes = [p, C.c_uint32, C.c_uint32, p]
    lib.bk_actor_forest_visibility.restype = n
    for name, args, result in [
        ('create', [p, C.POINTER(Load), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
                    C.POINTER(CameraState), C.POINTER(Presets), p], p),
        ('destroy', [p], None), ('load_background', [p, p, p], n),
        ('config', [p], C.POINTER(Config)), ('replaced_background', [p], n),
        ('background_first', [p], n), ('background', [p], p), ('forest', [p], p),
        ('pose', [p, u], p), ('root', [p, u], C.c_uint32),
        ('model_name', [p, u], C.c_char_p), ('advance', [p, u, f, p], n),
        ('advance_plain', [p, f, n, p], n),
        ('pointer', [p, C.POINTER(C.c_int32), C.POINTER(C.c_int32),
                      C.POINTER(C.c_int32), p], n),
        ('drag', [p, C.c_int32, C.c_int32, fp, p], n),
        ('materials', [p, u], p), ('material_animation', [p, u], p), ('morph', [p, u], p),
        ('mesh', [p, u, C.c_uint32], p),
        ('material_alpha', [p, C.c_char_p, C.c_uint32, f, p], n),
        ('face', [p], p), ('face_state', [p], C.POINTER(FaceState)), ('eyes', [p], p),
        ('cameras', [p], p), ('node', [p, u], C.c_uint32), ('anchor', [p], C.c_uint32),
        ('follow', [p], C.c_uint32), ('special', [p], C.c_uint32),
        ('visible_node', [p, u], C.c_uint32), ('missing_visible', [p], u),
        ('target', [p, u], fp),
    ]:
        fn = getattr(lib, 'bk_ending_selected_assets_' + name)
        fn.argtypes, fn.restype = args, result
    lib.bk_menu_camera_dialogue.argtypes = [C.POINTER(CameraState)]
    lib.bk_menu_camera_dialogue.restype = C.c_int
    lib.bk_face_init.argtypes = [C.POINTER(FaceState), C.c_uint32, C.c_uint32, p]
    lib.bk_face_init.restype = C.c_int
    lib.bk_material_pose_material.argtypes = [p, C.c_uint32]
    lib.bk_material_pose_material.restype = C.POINTER(Material)
