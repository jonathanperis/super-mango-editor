/*
 * printf_format.h — Let the compiler check printf-style format strings.
 *
 * A function that takes a format string and "..." (like printf) hides its
 * arguments from the compiler, so a call such as
 *     editor_set_status(es, "Saved %s", count);   (an int, not a string)
 * compiles cleanly and then reads garbage at run time.  GCC and Clang can
 * check these calls the same way they check printf itself, if the function
 * declaration says which argument is the format:
 *
 *     void editor_set_status(EditorState *es, const char *fmt, ...)
 *         PRINTF_FORMAT(2, 3);
 *
 * format_index is the position of the format string (counting from 1) and
 * first_arg_index the position of the first value it formats, i.e. of the
 * "...".  Other compilers do not know the attribute, so there the macro
 * expands to nothing and the declaration is unchanged.
 */
#pragma once

#if defined(__MINGW32__) && !defined(__clang__)
/*
 * MinGW GCC checks plain "printf" formats against the old Microsoft rules,
 * which do not know C99 sizes such as %zu.  The UCRT runtime these builds
 * use prints them correctly, so check against the standard (GNU) rules.
 */
#define PRINTF_FORMAT(format_index, first_arg_index) \
    __attribute__((format(gnu_printf, format_index, first_arg_index)))
#elif defined(__GNUC__) || defined(__clang__)
#define PRINTF_FORMAT(format_index, first_arg_index) \
    __attribute__((format(printf, format_index, first_arg_index)))
#else
#define PRINTF_FORMAT(format_index, first_arg_index)
#endif
