/*
 * level_path.c — Cross-platform level path resolution helpers.
 */

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif

#include "level_path.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
#include "../shared/serializer_io.h"
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#elif !defined(__EMSCRIPTEN__)
#include <limits.h>
#include <stdlib.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

static int level_path_copy(char *dst, size_t dst_size, const char *src)
{
    if (strlen(src) >= dst_size) return -1;
    memcpy(dst, src, strlen(src) + 1);
    return 0;
}

int level_resolve_path(const char *path, char *resolved_path, size_t resolved_path_size)
{
    if (!path || !resolved_path || resolved_path_size == 0) return -1;
    resolved_path[0] = '\0';
    if (path[0] == '\0') return -1;

#if defined(__EMSCRIPTEN__)
    /* Emscripten has no realpath — use the path as-is. */
    return level_path_copy(resolved_path, resolved_path_size, path);
#elif defined(_WIN32)
    {
        wchar_t *wide = serializer_utf8_to_wide(path);
        if (!wide) return -1;
        DWORD needed = GetFullPathNameW(wide, 0, NULL, NULL);
        wchar_t *resolved = needed ? malloc((size_t)needed * sizeof(*resolved)) : NULL;
        char *utf8 = NULL;
        if (resolved) {
            DWORD length = GetFullPathNameW(wide, needed, resolved, NULL);
            if (length > 0 && length < needed) utf8 = serializer_wide_to_utf8(resolved);
        }
        int result = utf8 ? level_path_copy(resolved_path, resolved_path_size, utf8) : -1;
        free(utf8);
        free(resolved);
        free(wide);
        return result;
    }
#else
    {
        char resolved[PATH_MAX];
        if (realpath(path, resolved) == NULL) return -1;
        return level_path_copy(resolved_path, resolved_path_size, resolved);
    }
#endif
}
