/*
 * file_dialog.h — Native file dialog for opening TOML level files.
 *
 * Provides a cross-platform function that opens the OS file picker and
 * returns the selected file path.  Uses platform-specific commands:
 *   macOS  → osascript (AppleScript NSOpenPanel)
 *   Linux  → zenity --file-selection, or kdialog when zenity is missing
 *   Windows → PowerShell System.Windows.Forms.OpenFileDialog
 *
 * These are launched via popen() — no library dependencies required.
 * When the system has no such program (a Linux desktop with neither
 * zenity nor kdialog), the path is asked for inside the editor window
 * instead (dialog_path), so Open and Save As always work.
 */
#pragma once

#include <stdio.h> /* FILE */

#define FILE_DIALOG_UNAVAILABLE  (-3) /* no file picker program is installed   */
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
 *         FILE_DIALOG_ERROR if the dialog failed (buf untouched),
 *         FILE_DIALOG_UNAVAILABLE if there is no picker program and the
 *         in-window prompt could not be shown either.
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
 * dialog_path — Ask for a file path inside the editor window (dialog_choice.c).
 *
 * The same box as dialog_choice with one line to type into and OK / Cancel
 * below it.  buf holds the text to start from (often empty) and receives
 * the path; typing appends, Backspace erases, Ctrl+V pastes, Enter is OK
 * and Esc is Cancel.  Returns FILE_DIALOG_SELECTED, FILE_DIALOG_CANCELLED
 * (Cancel, or OK on an empty line) or FILE_DIALOG_ERROR when no one can
 * answer.  file_dialog_open and file_dialog_save fall back to it when the
 * system has no file picker to show.
 */
int dialog_path(const char *title, const char *message, char *buf, int buf_size);

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

/*
 * file_dialog_result — What a picker's exit means, given what it printed.
 *
 * read_result is file_dialog_read_path's answer; exited says the program
 * ended by itself (not by a signal) and exit_code is then its exit code.
 *   - exit code 127 is the shell's "command not found": no picker is
 *     installed, FILE_DIALOG_UNAVAILABLE, whatever was read.
 *   - nothing printed and exit 0 or 1: the user cancelled (PowerShell exits
 *     0 on cancel; osascript, zenity and kdialog exit 1).
 *   - nothing printed and anything else: FILE_DIALOG_ERROR.
 *   - a path printed but a failing exit: FILE_DIALOG_ERROR.
 */
int file_dialog_result(int read_result, int exited, int exit_code);

#ifdef MANGO_TESTING
/* Deterministic picker seam used by editor workflow tests (test builds only). */
void file_dialog_test_set_open_result(int result, const char *path);
void file_dialog_test_set_save_result(int result, const char *path);
#endif
