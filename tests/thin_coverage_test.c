/*
 * thin_coverage_test.c — Direct calls into the small modules the level
 * fixtures barely reach: spike platforms, the last star, circular saws,
 * bouncepad hit dispatch and fog waves.
 *
 * Linked into gameplay-mechanics-test, which has already opened the window,
 * so a 16×16 texture can be uploaded and the draw paths actually run.
 */
#include <stdio.h>
#include <string.h>

#include "raylib.h"

#include "collectibles/last_star.h"
#include "core/game_bouncepads.h"
#include "core/game_random.h"
#include "effects/fog.h"
#include "game_constants.h"
#include "hazards/circular_saw.h"
#include "hazards/spike_platform.h"
#include "gameplay_mechanics_test.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "thin_coverage_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)

static Texture2D *upload_square(Texture2D *storage)
{
    Image image = GenImageColor(16, 16, WHITE);
    *storage = LoadTextureFromImage(image);
    UnloadImage(image);
    return storage;
}

static int draw_spike_platforms_and_the_last_star(Texture2D *tex)
{
    int failed = 0;
    SpikePlatform spikes[3];
    LastStar star;

    memset(spikes, 0, sizeof(spikes));
    spike_platforms_render(spikes, 3, NULL, 0);

    spikes[0].active = 0;
    spikes[0].w = 48;
    spikes[1].active = 1;
    spikes[1].x = 10000.0f;
    spikes[1].w = 48;
    spikes[2].active = 1;
    spikes[2].x = 20.0f;
    spikes[2].y = 80.0f;
    spikes[2].w = 48; /* three pieces: left, centre, right */
    spike_platforms_render(spikes, 3, tex, 0);

    spikes[2].w = SPIKE_PLAT_PIECE_W; /* one piece, the left cap only */
    spike_platforms_render(&spikes[2], 1, tex, 0);

    memset(&star, 0, sizeof(star));
    last_star_render(&star, tex, 0);
    star.active = 1;
    star.w = LAST_STAR_DISPLAY_W;
    star.h = LAST_STAR_DISPLAY_H;
    last_star_render(&star, NULL, 0);
    star.x = 10000.0f;
    last_star_render(&star, tex, 0);
    star.x = -40.0f;
    last_star_render(&star, tex, 0);
    star.x = 30.0f;
    star.y = 40.0f;
    last_star_render(&star, tex, 0);
    CHECK(last_star_get_hitbox(&star).w == star.w - 4);
done:
    return failed;
}

static int saws_patrol_bounce_and_draw(Texture2D *tex)
{
    int failed = 0;
    CircularSaw saws[2];

    memset(saws, 0, sizeof(saws));
    saws[0].active = 0;
    saws[1].active = 1;
    saws[1].x = 190.0f;
    saws[1].y = 40.0f;
    saws[1].w = SAW_DISPLAY_W;
    saws[1].h = SAW_DISPLAY_H;
    saws[1].patrol_x0 = 100.0f;
    saws[1].patrol_x1 = 200.0f;
    saws[1].direction = 1;
    saws[1].spin_angle = 350.0f;

    circular_saws_update(saws, 2, 0.1f);
    CHECK(saws[1].spin_angle >= 0.0f && saws[1].spin_angle < 360.0f);
    CHECK(saws[1].x == 200.0f && saws[1].direction == -1);

    saws[1].x = 110.0f;
    circular_saws_update(&saws[1], 1, 0.1f);
    CHECK(saws[1].x == 100.0f && saws[1].direction == 1);

    circular_saws_render(saws, 2, NULL, 0);
    saws[1].active = 0;
    circular_saws_render(saws, 2, tex, 0);
    saws[1].active = 1;
    saws[1].x = 10000.0f;
    circular_saws_render(&saws[1], 1, tex, 0);
    saws[1].x = -80.0f;
    circular_saws_render(&saws[1], 1, tex, 0);
    saws[1].x = 40.0f;
    circular_saws_render(&saws[1], 1, tex, 0);
    CHECK(circular_saw_get_hitbox(&saws[1]).w == saws[1].w - 8);
done:
    return failed;
}

