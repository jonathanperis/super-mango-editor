/*
 * file_dialog.c — Native file dialog implementation using popen().
 *
 * Instead of linking a GUI toolkit or Objective-C framework, we shell out
 * to a platform-specific command that shows the native file picker.  The
 * command writes the selected path to stdout, which we read via popen().
 *
 * This approach has zero dependencies and works on macOS, Linux, and
 * Windows without any conditional compilation of GUI code.  The trade-off
 * is that popen() blocks the calling thread, but that's fine for a modal
 * file dialog — the user expects the editor to pause while they pick a file.
 */

#include <stdio.h>   /* popen, pclose, fgets, fprintf */
#include <string.h>  /* strlen, strchr */
#include <ctype.h>   /* tolower */
#if defined(__APPLE__) || defined(__unix__)
#include <sys/wait.h> /* WIFEXITED, WEXITSTATUS */
#endif
#include "file_dialog.h"

#define FILE_DIALOG_TEST_PATH_MAX 4096

static int file_dialog_test_open_result = -1;
static int file_dialog_test_save_result = -1;
static char file_dialog_test_open_path[FILE_DIALOG_TEST_PATH_MAX];
static char file_dialog_test_save_path[FILE_DIALOG_TEST_PATH_MAX];

void file_dialog_test_set_open_result(int result, const char *path)
{
    file_dialog_test_open_result = result;
    if (path) {
        strncpy(file_dialog_test_open_path, path,
                sizeof(file_dialog_test_open_path) - 1);
        file_dialog_test_open_path[sizeof(file_dialog_test_open_path) - 1] = '\0';
    } else {
        file_dialog_test_open_path[0] = '\0';
    }
}

void file_dialog_test_set_save_result(int result, const char *path)
{
    file_dialog_test_save_result = result;
    if (path) {
        strncpy(file_dialog_test_save_path, path,
                sizeof(file_dialog_test_save_path) - 1);
        file_dialog_test_save_path[sizeof(file_dialog_test_save_path) - 1] = '\0';
    } else {
        file_dialog_test_save_path[0] = '\0';
    }
}
/* ------------------------------------------------------------------ */

/*
 * file_dialog_open — Show a native file-open dialog and return the path.
 *
 * Platform detection uses predefined compiler macros:
 *   __APPLE__   → macOS     (osascript / AppleScript)
 *   _WIN32      → Windows   (PowerShell OpenFileDialog)
 *   otherwise   → Linux     (zenity)
 *
 * Each platform command is designed to:
 *   1. Show only .toml files by default (with a way to see all files).
 *   2. Print the selected absolute path to stdout.
 *   3. Exit with code 0 on selection, non-zero on cancel.
 *
 * Returns FILE_DIALOG_SELECTED, FILE_DIALOG_CANCELLED, or FILE_DIALOG_ERROR.
 */
int file_dialog_open(char *buf, int buf_size) {
    if (!buf || buf_size < 2) return FILE_DIALOG_ERROR;
    if (file_dialog_test_open_result >= 0) {
        int result = file_dialog_test_open_result;
        file_dialog_test_open_result = -1;
        if (result == FILE_DIALOG_SELECTED) {
            strncpy(buf, file_dialog_test_open_path, (size_t)buf_size - 1);
            buf[buf_size - 1] = '\0';
        }
        return result;
    }

#if defined(__APPLE__)
    /*
     * macOS: use osascript to run an AppleScript that invokes NSOpenPanel.
     *
     * "choose file" shows the native macOS open dialog.
     * "of type {\"toml\"}" filters to .toml files.
     * "POSIX path of" converts the result from an AppleScript alias
     * (e.g. "Macintosh HD:Users:...") to a POSIX path ("/Users/...").
     *
     * If the user cancels, osascript exits with code 1 and popen returns
     * NULL or fgets returns NULL — both handled below.
     */
    const char *cmd =
        "osascript -e '"
        "set f to choose file of type {\"toml\", \"public.toml\"} "
        "with prompt \"Open Level TOML\"' "
        "-e 'POSIX path of f' 2>/dev/null";

#elif defined(_WIN32)
    /*
     * Windows: use PowerShell to show System.Windows.Forms.OpenFileDialog.
     *
     * Add-Type loads the WinForms assembly.  The dialog is configured to
     * filter for .toml files.  ShowDialog() returns "OK" if a file was
     * selected; the FileName property holds the path.
     */
    const char *cmd =
        "powershell -NoProfile -Command \""
        "[Console]::OutputEncoding = [Text.Encoding]::UTF8;"
        "Add-Type -AssemblyName System.Windows.Forms;"
        "$d = New-Object System.Windows.Forms.OpenFileDialog;"
        "$d.Filter = 'TOML files (*.toml)|*.toml|All files (*.*)|*.*';"
        "$d.Title = 'Open Level TOML';"
        "if ($d.ShowDialog() -eq 'OK') { $d.FileName }\"";

#else
    /*
     * Linux / other POSIX: use zenity, a GTK dialog utility commonly
     * installed on GNOME desktops.  Falls back gracefully — if zenity
     * is not installed, popen returns NULL.
     *
     * --file-selection shows the open dialog.
     * --file-filter limits to .toml files.
     */
    const char *cmd =
        "zenity --file-selection "
        "--title='Open Level TOML' "
        "--file-filter='TOML files | *.toml' "
        "--file-filter='All files | *' "
        "2>/dev/null";
#endif

    /*
     * popen — launch the command in a child process and read its stdout.
     *
     * "r" opens the pipe for reading.  If the command fails to start
     * (e.g. osascript not found), popen returns NULL.
     */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Warning: could not open file dialog\n");
        return FILE_DIALOG_ERROR;
    }

    /*
     * fgets — read one line from the command's stdout.
     *
     * The file dialog commands print the selected path as a single line.
     * If the user cancelled, fgets returns NULL (no output).
     */
    char *result = fgets(buf, buf_size, fp);
    int status = pclose(fp);

    if (!result) {
        /* PowerShell exits successfully without output on cancel. */
        if (status == 0) return FILE_DIALOG_CANCELLED;
#if defined(__APPLE__) || defined(__unix__)
        if (status > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 1)
            return FILE_DIALOG_CANCELLED;
#endif
        return FILE_DIALOG_ERROR;
    }
    if (status != 0) return FILE_DIALOG_ERROR;
    if (!strchr(buf, '\n') && strlen(buf) == (size_t)buf_size - 1)
        return FILE_DIALOG_ERROR;

    /*
     * Strip the trailing newline that fgets preserves.
     * osascript and zenity both output "path\n".
     */
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';

    /* Also strip trailing carriage return (Windows PowerShell outputs \r\n) */
    char *cr = strchr(buf, '\r');
    if (cr) *cr = '\0';

    /* Empty string means no selection */
    if (buf[0] == '\0') return FILE_DIALOG_CANCELLED;

    return FILE_DIALOG_SELECTED;
}

