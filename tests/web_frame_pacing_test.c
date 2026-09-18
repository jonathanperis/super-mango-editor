/* Compile the production display boundary's Web path against tiny platform
 * fakes. A visible browser window must not arm raylib's blocking FPS limiter.
 * This complements real-browser verification without requiring a browser in
 * every native regression run or altering the production interface. */
#define __EMSCRIPTEN__ 1
#define display_open web_pacing_display_open
#define display_present web_pacing_display_present
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
        if (web_pacing_display_open(400, 300, "Web pacing", hidden) != 0 ||
            requested_fps != 0) {
            fprintf(stderr, "Web pacing: hidden=%d requested blocking FPS=%d\n",
                    hidden, requested_fps);
            return 1;
        }
    }
    return 0;
}
