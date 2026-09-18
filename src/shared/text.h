#pragma once
#include "graphics.h"

typedef struct {
    Font font;
    unsigned char *bytes;
    int byte_count, size;
    int *codepoints;
    int count, atlas_dirty;
} TextFont;

TextFont *font_load(const char *path, int size);
void font_unload(TextFont *font);
int font_measure(TextFont *font, const char *text, int *width, int *height);
Texture2D *font_texture(TextFont *font, const char *text, Color color);
void font_draw(TextFont *font, const char *text, int x, int y, Color color);
