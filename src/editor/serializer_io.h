/*
 * serializer_io.h — Internal file I/O helpers for TOML serialization.
 */

#pragma once

#include <stdio.h>  /* FILE */

/* Open a TOML output file with platform-appropriate permissions. */
FILE *serializer_open_write(const char *path);
