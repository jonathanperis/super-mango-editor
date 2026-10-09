#include "text.h"
#include <stdlib.h>

TextFont *font_load(void)
{
    /* GetFontDefault returns a copy of raylib's internal Font value. Copying
     * a Font struct does not transfer ownership: raylib keeps the atlas
     * texture and releases it in CloseWindow, so this module never unloads
     * it. Before InitWindow the atlas does not exist yet. */
    Font font = GetFontDefault();
    if (!IsFontValid(font) || !IsTextureValid(font.texture)) return NULL;
    TextFont *text_font = malloc(sizeof(*text_font));
    if (!text_font) return NULL;
    /* Point filtering copies the nearest texel instead of blending
     * neighbours, so the bitmap glyphs never blur when the canvas scales. */
    SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    /* raylib's DrawText spaces default-font glyphs by size / baseSize
     * pixels; the glyph images have no padding of their own. */
    *text_font = (TextFont){font, TEXT_FONT_SIZE, TEXT_FONT_SIZE / font.baseSize};
    return text_font;
}

void font_unload(TextFont *font)
{
    /* Only the handle is ours; the borrowed default font stays alive. */
    free(font);
}

int font_measure(TextFont *font, const char *text, int *width, int *height)
{
    if (!font || !text) return -1;
    Vector2 size = MeasureTextEx(font->font, text, (float)font->size,
                                 (float)font->spacing);
    if (width) *width = (int)size.x;
    if (height) *height = (int)size.y;
    return 0;
}

Texture2D *font_texture(TextFont *font, const char *text, Color color)
{
    if (!font || !text || !text[0]) return NULL;
    /* Image owns CPU pixels; Texture2D owns their GPU upload. Once uploaded,
     * release the Image. */
    Image image = ImageTextEx(font->font, text, (float)font->size,
                              (float)font->spacing, color);
    if (!IsImageValid(image)) return NULL;
    Texture2D value = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(value)) return NULL;
    Texture2D *texture = malloc(sizeof(*texture));
    if (!texture) {
        UnloadTexture(value);
        return NULL;
    }
    *texture = value;
    SetTextureFilter(value, TEXTURE_FILTER_POINT);
    return texture;
}

void font_draw(TextFont *font, const char *text, int x, int y, Color color)
{
    if (!font || !text) return;
    DrawTextEx(font->font, text, (Vector2){(float)x, (float)y}, (float)font->size,
               (float)font->spacing, color);
}

void font_draw_centered(TextFont *font, const char *text, int center_x, int y, Color color)
{
    /* Measure this exact label: glyphs and numbers differ in width, so a
     * fixed offset would drift. Half the width left of the centre is the
     * left edge; (2*centre - width)/2 rounds the same way on every label. */
    int width = 0;
    if (font_measure(font, text, &width, NULL)) return;
    font_draw(font, text, (2 * center_x - width) / 2, y, color);
}
