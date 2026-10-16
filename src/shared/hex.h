/*
 * hex.h — Read the lowercase hexadecimal the project's own files write.
 *
 * Ghost recordings and resume points store 64-bit hashes, masks and packed
 * samples as fixed-width lowercase hex (TOML integers are signed, so the top
 * bit of a 64-bit value needs a string). Uppercase, signs and spaces are
 * refused: the writer never produces them.
 */
#pragma once

#include <stdint.h>

/* Value of one lowercase hex digit, or -1. */
static inline int hex_digit_lower(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Read exactly `digits` hex digits from text into *out; -1 on a bad digit
 * (then *out is untouched). */
static inline int hex_parse_lower(const char *text, int digits, uint64_t *out)
{
    uint64_t value = 0;
    for (int i = 0; i < digits; i++) {
        int digit = hex_digit_lower(text[i]);
        if (digit < 0) return -1;
        value = (value << 4) | (uint64_t)digit;
    }
    *out = value;
    return 0;
}
