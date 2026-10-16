/*
 * editor_main.c — Standalone editor entry point.
 *
 * Initialize one EditorState (on the heap), optionally load a TOML
 * document, run frames, and clean up. editor_init owns graphics setup; this silent tool needs no
 * audio device. --smoke-test renders five frames rather than opening a loop.
 */
#include "editor.h"
#include "editor_frame.h"
#include "editor_files.h"
#include "editor_session.h"
#include "../shared/asset_root.h"
#include "../shared/serializer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int smoke = 0;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--smoke-test")) smoke = 1;
        else path = argv[i];
    }
    /* Palette art, level checks and Playtest all use paths such as
     * "assets/..." relative to the working folder. Started elsewhere, move
     * to the executable's folder, after making the document path absolute
     * so it still names the file the user meant (see shared/asset_root.h). */
    char *typed_path = NULL;
    if (!asset_root_contains("")) {
        if (path) {
            typed_path = asset_root_absolute(path);
            if (!typed_path) {
                fprintf(stderr, "Error: out of memory while reading the command line\n");
                return EXIT_FAILURE;
            }
            path = typed_path;
        }
        if (asset_root_enter() != 0)
            fprintf(stderr, "Warning: assets/ and levels/ were not found here or "
                            "next to the executable\n");
    }
    /* An EditorState is about 200 KB (the level, its last valid copy, the
     * clipboard...), too much for the stack of a thread on Windows (1 MB in
     * all), so it lives on the heap.  calloc gives pointers NULL and numeric
     * members zero, which lets a failed start-up use the same cleanup path
     * as a normal exit. */
    EditorState *editor = calloc(1, sizeof(*editor));
    if (!editor) {
        fprintf(stderr, "Error: out of memory starting the editor\n");
        free(typed_path);
        return EXIT_FAILURE;
    }
    if (editor_init(editor, smoke)) {
        free(editor);
        free(typed_path);
        return EXIT_FAILURE;
    }
    if (path) {
        int result;
        if (smoke) {
            /* Render the requested document without discovering personal
             * recovery/recent-file data or allocating persistence identities. */
            result = editor_path_fits(path) ? level_load_toml(path, &editor->level) : -1;
            if (!result) {
                str_copy(editor->file_path, path, sizeof(editor->file_path));
                editor_sync_config_resources(editor);
                editor_set_document_save_point(editor);
            }
        } else
            result = editor_load_level(editor, path);
        if (result)
            fprintf(stderr, "Warning: could not load '%s' — starting empty\n", path);
    }
    if (!smoke)
        editor_loop(editor);
    else
        for (int frame = 0; frame < 5 && editor->running; frame++)
            editor_run_frame(editor);
    int result = smoke && !editor->running ? EXIT_FAILURE : EXIT_SUCCESS;
    editor_cleanup(editor);
    free(editor);
    free(typed_path);
    return result;
}
