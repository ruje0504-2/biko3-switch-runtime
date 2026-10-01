#include "scene/control_help.h"
#include "ui/control_help_font.h"
#include <stdlib.h>
#include <string.h>

/* These describe the Switch adapters, including menus without a B shortcut.
 * Touch positions the original cursor; only the volume page synthesizes clicks. */
static const char *const pages[BK_HELP_COUNT] = {
  [BK_HELP_TITLE] = "左スティック\nカーソル移動\nZL ゆっくり移動\n十字キー 選択\nA 決定\n\nタッチで移動\nA で決定",
  [BK_HELP_SELECTION] = "左スティック\nカーソル移動\nZL ゆっくり移動\n十字キー 選択\nA 決定\n\nタッチで移動\nA で決定\n\n戻る項目で終了",
  [BK_HELP_DIALOGUE] = "A 次へ",
  [BK_HELP_GAME] = "左スティック\n十字キー：移動\n右スティック\n視点を動かす\n\nA 調べる・操作\nB 姿勢切替\nX カメラ切替\nY 撮影\nZL ゆっくり歩く\n+ 一時停止",
  [BK_HELP_PAUSE] = "十字キー 選択\nA 決定\n\nタッチで移動\nA で決定\n\n再開の項目を\n選んで戻る",
  [BK_HELP_CHOICE] = "十字キー 選択\nA 決定\n\nタッチで移動\nA で決定",
  [BK_HELP_SAVE] = "左スティック\nカーソル移動\nZL ゆっくり移動\n十字キー 選択\nA スロット決定\nA 確認\n\nタッチで移動\nA で決定\n\n戻る項目で取消",
  [BK_HELP_GALLERY] = "左スティック\n十字キー\nカーソル移動\nA 選択・決定\nB 戻る\nZL ゆっくり移動\n\nタッチで移動\nA で決定",
  [BK_HELP_VOLUME] = "十字キー上下\n音量・項目選択\n十字キー左右\n音量を微調整\nA 決定\n\n左スティック\nカーソル移動\nA を押しながら\nスライダー操作\nZL ゆっくり移動\n\nタッチで選択\nスライダーは\n指で直接動かす\n\n調整後に保存",
  [BK_HELP_ENDING] = "左スティック\n十字キー\nカーソル移動\nA 決定・操作\nB 取消・戻る\nZL ゆっくり移動\n\nL + 右スティック\n回転\nR + 右スティック\n距離・高さ\n\nタッチで移動\nA で決定\nA を押しながら\n指で対象を動かす\n\n右端でメニュー",
  [BK_HELP_SPECIAL] = "左スティック\n十字キー\nカーソル移動\nA 決定・スキップ\nY 撮影\nZL ゆっくり移動\n\nL + 右スティック\n回転\nR + 右スティック\n距離・高さ\n\nタッチで移動\nA で決定\n\n右端でメニュー\nメニューから終了",
  [BK_HELP_LOADING] = "確認表示が出たら\nA で進む",
  [BK_HELP_FAILURE] = "確認表示が出たら\nA で進む",
  [BK_HELP_INSPECTION] = "右スティック\n視点を動かす\nB タイトルへ\nX 軌道切替\nY キャラ切替\n+ 終了"
};

