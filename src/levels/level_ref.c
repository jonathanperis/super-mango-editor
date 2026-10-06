/*
 * level_ref.c — Validate levels/<name>.toml references.
 *
 * The rule is intentionally stricter than "a path that exists here": level
 * files travel between macOS, Linux and Windows, so a name that works on one
 * system but opens a device or fails on another is rejected everywhere.
 */

#include "level_ref.h"

#include <string.h>  /* memcmp, strchr */

/* ASCII-only case folding; locale-aware tolower() would vary by machine. */
static unsigned char ascii_upper(unsigned char ch)
{
    return (ch >= 'a' && ch <= 'z') ? (unsigned char)(ch - 'a' + 'A') : ch;
}

static int ascii_equal_nocase(const char *a, const char *upper, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (ascii_upper((unsigned char)a[i]) != (unsigned char)upper[i])
            return 0;
    }
    return 1;
}

/*
 * Windows maps these names to devices in every directory, with or without
 * an extension: "levels/con.toml" and "levels/NUL.x.toml" both open a device
 * instead of a file.  Only the stem before the first dot matters, and Windows
 * ignores trailing spaces in it.  COM and LPT also accept 0-9 and the
 * superscript digits ¹ ² ³ (UTF-8 C2 B9, C2 B2, C2 B3).
 */
static int is_windows_device_stem(const char *stem, size_t len)
{
    static const char *const plain[] = { "CON", "PRN", "AUX", "NUL" };
    static const char *const numbered[] = { "COM", "LPT" };

    while (len > 0 && stem[len - 1] == ' ') len--;

    if (len == 3) {
        for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); i++) {
            if (ascii_equal_nocase(stem, plain[i], 3)) return 1;
        }
        return 0;
    }
    if (len != 4 && len != 5) return 0;
    for (size_t i = 0; i < sizeof(numbered) / sizeof(numbered[0]); i++) {
        if (!ascii_equal_nocase(stem, numbered[i], 3)) continue;
        if (len == 4 && stem[3] >= '0' && stem[3] <= '9') return 1;
        if (len == 5 && (unsigned char)stem[3] == 0xC2 &&
            ((unsigned char)stem[4] == 0xB9 || (unsigned char)stem[4] == 0xB2 ||
             (unsigned char)stem[4] == 0xB3))
            return 1;
    }
    return 0;
}

int level_ref_valid(const char *ref, size_t length)
{
    static const char prefix[] = "levels/";
    static const char suffix[] = ".toml";
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t suffix_len = sizeof(suffix) - 1;
    const char *name;
    size_t name_len;
    size_t stem_len = 0;

    if (!ref || length <= prefix_len + suffix_len) return 0;
    if (memcmp(ref, prefix, prefix_len) != 0 ||
        memcmp(ref + length - suffix_len, suffix, suffix_len) != 0)
        return 0;

    name = ref + prefix_len;
    name_len = length - prefix_len;
    for (size_t i = 0; i < name_len; i++) {
        unsigned char ch = (unsigned char)name[i];
        /* '/' would mean a subdirectory; the rest are invalid on Windows. */
        if (ch < 0x20 || ch == 0x7F || strchr("/\\<>:\"|?*", ch) != NULL)
            return 0;
    }

    /* The stem ends at the first dot: "nul.x.toml" has stem "nul". */
    while (stem_len < name_len && name[stem_len] != '.') stem_len++;
    if (stem_len == 0) return 0;
    return !is_windows_device_stem(name, stem_len);
}
