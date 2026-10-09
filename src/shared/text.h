/* Text drawing with raylib's built-in font. The default font is a bitmap font
 * raylib creates inside InitWindow and releases inside CloseWindow; it ships
 * with raylib under its zlib license, so the project bundles no font file. */
#pragma once
#include "graphics.h"

/* The built-in font's glyphs are drawn on a 10 px grid. Drawing at exactly
 * this size maps every font pixel to one canvas pixel, so text stays crisp. */
#define TEXT_FONT_SIZE 10

typedef struct {
    Font font;      /* borrowed GetFontDefault(); never passed to UnloadFont */
    int size;       /* glyph height in logical pixels */
    int spacing;    /* extra pixels between glyphs, as raylib's DrawText uses */
} TextFont;

/* Requires a live graphics context. Returns an owned handle or NULL. Release
 * it before closing the context; that frees only the handle, not the font. */
TextFont *font_load(void);
void font_unload(TextFont *font);
/* Returns 0 on success, -1 on failure; either output pointer may be NULL. */
int font_measure(TextFont *font, const char *text, int *width, int *height);
/* Rasterize a label into a separate owned texture. Cache/reuse it as needed;
 * release with texture_unload. Characters outside the font's 224 glyphs
 * (ASCII plus Latin-1) draw as raylib's '?' fallback. */
Texture2D *font_texture(TextFont *font, const char *text, Color color);
/* Draw directly into the current render target; no separate label texture. */
void font_draw(TextFont *font, const char *text, int x, int y, Color color);
/* Draw text whose middle lands on center_x (y is still the top edge). Every
 * centred label in the game (menu, overlays, debug title) uses this one. */
void font_draw_centered(TextFont *font, const char *text, int center_x, int y, Color color);
