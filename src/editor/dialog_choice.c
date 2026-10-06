#define _POSIX_C_SOURCE 200809L
#include "file_dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

/* Every dynamic string is a literal argument, never executable shell/script
 * text. Windows executes an encoded UTF-16 script to avoid cmd.exe reparsing.
 *
 * Both quoting functions wrap text in single quotes, where neither a POSIX
 * shell nor PowerShell expands anything; only the quote character itself
 * needs care.  Worst case each input byte becomes 4 output bytes, plus two
 * quotes and the NUL. */

/*
 * PowerShell treats four Unicode quotation marks exactly like the ASCII
 * apostrophe: U+2018 ‘ U+2019 ’ U+201A ‚ U+201B ‛.  In UTF-8 they are
 * E2 80 98..9B.  Returns the byte length of such a mark at text, else 0.
 */
static size_t powershell_quote_mark_length(const char *text)
{
    const unsigned char *s = (const unsigned char *)text;
    if (s[0] == '\'') return 1;
    if (s[0] == 0xE2 && s[1] == 0x80 && s[2] >= 0x98 && s[2] <= 0x9B) return 3;
    return 0;
}

char *dialog_quote_powershell(const char *text)
{
    size_t length = strlen(text);
    char *out = malloc(length*4+3);
    if (!out) return NULL;
    char *p = out;
    *p++ = '\'';
    while (*text) {
        size_t mark = powershell_quote_mark_length(text);
        if (mark) {
            /* Inside '...' a doubled quote mark is one literal mark; a
             * single one would end the string and let the rest of a file
             * name such as  x’;Start-Process calc;’.toml  run as code. */
            memcpy(p, text, mark); p += mark;
            memcpy(p, text, mark); p += mark;
            text += mark;
        } else {
            *p++ = *text++;
        }
    }
    *p++ = '\''; *p = 0;
    return out;
}

char *dialog_quote_posix(const char *text)
{
    size_t length = strlen(text);
    char *out = malloc(length*4+3);
    if (!out) return NULL;
    char *p = out;
    *p++ = '\'';
    for (; *text; text++) {
        /* A POSIX shell has no escape inside '...': close the quote, add
         * an escaped apostrophe, and reopen:  it's  ->  'it'\''s'  */
        if (*text == '\'') { memcpy(p, "'\\''", 4); p += 4; }
        else *p++ = *text;
    }
    *p++ = '\''; *p = 0;
    return out;
}

static char *quote(const char *text)
{
#ifdef _WIN32
    return dialog_quote_powershell(text);
#else
    return dialog_quote_posix(text);
#endif
}

#ifdef _WIN32
static char *encoded_command(const char *script)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, script, -1, NULL, 0);
    if (!count) return NULL;
    wchar_t *wide = malloc((size_t)count*sizeof(*wide));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, script, -1, wide, count)) { free(wide); return NULL; }
    size_t bytes = (size_t)(count-1)*sizeof(*wide);
    char *command = malloc(4*((bytes+2)/3)+96);
    if (!command) { free(wide); return NULL; }
    strcpy(command, "powershell -NoProfile -STA -EncodedCommand ");
    char *out = command+strlen(command);
    const unsigned char *data = (const unsigned char *)wide;
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < bytes; i += 3) {
        unsigned int value = (unsigned int)data[i]<<16;
        if (i+1 < bytes) value |= (unsigned int)data[i+1]<<8;
        if (i+2 < bytes) value |= data[i+2];
        *out++ = alphabet[(value>>18)&63]; *out++ = alphabet[(value>>12)&63];
        *out++ = i+1 < bytes ? alphabet[(value>>6)&63] : '=';
        *out++ = i+2 < bytes ? alphabet[value&63] : '=';
    }
    *out = 0;
    free(wide);
    return command;
}
#endif

