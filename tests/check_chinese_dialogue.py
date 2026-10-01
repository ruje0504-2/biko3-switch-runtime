"""Real translated dialogue: 1.3x glyphs, visible rows, and the A-key branch.

Checks opening dialogue, in-world introductions, item and failure notices;
does not play story routes or change game data. BK3_BUILD_DIR selects the C
library so the same checks run under the existing ASan Python launcher.
"""
import argparse
import ctypes as C
import hashlib
import json
import re
from pathlib import Path

from model_binding import library
from original_dialogue_ui_oracle import (
    Dialogue, Message, Ui, Result, Bindings, Input, Ops, Frame,
    Op, Next, Text, Unlock, Schedule, Flow, Fade, Backdrop,
)
from original_text_draw_oracle import Style, Draw
from original_text_image_oracle import Image, Layout


class Blob(C.Structure):
    _fields_ = [('data', C.c_void_p), ('size', C.c_size_t)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('patch', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    lib = library()
    error = C.create_string_buffer(256)
    for name, result, params in [
        ('bk_resources_create', C.c_void_p, [C.c_void_p]),
        ('bk_resources_destroy', None, [C.c_void_p]),
        ('bk_resources_mount', C.c_int, [C.c_void_p, C.c_char_p, C.c_char_p, C.c_void_p]),
        ('bk_resources_mount_directory', C.c_int, [C.c_void_p, C.c_char_p, C.c_char_p, C.c_size_t, C.c_void_p]),
        ('bk_resources_load_patch', C.c_int, [C.c_void_p, C.c_char_p, C.c_void_p]),
        ('bk_resources_read', C.c_int, [C.c_void_p, C.c_char_p, C.c_char_p, C.POINTER(Blob), C.c_void_p]),
        ('bk_blob_free', None, [C.POINTER(Blob)]),
        ('bk_font_decode', C.c_void_p, [C.c_void_p, C.c_size_t, C.c_void_p]),
        ('bk_font_destroy', None, [C.c_void_p]),
        ('bk_text_canvas_create_scaled', C.c_void_p, [C.c_void_p, C.c_uint32, C.c_uint32, C.c_float, C.c_void_p]),
        ('bk_text_canvas_destroy', None, [C.c_void_p]),
        ('bk_text_canvas_image', C.POINTER(Image), [C.c_void_p]),
        ('bk_text_canvas_prepare', C.c_int, [C.c_void_p, C.POINTER(Style), C.c_void_p, C.c_size_t, C.c_float, C.c_uint32, C.POINTER(Flow), C.POINTER(Draw), C.POINTER(C.c_int), C.c_void_p]),
        ('bk_text_image', C.c_int, [C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(Layout), C.c_uint32, C.c_uint32, C.POINTER(Image), C.c_void_p]),
        ('bk_image_free', None, [C.POINTER(Image)]),
        ('bk_dialogue_open', C.c_int, [C.POINTER(Dialogue), C.c_void_p, C.c_size_t, C.c_char_p, C.c_void_p]),
        ('bk_dialogue_next', C.c_int, [C.POINTER(Dialogue), C.c_void_p, C.c_size_t, C.POINTER(C.c_int), C.c_void_p]),
        ('bk_message_lookup', C.c_int, [C.c_void_p, C.c_size_t, C.c_int32, C.POINTER(Message), C.c_void_p]),
        ('bk_dialogue_text_target', C.c_int, [C.POINTER(Message), C.c_uint, C.c_uint, C.c_uint, C.c_int, C.POINTER(C.c_float), C.c_void_p]),
        ('bk_dialogue_ui_initialize', None, [C.POINTER(Ui)]),
        ('bk_dialogue_ui_step', C.c_int, [C.POINTER(Ui), C.POINTER(Bindings), C.POINTER(Input), C.POINTER(Ops), C.POINTER(Frame), C.c_void_p]),
    ]:
        function = getattr(lib, name)
        function.restype, function.argtypes = result, params
    store = lib.bk_resources_create(error)
    assert store, error.value
    assert lib.bk_resources_mount(store, b'bk3_05', str(args.data/'bk3_05.pp').encode(), error), error.value
    assert lib.bk_resources_mount_directory(store, b'fonts', str(args.data).encode(), 8 << 20, error), error.value
    assert lib.bk_resources_load_patch(store, str(args.patch).encode(), error), error.value

    def read(pack, name):
        blob = Blob()
        assert lib.bk_resources_read(store, pack.encode(), name.encode(), C.byref(blob), error) == 1, error.value
        data = C.string_at(blob.data, blob.size)
        lib.bk_blob_free(C.byref(blob))
        return data

    rawfont = read('fonts', 'Type_S.FTT')
    font = lib.bk_font_decode(rawfont, len(rawfont), error)
    assert font, error.value
    def make_style(kind):
        x, y, width, height, step_y = {'dialogue': (104, 380, 432, 64, 16),
                                     'opening': (104, 380, 432, 72, 18),
                                     'failure': (104, 380, 432, 72, 18),
                                     'items': (192, 400, 316, 268, 16)}[kind]
        return Style(x, y, width, height, 16, step_y, 1, 1, (C.c_float*3)(1, 1, 1), 1)
    scripts = {f'i{group:02}_00.txt': read('bk3_05', f'i{group:02}_00.txt') for group in range(1, 6)}
    cases = []
    longest = None
    for filename, raw in scripts.items():
        ids = [int(x) for x in re.findall(rb'(?m)^#([0-9]{5})', raw)]
        state = Dialogue(first_label=ids[0], last_label=ids[-1], current_label=-1)
        assert lib.bk_dialogue_open(C.byref(state), raw, len(raw), filename.encode(), error), error.value
        for index in range(len(ids) + 3):
            done = C.c_int()
            assert lib.bk_dialogue_next(C.byref(state), raw, len(raw), C.byref(done), error), error.value
            if done.value:
                break
            case = (filename, state.current_label, Message.from_buffer_copy(state.text))
            if index < 5:
                cases.append(case)
            if longest is None or state.text.length > longest[2].length:
                longest = case
    if longest[:2] not in [case[:2] for case in cases]:
        cases.append(longest)
    cases = [('dialogue', *case) for case in cases]
    for kind in ['items', 'opening', 'failure']:
        for group in range(5):
            filename = 'i00_00.txt' if kind == 'items' else f'i{group+1:02}_{1 if kind == "opening" else 4:02}.txt'
            raw = read('bk3_05', filename)
            if kind == 'items':
                labels = range(group*10000, group*10000+5)
            elif kind == 'opening':
                labels = range((group+1)*10000, [10005, 20006, 30007, 40011, 50007][group]+1)
            else:
                labels = [(group+1)*10000+x for x in [0, 100, 200, 300, 400, 401]]
            for label in labels:
                message = Message()
                assert lib.bk_message_lookup(raw, len(raw), label, C.byref(message), error), error.value
                cases.append((kind, filename, label, message))

    digest = hashlib.sha256()
    checks = []
    for kind, filename, label, message in cases:
        style = make_style(kind)
        raster_width, raster_height = {'dialogue': (664, 98), 'opening': (664, 110),
                                       'failure': (664, 110), 'items': (486, 412)}[kind]
        step_y = style.step_y*2
        visible_height = raster_height//step_y*step_y
        canvas = lib.bk_text_canvas_create_scaled(font, style.width, style.height, 1.3, error)
        assert canvas, error.value
        data = bytes(message.bytes[:message.length])
        full = Image()
        layout = Layout(0, 0, raster_width, 32, step_y)
        assert lib.bk_text_image(font, data, len(data), C.byref(layout), 1280, 960, C.byref(full), error), error.value
        mask = C.string_at(full.rgba, 1280*960*4)[3::4]
        bottom = mask.rfind(b'\xff') // 1280 + 1
        wanted = max(0, (bottom - visible_height + step_y - 1) // step_y) * step_y
        if kind != 'dialogue':
            assert wanted == 0, (kind, filename, label, 'notice must fit before advancing or expiring')
        flow = Flow(2, 0, 999, 1, 0)
        draw, upload = Draw(), C.c_int()
        for source in [0, wanted]:
            flow.scroll, flow.started, flow.enabled = source, 0, 1
            assert lib.bk_text_canvas_prepare(canvas, C.byref(style), data, len(data), 0, 960, C.byref(flow), C.byref(draw), C.byref(upload), error), error.value
            assert flow.target == wanted and flow.scroll == source, (filename, label, flow.target, wanted)
            image = lib.bk_text_canvas_image(canvas).contents
            assert (image.width, image.height) == (raster_width, raster_height)
            rgba = C.string_at(image.rgba, image.width * image.height * 4)
            crop = b''.join(mask[(source+y)*1280:(source+y)*1280+raster_width] for y in range(visible_height)) + bytes(raster_width*(raster_height-visible_height))
            assert rgba[::4] == crop and rgba[1::4] == crop and rgba[2::4] == crop
            assert rgba[3::4] == b'\xff' * len(crop)
            for p in draw.passes[:draw.count]:
                assert abs(p.width / raster_width / .75 - 1.3) < 1e-6
                assert abs(p.height / raster_height / .75 - 1.3) < 1e-6
                last_ink = crop.rfind(b'\xff')//raster_width + 1
                assert p.x + p.width < 864 and p.y + last_ink*p.height/raster_height < 702
            digest.update(rgba)
        checks.append({'kind': kind, 'file': filename, 'label': label, 'target': wanted, 'ink_height': bottom})
        lib.bk_image_free(C.byref(full))
        lib.bk_text_canvas_destroy(canvas)

    # Reproduce the extra A press at actual opening label10001. The same
    # recovered UI controller selects scroll vs next; only its text service
    # differs, as in the production renderer.
    advances = []
    style = make_style('dialogue')
    for zoom in [1, 1.3]:
        raw = scripts['i01_00.txt']
        dialogue = Dialogue(first_label=10001, last_label=10002, current_label=-1)
        done = C.c_int()
        assert lib.bk_dialogue_open(C.byref(dialogue), raw, len(raw), b'i01_00.txt', error)
        assert lib.bk_dialogue_next(C.byref(dialogue), raw, len(raw), C.byref(done), error)
        dialogue.image_kind = 0
        flow, ui, backdrop, curtain = Flow(2, 0, 0, 1, 0), Ui(), Backdrop(), Fade()
        phase, wanted, result = C.c_uint8(), C.c_uint8(), Result()
        target = C.c_float()
        assert lib.bk_dialogue_text_target(C.byref(dialogue.text), 27, 4, 16, 0, C.byref(target), error)
        flow.target = target.value
        canvas = lib.bk_text_canvas_create_scaled(font, 432, 64, zoom, error)
        assert canvas
        calls = [0]
        @Next
        def next_page(_, done, err):
            calls[0] += 1
            return lib.bk_dialogue_next(C.byref(dialogue), raw, len(raw), done, err)
        @Text
        def prepare(_, seconds, err):
            message = dialogue.text
            draw, upload = Draw(), C.c_int()
            return lib.bk_text_canvas_prepare(canvas, C.byref(style), bytes(message.bytes[:message.length]), message.length, seconds, 960, C.byref(flow), C.byref(draw), C.byref(upload), err)
        ok, unlock, schedule = Op(lambda *_: 1), Unlock(lambda *_: 1), Schedule(lambda *_: 1)
        ops = Ops(None, ok, next_page, ok, prepare, ok, ok, unlock, schedule)
        bindings = Bindings(C.pointer(dialogue), C.pointer(flow), C.pointer(backdrop), C.pointer(phase), C.pointer(curtain), C.pointer(wanted), C.pointer(result))
        lib.bk_dialogue_ui_initialize(C.byref(ui))
        assert prepare(None, 1/60, error), error.value
        assert lib.bk_dialogue_ui_step(C.byref(ui), C.byref(bindings), C.byref(Input(1/60, 1, 0, 0x38, 0, 0, 0)), C.byref(ops), C.byref(Frame()), error), error.value
        advances.append(calls[0])
        lib.bk_text_canvas_destroy(canvas)
    assert advances == [0, 1], advances
    lib.bk_font_destroy(font)
    lib.bk_resources_destroy(store)
    report = {'passed': True, 'zoom': 1.3, 'messages': checks, 'crop_checks': len(checks)*2,
              'rgba_sha256': digest.hexdigest(), 'one_A_next_calls_before_after': advances,
              'scope': 'Actual Chinese dialogue, world introductions, item/failure notices and font; measured wrap/last ink, first/tail crop, 1.3x geometry, visible text inside panels and original UI A branch. Menus/hints unchanged; no story-route or hardware acceptance.'}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(f'PASS Chinese dialogue: {len(checks)} messages, {len(checks)*2} crops, 1.3x glyph scale; one A advances {advances[0]} -> {advances[1]} pages; {digest.hexdigest()}')


if __name__ == '__main__':
    main()
