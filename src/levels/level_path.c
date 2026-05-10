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
#elif !defined(__EMSCRIPTEN__)
#include <limits.h>
#include <stdlib.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

static void level_path_copy(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) return;
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int level_resolve_path(const char *path, char *resolved_path, size_t resolved_path_size)
{
    if (!path || !resolved_path || resolved_path_size == 0) return -1;
    resolved_path[0] = '\0';
    if (path[0] == '\0') return -1;

#if defined(__EMSCRIPTEN__)
    /* Emscripten has no realpath — use the path as-is. */
    level_path_copy(resolved_path, resolved_path_size, path);
    return 0;
#elif defined(_WIN32)
    {
        char resolved[260]; /* MAX_PATH */
        DWORD len = GetFullPathNameA(path, (DWORD)sizeof(resolved), resolved, NULL);
        if (len == 0 || len >= (DWORD)sizeof(resolved)) return -1;
        level_path_copy(resolved_path, resolved_path_size, resolved);
        return 0;
    }
#else
    {
        char resolved[PATH_MAX];
        if (realpath(path, resolved) == NULL) return -1;
        level_path_copy(resolved_path, resolved_path_size, resolved);
        return 0;
    }
#endif
}