int dialog_choice(const char *title, const char *message, const char *const *labels,
                  int count, int default_index, int cancel_index, int *selected)
{
    if (!title || !message || !labels || !selected || count < 2 || count > 3 ||
        default_index < 0 || default_index >= count || cancel_index < 0 || cancel_index >= count || default_index == cancel_index) return -1;
    char *quoted[5] = {quote(title), quote(message), NULL, NULL, NULL};
    size_t capacity = 4096;
    int result = -1;
    for (int i = 0; i < count; i++) quoted[i+2] = quote(labels[i]);
    for (int i = 0; i < count+2; i++) {
        if (!quoted[i]) goto done;
        capacity += strlen(quoted[i])*3;
    }
    char *command = malloc(capacity);
    if (!command) goto done;
#ifdef _WIN32
    int written = snprintf(command, capacity,
        "Add-Type -AssemblyName System.Windows.Forms; $f=New-Object Windows.Forms.Form; "
        "$f.Text=%s; $f.Width=600; $f.Height=240; $f.StartPosition='CenterScreen'; "
        "$l=New-Object Windows.Forms.Label; $l.Text=%s; $l.SetBounds(16,16,552,120); $f.Controls.Add($l); "
        "$script:choice=%d; $buttons=@(); $names=@(%s,%s%s%s); "
        "for($i=0;$i -lt $names.Count;$i++) { $b=New-Object Windows.Forms.Button; $b.Text=$names[$i]; "
        "$b.Tag=$i; $b.SetBounds((16+$i*180),150,168,30); "
        "$b.Add_Click({$script:choice=[int]$this.Tag; $f.Close()}); $f.Controls.Add($b); $buttons+= $b }; "
        "$f.AcceptButton=$buttons[%d]; $f.CancelButton=$buttons[%d]; "
        "[void]$f.ShowDialog(); Write-Output $script:choice; $f.Dispose()",
        quoted[0],quoted[1],cancel_index,quoted[2],quoted[3],count==3?",":"",count==3?quoted[4]:"",default_index,cancel_index);
    if (written < 0 || (size_t)written >= capacity) { free(command); goto done; }
    char *encoded = encoded_command(command);
    free(command);
    command = encoded;
    if (!command) goto done;
    FILE *pipe = _popen(command, "r");
#elif defined(__APPLE__)
    const char *script = "on run argv\ntry\nreturn button returned of (display dialog (item 2 of argv) with title (item 1 of argv) buttons (items 3 thru -3 of argv) default button (item -2 of argv) cancel button (item -1 of argv))\non error number -128\nreturn item -1 of argv\nend try\nend run";
    char *script_arg = quote(script);
    if (!script_arg) { free(command); goto done; }
    snprintf(command,capacity,"osascript -e %s -- %s %s %s %s %s %s %s",
             script_arg,quoted[0],quoted[1],quoted[2],quoted[3],count==3?quoted[4]:"",quoted[default_index+2],quoted[cancel_index+2]);
    free(script_arg);
    FILE *pipe = popen(command,"r");
#else
    int extra = 0;
    while (extra == default_index || extra == cancel_index) extra++;
    /* --no-markup: zenity otherwise parses --text as Pango markup, so a
     * file name containing < or & would be mangled or hidden. */
    snprintf(command,capacity,"zenity --question --no-markup --title=%s --text=%s --ok-label=%s --cancel-label=%s %s%s",
             quoted[0],quoted[1],quoted[default_index+2],quoted[cancel_index+2],
             count==3?"--extra-button=":"",count==3?quoted[extra+2]:"");
    FILE *pipe = popen(command,"r");
#endif
    free(command);
    if (!pipe) goto done;
    char output[128] = {0};
    (void)fgets(output,sizeof(output),pipe);
    output[strcspn(output,"\r\n")] = 0;
#ifdef _WIN32
    int status = _pclose(pipe);
    char *end;
    long choice = strtol(output,&end,10);
    if (status == 0 && end != output && !*end && choice >= 0 && choice < count) {
        *selected = (int)choice; result = 0;
    }
#else
    int status = pclose(pipe);
    if (!WIFEXITED(status)) goto done;
#ifdef __APPLE__
    if (WEXITSTATUS(status) == 0) for (int i = 0; i < count; i++)
        if (!strcmp(output,labels[i])) { *selected = i; result = 0; break; }
#else
    if (WEXITSTATUS(status) == 1) { *selected = cancel_index; result = 0; }
    else if (WEXITSTATUS(status) == 0) {
        *selected = default_index;
        for (int i = 0; i < count; i++) if (!strcmp(output,labels[i])) *selected = i;
        result = 0;
    }
#endif
#endif
done:
    for (int i = 0; i < 5; i++) free(quoted[i]);
    return result;
}
