/* UTF-8 font owner. A font atlas is a texture containing many glyph images;
 * keeping the original TTF bytes lets us rebuild it for newly requested text. */
#pragma once
#include "graphics.h"

typedef struct {
    Font font;                 /* owns the current GPU atlas and glyph metadata */
    unsigned char *bytes;      /* owns original TTF data for later glyph loads */
    int byte_count, size;
    int *codepoints;            /* owns the list of requested Unicode codepoints */
    int count, atlas_dirty;
} TextFont;

/* Requires a live graphics context; size is glyph height in logical pixels.
 * Returns an owned font or NULL. Release it before closing the context. */
TextFont *font_load(const char *path, int size);
void font_unload(TextFont *font);
/* Returns 0 on success, -1 on failure; either output pointer may be NULL. */
int font_measure(TextFont *font, const char *text, int *width, int *height);
/* Rasterize a label into a separate owned texture. Cache/reuse it as needed;
 * release with texture_unload. Missing font glyphs use raylib's fallback. */
Texture2D *font_texture(TextFont *font, const char *text, Color color);
/* Draw directly into the current render target; no separate label texture. */
void font_draw(TextFont *font, const char *text, int x, int y, Color color);
