/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Click down the palette column. A click that switches to the Place tool
 * hit an entry; any other click hit a header (which opens or closes its
 * category), so the next probe skips past it. Every probe starts from the
 * Select tool so each pick is visible.
 */
static int scan_palette(EditorState *es, int top, int bottom, int picked[ENT_COUNT])
{
    int found = 0;
    for (int y = top; y < bottom; ) {
        push_key(KEY_ONE, 0);
        click_frame(es, PANEL_LABEL_X, y);
        if (es->tool == TOOL_PLACE) {
            if (es->palette_type < 0 || es->palette_type >= ENT_COUNT) return -1;
            if (!picked[es->palette_type]) found++;
            picked[es->palette_type] = 1;
            y += 9;
        } else {
            y += 30;
        }
    }
    return found;
}

/* UIScroll: trackpad fractions add up to whole units, the offset stays
 * within the content, and reveal moves just far enough. */
static int ui_scroll_keeps_fractions_and_stays_in_range(void)
{
    int failed = 0;
    UIScroll rows = {0};
    for (int i = 0; i < 3; i++) CHECK(ui_scroll_wheel(&rows, -0.25f, 1.0f) == 0);
    CHECK(rows.offset == 0);
    CHECK(ui_scroll_wheel(&rows, -0.25f, 1.0f) == 1 && rows.offset == 1);
    (void)ui_scroll_wheel(&rows, 5.0f, 1.0f);           /* far past the top */
    CHECK(rows.offset == 0);

    UIScroll pixels = {0};
    CHECK(ui_scroll_wheel(&pixels, -0.1f, 20.0f) == 2 && pixels.offset == 2);
    pixels.offset = 500;
    ui_scroll_clamp(&pixels, 300, 100);                 /* 200 px can scroll */
    CHECK(pixels.offset == 200);
    ui_scroll_clamp(&pixels, 50, 100);                  /* everything fits */
    CHECK(pixels.offset == 0);

    ui_scroll_reveal(&pixels, 300, 320, 100, 200, 8);   /* below the view */
    CHECK(pixels.offset == 128);
    ui_scroll_reveal(&pixels, 90, 110, 100, 200, 8);    /* above the view */
    CHECK(pixels.offset == 110);
done:
    return failed;
}

/* A scrollbar: a track click moves one view, dragging the thumb scrolls in
 * proportion, its clicks are used up, and with nothing to scroll it is
 * absent and lets clicks through. */
static int ui_scrollbar_pages_drags_and_uses_its_clicks(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);

    /* view 100 of 400 on a 100 px track: a 25 px thumb at the top. */
    ui.mouse_x = 2; ui.mouse_y = 80; ui.mouse_clicked = 1;
    CHECK(ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400) == 1);
    CHECK(scroll.offset == 100 && ui.mouse_clicked == 0 && ui.scroll_drag == NULL);

    /* The thumb now spans y 25..50: take it at 30 and drag to the bottom. */
    ui.mouse_y = 30; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll && ui.scroll_grab == 5 && ui.mouse_clicked == 0);
    ui.mouse_clicked = 0; ui.mouse_y = 200;
    CHECK(ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400) == 1);
    CHECK(scroll.offset == 300);
    ui.mouse_y = 5 + 37;              /* thumb top at 37 of 75: half way */
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(scroll.offset == 148);
    ui.mouse_down = 0;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == NULL && scroll.offset == 148);

    /* Everything fits: no bar, the click is left for the content. */
    UIScroll fits = {0};
    ui.mouse_y = 50; ui.mouse_clicked = 1;
    CHECK(ui_scrollbar_input(&ui, &fits, 0, 0, 5, 100, 100, 80) == 0);
    CHECK(ui.mouse_clicked == 1 && fits.offset == 0);
done:
    return failed;
}

/*
 * A thumb drag ends at the next frame once its scrollbar is no longer run
 * (F5 hands the window to the playtest overlay, and the panel's release
 * is never seen) or the button was up, so the up-down cursor goes back.
 */
static int scrollbar_drag_ends_with_its_bar(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    ui_init(&ui, NULL);

    /* Take the 25 px thumb at the top of a 100 px track. */
    ui_begin_frame(&ui);
    ui.mouse_x = 2; ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_RESIZE_NS);
    /* Held, and the bar runs again: the drag goes on. */
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    /* Still held, but the panel is not drawn: the next frame lets go. */
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    ui_begin_frame(&ui);
    ui.mouse_down = 1;
    CHECK(ui.scroll_drag == NULL);
    ui_apply_cursor(&ui);
    CHECK(ui.cursor_shown == MOUSE_CURSOR_DEFAULT);

    /* Released in a frame the bar did not see the release in. */
    ui_begin_frame(&ui);
    ui.mouse_y = 10; ui.mouse_clicked = 1; ui.mouse_down = 1;
    (void)ui_scrollbar_input(&ui, &scroll, 0, 0, 5, 100, 100, 400);
    CHECK(ui.scroll_drag == &scroll);
    ui.mouse_down = 0;
    ui_begin_frame(&ui);
    CHECK(ui.scroll_drag == NULL);
done:
    return failed;
}

/* A scrolled panel still draws its fields scrolled out of view; those must
 * not show the I-beam over whatever is under the pointer there. */
static int scrolled_out_field_wants_no_ibeam(void)
{
    int failed = 0;
    UIState ui;
    UIScroll scroll = {0};
    int value = 5;
    ui_init(&ui, NULL);
    ui_begin_frame(&ui);
    ui.mouse_x = 20; ui.mouse_y = 150;   /* below the 100 px view */
    int top = ui_scroll_begin(&ui, &scroll, 0, 0, 200, 100, 400);
    (void)ui_int_field(&ui, 1, 10, top + 145, 80, &value);
    ui_scroll_end(&ui);
    CHECK(ui.cursor_wish != MOUSE_CURSOR_IBEAM);
    /* The same field inside the view does want it. */
    ui_begin_frame(&ui);
    ui.mouse_x = 20; ui.mouse_y = 50;
    top = ui_scroll_begin(&ui, &scroll, 0, 0, 200, 100, 400);
    (void)ui_int_field(&ui, 1, 10, top + 45, 80, &value);
    ui_scroll_end(&ui);
    CHECK(ui.cursor_wish == MOUSE_CURSOR_IBEAM);
done:
    return failed;
}

/* WCAG relative luminance of an sRGB colour. */
static double luminance(Color c)
{
    double channel[3] = {c.r / 255.0, c.g / 255.0, c.b / 255.0};
    for (int i = 0; i < 3; i++)
        channel[i] = channel[i] <= 0.03928 ? channel[i] / 12.92
                                           : pow((channel[i] + 0.055) / 1.055, 2.4);
    return 0.2126 * channel[0] + 0.7152 * channel[1] + 0.0722 * channel[2];
}

