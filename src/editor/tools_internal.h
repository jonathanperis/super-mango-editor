/*
 * tools_internal.h — Helpers shared by the tools_*.c files.
 *
 * Not the public tool API (that is tools.h). Drag, place, nudge and
 * delete call across those files; the functions here are that seam.
 */
#pragma once

#include "tools.h"

int tools_can_hit_test(EditorState *es);
int delete_entity(EditorState *es, EntityType type, int index);
float clampf(float v, float lo, float hi);
float clamp_rail_t(const LevelDef *level, int rail_index, float t);
int get_entity_anchor(const LevelDef *level, EntityType type, int index,
                      float *x, float *y);
int place_entity(EditorState *es, float world_x, float world_y);
void begin_paint(EditorState *es, float world_x, float world_y);
void paint_step(EditorState *es, float world_x, float world_y);
void delete_at(EditorState *es, float world_x, float world_y);
