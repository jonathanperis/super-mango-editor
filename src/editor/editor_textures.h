/*
 * editor_textures.h — Editor entity texture loading and cleanup helpers.
 */
#pragma once

#include "editor.h"  /* EditorState, EntityTextures */

/* Load all editor preview textures from assets/. Missing textures are non-fatal. */
void editor_textures_load(EditorState *es);

/* Unload every raylib texture slot owned by EditorState::textures. */
void editor_textures_cleanup(EditorState *es);

/*
 * editor_platform_tile_texture — The image a platform draws with.
 *
 * tile_path is the platform's own field: "" means the default grass
 * tile; any other path is loaded the first time a platform names it and
 * kept (up to EDITOR_TILE_CACHE_SIZE different files), exactly as the game
 * loads it, so the canvas shows the platform as it will look in play.
 * A file that does not load is remembered too and falls back to the
 * default grass, instead of being tried again on every frame.
 */
Texture2D *editor_platform_tile_texture(EditorState *es, const char *tile_path);

/* The asset file a texture slot was loaded from, by its offset in
 * EntityTextures (EditorEntityPreview.texture), or NULL.  The palette reads
 * the image itself to trim its thumbnails. */
const char *editor_texture_path(size_t texture_offset);