struct BkControlHelp {
  BkRenderer *renderer;
  BkTexture *font;
  BkGpuMesh *pages[BK_HELP_COUNT];
  BkViewport bar;
};
static unsigned codepoint(const char **text) {
  const unsigned char *p = (const unsigned char *)*text;
  unsigned value = *p++;
  if (value >= 0xe0) {
    value = (value & 15) << 12;
    value |= (*p++ & 63) << 6;
    value |= *p++ & 63;
  } else if (value >= 0xc0) {
    value = (value & 31) << 6;
    value |= *p++ & 63;
  }
  *text = (const char *)p;
  return value;
}
static int glyph(unsigned cp) {
  for (unsigned i = 0; i < BK_HELP_GLYPHS; ++i)
    if (bk_help_glyphs[i].codepoint == cp) return (int)i;
  return -1;
}
static float line_width(const char *text) {
  float width = 0;
  while (*text && *text != '\n') {
    int g = glyph(codepoint(&text));
    if (g < 0) return -1;
    width += bk_help_glyphs[g].advance;
  }
  return width;
}
static BkGpuMesh *page_mesh(BkControlHelp *s, const char *text, char e[256]) {
  size_t capacity = strlen(text);
  BkVertex *vertices = calloc(capacity * 4, sizeof(*vertices));
  uint16_t *indices = calloc(capacity * 6, sizeof(*indices));
  BkGpuMesh *mesh = NULL;
  if (!vertices || !indices) { snprintf(e, 256, "control help allocation failed"); goto done; }
  unsigned lines = 1;
  for (const char *p = text; *p; ++p) lines += *p == '\n';
  float x = (160 - line_width(text)) * .5f;
  float y = (720 - ((lines - 1) * 26 + BK_HELP_CELL)) * .5f;
  unsigned count = 0, line = 0;
  for (const char *p = text; *p;) {
    unsigned cp = codepoint(&p);
    if (cp == '\n') { x = (160 - line_width(p)) * .5f; y += 26; ++line; continue; }
    int g = glyph(cp);
    if (g < 0 || x + bk_help_glyphs[g].advance > 148 || y + BK_HELP_CELL > 710) {
      snprintf(e, 256, "control help: missing glyph or text overflow at line%u", line); goto done;
    }
    if (cp != ' ') {
      float x0 = 2*x/160-1, x1 = 2*(x+BK_HELP_CELL)/160-1;
      float y0 = 2*y/720-1, y1 = 2*(y+BK_HELP_CELL)/720-1;
      float u0 = (float)(g % BK_HELP_COLUMNS) / BK_HELP_COLUMNS;
      float v0 = (float)(g / BK_HELP_COLUMNS) / BK_HELP_ROWS;
      float u1 = u0 + 1.f/BK_HELP_COLUMNS, v1 = v0 + 1.f/BK_HELP_ROWS;
      const float red = 226.f/255, green = 191.f/255, blue = 114.f/255;
      BkVertex q[4] = {{x0,y0,0,u0,v0,red,green,blue,1}, {x1,y0,0,u1,v0,red,green,blue,1},
                       {x1,y1,0,u1,v1,red,green,blue,1}, {x0,y1,0,u0,v1,red,green,blue,1}};
      memcpy(vertices+count*4, q, sizeof q);
      const unsigned order[] = {0,1,2,0,2,3};
      for (unsigned i=0;i<6;++i) indices[count*6+i]=(uint16_t)(count*4+order[i]);
      ++count;
    }
    x += bk_help_glyphs[g].advance;
  }
  mesh = bk_mesh_create(s->renderer, vertices, count*4, indices, count*6, e);
done:
  free(vertices); free(indices); return mesh;
}
BkControlHelp *bk_control_help_create(BkRenderer *r, char e[256]) {
  BkControlHelp *s = calloc(1, sizeof(*s));
  if (!s) { snprintf(e,256,"control help allocation failed"); return NULL; }
  s->renderer = r;
  unsigned width, height; BkViewport view;
  bk_renderer_extent(r, &width, &height);
  if (!bk_camera_fit(&view, width, height, 4, 3)) goto bad;
  s->bar = (BkViewport){0, view.y, view.x, view.height};
  if (!view.x) return s; /* A 4:3 display has no side space to use. */
  BkImage image = {BK_HELP_COLUMNS*BK_HELP_CELL, BK_HELP_ROWS*BK_HELP_CELL, NULL};
  image.rgba = calloc((size_t)image.width*image.height, 4);
  if (!image.rgba) { snprintf(e,256,"control help atlas allocation failed"); goto bad; }
  for (unsigned g=0;g<BK_HELP_GLYPHS;++g)
    for (unsigned p=0;p<BK_HELP_CELL*BK_HELP_CELL;++p) {
      unsigned x=(g%BK_HELP_COLUMNS)*BK_HELP_CELL+p%BK_HELP_CELL;
      unsigned y=(g/BK_HELP_COLUMNS)*BK_HELP_CELL+p/BK_HELP_CELL;
      uint8_t *pixel=image.rgba+((size_t)y*image.width+x)*4;
      pixel[0]=pixel[1]=pixel[2]=255;
      pixel[3]=((bk_help_glyphs[g].mask[p/2] >> (p%2 ? 0 : 4)) & 15)*17;
    }
  s->font = bk_texture_create(r, &image, e);
  bk_image_free(&image);
  if (!s->font) goto bad;
  for (unsigned i=1;i<BK_HELP_COUNT;++i)
    if (!(s->pages[i]=page_mesh(s,pages[i],e))) goto bad;
  return s;
bad:
  bk_control_help_destroy(s); return NULL;
}
int bk_control_help_draw(BkControlHelp *s, BkControlHelpPage page, char e[256]) {
  if (!s || page<0 || page>=BK_HELP_COUNT) { snprintf(e,256,"invalid control help page"); return 0; }
  if (!s->bar.width || page==BK_HELP_NONE) return 1;
  if (!bk_renderer_viewport(s->renderer,&s->bar,e) ||
      !bk_renderer_draw_mesh(s->renderer,s->font,s->pages[page],bk_identity,
                             (BkDrawState){BK_BLEND_UI_ALPHA,0,BK_CULL_NONE},e)) return 0;
  return bk_renderer_viewport(s->renderer,NULL,e);
}
void bk_control_help_destroy(BkControlHelp *s) {
  if (!s) return;
  for (unsigned i=0;i<BK_HELP_COUNT;++i) bk_mesh_destroy(s->renderer,s->pages[i]);
  bk_texture_destroy(s->renderer,s->font);
  free(s);
}
