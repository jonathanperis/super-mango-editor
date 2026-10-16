/*
 * test_folders.h — Make a scratch folder, and step into it and back out,
 * on every desktop system.
 *
 * The campaign tests build a small game folder (levels/, levels/campaigns/)
 * under TEST_OUT and run inside it, because the editor names campaign levels
 * relative to the working folder, as the game does. POSIX spells the calls
 * mkdir, getcwd and chdir; Windows spells them _mkdir, _wgetcwd and _wchdir,
 * and the last two take UTF-16, converted with the serializer's UTF-8
 * helpers exactly as src/shared/asset_root.c does.
 *
 * Include it after defining _POSIX_C_SOURCE (getcwd and chdir need it under
 * -std=c11), in a test that links src/shared/serializer_io.c.
 */
#ifndef MANGO_TEST_FOLDERS_H
#define MANGO_TEST_FOLDERS_H

#include <stdlib.h>
#include <string.h>

#include "shared/serializer_io.h"   /* serializer_utf8_to_wide, _wide_to_utf8 */

#ifdef _WIN32
#include <direct.h>     /* _mkdir, _wgetcwd, _wchdir */
#else
#include <sys/stat.h>   /* mkdir */
#include <unistd.h>     /* getcwd, chdir */
#endif

/* Create folder `path`; one that already exists is fine. */
static inline void test_make_folder(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0755);
#endif
}

/* Copy the working folder into buf as UTF-8. Returns 0, or -1 when it
 * cannot be read or does not fit. */
static inline int test_working_folder(char *buf, size_t size)
{
#ifdef _WIN32
    wchar_t wide[SERIALIZER_IO_PATH_MAX];
    char *utf8;
    size_t length;

    if (!_wgetcwd(wide, SERIALIZER_IO_PATH_MAX)) return -1;
    utf8 = serializer_wide_to_utf8(wide);
    if (!utf8) return -1;
    length = strlen(utf8);
    if (length >= size) {
        free(utf8);
        return -1;
    }
    memcpy(buf, utf8, length + 1);
    free(utf8);
    return 0;
#else
    return getcwd(buf, size) ? 0 : -1;
#endif
}

/* Make `path` (UTF-8) the working folder. Returns 0, or -1 on failure. */
static inline int test_change_folder(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = serializer_utf8_to_wide(path);
    int result = wide ? _wchdir(wide) : -1;
    free(wide);
    return result == 0 ? 0 : -1;
#else
    return chdir(path) == 0 ? 0 : -1;
#endif
}

#endif /* MANGO_TEST_FOLDERS_H */
