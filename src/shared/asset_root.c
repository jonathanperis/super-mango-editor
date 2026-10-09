/*
 * asset_root.c — Find the folder that holds assets/ and levels/.
 *
 * See asset_root.h for why the game needs this. The work is three small
 * operating-system calls: ask where the executable is (application_path in
 * platform.c), ask for the working folder (getcwd), and change it (chdir).
 * Windows spells the last two _wgetcwd/_wchdir and wants UTF-16 paths.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L  /* getcwd, chdir */
#endif

#include "asset_root.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"      /* application_path, str_copy */
#include "serializer_io.h" /* serializer_file_exists_utf8, UTF-8/UTF-16 */

#ifdef _WIN32
#include <direct.h>  /* _wgetcwd, _wchdir */
#else
#include <unistd.h>  /* getcwd, chdir */
#endif

#define ASSET_ROOT_PATH_MAX 4096
/* out/super-mango and out/release/super-mango sit one and two folders below
 * the checkout; a release zip keeps its executables beside assets/. */
#define ASSET_ROOT_PARENT_LEVELS 2

/* One file the game cannot start without from each folder it needs. A plain
 * "does assets/ exist" test would accept an unrelated assets folder. */
static const char *const ASSET_ROOT_MARKERS[] = {
    "assets/sprites/levels/grass_tileset.png",
    "levels/campaigns/main.toml"
};

int asset_root_contains(const char *folder)
{
    char path[ASSET_ROOT_PATH_MAX];

    if (!folder) return 0;
    for (size_t i = 0; i < sizeof(ASSET_ROOT_MARKERS) / sizeof(ASSET_ROOT_MARKERS[0]); i++) {
        int size = snprintf(path, sizeof(path), "%s%s", folder, ASSET_ROOT_MARKERS[i]);
        if (size < 0 || (size_t)size >= sizeof(path)) return 0;
        if (!serializer_file_exists_utf8(path)) return 0;
    }
    return 1;
}

static int change_directory(const char *folder)
{
#ifdef _WIN32
    wchar_t *wide = serializer_utf8_to_wide(folder);
    int result = wide ? _wchdir(wide) : -1;
    free(wide);
    return result == 0 ? 0 : -1;
#else
    return chdir(folder) == 0 ? 0 : -1;
#endif
}

int asset_root_enter_from(const char *start)
{
    char folder[ASSET_ROOT_PATH_MAX];

    if (!start || str_copy(folder, start, sizeof(folder)) >= sizeof(folder)) return -1;
    for (int level = 0; level <= ASSET_ROOT_PARENT_LEVELS; level++) {
        if (asset_root_contains(folder)) return change_directory(folder);
        /* "dir/" + "../" names dir's parent without needing to edit the
         * string; both separators work on every desktop system. */
        size_t used = strlen(folder);
        if (used + sizeof("../") > sizeof(folder)) break;
        memcpy(folder + used, "../", sizeof("../"));
    }
    return -1;
}

int asset_root_enter(void)
{
    char *folder = application_path();
    int result = folder ? asset_root_enter_from(folder) : -1;
    free(folder);
    return result;
}

int asset_root_path_is_absolute(const char *path)
{
    if (!path || !path[0]) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    /* A Windows drive letter: "C:\levels" or "C:/levels". */
    return isalpha((unsigned char)path[0]) && path[1] == ':' &&
           (path[2] == '\\' || path[2] == '/');
}

/* The working folder as an allocated UTF-8 string without a trailing
 * separator (except for a bare root such as "/"). */
static char *current_directory(void)
{
#ifdef _WIN32
    wchar_t wide[ASSET_ROOT_PATH_MAX];
    if (!_wgetcwd(wide, ASSET_ROOT_PATH_MAX)) return NULL;
    return serializer_wide_to_utf8(wide);
#else
    char buffer[ASSET_ROOT_PATH_MAX];
    if (!getcwd(buffer, sizeof(buffer))) return NULL;
    size_t size = strlen(buffer) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, buffer, size);
    return copy;
#endif
}

char *asset_root_absolute(const char *path)
{
    if (!path || !path[0]) return NULL;
    if (asset_root_path_is_absolute(path)) {
        size_t size = strlen(path) + 1;
        char *copy = malloc(size);
        if (copy) memcpy(copy, path, size);
        return copy;
    }
    char *folder = current_directory();
    if (!folder) return NULL;
    size_t folder_len = strlen(folder);
    int needs_separator = folder_len && folder[folder_len - 1] != '/' &&
                          folder[folder_len - 1] != '\\';
    size_t size = folder_len + (size_t)needs_separator + strlen(path) + 1;
    char *joined = malloc(size);
    if (joined) snprintf(joined, size, "%s%s%s", folder, needs_separator ? "/" : "", path);
    free(folder);
    return joined;
}
