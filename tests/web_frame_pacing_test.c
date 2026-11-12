/* Compile the production display boundary's Web path against tiny platform
 * fakes. A visible browser window must not arm raylib's blocking FPS limiter.
 * This complements real-browser verification without requiring a browser in
 * every native regression run or altering the production interface. */
#define __EMSCRIPTEN__ 1
#define display_open web_pacing_display_open
#define display_present web_pacing_display_present
#define display_viewport web_pacing_display_viewport
#define display_scale_fit web_pacing_display_scale_fit
#define display_keep_on_monitor web_pacing_display_keep_on_monitor
#define display_frame_cap web_pacing_display_frame_cap
#define display_follow_monitor web_pacing_display_follow_monitor
#define display_monitor_changed web_pacing_display_monitor_changed
#define SetTraceLogLevel web_pacing_trace
#define SetConfigFlags web_pacing_flags
#define InitWindow web_pacing_init
#define IsWindowReady web_pacing_ready
#define SetExitKey web_pacing_exit_key
#define SetTargetFPS web_pacing_target
#include "../src/shared/graphics.c"

#include <stdio.h>

static int requested_fps;

void web_pacing_trace(int level) { (void)level; }
void web_pacing_flags(unsigned int flags) { (void)flags; }
void web_pacing_init(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
}
bool web_pacing_ready(void) { return true; }
void web_pacing_exit_key(int key) { (void)key; }
void web_pacing_target(int fps) { requested_fps = fps; }

int web_frame_pacing_contract_test(void)
{
    for (int hidden = 0; hidden <= 1; hidden++) {
        requested_fps = -1;
        if (web_pacing_display_open(400, 300, "Web pacing", hidden, 0) != 0 ||
            requested_fps != 0) {
            fprintf(stderr, "Web pacing: hidden=%d requested blocking FPS=%d\n",
                    hidden, requested_fps);
            return 1;
        }
    }
    /* A native window draws one frame per refresh of its display; a rate
     * the system cannot report, or an impossible one, falls back to 60. */
    static const struct { int refresh_hz, cap; } cases[] = {
        {60, 60}, {75, 75}, {120, 120}, {144, 144}, {240, 240}, {360, 360},
        {30, 30}, {0, 60}, {-1, 60}, {29, 60}, {361, 60}, {100000, 60},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (web_pacing_display_frame_cap(cases[i].refresh_hz) != cases[i].cap) {
            fprintf(stderr, "Frame cap: %d Hz gave %d, expected %d\n", cases[i].refresh_hz,
                    web_pacing_display_frame_cap(cases[i].refresh_hz), cases[i].cap);
            return 1;
        }
    }
    return 0;
}
