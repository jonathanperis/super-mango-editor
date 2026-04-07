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
#include "file_dialog.h"

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
 * Returns 1 on success (path in buf), 0 on cancel/error.
 */
int file_dialog_open(char *buf, int buf_size) {
    if (!buf || buf_size < 2) return 0;

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
        return 0;
    }

    /*
     * fgets — read one line from the command's stdout.
     *
     * The file dialog commands print the selected path as a single line.
     * If the user cancelled, fgets returns NULL (no output).
     */
    char *result = fgets(buf, buf_size, fp);
    int status = pclose(fp);

    if (!result || status != 0) {
        /* User cancelled or command failed — no file selected */
        return 0;
    }

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
    if (buf[0] == '\0') return 0;

    return 1;
}
