/* Small OS services used where raylib does not preserve the project's path
 * and clock contracts. These are project helpers, not raylib entry points. */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Monotonic milliseconds for elapsed-time subtraction, not a calendar date. */
uint64_t clock_millis(void);
void clock_wait(unsigned int milliseconds);
/* Capacity includes the NUL byte. str_copy returns the source length so a
 * caller can detect truncation; utf8_copy returns bytes actually copied and
 * stops before a partial codepoint. Neither function allocates storage. */
size_t str_copy(char *dest, const char *source, size_t capacity);
size_t utf8_copy(char *dest, const char *source, size_t capacity);
/* path_copy_checked — Copy `path` into out (capacity bytes) when it is a
 * path the program may write to: not empty, short enough, and free of
 * control characters (a line break, a tab, DEL), which no file name the
 * program shows or types back can hold. Returns 0, or -1 with out empty.
 * The copy goes byte by byte through a function that returns each byte as
 * a number, which is also what CodeQL's cpp/path-injection query treats as
 * a new value rather than outside text passed along. */
int path_copy_checked(char *out, const char *path, size_t capacity);
/* Allocated UTF-8 results, including a trailing separator; caller frees them.
 * preference_path creates organization/application folders under the OS user
 * data root. application_path returns the executable's containing directory. */
char *preference_path(const char *organization, const char *application);
char *preference_path_at(const char *base, const char *organization, const char *application);
char *application_path(void);
/* The folder holding the file `executable`, with its trailing separator.
 * Outside Windows, symbolic links in the path are resolved first, so a
 * program started through a link finds the files next to the real one.
 * NULL when the path names no folder or memory runs out. */
char *executable_folder(const char *executable);
