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

/* Native two/three-button decision. Cancel/window-close selects cancel_index;
 * errors return -1 without granting a destructive action. */
int dialog_choice(const char *title, const char *message, const char *const *labels,
                  int count, int default_index, int cancel_index, int *selected);

/* Deterministic picker seam used by editor workflow tests. */
void file_dialog_test_set_open_result(int result, const char *path);
void file_dialog_test_set_save_result(int result, const char *path);
