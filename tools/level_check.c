/*
 * level_check.c — Validate level files with the game's own loader.
 *
 * Usage: level-check <level.toml>...
 *
 * Each file goes through level_load_toml() exactly as the game and editor
 * load it: tomlc17 parse -> schema check -> section loaders ->
 * level_validate_runtime().  There is no second copy of the rules to keep in
 * step, so a new check in src/levels/level_validate.c applies here as soon as
 * it is written.  The loader prints the reason for each rejected file.
 *
 * `make validate-levels` runs this over levels/ and levels/labs/, then runs
 * tools/validate_levels.py for what a single file cannot show: that assets
 * and next_phase targets exist on disk and that the campaign manifest chains.
 *
 * Needs no window, GPU or audio: it links only the serializer, the validator,
 * level_ref and tomlc17.
 */

#include <stdio.h>

#include "shared/serializer.h"
#include "levels/level.h"

int main(int argc, char **argv)
{
    /* LevelDef is large; static storage keeps it off the stack. */
    static LevelDef def;
    int failed = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <level.toml>...\n", argv[0]);
        return 2;
    }
    for (int i = 1; i < argc; i++) {
        if (level_load_toml(argv[i], &def) != 0) {
            fprintf(stderr, "level_check: rejected %s\n", argv[i]);
            failed++;
        }
    }
    if (failed) {
        fprintf(stderr, "level_check: %d of %d levels rejected\n", failed, argc - 1);
        return 1;
    }
    printf("level_check: ok (%d levels)\n", argc - 1);
    return 0;
}
