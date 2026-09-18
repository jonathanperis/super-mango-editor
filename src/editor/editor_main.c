/* The standalone editor owns a raylib window and deliberately opens no audio. */
#include "editor.h"
#include "editor_frame.h"
#include "editor_files.h"
#include "editor_session.h"
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
    EditorState editor = {0};
    if (editor_init(&editor, smoke)) return EXIT_FAILURE;
    if (path) {
        int result;
        if (smoke) {
            /* Render the requested document without discovering personal
             * recovery/recent-file data or allocating persistence identities. */
            result = editor_path_fits(path) ? level_load_toml(path, &editor.level) : -1;
            if (!result) {
                str_copy(editor.file_path, path, sizeof(editor.file_path));
                editor_sync_config_resources(&editor);
                editor_set_document_save_point(&editor);
            }
        } else result = editor_load_level(&editor, path);
        if (result) fprintf(stderr, "Warning: could not load '%s' — starting empty\n", path);
    }
    if (!smoke) editor_loop(&editor);
    else for (int frame = 0; frame < 5 && editor.running; frame++) editor_run_frame(&editor);
    int result = smoke && !editor.running ? EXIT_FAILURE : EXIT_SUCCESS;
    editor_cleanup(&editor);
    return result;
}