static double contrast(Color a, Color b)
{
    double la = luminance(a), lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

static int same_color(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* ------------------------------------------------------------------ */
/* Canvas                                                              */
/* ------------------------------------------------------------------ */

/*
 * The floor is drawn only where the canvas shows it, so check the one
 * thing culling could break: the strip still reaches both canvas edges,
 * scrolled anywhere and at any zoom, with gaps left open.  This renders
 * the canvas alone into the window and reads it back.
 */
static int floor_reaches_both_canvas_edges_at_any_scroll(void)
{
    int failed = 0;
    EditorState es;
    Image shot = {0};
    const Color backdrop = {255, 0, 255, 255};
    const float zooms[] = {1.0f, 2.0f, 3.0f, 5.0f};  /* the zoom menu */
    CHECK(open_editor(&es, NULL) == 0);
    /* A new level has no sky image, so the canvas starts as flat sky blue:
     * a stretch the floor failed to cover shows that blue. */
    const Color sky = {0x87, 0xCE, 0xEB, 255};
    const Color sky_swapped = {0xEB, 0xCE, 0x87, 255};   /* Memory reads BGRA */
    CHECK(es.textures.sky == NULL);
    es.level.screen_count = 12;
    es.level.floor_gaps[0] = 400;
    es.level.floor_gaps[1] = 2000;
    es.level.floor_gap_count = 2;
    const float world_w = (float)(es.level.screen_count * GAME_W);
    for (size_t z = 0; z < sizeof(zooms) / sizeof(zooms[0]); z++) {
        for (int step = 0; step <= 8; step++) {
            es.camera.zoom = zooms[z];
            /* Start, end and odd fractional positions in between. */
            es.camera.x = step * (world_w / 8.0f) + (step % 3) * 7.3f;
            /* Centre the dirt row vertically so it is on screen at every
             * zoom (at 3x and 5x the world is taller than the canvas). */
            es.camera.y = (float)(FLOOR_Y + TILE_SIZE / 2) - (CANVAS_H / 2) / es.camera.zoom;
            canvas_clamp_camera(&es);
            /* Drawn straight to the window: the Memory backend reads back
             * the framebuffer but not textures. */
            BeginDrawing();
            ClearBackground(backdrop);
            canvas_render(&es);
            rlDrawRenderBatchActive();
            shot = LoadImageFromScreen();
            EndDrawing();
            CHECK(IsImageValid(shot) && shot.width == EDITOR_W && shot.height == EDITOR_H);
#ifdef MANGO_MEMORY_TESTS
            /* Memory readback is bottom-origin (and BGRA: the magenta
             * backdrop does not notice, the sky is checked both ways). */
            ImageFlipVertical(&shot);
#endif
            /* Middle of the dirt row, in screen pixels. */
            int sy = (int)((FLOOR_Y + TILE_SIZE / 2 - es.camera.y) * es.camera.zoom) + TOOLBAR_H;
            CHECK(sy >= TOOLBAR_H && sy < TOOLBAR_H + CANVAS_H);
            int checked = 0;
            for (int sx = 0; sx < CANVAS_W; sx++) {
                float wx = sx / es.camera.zoom + es.camera.x;
                int near_gap = 0;
                for (int g = 0; g < es.level.floor_gap_count; g++)
                    near_gap |= wx > es.level.floor_gaps[g] - 2.0f &&
                                wx < es.level.floor_gaps[g] + FLOOR_GAP_W + 2.0f;
                if (near_gap || wx >= world_w - 1.0f) continue;
                Color c = GetImageColor(shot, sx, sy);
                checked++;
                if (same_color(c, backdrop) || same_color(c, sky) || same_color(c, sky_swapped)) {
                    fprintf(stderr, "floor missing at zoom %.1f camera %.1f sx %d\n",
                            es.camera.zoom, es.camera.x, sx);
                    CHECK(0);
                }
            }
            /* Most of the row is floor (only the gaps and the world's end
             * are skipped), so the check above really ran. */
            CHECK(checked > CANVAS_W / 2);
            UnloadImage(shot);
            shot = (Image){0};
        }
    }
done:
    if (shot.data) UnloadImage(shot);
    close_editor(&es);
    return failed;
}

/* Rest the pointer at (x, y) until time_ms, and run that frame. */
static void hover_frame(EditorState *es, int x, int y, uint32_t time_ms)
{
    es->ui.time_ms = time_ms;
    ui_frame(es, x, y);
}

/*
 * Toolbar tooltips: shown once the pointer has rested on a button for
 * UI_TOOLTIP_DELAY_MS, restarted by moving to another button, hidden by a
 * click until the pointer leaves, and kept inside the window at its edge.
 */
static int toolbar_tooltips_wait_then_name_the_shortcut(void)
{
    int failed = 0;
    EditorState es;
    const int select_x = 30, grid_x = 4 + 3 * (64 + 4) + 16 + 20, bar_y = 15;
    CHECK(open_editor(&es, NULL) == 0);

    hover_frame(&es, select_x, bar_y, 1000);
    CHECK(es.ui.tooltip_text && strstr(es.ui.tooltip_text, "(1)") != NULL);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, select_x + 3, bar_y, 1000 + UI_TOOLTIP_DELAY_MS - 1);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, select_x, bar_y, 1000 + UI_TOOLTIP_DELAY_MS);
    CHECK(es.ui.tooltip_shown);
    /* Below and right of the pointer, inside the window. */
    CHECK(es.ui.tip_box[0] > select_x && es.ui.tip_box[1] > bar_y);

    /* Another button starts its own wait. */
    hover_frame(&es, grid_x, bar_y, 2000);
    CHECK(es.ui.tooltip_text && strcmp(es.ui.tooltip_text, "Show the grid (G)") == 0);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 2600);
    CHECK(es.ui.tooltip_shown);

    /* A click hides it while the pointer stays... */
    CHECK(es.show_grid);
    push_click(grid_x, bar_y);
    hover_frame(&es, grid_x, bar_y, 2700);
    CHECK(!es.show_grid);
    hover_frame(&es, grid_x, bar_y, 5000);
    CHECK(!es.ui.tooltip_shown);
    /* ...and leaving the button resets it. */
    hover_frame(&es, NEUTRAL_X, NEUTRAL_Y, 5100);
    CHECK(es.ui.tooltip_text == NULL && !es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 5200);
    CHECK(!es.ui.tooltip_shown);
    hover_frame(&es, grid_x, bar_y, 5700);
    CHECK(es.ui.tooltip_shown);

    /* Play sits at the right edge: its tip moves left to stay inside. */
    hover_frame(&es, EDITOR_W - 10, bar_y, 6000);
    CHECK(es.ui.tooltip_text && strstr(es.ui.tooltip_text, "F5") != NULL);
    hover_frame(&es, EDITOR_W - 10, bar_y, 7000);
    CHECK(es.ui.tooltip_shown);
    CHECK(es.ui.tip_box[0] + es.ui.tip_box[2] <= EDITOR_W);
    CHECK(es.ui.tip_box[0] < EDITOR_W - 10);
