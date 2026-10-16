/*
 * raymath_inline.h — raylib's raymath, compiled into each file that uses it.
 *
 * Plain raymath.h declares its functions `inline` and relies on libraylib.a
 * for the out-of-line copies. Including it as static inline instead keeps
 * every call compiled with this project's own flags, so a helper such as
 * Clamp gives the same result in the game, the editor, the tools and the
 * tests, linked against raylib or not.
 *
 * Use raymath only where it computes exactly what the code did before:
 * gameplay results feed replays and saved experiments.
 */
#pragma once

#define RAYMATH_STATIC_INLINE
#include <raymath.h>
