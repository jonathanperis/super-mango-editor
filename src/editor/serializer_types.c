/*
 * serializer_types.c — Enum/string conversion helpers for level TOML.
 */

#include "serializer_types.h"

#include <string.h>  /* strcmp */

const char *serializer_rail_layout_to_str(RailLayout layout)
{
    switch (layout) {
        case RAIL_LAYOUT_RECT:  return "RECT";
        case RAIL_LAYOUT_HORIZ: return "HORIZ";
        default:                return "RECT";
    }
}

RailLayout serializer_rail_layout_from_str(const char *s)
{
    if (s && strcmp(s, "HORIZ") == 0) return RAIL_LAYOUT_HORIZ;
    return RAIL_LAYOUT_RECT;
}

const char *serializer_axe_mode_to_str(AxeTrapMode mode)
{
    switch (mode) {
        case AXE_MODE_PENDULUM: return "PENDULUM";
        case AXE_MODE_SPIN:     return "SPIN";
        default:                return "PENDULUM";
    }
}

AxeTrapMode serializer_axe_mode_from_str(const char *s)
{
    if (s && strcmp(s, "SPIN") == 0) return AXE_MODE_SPIN;
    return AXE_MODE_PENDULUM;
}

const char *serializer_float_mode_to_str(FloatPlatformMode mode)
{
    switch (mode) {
        case FLOAT_PLATFORM_STATIC:  return "STATIC";
        case FLOAT_PLATFORM_CRUMBLE: return "CRUMBLE";
        case FLOAT_PLATFORM_RAIL:    return "RAIL";
        default:                     return "STATIC";
    }
}

FloatPlatformMode serializer_float_mode_from_str(const char *s)
{
    if (s && strcmp(s, "CRUMBLE") == 0) return FLOAT_PLATFORM_CRUMBLE;
    if (s && strcmp(s, "RAIL") == 0)    return FLOAT_PLATFORM_RAIL;
    return FLOAT_PLATFORM_STATIC;
}

const char *serializer_bouncepad_type_to_str(BouncepadType type)
{
    switch (type) {
        case BOUNCEPAD_GREEN: return "GREEN";
        case BOUNCEPAD_WOOD:  return "WOOD";
        case BOUNCEPAD_RED:   return "RED";
        default:              return "GREEN";
    }
}

BouncepadType serializer_bouncepad_type_from_str(const char *s)
{
    if (s && strcmp(s, "WOOD") == 0) return BOUNCEPAD_WOOD;
    if (s && strcmp(s, "RED") == 0)  return BOUNCEPAD_RED;
    return BOUNCEPAD_GREEN;
}