done:
    close_editor(&es);
    return failed;
}

/*
 * A status message too long for the room left of the window's edge ends in
 * "..." there, and resting the pointer on it shows the whole message; a
 * short one needs no tooltip.
 */
static int long_status_messages_show_whole_in_a_tooltip(void)
{
    int failed = 0;
    EditorState es;
    const int bar_y = EDITOR_H - STATUS_H + 10;
    CHECK(open_editor(&es, NULL) == 0);

    editor_set_status(&es, "Saved");
    hover_frame(&es, 930, bar_y, 1000);
    hover_frame(&es, 930, bar_y, 1000 + UI_TOOLTIP_DELAY_MS);
    CHECK(!es.ui.tooltip_shown);

    editor_set_status(&es, "Campaign view: Esc returns to the level, and every edit here "
                           "is kept in its own history until you save the campaign");
    hover_frame(&es, 930, bar_y, 2000);
    hover_frame(&es, 930, bar_y, 2000 + UI_TOOLTIP_DELAY_MS);
    CHECK(es.ui.tooltip_shown && es.ui.tooltip_text == es.status_message);
    CHECK(es.ui.tip_box[0] + es.ui.tip_box[2] <= EDITOR_W);

    /* A message longer than any window wraps into a box that fits. */
    {
        char longest[300];
        memset(longest, 'a', sizeof(longest) - 1);
        longest[sizeof(longest) - 1] = '\0';
        memcpy(longest, "Missing file assets/", 20);
        editor_set_status(&es, "%s", longest);
        hover_frame(&es, 930, bar_y, 3000);
        hover_frame(&es, 930, bar_y, 3000 + UI_TOOLTIP_DELAY_MS);
        CHECK(es.ui.tooltip_shown);
        CHECK(es.ui.tip_box[0] >= 0 && es.ui.tip_box[0] + es.ui.tip_box[2] <= EDITOR_W);
        CHECK(es.ui.tip_box[2] <= 480 + 12 && es.ui.tip_box[3] > 30);
    }

    /* A cut label outside the active clip (a row scrolled out of a panel's
     * view) gives no tooltip there. */
    {
        static const char long_text[] = "A message far too long for the eighty pixels it is given here";
        ui_begin_frame(&es.ui);
        es.ui.mouse_x = 20;
        es.ui.mouse_y = 105;
        ui_clip_begin(&es.ui, 0, 0, 200, 50);
        CHECK(ui_label_fit(&es.ui, 10, 100, 80, long_text, WHITE) == 1);
        ui_clip_end(&es.ui);
        CHECK(es.ui.tooltip_text != long_text);
        CHECK(ui_label_fit(&es.ui, 10, 100, 80, long_text, WHITE) == 1);
        CHECK(es.ui.tooltip_text == long_text);
    }
done:
    close_editor(&es);
    return failed;
}

/* The default style is the editor look the UI_* colours describe, with no
 * control outlines; the high-contrast style keeps every text colour at the
 * WCAG AAA ratio (7:1) or more on every face it is drawn on. */
/* A shown editor slows to 10 frames a second while another window has
 * focus and returns to 60 with it; a hidden (smoke) window is left alone. */
static int unfocused_editor_slows_its_frames(void)
{
    int failed = 0;
    CHECK(editor_frame_rate_for(0, 1, 0) == EDITOR_FPS_FOCUSED && EDITOR_FPS_FOCUSED == 60);
    CHECK(editor_frame_rate_for(0, 1, 1) == EDITOR_FPS_IDLE && EDITOR_FPS_IDLE == 30);
    CHECK(editor_frame_rate_for(0, 0, 0) == EDITOR_FPS_UNFOCUSED && EDITOR_FPS_UNFOCUSED == 10);
    CHECK(editor_frame_rate_for(0, 0, 1) == EDITOR_FPS_UNFOCUSED);
    CHECK(editor_frame_rate_for(1, 0, 0) == 0 && editor_frame_rate_for(1, 1, 1) == 0);
done:
    return failed;
}

static int ui_styles_keep_the_default_look_and_high_contrast(void)
{
    int failed = 0;
    const UIStyle *d = &UI_STYLE_DEFAULT, *h = &UI_STYLE_HIGH_CONTRAST;
    CHECK(same_color(d->color[UI_COLOR_PANEL], UI_BG) && same_color(d->color[UI_COLOR_TITLE], UI_TITLE_BG));
    CHECK(same_color(d->color[UI_COLOR_INPUT], UI_INPUT_BG) && same_color(d->color[UI_COLOR_BUTTON], UI_BTN));
    CHECK(same_color(d->color[UI_COLOR_HOT], UI_BTN_HOT) && same_color(d->color[UI_COLOR_SELECTED], UI_BTN_ACTIVE));
    CHECK(same_color(d->color[UI_COLOR_TEXT], UI_TEXT) && same_color(d->color[UI_COLOR_TEXT_DIM], UI_TEXT_DIM));
    CHECK(same_color(d->color[UI_COLOR_ACCENT], UI_ACCENT) && d->border_width == 0);
    /* Dim text stays readable on the faces it is drawn on (WCAG AA). */
    CHECK(contrast(d->color[UI_COLOR_TEXT_DIM], d->color[UI_COLOR_PANEL]) >= 4.5);
    CHECK(contrast(d->color[UI_COLOR_TEXT_DIM], d->color[UI_COLOR_TITLE]) >= 4.5);
    CHECK(contrast(d->color[UI_COLOR_TEXT_DIM], d->color[UI_COLOR_INPUT]) >= 4.5);
    CHECK(h->border_width > 0);
    const UIColorRole faces[] = {UI_COLOR_PANEL, UI_COLOR_TITLE, UI_COLOR_INPUT,
                                 UI_COLOR_BUTTON, UI_COLOR_HOT, UI_COLOR_SELECTED};
    const UIColorRole inks[] = {UI_COLOR_TEXT, UI_COLOR_TEXT_DIM};
    for (size_t f = 0; f < sizeof(faces) / sizeof(faces[0]); f++)
        for (size_t i = 0; i < sizeof(inks) / sizeof(inks[0]); i++)
            CHECK(contrast(h->color[inks[i]], h->color[faces[f]]) >= 7.0);
    const UIColorRole on_panels[] = {UI_COLOR_ACCENT, UI_COLOR_WARNING, UI_COLOR_ERROR, UI_COLOR_BORDER};
    for (size_t i = 0; i < sizeof(on_panels) / sizeof(on_panels[0]); i++) {
        CHECK(contrast(h->color[on_panels[i]], h->color[UI_COLOR_PANEL]) >= 7.0);
        CHECK(contrast(h->color[on_panels[i]], h->color[UI_COLOR_TITLE]) >= 7.0);
    }
done:
    return failed;
}