static int file_dialog_add_toml_extension(char *buf, int buf_size)
{
    const char *slash;
    const char *backslash;
    const char *dot;
    size_t len;

    if (!buf || buf_size < 2 || buf[0] == '\0') return 0;
    slash = strrchr(buf, '/');
    backslash = strrchr(buf, '\\');
    dot = strrchr(buf, '.');
    if (backslash && (!slash || backslash > slash)) slash = backslash;

    if (dot && (!slash || dot > slash) && strlen(dot) == 5 &&
        tolower((unsigned char)dot[1]) == 't' &&
        tolower((unsigned char)dot[2]) == 'o' &&
        tolower((unsigned char)dot[3]) == 'm' &&
        tolower((unsigned char)dot[4]) == 'l') {
        return 1;
    }

    len = strlen(buf);
    if (len + 5 >= (size_t)buf_size) return 0;
    memcpy(buf + len, ".toml", 6);
    return 1;
}

int file_dialog_save(char *buf, int buf_size)
{
    if (!buf || buf_size < 2) return FILE_DIALOG_ERROR;
    if (file_dialog_test_save_result >= 0) {
        int result = file_dialog_test_save_result;
        file_dialog_test_save_result = -1;
        if (result == FILE_DIALOG_SELECTED) {
            strncpy(buf, file_dialog_test_save_path, (size_t)buf_size - 1);
            buf[buf_size - 1] = '\0';
        }
        return result;
    }

#if defined(__APPLE__)
    const char *cmd =
        "osascript -e '"
        "set f to choose file name with prompt \"Save Level TOML\" "
        "default name \"untitled.toml\"' "
        "-e 'POSIX path of f' 2>/dev/null";
#elif defined(_WIN32)
    const char *cmd =
        "powershell -NoProfile -Command \""
        "[Console]::OutputEncoding = [Text.Encoding]::UTF8;"
        "Add-Type -AssemblyName System.Windows.Forms;"
        "$d = New-Object System.Windows.Forms.SaveFileDialog;"
        "$d.Filter = 'TOML files (*.toml)|*.toml|All files (*.*)|*.*';"
        "$d.DefaultExt = 'toml';"
        "$d.AddExtension = $true;"
        "$d.OverwritePrompt = $false;"
        "$d.Title = 'Save Level TOML';"
        "if ($d.ShowDialog() -eq 'OK') { $d.FileName }\"";
#else
    const char *cmd =
        "zenity --file-selection --save "
        "--title='Save Level TOML' "
        "--file-filter='TOML files | *.toml' "
        "--file-filter='All files | *' 2>/dev/null";
#endif

    FILE *fp = popen(cmd, "r");
    char *result;
    int status;
    char *nl;
    char *cr;

    if (!fp) {
        fprintf(stderr, "Warning: could not open save file dialog\n");
        return FILE_DIALOG_ERROR;
    }

    result = fgets(buf, buf_size, fp);
    status = pclose(fp);
    if (!result) {
        if (status == 0) return FILE_DIALOG_CANCELLED;
#if defined(__APPLE__) || defined(__unix__)
        if (status > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 1)
            return FILE_DIALOG_CANCELLED;
#endif
        return FILE_DIALOG_ERROR;
    }
    if (status != 0) return FILE_DIALOG_ERROR;
    if (!strchr(buf, '\n') && strlen(buf) == (size_t)buf_size - 1)
        return FILE_DIALOG_ERROR;

    nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    cr = strchr(buf, '\r');
    if (cr) *cr = '\0';
    if (buf[0] == '\0') return FILE_DIALOG_CANCELLED;
    if (!file_dialog_add_toml_extension(buf, buf_size)) return FILE_DIALOG_ERROR;
    return FILE_DIALOG_SELECTED;
}
