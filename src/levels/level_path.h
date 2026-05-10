/*
 * level_path.h — Cross-platform level path resolution helpers.
 */

#pragma once

#include <stddef.h>

/* Resolve a user-provided level path into a safe path for loading. */
int level_resolve_path(const char *path, char *resolved_path, size_t resolved_path_size);