/* The toolbar's Contrast button switches every widget to the high-contrast
 * style and back. */
static int toolbar_contrast_toggles_the_high_contrast_style(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(es.high_contrast == 0 && es.ui.style.border_width == 0);
    const int x = 520 + 42, y = 4 + 12;   /* centre of the Contrast button */
    ui_frame(&es, x, y);
    click_frame(&es, x, y);
    CHECK(es.high_contrast == 1);
    CHECK(es.ui.style.border_width == UI_STYLE_HIGH_CONTRAST.border_width);
    CHECK(same_color(ui_color(&es.ui, UI_COLOR_TEXT), UI_STYLE_HIGH_CONTRAST.color[UI_COLOR_TEXT]));
    ui_frame(&es, x, y);
    click_frame(&es, x, y);
    CHECK(es.high_contrast == 0 && es.ui.style.border_width == 0);
    CHECK(same_color(ui_color(&es.ui, UI_COLOR_TEXT), UI_TEXT));
done:
    close_editor(&es);
    return failed;
}

/*
 * The colour at (x, y) of the toolbar alone, drawn into the window with
 * the pointer parked in the status bar.  Memory readback is bottom-origin
 * and BGRA (see session_test.c); this puts it back to RGBA top-down.
 */
static Color toolbar_pixel(EditorState *es, int x, int y)
{
    Color c = {0};
    es->ui.mouse_x = NEUTRAL_X;
    es->ui.mouse_y = NEUTRAL_Y;
    BeginDrawing();
    ClearBackground(BLACK);
    editor_render_toolbar(es);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    if (!IsImageValid(shot)) return c;
#ifdef MANGO_MEMORY_TESTS
    c = GetImageColor(shot, x, shot.height - 1 - y);
    unsigned char red = c.r;
    c.r = c.b;
    c.b = red;
#else
    c = GetImageColor(shot, x, y);
#endif
    UnloadImage(shot);
    return c;
}

/*
 * The tools and the on/off buttons are toggles: a click switches them,
 * and the one that is on is drawn in the selected colour, outlined in the
 * accent colour under high contrast, instead of a bracketed label.
 */
static int toolbar_toggles_show_what_is_on(void)
{
    int failed = 0;
    EditorState es;
    const int by = 4 + 12, select_x = 4 + 32, place_x = 72 + 32, delete_x = 140 + 32;
    const int grid_x = 224 + 28, debug_x = 284 + 28;
    Color selected, button, accent;
    CHECK(open_editor(&es, NULL) == 0);
    selected = ui_color(&es.ui, UI_COLOR_SELECTED);
    button = ui_color(&es.ui, UI_COLOR_BUTTON);

    /* Select is the starting tool, and the grid starts shown. */
    CHECK(es.tool == TOOL_SELECT && es.show_grid && !es.debug_play);
    CHECK(same_color(toolbar_pixel(&es, select_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, place_x, by - 8), button));
    CHECK(same_color(toolbar_pixel(&es, grid_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 8), button));

    click_frame(&es, place_x, by);
    CHECK(es.tool == TOOL_PLACE);
    click_frame(&es, delete_x, by);
    CHECK(es.tool == TOOL_DELETE);
    CHECK(same_color(toolbar_pixel(&es, delete_x, by - 8), selected));
    CHECK(same_color(toolbar_pixel(&es, select_x, by - 8), button));
    click_frame(&es, select_x, by);
    CHECK(es.tool == TOOL_SELECT);

    click_frame(&es, grid_x, by);
    CHECK(!es.show_grid);
    CHECK(same_color(toolbar_pixel(&es, grid_x, by - 8), button));
    click_frame(&es, debug_x, by);
    CHECK(es.debug_play);
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 8), selected));

    /* High contrast outlines the lit toggle in the accent colour, and the
     * others in the plain border colour. */
    editor_set_high_contrast(&es, 1);
    accent = ui_color(&es.ui, UI_COLOR_ACCENT);
    CHECK(same_color(toolbar_pixel(&es, debug_x, 4), accent));
    CHECK(same_color(toolbar_pixel(&es, grid_x, 4), ui_color(&es.ui, UI_COLOR_BORDER)));
    CHECK(same_color(toolbar_pixel(&es, debug_x, by - 4),
                     ui_color(&es.ui, UI_COLOR_SELECTED)));
done:
    close_editor(&es);
    return failed;
}

/*
 * Every toolbar icon is drawn pixel for pixel from raygui's bitmap: a set
 * bit is the given colour and a clear bit leaves the background.
 */
static int raygui_icons_draw_their_bits(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    for (int icon = UI_ICON_NONE + 1; icon < UI_ICON_COUNT; icon++) {
        BeginDrawing();
        ClearBackground(BLACK);
        ui_draw_icon((UIIcon)icon, 3, 5, WHITE);
        rlDrawRenderBatchActive();
        Image shot = LoadImageFromScreen();
        EndDrawing();
        CHECK(IsImageValid(shot));
        int lit = 0, wrong = 0;
        for (int y = 0; y < UI_ICON_SIZE; y++) {
            for (int x = 0; x < UI_ICON_SIZE; x++) {
                int bit = (int)(raygui_icon_bits[icon][y / 2] >> (y % 2 * 16 + x) & 1u);
#ifdef MANGO_MEMORY_TESTS
                Color c = GetImageColor(shot, 3 + x, shot.height - 1 - (5 + y));
#else
                Color c = GetImageColor(shot, 3 + x, 5 + y);
#endif
                if (bit) lit++;
                if (c.r != (bit ? 255 : 0)) wrong++;
            }
        }
        UnloadImage(shot);
        CHECK(wrong == 0 && lit > 0);
    }
done:
    close_editor(&es);
    return failed;
}

/* Is path one of the choice list's paths? */
static int asset_offered(const AssetChoices *list, const char *path)
{
    for (int i = 0; i < list->count; i++)
        if (strcmp(list->paths[i], path) == 0) return 1;
    return 0;
}

