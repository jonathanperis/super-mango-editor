/*
 * exporter.h — Public interface for the level C code exporter.
 *
 * Declares level_export_c(), which generates compilable C source files
 * (.h and .c) from a LevelDef struct.  The output matches the exact
 * style of the hand-written sandbox_00.c so generated levels are
 * indistinguishable from manually authored ones.
 *
 * Include this header in any editor module that needs to export levels.
 */
#pragma once

#include "../levels/level.h"   /* LevelDef — the struct we export */

/* ------------------------------------------------------------------ */

/*
 * level_export_c — Write a LevelDef as two compilable C files.
 *
 * Generates:
 *   {dir_path}/{var_name}.h  — header with extern declaration
 *   {dir_path}/{var_name}.c  — source with the full const LevelDef initialiser
 *
 * The generated .c file includes all entity/hazard/surface headers so that
 * speed constants (SPIDER_SPEED, BOUNCEPAD_VY_MEDIUM, etc.) resolve at
 * compile time, exactly like the hand-written level files.
 *
 * Parameters:
 *   def       — pointer to the LevelDef to serialise.
 *   var_name  — C identifier used for both the file name and the variable
 *               name, e.g. "level_02" produces level_02_def.
 *   dir_path  — directory where the two files are written; must already exist.
 *
 * Returns 0 on success, -1 on error (file open failure).
 * On error a message is printed to stderr.
 */
int level_export_c(const LevelDef *def, const char *var_name, const char *dir_path);
