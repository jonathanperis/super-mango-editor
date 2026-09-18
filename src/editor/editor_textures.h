/*
 * editor_textures.h — Editor entity texture loading and cleanup helpers.
 */
#pragma once

#include "editor.h"  /* EditorState, EntityTextures */

/* Load all editor preview textures from assets/. Missing textures are non-fatal. */
void editor_textures_load(EditorState *es);

/* Unload every raylib texture slot owned by EditorState::textures. */
void editor_textures_cleanup(EditorState *es);