/*
 * Every file in folder whose name ends in suffix is offered by one of the
 * lists a or b (b may be NULL); returns how many files it saw, or -1 when
 * one is missing.
 */
static int folder_files_offered(const char *folder, const char *suffix,
                                const AssetChoices *a, const AssetChoices *b)
{
    FilePathList files = LoadDirectoryFiles(folder);
    int seen = 0, missing = 0;
    size_t suffix_len = strlen(suffix);
    for (unsigned int i = 0; i < files.count; i++) {
        /* raylib joins folder and name with '\\' on Windows. */
        char path[512];
        snprintf(path, sizeof(path), "%s", files.paths[i]);
        for (char *c = path; *c; c++) if (*c == '\\') *c = '/';
        size_t len = strlen(path);
        if (len < suffix_len || strcmp(path + len - suffix_len, suffix) != 0) continue;
        seen++;
        if (!asset_offered(a, path) && !(b && asset_offered(b, path))) {
            fprintf(stderr, "editor_ui_test: no dropdown offers %s\n", path);
            missing = 1;
        }
    }
    UnloadDirectoryFiles(files);
    return missing ? -1 : seen;
}

/*
 * The asset dropdowns offer every file in their folders (the volcanic and
 * smoke backgrounds were once left out), every file they offer exists,
 * and a path none of them names shows its own file name.
 */
static int asset_dropdowns_offer_every_asset_file(void)
{
    int failed = 0;
    const AssetChoices *bg = editor_asset_choices(ASSET_CHOICES_BACKGROUNDS);
    const AssetChoices *fg = editor_asset_choices(ASSET_CHOICES_FOREGROUNDS);
    const AssetChoices *fog = editor_asset_choices(ASSET_CHOICES_FOG);
    const AssetChoices *floor = editor_asset_choices(ASSET_CHOICES_FLOOR_TILES);
    const AssetChoices *plat = editor_asset_choices(ASSET_CHOICES_PLATFORM_TILES);
    const AssetChoices *music = editor_asset_choices(ASSET_CHOICES_MUSIC);

    CHECK(folder_files_offered("assets/sprites/backgrounds", ".png", bg, NULL) == bg->count);
    CHECK(folder_files_offered("assets/sprites/foregrounds", ".png", fg, fog) > 0);
    CHECK(folder_files_offered("assets/sprites/levels", "_tileset.png", floor, NULL) == floor->count);
    /* The platform and music lists also have an empty "(default)"/"(none)". */
    CHECK(folder_files_offered("assets/sprites/levels", "_platform.png", plat, NULL) == plat->count - 1);
    CHECK(folder_files_offered("assets/sounds/levels", ".qoa", music, NULL) == music->count - 1);
    for (int k = 0; k < ASSET_CHOICES_COUNT; k++) {
        const AssetChoices *list = editor_asset_choices((AssetChoicesKind)k);
        for (int i = 0; i < list->count; i++)
            CHECK(list->paths[i][0] == '\0' || FileExists(list->paths[i]));
    }
    CHECK(editor_asset_choices(ASSET_CHOICES_COUNT) == NULL);

    CHECK(strcmp(editor_asset_unlisted_name("assets/sprites/backgrounds/old.png"), "old.png") == 0);
    CHECK(strcmp(editor_asset_unlisted_name("mine\\custom.png"), "custom.png") == 0);
    CHECK(strcmp(editor_asset_unlisted_name("plain.png"), "plain.png") == 0);
    CHECK(strcmp(editor_asset_unlisted_name(""), "---") == 0);

    /* The canvas has a texture for every platform tile it offers. */
    {
        EditorState es;
        CHECK(open_editor(&es, NULL) == 0);
        int loaded = es.textures.platform && es.textures.platform_stone &&
                     es.textures.platform_leaf && es.textures.platform_brick &&
                     es.textures.platform_grass_rock;
        close_editor(&es);
        CHECK(loaded && plat->count == 6);
    }
done:
    return failed;
}

/*
 * The side column follows the tool.  Selecting with Select folds the
 * palette to its title and gives the properties the rest of the column;
 * Place opens the palette again above a 200 px properties panel; a click
 * on the folded palette's title takes up Place.  Fields taller than the
 * panel scroll under the wheel, and a widget scrolled out of Level
 * Config's view takes no click meant for the panel drawn there.
 */
