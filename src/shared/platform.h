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
/* Allocated UTF-8 results, including a trailing separator; caller frees them.
 * preference_path creates organization/application folders under the OS user
 * data root. application_path returns the executable's containing directory. */
char *preference_path(const char *organization, const char *application);
char *preference_path_at(const char *base, const char *organization, const char *application);
char *application_path(void);
