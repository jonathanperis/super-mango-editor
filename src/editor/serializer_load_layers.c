/*
 * serializer_load_layers.c — TOML layer parsing helpers.
 */

#include <stddef.h> /* size_t */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy */

#include "serializer_load_layers.h"
#include "serializer_parse.h"
#include "../game.h" /* MAX_BACKGROUND_LAYERS, MAX_FOG_TEXTURES */

static void copy_layer_path(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int serializer_load_layers(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Background layers */
    {
        toml_datum_t plx = toml_get(top, "background_layers");
        if (plx.type == TOML_ARRAY) {
            int n = plx.u.arr.size;
            if (n > MAX_BACKGROUND_LAYERS) {
                fprintf(stderr, "serializer: background_layers array has %d items "
                        "(max %d)\n", n, MAX_BACKGROUND_LAYERS);
                return -1;
            }
            def->background_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = plx.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                copy_layer_path(def->background_layers[i].path,
                                sizeof(def->background_layers[i].path), p);
                def->background_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Foreground layers */
    {
        toml_datum_t fg = toml_get(top, "foreground_layers");
        if (fg.type == TOML_ARRAY) {
            int n = fg.u.arr.size;
            if (n > MAX_BACKGROUND_LAYERS) {
                fprintf(stderr, "serializer: foreground_layers array has %d items "
                        "(max %d)\n", n, MAX_BACKGROUND_LAYERS);
                return -1;
            }
            def->foreground_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fg.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                copy_layer_path(def->foreground_layers[i].path,
                                sizeof(def->foreground_layers[i].path), p);
                def->foreground_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Fog layers */
    {
        toml_datum_t fl = toml_get(top, "fog_layers");
        if (fl.type == TOML_ARRAY) {
            int n = fl.u.arr.size;
            if (n > MAX_FOG_TEXTURES) {
                fprintf(stderr, "serializer: fog_layers array has %d items "
                        "(max %d)\n", n, MAX_FOG_TEXTURES);
                return -1;
            }
            def->fog_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fl.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                copy_layer_path(def->fog_layers[i].path,
                                sizeof(def->fog_layers[i].path), p);
                def->fog_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    return 0;
}