static int side_column_follows_the_tool(void)
{
    int failed = 0;
    EditorState es;
    EditorSideLayout layout;
    CHECK(open_editor(&es, "levels/02_lugio_02.toml") == 0);
    /* Long Level Config content: its later headers and lists lie below its
     * view, under the panels drawn there. */
    g_plx_open = g_fg_open = g_fog_open = 1;
    g_phys_open = 0;

    editor_side_panel_layout(&es, &layout);
    CHECK(layout.props_h == 0 && !layout.palette_folded && layout.palette_h > 200);

    CHECK(es.level.rail_count > 0);
    es.tool = TOOL_SELECT;
    editor_select_only(&es, ENT_RAIL, 0);
    es.panel_open = 1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    editor_side_panel_layout(&es, &layout);
    CHECK(layout.palette_folded && layout.palette_h == 28);
    CHECK(layout.props_y + layout.props_h == EDITOR_H - STATUS_H && layout.props_h >= 300);

    /* Under Level Config's view, the click reaches the rail's first field
     * (its layout list), not a layer list scrolled out of sight there. */
    int plx_open = g_plx_open, fg_open = g_fg_open, fog_open = g_fog_open, phys_open = g_phys_open;
    click_frame(&es, PANEL_FIELD_X, layout.props_y + 36);
    CHECK(es.ui.dropdown_open_id == (int)ENT_RAIL * 100 + 1);
    /* ...and no Level Config header scrolled out of sight toggles. */
    CHECK(g_plx_open == plx_open && g_fg_open == fg_open && g_fog_open == fog_open && g_phys_open == phys_open);
    click_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.ui.dropdown_open_id == 0);

    /* The folded title brings the palette back with the Place tool, even
     * when the designer had closed it. */
    es.palette_open = 0;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    click_frame(&es, PANEL_LABEL_X, layout.palette_y + 10);
    CHECK(es.tool == TOOL_PLACE && es.palette_open);
    editor_side_panel_layout(&es, &layout);
    CHECK(!layout.palette_folded && layout.props_h == 200);

    /* A float platform's fields need more than 200 px: the wheel scrolls
     * them. */
    CHECK(es.level.float_platform_count > 0);
    editor_select_only(&es, ENT_FLOAT_PLATFORM, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    editor_side_panel_layout(&es, &layout);
    CHECK(es.props_content_h > layout.props_h - 32);
    push_wheel(PANEL_FIELD_X, layout.props_y + 100, -3.0f, 0);
    ui_frame(&es, PANEL_FIELD_X, layout.props_y + 100);
    CHECK(es.props_scroll.offset > 0);
    /* Another selection starts at the top. */
    editor_select_only(&es, ENT_RAIL, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(es.props_scroll.offset == 0);
done:
    g_plx_open = g_fg_open = g_fog_open = 0;
    close_editor(&es);
    return failed;
}

/*
 * Placing names what a click adds in the status bar; a new type to place
 * opens its category; each row shows the entity's thumbnail.
 */
static int palette_shows_what_place_will_add(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    int cat = (int)editor_entity_category(ENT_CIRCULAR_SAW);
    CHECK(!palette_category_is_open(cat));
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_CIRCULAR_SAW;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(palette_category_is_open(cat));
    CHECK(strcmp(editor_entity_palette_name(ENT_CIRCULAR_SAW), "Circular Saw") == 0);
    /* Closed while the palette is closed, the category opens again when
     * the palette shows while placing the same type. */
    es.palette_open = 0;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    palette_set_category_open_for_test(cat, 0);
    es.palette_open = 1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(palette_category_is_open(cat));

    {
        char label[64];
        editor_status_tool_text(&es, label, sizeof(label));
        CHECK(strcmp(label, "Place: Circular Saw") == 0);
        es.tool = TOOL_SELECT;
        editor_status_tool_text(&es, label, sizeof(label));
        CHECK(strcmp(label, "Tool: Select") == 0);
    }
done:
    close_editor(&es);
    return failed;
}

/*
 * A mistake the canvas can draw past (a bad next_phase) leaves the level
 * on screen and editable: a click still places a coin.  An edit is only
 * refused when it adds an error or breaks what the canvas needs.
 */
static int a_drawable_mistake_keeps_the_canvas_editable(void)
{
    int failed = 0;
    EditorState es;
    char err[128];
    CHECK(open_editor(&es, NULL) == 0);
    snprintf(es.level.next_phase, sizeof(es.level.next_phase), "levels/../oops.toml");
    EditorIssueSnapshot issues;
    editor_level_issue_snapshot(&es.level, &issues);
    CHECK(issues.count == 1);

    es.palette_type = ENT_COIN;
    es.tool = TOOL_PLACE;
    ui_frame(&es, 300, 250);
    click_frame(&es, 300, 250);
    CHECK(es.level.coin_count == 1);
    CHECK(strstr(es.status_message, "Canvas paused") == NULL);

    /* The banner along the canvas's bottom takes a click to the problem
     * (the next_phase field) and places nothing. */
    {
        int placed = es.level.coin_count;
        CHECK(canvas_banner_contains(&es, 300, CANVAS_BANNER_Y + 5));
        CHECK(!canvas_banner_contains(&es, 300, CANVAS_BANNER_Y - 5));
        click_frame(&es, 300, CANVAS_BANNER_Y + 5);
        CHECK(es.level.coin_count == placed);
        CHECK(strstr(es.status_message, "next_phase") != NULL);
    }
    {
        EditorIssueSnapshot now;
        editor_level_issue_snapshot(&es.level, &now);
        CHECK(now.count == issues.count);
    }

    /* The same coin moved out of the world would be a second error, and
     * the refusal names it rather than the next_phase mistake. */
    CHECK(editor_edit_kept_level(&es.level, &issues, err, sizeof(err)) == 0);
    es.level.coins[0].x = -500.0f;
    CHECK(editor_edit_kept_level(&es.level, &issues, err, sizeof(err)) != 0);
    CHECK(strncmp(err, "coins[0]", 8) == 0);
    es.level.coins[0].x = 100.0f;
    /* Breaking what the canvas draws from is refused whatever came before. */
    es.level.coin_count = MAX_COINS + 1;
    CHECK(editor_edit_kept_level(&es.level, &issues, err, sizeof(err)) != 0);
    es.level.coin_count = 1;
done:
    close_editor(&es);
    return failed;
}

/* The colour at (x, y) of the canvas alone (see toolbar_pixel). */
static Color canvas_pixel(EditorState *es, int x, int y)
{
    Color c = {0};
    BeginDrawing();
    ClearBackground(BLACK);
    canvas_render(es);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    if (!IsImageValid(shot)) return c;
#ifdef MANGO_MEMORY_TESTS
    c = GetImageColor(shot, x, shot.height - 1 - y);
    unsigned char red = c.r;
    c.r = c.b;
    c.b = red;
#else
    c = GetImageColor(shot, x, y);
#endif
    UnloadImage(shot);
    return c;
}

/*
 * The canvas draws a level's background layers and, with Fog on, its fog,
 * as the game does; each layer image follows the level's list.  Outside
 * the level (below its height) the canvas is darkened.
 */
static int canvas_draws_the_level_layers_and_its_edges(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, "levels/01_lugio_01.toml") == 0);
    CHECK(es.level.background_layer_count > 2 && es.level.fog_layer_count > 0);
    CHECK(es.textures.sky != NULL);
    for (int i = 1; i < es.level.background_layer_count; i++) CHECK(es.preview_layers[i] != NULL);
    for (int i = 0; i < es.level.fog_layer_count; i++) CHECK(es.preview_fog[i] != NULL);
    /* Fewer layers drop the images they no longer name. */
    int last = es.level.background_layer_count - 1;
    es.level.background_layer_count = 1;
    es.level.fog_layer_count = 0;
    editor_sync_config_resources(&es);
    CHECK(es.preview_layers[last] == NULL && es.preview_fog[0] == NULL);
    close_editor(&es);

    /* A new level: flat sky above its floor line, darker below the level. */
    CHECK(open_editor(&es, NULL) == 0);
    es.camera.zoom = 1.0f;
    es.camera.x = es.camera.y = 0.0f;
    es.show_grid = 0;
    Color inside = canvas_pixel(&es, 5, TOOLBAR_H + 40);
    Color outside = canvas_pixel(&es, 5, TOOLBAR_H + GAME_H + 40);
    CHECK(inside.r == 0x87 && inside.g == 0xCE && inside.b == 0xEB);
    CHECK(outside.r < inside.r && outside.g < inside.g && outside.b < inside.b);

    /* The Fog toggle sits after Contrast. */
    CHECK(es.show_fog == 0);
    click_frame(&es, 608 + 28, 16);
    CHECK(es.show_fog == 1);
    click_frame(&es, 608 + 28, 16);
    CHECK(es.show_fog == 0);
done:
    close_editor(&es);
    return failed;
}

/*
 * Navigation: the middle button, or Space with the left button, drags the
 * view (and places nothing); F zooms to the whole level; Ctrl+A selects
 * every entity, up to a selection's size; Ctrl+X cuts.
 */
