/*
 * editor_layout.c — Editor layout measurement helpers.
 */

#include "editor_layout.h"

#include "properties.h"  /* g_plx_open/g_fg_open/g_fog_open/g_phys_open */

enum {
    CFG_H_HEADER          = 28,
    CFG_H_MARGIN_TOP      = 8,
    CFG_H_VALIDATION_BASE = 24,
    CFG_H_VALIDATION_ROW  = 18,
    CFG_H_RECENT_HEADER   = 20,
    CFG_H_RECENT_ROW      = 18,
    CFG_H_METADATA        = 72,
    CFG_H_SCREENS         = 24,
    CFG_H_NEXT_PHASE      = 24,
    CFG_H_MUSIC_LABEL     = 24,
    CFG_H_MUSIC_DROPDOWN  = 22,
    CFG_H_MUSIC_VOLUME    = 24,
    CFG_H_FLOOR_TILE      = 30,
    CFG_H_HEARTS_SPACER   = 6,
    CFG_H_HEARTS_ROW      = 22,
    CFG_H_SCORE_ROW       = 24,
    CFG_H_PHYS_HEADER     = 24,
    CFG_H_BG_HEADER       = 24,
    CFG_H_FG_HEADER       = 24,
    CFG_H_FOG_HEADER      = 24,
    CFG_H_MARGIN_BOTTOM   = 10,
    CFG_H_LAYER_ROW       = 20,
    CFG_H_LAYER_ACTIONS   = 24,
    CFG_H_PHYS_ROWS       = 6 * 22
};

int editor_config_total_height(const EditorState *es)
{
    int validation_h = CFG_H_VALIDATION_BASE
                     + es->validation_report.message_count * CFG_H_VALIDATION_ROW;
    int recent_h = es->recent_file_count > 0
                 ? CFG_H_RECENT_HEADER + es->recent_file_count * CFG_H_RECENT_ROW
                 : 0;
    int total = CFG_H_HEADER + validation_h + recent_h + CFG_H_MARGIN_TOP
              + CFG_H_METADATA + CFG_H_SCREENS + CFG_H_NEXT_PHASE
              + CFG_H_MUSIC_LABEL + CFG_H_MUSIC_DROPDOWN + CFG_H_MUSIC_VOLUME
              + CFG_H_FLOOR_TILE + CFG_H_HEARTS_SPACER + CFG_H_HEARTS_ROW
              + CFG_H_SCORE_ROW + CFG_H_PHYS_HEADER + CFG_H_BG_HEADER
              + CFG_H_FG_HEADER + CFG_H_FOG_HEADER + CFG_H_MARGIN_BOTTOM;

    if (g_plx_open)
        total += es->level.background_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_fg_open)
        total += es->level.foreground_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_fog_open)
        total += es->level.fog_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_phys_open)
        total += CFG_H_PHYS_ROWS;

    return total;
}
