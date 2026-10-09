/*
 * asset_root.h — Find the folder that holds assets/ and levels/.
 *
 * Every texture, sound and level path in the game and editor is relative,
 * such as "assets/sprites/player/player.png". The operating system reads a
 * relative path from the process's *working folder*, which is wherever the
 * program was started from, not where the executable lives. Started from a
 * desktop shortcut, a file manager or another terminal folder, nothing loads.
 *
 * Both native entry points (src/main.c and src/editor/editor_main.c) call
 * these helpers once at startup: when the working folder has no assets/,
 * they change it to the folder holding the executable (or one or two folders
 * above it, which covers out/super-mango in a source checkout). Paths typed
 * on the command line are turned into absolute paths first, so they still
 * mean what they meant in the folder the user typed them in.
 *
 * The browser build never calls these: its files are preloaded at "/".
 */
#pragma once

/* Non-zero when folder holds the game's marker files. folder is "" for the
 * working folder, or a path that ends in a separator. */
int asset_root_contains(const char *folder);

/* Look in start (ending in a separator), then up to two parent folders, and
 * change the working folder to the first that holds the markers. Returns 0
 * on success; -1 leaves the working folder unchanged. */
int asset_root_enter_from(const char *start);

/* asset_root_enter_from(the executable's folder). */
int asset_root_enter(void);

/* Non-zero for "/x", "\\x" and "C:\x" style paths. */
int asset_root_path_is_absolute(const char *path);

/* An allocated absolute copy of path, read against the current working
 * folder; an absolute path is copied unchanged. NULL on failure. The caller
 * frees the result. Call it before asset_root_enter changes the folder. */
char *asset_root_absolute(const char *path);