static int canvas_navigates_like_a_level_editor(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, "levels/01_lugio_01.toml") == 0);
    es.camera.zoom = 2.0f;
    es.camera.x = 200.0f;
    es.camera.y = 0.0f;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_MIDDLE, 0, 400, 300);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 300, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_MIDDLE, 0, 300, 300);
    ui_frame(&es, 300, 300);
    CHECK(es.camera.x == 250.0f && !es.panning);

    int coins = es.level.coin_count;
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    push_event(INPUT_KEY_DOWN, KEY_SPACE, 0, 0, 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 300, 300);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 400, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 400, 300);
    push_event(INPUT_KEY_UP, KEY_SPACE, 0, 0, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.x == 200.0f && es.level.coin_count == coins && !es.space_held);

    /* A left release does not end a middle-button pan. */
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_MIDDLE, 0, 400, 300);
    push_event(INPUT_KEY_DOWN, KEY_SPACE, 0, 0, 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 400, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 400, 300);
    push_event(INPUT_KEY_UP, KEY_SPACE, 0, 0, 0);
    ui_frame(&es, 400, 300);
    CHECK(es.panning && es.pan_button == MOUSE_BUTTON_MIDDLE);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_MIDDLE, 0, 400, 300);
    ui_frame(&es, 400, 300);
    CHECK(!es.panning);

    /* With the F1 list open, the releases still end Space and a pan. */
    push_event(INPUT_KEY_DOWN, KEY_SPACE, 0, 0, 0);
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_MIDDLE, 0, 400, 300);
    ui_frame(&es, 400, 300);
    CHECK(es.space_held && es.panning);
    es.shortcut_sheet_open = 1;
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_MIDDLE, 0, 400, 300);
    push_event(INPUT_KEY_UP, KEY_SPACE, 0, 0, 0);
    ui_frame(&es, 400, 300);
    CHECK(!es.space_held && !es.panning);
    es.shortcut_sheet_open = 0;

    /* Eight screens (3200 px) fit at a quarter. */
    key_frame(&es, KEY_F, 0);
    CHECK(es.camera.zoom == 0.25f && es.camera.x == 0.0f);
    CHECK(canvas_zoom_index(es.camera.zoom) == 0);
    CHECK(strcmp(es.status_message, "Zoom to fit: 1/4x") == 0);
    /* Forty screens fit only below every preset: the exact zoom; the
     * wheel then goes in to a quarter, and out stays put. */
    es.level.screen_count = 40;
    key_frame(&es, KEY_F, 0);
    CHECK(es.camera.zoom < 0.25f && 40.0f * GAME_W * es.camera.zoom <= (float)CANVAS_W);
    float fitted = es.camera.zoom;
    push_wheel(400, 300, -1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == fitted);
    push_wheel(400, 300, 1.0f, INPUT_CTRL);
    ui_frame(&es, 400, 300);
    CHECK(es.camera.zoom == 0.25f);
    es.level.screen_count = 8;

    es.tool = TOOL_SELECT;
    int total = editor_placed_entity_total(&es.level);
    CHECK(total > EDITOR_MAX_SELECTION);
    key_frame(&es, KEY_A, INPUT_CTRL);
    CHECK(editor_selection_count(&es) == EDITOR_MAX_SELECTION);
    CHECK(strstr(es.status_message, "Selected 64 of") != NULL);

    editor_select_only(&es, ENT_COIN, 0);
    coins = es.level.coin_count;
    key_frame(&es, KEY_X, INPUT_CTRL);
    CHECK(es.level.coin_count == coins - 1 && es.clipboard_count == 1);
    key_frame(&es, KEY_V, INPUT_CTRL);
    CHECK(es.level.coin_count == coins);
done:
    close_editor(&es);
    return failed;
}

/*
 * Property labels read as words ("Patrol left:", not "patrol_x0:") and
 * explain themselves: resting on one shows its meaning, its unit and the
 * TOML key.  The hearts field takes 0, which means the default.
 */
static int property_labels_explain_themselves(void)
{
    int failed = 0;
    EditorState es;
    const char *found = NULL;
    uint32_t t = 1000;
    CHECK(open_editor(&es, "levels/01_lugio_01.toml") == 0);
    CHECK(es.level.spider_count > 0);
    es.tool = TOOL_SELECT;
    editor_select_only(&es, ENT_SPIDER, 0);
    es.panel_open = 1;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    /* Scan down the label column for the patrol label's tooltip. */
    for (int y = TOOLBAR_H; y < EDITOR_H - STATUS_H && !found; y += 4) {
        hover_frame(&es, CANVAS_W + 20, y, t);
        t += UI_TOOLTIP_DELAY_MS;
        hover_frame(&es, CANVAS_W + 20, y, t);
        t += 10;
        if (es.ui.tooltip_shown && es.ui.tooltip_text &&
            strstr(es.ui.tooltip_text, "(patrol_x0)")) found = es.ui.tooltip_text;
    }
    CHECK(found && strstr(found, "Left end of the patrol, in pixels") == found);

    es.level.initial_hearts = 0;
    CHECK(level_is_valid(&es));
done:
    close_editor(&es);
    return failed;
}

/*
 * Dragging with the Place tool paints a row, one entity per step of its
 * own width, and the whole row is one undo step.  The player spawn (one
 * per level) moves instead and paints nothing.
 */
static int place_drag_paints_a_row(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);
    es.camera.zoom = 2.0f;
    es.camera.x = es.camera.y = 0.0f;
    es.tool = TOOL_PLACE;
    es.palette_type = ENT_COIN;
    int undo_top = es.undo->top;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 100, 300);
    for (int x = 110; x <= 420; x += 10) push_event(INPUT_MOUSE_MOVE, 0, 0, x, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 420, 300);
    ui_frame(&es, 420, 300);
    /* 320 canvas px at 2x is 160 world px: about ten 16 px coins. */
    CHECK(es.level.coin_count >= 8 && es.level.coin_count <= 11);
    CHECK(!es.painting && es.undo->top == undo_top + 1);
    CHECK(strstr(es.status_message, "Placed") != NULL);
    for (int i = 1; i < es.level.coin_count; i++)
        CHECK(es.level.coins[i].x - es.level.coins[i - 1].x >= 16.0f);
    key_frame(&es, KEY_Z, INPUT_CTRL);
    CHECK(es.level.coin_count == 0);

    /* Dragging on past the world's left edge stacks nothing there: the
     * clamp lands each further copy where the last one went. */
    es.camera.x = 0.0f;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 40, 300);
    for (int x = 30; x >= -400; x -= 10) push_event(INPUT_MOUSE_MOVE, 0, 0, x, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, -400, 300);
    ui_frame(&es, 100, 300);
    CHECK(es.level.coin_count <= 2);
    key_frame(&es, KEY_Z, INPUT_CTRL);

    es.palette_type = ENT_PLAYER_SPAWN;
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, 100, 300);
    push_event(INPUT_MOUSE_MOVE, 0, 0, 300, 300);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 300, 300);
    ui_frame(&es, 300, 300);
    CHECK(!es.painting && es.level.coin_count == 0);
