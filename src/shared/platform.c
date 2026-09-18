#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

uint64_t clock_millis(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
#endif
}

void clock_wait(unsigned int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec delay = {(time_t)(milliseconds / 1000), (long)(milliseconds % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
#endif
}

size_t str_copy(char *dest, const char *source, size_t capacity)
{
    size_t size = strlen(source);
    if (capacity) {
        size_t copied = size < capacity ? size : capacity - 1;
        memcpy(dest, source, copied);
        dest[copied] = 0;
    }
    return size;
}

size_t utf8_copy(char *dest, const char *source, size_t capacity)
{
    size_t size = strlen(source);
    if (!capacity) return 0;
    size_t copied = size < capacity ? size : capacity - 1;
    if (copied < size)
        while (copied && ((unsigned char)source[copied] & 0xc0) == 0x80) copied--;
    memcpy(dest, source, copied);
    dest[copied] = 0;
    return copied;
}

static int make_directory(const char *path)
{
#ifdef _WIN32
    wchar_t wide[4096];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 4096)) return -1;
    return _wmkdir(wide);
#else
    return mkdir(path, 0700);
#endif
}

char *preference_path(const char *organization, const char *application)
{
    char root[4096];
#ifdef _WIN32
    wchar_t wide[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, wide) != S_OK) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, root, sizeof(root), NULL, NULL)) return NULL;
#else
    const char *home = getenv("HOME");
#ifdef __APPLE__
    if (!home || snprintf(root, sizeof(root), "%s/Library/Application Support", home) >= (int)sizeof(root)) return NULL;
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    int length = xdg && xdg[0] ? snprintf(root, sizeof(root), "%s", xdg) :
        home ? snprintf(root, sizeof(root), "%s/.local/share", home) : -1;
    if (length < 0 || length >= (int)sizeof(root)) return NULL;
#endif
#endif
    return preference_path_at(root, organization, application);
}

char *preference_path_at(const char *base, const char *organization, const char *application)
{
    size_t capacity = strlen(base) + strlen(organization) + strlen(application) + 4;
    char *result = malloc(capacity);
    if (!result) return NULL;
    snprintf(result, capacity, "%s/%s/%s/", base, organization, application);
#ifdef _WIN32
    /* SHGetFolderPathW creates the base. Do not try to mkdir a UNC server or
     * share while walking its path; only create our organization/app suffix. */
    char *start = result + strlen(base);
#else
    char *start = result + 1;
#endif
    for (char *p = start; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
#ifdef _WIN32
        if (p[-1] == ':') continue;
#endif
        char separator = *p;
        *p = 0;
        int status = make_directory(result);
        *p = separator;
        if (status != 0 && errno != EEXIST) { free(result); return NULL; }
    }
    return result;
}

char *application_path(void)
{
    char path[4096];
#ifdef _WIN32
    wchar_t wide[4096];
    DWORD length = GetModuleFileNameW(NULL, wide, 4096);
    if (!length || length >= 4096 || !WideCharToMultiByte(CP_UTF8, 0, wide, -1, path, sizeof(path), NULL, NULL)) return NULL;
#elif defined(__APPLE__)
    uint32_t path_capacity = sizeof(path);
    if (_NSGetExecutablePath(path, &path_capacity)) return NULL;
#elif defined(__EMSCRIPTEN__)
    str_copy(path, "/super-mango", sizeof(path));
#else
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0 || length >= (ssize_t)sizeof(path) - 1) return NULL;
    path[length] = 0;
#endif
    char *last = strrchr(path, '/');
    char *backslash = strrchr(path, '\\');
    if (!last || (backslash && backslash > last)) last = backslash;
    if (!last) return NULL;
    last[1] = 0;
    size_t size = strlen(path) + 1;
    char *result = malloc(size);
    if (result) memcpy(result, path, size);
    return result;
}