static int bounce_hits_each_pad_list(void)
{
    int failed = 0;
    GameState gs;
    BouncepadList lists[GAME_BOUNCEPAD_LIST_COUNT];

    memset(&gs, 0, sizeof(gs));
    gs.world.bouncepad_medium_count = 1;
    gs.world.bouncepad_small_count = 1;
    gs.world.bouncepad_high_count = 1;
    game_bouncepads_lists(&gs, lists);
    CHECK(lists[0].pads == gs.world.bouncepads_medium && lists[0].count == 1);
    CHECK(lists[1].pads == gs.world.bouncepads_small && lists[1].count == 1);
    CHECK(lists[2].pads == gs.world.bouncepads_high && lists[2].count == 1);

    game_bouncepads_handle_hit(&gs, -1);
    CHECK(gs.world.bouncepads_medium[0].state != BOUNCE_ACTIVE);

    game_bouncepads_handle_hit(&gs, 0);
    CHECK(gs.world.bouncepads_medium[0].state == BOUNCE_ACTIVE);
    CHECK(gs.world.bouncepads_medium[0].anim_frame == 1);

    gs.screen.debug_mode = 1;
    game_bouncepads_handle_hit(&gs, 1);
    CHECK(gs.world.bouncepads_small[0].state == BOUNCE_ACTIVE);
    game_bouncepads_handle_hit(&gs, 2);
    CHECK(gs.world.bouncepads_high[0].state == BOUNCE_ACTIVE);
    CHECK(gs.screen.debug.log_count == 2);
done:
    return failed;
}

static int fog_waves_fade_overlap_and_fill(void)
{
    int failed = 0;
    FogSystem fog;
    char paths[MAX_FOG_TEXTURES + 1][64];

    memset(&fog, 0, sizeof(fog));
    game_random_seed(1);
    fog_init(&fog, NULL, NULL, 0);
    fog_update(&fog, 1.0f);
    fog_render(&fog);
    fog_cleanup(&fog, NULL);
    CHECK(fog.tex_count == 0);

    for (int i = 0; i < MAX_FOG_TEXTURES + 1; i++)
        snprintf(paths[i], sizeof(paths[i]), "assets/no-such-fog-%d.png", i);
    snprintf(paths[0], sizeof(paths[0]), "assets/sprites/foregrounds/fog_1.png");
    /* The parameter is an array of const rows. GCC -Wpedantic rejects
     * passing char[][64] for that before C23, so the call names the type. */
    fog_init(&fog, NULL, (const char (*)[64])paths, MAX_FOG_TEXTURES + 1);
    CHECK(fog.tex_count == MAX_FOG_TEXTURES);
    CHECK(fog.textures[0] != NULL);
    CHECK(fog.instances[0].active);
    fog.instances[0].tex_index = 0;

    fog.instances[0].elapsed = 0.0f;
    fog_render(&fog);
    fog.instances[0].elapsed = fog.instances[0].duration * 0.5f;
    fog_render(&fog);
    fog.instances[0].elapsed = fog.instances[0].duration - 0.1f;
    fog_render(&fog);

    fog.instances[0].elapsed = fog.instances[0].duration - 1.5f;
    fog.instances[0].spawned_next = 0;
    fog_update(&fog, 0.6f);
    CHECK(fog.instances[0].spawned_next == 1);
    CHECK(fog.instances[0].active);

    for (int i = 0; i < FOG_MAX; i++) fog.instances[i].active = 1;
    fog.instances[0].spawned_next = 0;
    fog.instances[0].elapsed = fog.instances[0].duration - 0.5f;
    fog_update(&fog, 0.0f);
    CHECK(fog.instances[0].spawned_next == 1);

    fog.instances[0].elapsed = fog.instances[0].duration;
    fog_update(&fog, 0.0f);
    CHECK(fog.instances[0].active == 0);

    fog.instances[1].dir = -1;
    fog.instances[1].active = 1;
    fog.instances[1].duration = 8.0f;
    fog.instances[1].elapsed = 0.0f;
    fog.instances[1].spawned_next = 1;
    fog_update(&fog, 0.5f);
    CHECK(fog.instances[1].x < 0.0f);

    fog_cleanup(&fog, NULL);
    CHECK(fog.tex_count == 0 && fog.textures[0] == NULL);
done:
    return failed;
}

int thin_modules_cover_their_branches(void)
{
    int failed = 0;
    Texture2D storage;
    Texture2D *tex = upload_square(&storage);

    CHECK(IsTextureValid(*tex));
    CHECK(draw_spike_platforms_and_the_last_star(tex) == 0);
    CHECK(saws_patrol_bounce_and_draw(tex) == 0);
    CHECK(bounce_hits_each_pad_list() == 0);
    CHECK(fog_waves_fade_overlap_and_fill() == 0);
    UnloadTexture(*tex);
    return 0;
done:
    if (IsTextureValid(storage)) UnloadTexture(storage);
    return failed;
}