done:
    close_editor(&es);
    return failed;
}

static int palette_clicks_choose_what_the_place_tool_adds(void)
{
    int failed = 0;
    EditorState es;
    int picked[ENT_COUNT] = {0};
    CHECK(open_editor(&es, NULL) == 0);

    /* Level Config is taller than its part of the column: a click low on
     * its scrollbar's track scrolls it a page and changes nothing else. */
    {
        uint64_t hash_before = doc_hash(&es);
        CHECK(es.config_open == 1 && es.config_scroll.offset == 0);
        const int bar_x = CANVAS_W + PANEL_W - 2, bar_y = TOOLBAR_H + 28 + 290;
        ui_frame(&es, bar_x, bar_y);
        click_frame(&es, bar_x, bar_y);
        CHECK(es.config_scroll.offset > 0 && es.config_open == 1);
        CHECK(doc_hash(&es) == hash_before && es.ui.active_id == 0);
        push_wheel(PANEL_LABEL_X, TOOLBAR_H + 100, 200.0f, 0);   /* back to the top */
        ui_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 100);
        CHECK(es.config_scroll.offset == 0);
    }

    /* Collapse Level Config by its header so the palette gets the column. */
    CHECK(es.config_open == 1);
    click_frame(&es, PANEL_LABEL_X, TOOLBAR_H + 8);
    CHECK(es.config_open == 0);
    const int column_bottom = EDITOR_H - STATUS_H;

    /* Find the palette title just below the folded config header: the
     * click that folds the palette away. Clicking it again unfolds it. */
    int title_y = -1;
    for (int y = TOOLBAR_H + 20; y < TOOLBAR_H + 120 && title_y < 0; y += 4) {
        click_frame(&es, PANEL_LABEL_X, y);
        if (!es.palette_open) title_y = y;
    }
    CHECK(title_y > 0 && es.config_open == 0);
    /* Nothing is under a folded palette. */
    push_key(KEY_ONE, 0);
    click_frame(&es, PANEL_LABEL_X, title_y + 60);
    CHECK(es.tool == TOOL_SELECT);
    click_frame(&es, PANEL_LABEL_X, title_y);
    CHECK(es.palette_open == 1);
    /* Rows start below the title, which is no taller than a header. */
    const int palette_top = title_y + 30;

    /* Pass 1 opens categories top-down; the list then outgrows the column,
     * so scroll to the end and pass again for the rest. */
    int found = scan_palette(&es, palette_top, column_bottom - 4, picked);
    CHECK(found >= 4);
    for (int pass = 0; pass < 3; pass++) {
        push_wheel(PANEL_LABEL_X, palette_top + 100, -40.0f, 0);
        ui_frame(&es, PANEL_LABEL_X, palette_top + 100);
        int more = scan_palette(&es, palette_top, column_bottom - 4, picked);
        CHECK(more >= 0);
        found += more;
    }
    push_wheel(PANEL_LABEL_X, palette_top + 100, 80.0f, 0);   /* back to the top */
    ui_frame(&es, PANEL_LABEL_X, palette_top + 100);
    CHECK(found >= 15);

    CHECK(!picked[ENT_PLAYER_SPAWN] || editor_entity_type_is_singleton(ENT_PLAYER_SPAWN));
    CHECK(doc_hash(&es) == es.saved_document_hash && es.undo->top == 0);

    /* Each picked type is what a canvas click then places, with its ghost
     * drawn under the cursor first. Some need something to attach to (a
     * rail, a gap); those refuse with a status message. Either way the
     * level stays valid and every placement is one undo step. Each new
     * array placement also adds one to the status bar's "Entities: N". */
    int placed = 0, refused = 0;
    for (int type = 0; type < ENT_COUNT; type++) {
        if (!picked[type]) continue;
        es.palette_type = (EntityType)type;
        es.tool = TOOL_PLACE;
        int x = 40 + (placed * 53) % (CANVAS_W - 120);
        int y = TOOLBAR_H + 120 + (placed % 3) * 60;
        ui_frame(&es, x, y);                       /* ghost preview */
        int count = editor_entity_count(&es.level, (EntityType)type);
        int total = editor_placed_entity_total(&es.level);
        int undo_top = es.undo->top;
        click_frame(&es, x, y);
        CHECK(level_is_valid(&es));
        if (editor_entity_count(&es.level, (EntityType)type) == count + 1 ||
            (editor_entity_type_is_singleton((EntityType)type) && es.undo->top == undo_top + 1)) {
            CHECK(es.undo->top == undo_top + 1);
            if (!editor_entity_type_is_singleton((EntityType)type))
                CHECK(editor_placed_entity_total(&es.level) == total + 1);
            placed++;
        } else {
            CHECK(es.undo->top == undo_top && es.status_message[0] != '\0');
            refused++;
        }
    }
    CHECK(placed >= 12 && placed + refused == found);

    /* Checkpoints count toward "Entities: N" too (the status bar used to
     * leave them out). One must sit right of the player start, which the
     * sweep above may not reach, so place one near the canvas's right edge. */
    {
        const int cx = CANVAS_W - 60, cy = TOOLBAR_H + 200;
        float wx, wy;
        canvas_screen_to_world(&es, cx, cy, &wx, &wy);
        CHECK(wx > es.level.player_start_x);
        int total = editor_placed_entity_total(&es.level);
        int count = es.level.checkpoint_count;
        es.palette_type = ENT_CHECKPOINT;
        es.tool = TOOL_PLACE;
        ui_frame(&es, cx, cy);
        click_frame(&es, cx, cy);
        CHECK(es.level.checkpoint_count == count + 1 && level_is_valid(&es));
        CHECK(editor_placed_entity_total(&es.level) == total + 1);
    }
    CHECK(es.modified == 1);

    /* Esc leaves the Place tool; the grid key toggles the overlay. */
    key_frame(&es, KEY_ESCAPE, 0);
    CHECK(es.tool == TOOL_SELECT);
    int grid = es.show_grid;
    key_frame(&es, KEY_G, 0);
    CHECK(es.show_grid == !grid);
    key_frame(&es, KEY_G, 0);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

