/*
 * serializer_types.h — Enum/string conversion helpers for level TOML.
 */
#pragma once

#include "../levels/level.h"  /* RailLayout, AxeTrapMode, platform/pad enums */

/* Convert rail layout enum values to/from TOML strings. */
const char *serializer_rail_layout_to_str(RailLayout layout);
RailLayout serializer_rail_layout_from_str(const char *s);

/* Convert axe trap mode enum values to/from TOML strings. */
const char *serializer_axe_mode_to_str(AxeTrapMode mode);
AxeTrapMode serializer_axe_mode_from_str(const char *s);

/* Convert float platform mode enum values to/from TOML strings. */
const char *serializer_float_mode_to_str(FloatPlatformMode mode);
FloatPlatformMode serializer_float_mode_from_str(const char *s);

/* Convert bouncepad type enum values to/from TOML strings. */
const char *serializer_bouncepad_type_to_str(BouncepadType type);
BouncepadType serializer_bouncepad_type_from_str(const char *s);
