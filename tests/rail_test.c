#include <math.h>
#include <stdio.h>

#include "levels/level.h"
#include "surfaces/rail.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "rail_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float(const char *name, float actual, float expected)
{
    if (fabsf(actual - expected) > 0.001f) {
        fprintf(stderr, "rail_test: %s got %.3f expected %.3f\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int builds_rect_rail_from_placement(void)
{
    Rail rails[MAX_RAILS] = {0};
    RailPlacement placements[1] = {{RAIL_LAYOUT_RECT, 100, 40, 4, 3, 0}};
    int count = 0;

    rail_init_from_placements(rails, &count, placements, 1);

    if (expect_int("rail count", count, 1) != 0) return 1;
    if (expect_int("rect tile count", rails[0].count, 10) != 0) return 1;
    if (expect_int("rect closed", rails[0].closed, 1) != 0) return 1;
    if (expect_int("rect end_cap", rails[0].end_cap, 1) != 0) return 1;
    if (expect_int("top-left x", rails[0].tiles[0].x, 100) != 0) return 1;
    if (expect_int("top-left y", rails[0].tiles[0].y, 40) != 0) return 1;
    if (expect_int("top-left conn", rails[0].tiles[0].connections, RAIL_E | RAIL_S) != 0) return 1;
    if (expect_int("top-right x", rails[0].tiles[3].x, 148) != 0) return 1;
    if (expect_int("top-right conn", rails[0].tiles[3].connections, RAIL_W | RAIL_S) != 0) return 1;

    return 0;
}

static int builds_open_horizontal_rail_from_placement(void)
{
    Rail rails[MAX_RAILS] = {0};
    RailPlacement placements[1] = {{RAIL_LAYOUT_HORIZ, 200, 88, 5, 0, 0}};
    int count = 0;

    rail_init_from_placements(rails, &count, placements, 1);

    if (expect_int("rail count", count, 1) != 0) return 1;
    if (expect_int("horiz tile count", rails[0].count, 5) != 0) return 1;
    if (expect_int("horiz closed", rails[0].closed, 0) != 0) return 1;
    if (expect_int("horiz end_cap", rails[0].end_cap, 0) != 0) return 1;
    if (expect_int("left cap conn", rails[0].tiles[0].connections, RAIL_E) != 0) return 1;
    if (expect_int("uncapped right conn", rails[0].tiles[4].connections, RAIL_E | RAIL_W) != 0) return 1;
    if (expect_int("last x", rails[0].tiles[4].x, 264) != 0) return 1;

    return 0;
}

static int interpolates_world_position(void)
{
    Rail rail = {0};
    rail.count = 2;
    rail.closed = 1;
    rail.tiles[0].x = 100;
    rail.tiles[0].y = 40;
    rail.tiles[1].x = 116;
    rail.tiles[1].y = 40;

    float x = 0.0f;
    float y = 0.0f;
    rail_get_world_pos(&rail, 0.5f, &x, &y);

    if (expect_float("interp x", x, 116.0f) != 0) return 1;
    if (expect_float("interp y", y, 48.0f) != 0) return 1;

    return 0;
}

static int advances_closed_rail_with_wrap(void)
{
    Rail rail = {0};
    rail.count = 4;
    rail.closed = 1;

    if (expect_float("forward wrap", rail_advance(&rail, 3.5f, 2.0f, 0.5f), 0.5f) != 0)
        return 1;
    if (expect_float("backward wrap", rail_advance(&rail, 0.25f, -1.0f, 0.5f), 3.75f) != 0)
        return 1;

    return 0;
}

static int advances_open_rail_without_wrap(void)
{
    Rail rail = {0};
    rail.count = 4;
    rail.closed = 0;

    if (expect_float("open advance", rail_advance(&rail, 3.5f, 2.0f, 0.5f), 4.5f) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (builds_rect_rail_from_placement() != 0) return 1;
    if (builds_open_horizontal_rail_from_placement() != 0) return 1;
    if (interpolates_world_position() != 0) return 1;
    if (advances_closed_rail_with_wrap() != 0) return 1;
    if (advances_open_rail_without_wrap() != 0) return 1;

    puts("rail_test: ok");
    return 0;
}
