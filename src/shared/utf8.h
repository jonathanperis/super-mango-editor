/*
 * utf8.h — Strict UTF-8 checks shared by the level loader and text fields.
 *
 * TOML files must be valid UTF-8.  tomlc17 does not check raw string bytes,
 * but Python's tomllib (tools/validate_levels.py) and other strict readers
 * reject the whole file.  The loader therefore refuses such strings, and the
 * editor's text fields never produce them, so every saved level stays
 * readable everywhere.
 *
 * Header-only (static inline) so any file can use it without new link
 * dependencies.
 */
#pragma once

#include <stddef.h> /* size_t */

/*
 * utf8_sequence_length — Byte length of the valid character at text, or 0.
 *
 * `available` bounds how far the check may read.  A UTF-8 character is one
 * lead byte plus 0–3 continuation bytes (10xxxxxx).  Returns 0 for:
 *   - a stray continuation byte where a lead byte belongs,
 *   - a sequence cut short (too few bytes, or a non-continuation byte),
 *   - an overlong form (a longer encoding of a value that fits a shorter
 *     one, such as C0 AF for '/'; decoders that accept them let one string
 *     hide behind several spellings),
 *   - a UTF-16 surrogate, U+D800..U+DFFF, which is not a character,
 *   - a value above U+10FFFF, the last Unicode code point.
 */
static inline size_t utf8_sequence_length(const char *text, size_t available)
{
    const unsigned char *bytes = (const unsigned char *)text;
    unsigned long codepoint;
    unsigned long smallest;   /* below this, a shorter encoding was possible */
    size_t length;

    if (!text || available == 0) return 0;
    if (bytes[0] < 0x80) return 1;                     /* plain ASCII */
    if ((bytes[0] & 0xE0) == 0xC0) {
        length = 2; codepoint = bytes[0] & 0x1Fu; smallest = 0x80;
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        length = 3; codepoint = bytes[0] & 0x0Fu; smallest = 0x800;
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        length = 4; codepoint = bytes[0] & 0x07u; smallest = 0x10000;
    } else {
        return 0;   /* continuation byte (10xxxxxx) or an invalid F8..FF lead */
    }
    if (available < length) return 0;
    for (size_t i = 1; i < length; i++) {
        if ((bytes[i] & 0xC0) != 0x80) return 0;
        codepoint = (codepoint << 6) | (bytes[i] & 0x3Fu);
    }
    if (codepoint < smallest || codepoint > 0x10FFFFul ||
        (codepoint >= 0xD800ul && codepoint <= 0xDFFFul)) return 0;
    return length;
}

/* Non-zero when all `length` bytes of text form valid UTF-8. */
static inline int utf8_valid(const char *text, size_t length)
{
    size_t i = 0;

    while (i < length) {
        size_t step = utf8_sequence_length(text + i, length - i);
        if (step == 0) return 0;
        i += step;
    }
    return 1;
}
