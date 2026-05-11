/*
 * serializer_io.c — Internal file I/O helpers for TOML serialization.
 */

#include "serializer_io.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <sys/stat.h> /* open, O_WRONLY, O_CREAT, O_TRUNC */
#endif

FILE *serializer_open_write(const char *path)
{
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return fd >= 0 ? fdopen(fd, "w") : NULL;
#else
    return fopen(path, "w");
#endif
}
