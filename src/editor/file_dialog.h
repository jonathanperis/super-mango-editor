/*
 * file_dialog.h — Native file dialog for opening TOML level files.
 *
 * Provides a cross-platform function that opens the OS file picker and
 * returns the selected file path.  Uses platform-specific commands:
 *   macOS  → osascript (AppleScript NSOpenPanel)
 *   Linux  → zenity --file-selection
 *   Windows → PowerShell System.Windows.Forms.OpenFileDialog
 *
 * These are launched via popen() — no library dependencies required.
 */
#pragma once

#include <stdio.h> /* FILE */

#define FILE_DIALOG_INVALID_PATH (-2) /* picked name contains a line break */
#define FILE_DIALOG_ERROR     (-1)
#define FILE_DIALOG_CANCELLED  0
#define FILE_DIALOG_SELECTED   1

/*
 * file_dialog_open — Show a native file open dialog filtered to .toml files.
 *
 * buf      : buffer to receive the selected file path (null-terminated).
 * buf_size : size of the buffer in bytes.
 *
 * Returns FILE_DIALOG_SELECTED if the user selected a file,
 *         FILE_DIALOG_CANCELLED if the user cancelled,
 *         FILE_DIALOG_ERROR if the dialog failed (buf untouched).
 *
 * The dialog blocks until the user selects a file or cancels.  Because
 * it uses popen() to run an external process, the editor event loop is
 * paused during this time — this is acceptable for a file dialog.
 */
int file_dialog_open(char *buf, int buf_size);

/* Show a native save picker filtered to .toml files, without overwrite UI. */
int file_dialog_save(char *buf, int buf_size);

/*
 * Ask a two- or three-button question inside the editor window and wait
 * for the answer (dialog_choice.c; dialog_choice.h has the rest of it).
 * Enter presses the highlighted button, which starts as default_index;
 * Tab and the arrow keys move the highlight; Esc, or closing the window,
 * chooses cancel_index.  Returns 0 with the button in *selected, or -1
 * when no one can answer (no window, a hidden one, nothing attached),
 * which must never grant a destructive action.
 */
int dialog_choice(const char *title, const char *message, const char *const *labels,
                  int count, int default_index, int cancel_index, int *selected);

/*
 * file_dialog_read_path — Read the one line a native picker prints.
 *
 * Stores the path without its line ending in buf.  Returns
 * FILE_DIALOG_SELECTED, FILE_DIALOG_CANCELLED (no output),
 * FILE_DIALOG_ERROR (longer than buf), or FILE_DIALOG_INVALID_PATH when the
 * file name itself contains a line break: the picker prints such a name
 * across several lines, and keeping only the first would silently open or
 * overwrite a different file.  The line ending is "\n", or "\r\n" on
 * Windows only; any other '\r' counts as part of the name and is refused
 * too.  Reads fp to its end; does not close it.
 */
int file_dialog_read_path(FILE *fp, char *buf, int buf_size);

#ifdef MANGO_TESTING
/* Deterministic picker seam used by editor workflow tests (test builds only). */
void file_dialog_test_set_open_result(int result, const char *path);
void file_dialog_test_set_save_result(int result, const char *path);
#endif
