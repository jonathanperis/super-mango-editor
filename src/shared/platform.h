#pragma once
#include <stddef.h>
#include <stdint.h>

uint64_t clock_millis(void);
void clock_wait(unsigned int milliseconds);
size_t str_copy(char *dest, const char *source, size_t capacity);
size_t utf8_copy(char *dest, const char *source, size_t capacity);
/* Allocated UTF-8 result, including trailing separator. Caller frees it. */
char *preference_path(const char *organization, const char *application);
char *preference_path_at(const char *base, const char *organization, const char *application);
char *application_path(void);
