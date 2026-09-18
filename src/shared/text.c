#include "text.h"
#include <limits.h>
#include <string.h>

/* Collect requested Unicode glyphs instead of restricting editor fields to
 * the font loader's default atlas. Existing label textures remain independent
 * of an atlas replacement. Unsupported font glyphs retain raylib's fallback. */
static int font_prepare(TextFont *font, const char *text)
{
    if (!font || !text) return -1;
    for (const char *cursor = text; *cursor;) {
        int bytes = 0;
        int codepoint = GetCodepointNext(cursor, &bytes);
        if (bytes <= 0) return -1;
        cursor += bytes;
        if (codepoint < 32) continue;
        int found = 0;
        for (int i = 0; i < font->count; i++)
            if (font->codepoints[i] == codepoint) { found = 1; break; }
        if (found) continue;
        /* Keep requested codepoints, including unavailable glyphs, so the
         * same missing character does not trigger an atlas rebuild each frame. */
        if (font->count == INT_MAX / (int)sizeof(int) - 1) return -1;
        int *points = realloc(font->codepoints, (size_t)(font->count + 1) * sizeof(int));
        if (!points) return -1;
        font->codepoints = points;
        points[font->count++] = codepoint;
        font->atlas_dirty = 1;
    }
    if (font->atlas_dirty) {
        Font replacement = LoadFontFromMemory(".ttf", font->bytes, font->byte_count,
                                              font->size, font->codepoints, font->count);
        if (!IsFontValid(replacement) || !IsTextureValid(replacement.texture) ||
            replacement.texture.id == GetFontDefault().texture.id) {
            UnloadFont(replacement); /* Also releases partially loaded glyph data. */
            return -1;
        }
        /* raylib batches drawing. Flush commands that reference the old atlas
         * before releasing it, even if this replacement happens mid-frame. */
        rlDrawRenderBatchActive();
        UnloadFont(font->font);
        font->font = replacement;
        font->atlas_dirty = 0;
        SetTextureFilter(font->font.texture, TEXTURE_FILTER_POINT);
    }
    return 0;
}

TextFont *font_load(const char *path, int size)
{
    TextFont *font = calloc(1, sizeof(*font));
    if (!font) return NULL;
    font->size = size;
    font->bytes = LoadFileData(path, &font->byte_count);
    if (!font->bytes) {
        free(font);
        return NULL;
    }
    font->count = 95;
    font->codepoints = malloc((size_t)font->count * sizeof(int));
    if (!font->codepoints) {
        font_unload(font);
        return NULL;
    }
    for (int i = 0; i < font->count; i++) font->codepoints[i] = i + 32;
    font->font = LoadFontFromMemory(".ttf", font->bytes, font->byte_count,
                                   size, font->codepoints, font->count);
    if (!IsFontValid(font->font) || !IsTextureValid(font->font.texture) ||
        font->font.texture.id == GetFontDefault().texture.id) {
        font_unload(font);
        return NULL;
    }
    SetTextureFilter(font->font.texture, TEXTURE_FILTER_POINT);
    return font;
}

void font_unload(TextFont *font)
{
    if (!font) return;
    if (font->font.texture.id != GetFontDefault().texture.id) {
        rlDrawRenderBatchActive();
        UnloadFont(font->font);
    }
    UnloadFileData(font->bytes);
    free(font->codepoints);
    free(font);
}

int font_measure(TextFont *font, const char *text, int *width, int *height)
{
    if (font_prepare(font, text)) return -1;
    Vector2 size = MeasureTextEx(font->font, text, (float)font->size, 0);
    if (width) *width = (int)size.x;
    if (height) *height = (int)size.y;
    return 0;
}

Texture2D *font_texture(TextFont *font, const char *text, Color color)
{
    if (!text || !text[0] || font_prepare(font, text)) return NULL;
    /* Image owns CPU pixels; Texture2D owns their GPU upload. Once uploaded,
     * release the Image. This label texture survives later atlas replacements. */
    Image image = ImageTextEx(font->font, text, (float)font->size, 0, color);
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
    if (font_prepare(font, text)) return;
    DrawTextEx(font->font, text, (Vector2){(float)x, (float)y}, (float)font->size, 0, color);
}
