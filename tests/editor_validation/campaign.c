/* Included by tests/editor_validation_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* A minimal valid level named `name` whose next_phase is `next` ("" none). */
static int write_campaign_level(const char *path, const char *name, const char *next)
{
    char text[512];
    if (next[0])
        snprintf(text, sizeof(text),
                 "format_version = 1\nname = \"%s\"\nscreen_count = 4\n\n"
                 "[last_star]\nx = 300.0\ny = 200.0\nnext_phase = \"%s\"\n", name, next);
    else
        snprintf(text, sizeof(text),
                 "format_version = 1\nname = \"%s\"\nscreen_count = 4\n\n"
                 "[last_star]\nx = 300.0\ny = 200.0\n", name);
    return write_text_file(path, text);
}

/*
 * A scratch game folder for the Campaign view: levels/a, b, c listed in
 * that order and chained, levels/d not listed, other/e outside levels/.
 * The view names levels relative to the working folder, as the game does,
 * so the tests run inside it and never touch the real levels/.
 */
#define CAMPAIGN_ROOT TEST_OUT "campaign-root"

/* TEST_OUT follows OUTDIR, which may be relative (as in CI) or absolute;
 * a full path is built from the working folder only in the first case. */
static int test_path_is_absolute(const char *path)
{
    if (path[0] == '/' || path[0] == '\\') return 1;
    return path[0] != '\0' && path[1] == ':';   /* a Windows drive letter */
}
static int make_campaign_root(void)
{
    test_make_folder(CAMPAIGN_ROOT);
    test_make_folder(CAMPAIGN_ROOT "/levels");
    test_make_folder(CAMPAIGN_ROOT "/levels/campaigns");
    test_make_folder(CAMPAIGN_ROOT "/other");
    return write_campaign_level(CAMPAIGN_ROOT "/levels/a.toml", "Alpha", "levels/b.toml") ||
           write_campaign_level(CAMPAIGN_ROOT "/levels/b.toml", "Bravo", "levels/c.toml") ||
           write_campaign_level(CAMPAIGN_ROOT "/levels/c.toml", "Charlie", "") ||
           write_campaign_level(CAMPAIGN_ROOT "/levels/d.toml", "Delta", "") ||
           write_campaign_level(CAMPAIGN_ROOT "/other/e.toml", "Echo", "") ||
           write_text_file(CAMPAIGN_ROOT "/levels/campaigns/main.toml",
                           "format_version = 1\nlevels = [\n    \"levels/a.toml\",\n"
                           "    \"levels/b.toml\",\n    \"levels/c.toml\",\n]\n")
           ? -1 : 0;
}

/* The order of the paths in a catalog, joined by spaces, for comparisons. */
static const char *campaign_order(const CampaignCatalog *catalog)
{
    static char order[256];
    size_t used = 0;
    order[0] = '\0';
    for (size_t i = 0; catalog && i < catalog->count; i++) {
        const char *name = strrchr(catalog->levels[i].path, '/');
        int n = snprintf(order + used, sizeof(order) - used, "%s%s",
                         used ? " " : "", name ? name + 1 : catalog->levels[i].path);
        if (n < 0 || (size_t)n >= sizeof(order) - used) break;
        used += (size_t)n;
    }
    return order;
}

/*
 * The Campaign view edits levels/campaigns/main.toml with the game's own
 * rules: a reorder that breaks the next_phase chain is shown (and blocks
 * Save) until "Link in order" rewrites the links; Add takes only files
 * directly in levels/ and not already listed; Save writes the changed
 * levels and the manifest, which the game's loader then accepts in the new
 * order; a file changed behind the view's back is not overwritten; the
 * level open in the editor is not rewritten under unsaved edits.
 */
static int campaign_view_edits_and_saves_the_manifest(void)
{
    EditorState es;
    char cwd[4096];
    char absolute_d[4096];
    CampaignCatalog reloaded = {0};
    LevelDef level;
    const CampaignCatalog *view;
    int result = 1;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    if (make_campaign_root() != 0 || test_working_folder(cwd, sizeof(cwd)) != 0 ||
        test_change_folder(CAMPAIGN_ROOT) != 0) return 1;

    if (expect_int("opens", editor_campaign_open(&es, NULL), 0) != 0) goto done;
    view = editor_campaign_entries(&es);
    if (expect_int("three entries", (int)view->count, 3) != 0 ||
        expect_int("consistent", editor_campaign_problem(&es)[0] == '\0', 1) != 0 ||
        expect_int("all playable", view->levels[0].available && view->levels[1].available &&
                   view->levels[2].available, 1) != 0 ||
        expect_int("named by the level", strcmp(view->levels[1].display_name, "Bravo"), 0) != 0 ||
        expect_int("nothing unsaved", editor_campaign_unsaved(&es), 0) != 0)
        goto done;

    /* Moving c above b breaks every link: the game would play nothing. */
    if (editor_campaign_move(&es, 2, -1) != 0 ||
        expect_int("reordered", strcmp(campaign_order(view), "a.toml c.toml b.toml"), 0) != 0 ||
        expect_int("a out of order", view->levels[0].available, 0) != 0 ||
        expect_int("final has a link", strcmp(view->levels[2].problem,
                                              "final level has a next_phase"), 0) != 0 ||
        expect_int("unplayable reported", strstr(editor_campaign_problem(&es),
                                                 "playable") != NULL, 1) != 0 ||
        expect_int("save refused", editor_campaign_save(&es), -1) != 0 ||
        expect_prefix("refusal status", es.status_message, "Campaign not saved") != 0)
        goto done;

    /* Link in order rewrites all three, and the campaign is whole again. */
    if (expect_int("linked", editor_campaign_link_in_order(&es), 3) != 0 ||
        expect_int("whole again", editor_campaign_problem(&es)[0] == '\0', 1) != 0 ||
        expect_int("a leads to c", strcmp(view->levels[0].level.next_phase, "levels/c.toml"), 0) != 0 ||
        expect_int("b is last", view->levels[2].level.next_phase[0] == '\0', 1) != 0 ||
        expect_int("unsaved", editor_campaign_unsaved(&es), 1) != 0)
        goto done;

    /* Add: only files directly in levels/, each once. */
    if (expect_int("outside levels/", editor_campaign_add(&es, "other/e.toml"), -1) != 0 ||
        expect_int("already listed", editor_campaign_add(&es, "levels/a.toml"), -1) != 0 ||
        expect_int("missing file", editor_campaign_add(&es, "levels/zz.toml"), -1) != 0 ||
        snprintf(absolute_d, sizeof(absolute_d), "%s%s%s/levels/d.toml",
                 test_path_is_absolute(CAMPAIGN_ROOT) ? "" : cwd,
                 test_path_is_absolute(CAMPAIGN_ROOT) ? "" : "/",
                 CAMPAIGN_ROOT) >= (int)sizeof(absolute_d) ||
        expect_int("picked by its full path", editor_campaign_add(&es, absolute_d), 0) != 0 ||
        expect_int("listed as levels/d.toml", strcmp(view->levels[3].path, "levels/d.toml"), 0) != 0 ||
        expect_int("b now needs a link", view->levels[2].available, 0) != 0)
        goto done;
    (void)editor_campaign_link_in_order(&es);
    if (expect_int("remove a", editor_campaign_remove(&es, 0), 0) != 0 ||
        expect_int("order c b d", strcmp(campaign_order(view), "c.toml b.toml d.toml"), 0) != 0 ||
        expect_int("removed file kept", serializer_file_exists_utf8("levels/a.toml"), 1) != 0)
        goto done;

    /* The level open in the editor is never rewritten under unsaved edits. */
    snprintf(es.file_path, sizeof(es.file_path), "levels/b.toml");
    es.modified = 1;
    if (expect_int("open document protected", editor_campaign_save(&es), -1) != 0 ||
        expect_int("protected status", strstr(es.status_message, "levels/b.toml") != NULL, 1) != 0)
        goto done;
    es.file_path[0] = '\0';
    es.modified = 0;

    /* Save: the changed levels, then the manifest; the game's own loader
     * accepts the result in the new order. */
    if (expect_int("saved", editor_campaign_save(&es), 0) != 0 ||
        expect_int("nothing left unsaved", editor_campaign_unsaved(&es), 0) != 0 ||
        campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) != 0 ||
        expect_int("game reads the new order",
                   strcmp(campaign_order(&reloaded), "c.toml b.toml d.toml"), 0) != 0 ||
        expect_int("every entry playable", reloaded.levels[0].available &&
                   reloaded.levels[1].available && reloaded.levels[2].available, 1) != 0)
        goto done;
    level_def_init_defaults(&level);
    if (level_load_toml("levels/c.toml", &level) != 0 ||
        expect_int("c's link written", strcmp(level.next_phase, "levels/b.toml"), 0) != 0 ||
        expect_int("c keeps its name", strcmp(level.name, "Charlie"), 0) != 0)
        goto done;

    /* A level changed behind the view's back is not overwritten. */
    if (editor_campaign_move(&es, 2, -1) != 0 ||        /* c d b */
        editor_campaign_link_in_order(&es) == 0 ||
        write_campaign_level("levels/d.toml", "Delta by hand", "") != 0 ||
        expect_int("conflict refused", editor_campaign_save(&es), -1) != 0 ||
        expect_int("conflict status", strstr(es.status_message, "changed on disk") != NULL, 1) != 0)
        goto done;
    level_def_init_defaults(&level);
    if (level_load_toml("levels/d.toml", &level) != 0 ||
        expect_int("hand edit kept", strcmp(level.name, "Delta by hand"), 0) != 0)
        goto done;
    /* ...and nothing else was written either: c still leads to b. */
    level_def_init_defaults(&level);
    if (level_load_toml("levels/c.toml", &level) != 0 ||
        expect_int("no half chain", strcmp(level.next_phase, "levels/b.toml"), 0) != 0)
        goto done;

    /* Close asks once about unsaved edits, then discards them. */
    if (expect_int("first close refused", editor_campaign_close(&es, 0), 0) != 0 ||
        expect_int("second close", editor_campaign_close(&es, 0), 1) != 0 ||
        expect_int("view gone", editor_campaign_entries(&es) == NULL, 1) != 0)
        goto done;
    result = 0;

done:
    campaign_catalog_cleanup(&reloaded);
    editor_campaign_free(&es);
    if (test_change_folder(cwd) != 0) result = 1;
    return result;
}

/* 1 when a save of `path` left its first-choice temporary file behind. */
static int temp_left_for(const char *path)
{
    char temp[4096];
    return serializer_make_temp_path(path, temp, sizeof(temp)) == 0 &&
           serializer_file_exists_utf8(temp);
}

/* The next_phase levels/<file> holds on disk now ("?" when it won't load). */
static const char *next_on_disk(const char *path)
{
    static LevelDef level;
    level_def_init_defaults(&level);
    return level_load_toml(path, &level) == 0 ? level.next_phase : "?";
}

/*
 * Campaign Save writes every changed file to a temporary file first and
 * only then replaces them, one quick rename each.  A failure while writing
 * changes nothing on disk and leaves no temporary file; a rename failing
 * half-way says how many files were written and which one failed, and the
 * next Save finishes the job.
 */
static int campaign_save_writes_everything_before_replacing(void)
{
    static const char *const files[] = {
        "levels/a.toml", "levels/b.toml", "levels/c.toml", CAMPAIGN_MANIFEST_PATH
    };
    EditorState es;
    char cwd[4096];
    CampaignCatalog reloaded = {0};
    int result = 1;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    if (make_campaign_root() != 0 || test_working_folder(cwd, sizeof(cwd)) != 0 ||
        test_change_folder(CAMPAIGN_ROOT) != 0) return 1;

    /* a c b: three levels to relink, and the manifest: four files. */
    if (expect_int("opens", editor_campaign_open(&es, NULL), 0) != 0 ||
        editor_campaign_move(&es, 2, -1) != 0 ||
        expect_int("linked", editor_campaign_link_in_order(&es), 3) != 0)
        goto done;

    /* Writing the first temporary file fails: nothing changes at all. */
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_WRITE);
    if (expect_int("write failure refused", editor_campaign_save(&es), -1) != 0 ||
        expect_prefix("write failure status", es.status_message,
                      "Campaign not saved: writing levels/a.toml failed; no file was changed") != 0 ||
        expect_int("a untouched", strcmp(next_on_disk("levels/a.toml"), "levels/b.toml"), 0) != 0 ||
        campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) != 0 ||
        expect_int("manifest untouched", strcmp(campaign_order(&reloaded),
                                                "a.toml b.toml c.toml"), 0) != 0)
        goto done;
    campaign_catalog_cleanup(&reloaded);
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
        if (expect_int("no temporary file after a write failure", temp_left_for(files[i]), 0) != 0)
            goto done;

    /* Every file is written, then the third rename (levels/b.toml) fails:
     * a and c are in place, b and the manifest are not, and the status
     * says so; their temporary files are gone. */
    serializer_test_set_failure_after(SERIALIZER_TEST_FAILURE_REPLACE, 2);
    if (expect_int("rename failure", editor_campaign_save(&es), -1) != 0 ||
        expect_prefix("partial status", es.status_message,
                      "Campaign partly saved: 2 of 4 files written; writing levels/b.toml failed") != 0 ||
        expect_int("a written", strcmp(next_on_disk("levels/a.toml"), "levels/c.toml"), 0) != 0 ||
        expect_int("c written", strcmp(next_on_disk("levels/c.toml"), "levels/b.toml"), 0) != 0 ||
        expect_int("b not written", strcmp(next_on_disk("levels/b.toml"), "levels/c.toml"), 0) != 0 ||
        expect_int("still unsaved", editor_campaign_unsaved(&es), 1) != 0)
        goto done;
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
        if (expect_int("no temporary file after a rename failure", temp_left_for(files[i]), 0) != 0)
            goto done;

    /* Saving again writes only what is left: b and the manifest. */
    if (expect_int("second save", editor_campaign_save(&es), 0) != 0 ||
        expect_prefix("second save status", es.status_message,
                      "Campaign saved: 3 levels, 1 level file updated") != 0 ||
        campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &reloaded) != 0 ||
        expect_int("game reads a c b", strcmp(campaign_order(&reloaded), "a.toml c.toml b.toml"), 0) != 0 ||
        expect_int("every entry playable", reloaded.levels[0].available &&
                   reloaded.levels[1].available && reloaded.levels[2].available, 1) != 0)
        goto done;

    /* Windows can move a file aside and then fail to move the new one in
     * (the seam acts it out): that temporary file is the only complete
     * copy, so it stays, and the status bar names it. */
    if (editor_campaign_move(&es, 2, -1) != 0 ||          /* a b c */
        expect_int("relinked", editor_campaign_link_in_order(&es), 3) != 0)
        goto done;
    serializer_test_set_failure_after(SERIALIZER_TEST_FAILURE_REPLACE_STRANDED, 1);
    if (expect_int("stranded", editor_campaign_save(&es), -1) != 0 ||
        expect_prefix("stranded status", es.status_message,
                      "Campaign partly saved (1 of 4 files written). Save incomplete: "
                      "the new levels/b.toml is safe in ") != 0 ||
        expect_int("names the copy", strstr(es.status_message, "b.toml.tmp.") != NULL, 1) != 0 ||
        expect_int("kept copy stays", temp_left_for("levels/b.toml"), 1) != 0 ||
        expect_int("c temporary gone", temp_left_for("levels/c.toml"), 0) != 0 ||
        expect_int("manifest temporary gone", temp_left_for(CAMPAIGN_MANIFEST_PATH), 0) != 0)
        goto done;
    {
        char kept[4096];
        if (serializer_make_temp_path("levels/b.toml", kept, sizeof(kept)) == 0)
            serializer_remove_temp(kept);
    }
    result = 0;

done:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    campaign_catalog_cleanup(&reloaded);
    editor_campaign_free(&es);
    if (test_change_folder(cwd) != 0) result = 1;
    return result;
}

/*
 * The manifest is fingerprinted when the view opens, like each level file:
 * a manifest another program rewrote since is never overwritten, whether
 * the change came before Save or during it (the seam), and Save then
 * writes nothing at all.
 */
static int campaign_save_keeps_a_manifest_changed_by_hand(void)
{
    static const char hand_edit[] =
        "format_version = 1\nlevels = [\n    \"levels/a.toml\",\n"
        "    \"levels/b.toml\",\n    \"levels/c.toml\",\n    \"levels/d.toml\",\n]\n";
    EditorState es;
    char cwd[4096];
    CampaignCatalog reloaded = {0};
    int result = 1;

    memset(&es, 0, sizeof(es));
    editor_level_init_defaults(&es.level);
    if (make_campaign_root() != 0 || test_working_folder(cwd, sizeof(cwd)) != 0 ||
        test_change_folder(CAMPAIGN_ROOT) != 0) return 1;

    /* Remove c and relink: b changes, and so does the manifest. */
    if (expect_int("opens", editor_campaign_open(&es, NULL), 0) != 0 ||
        editor_campaign_remove(&es, 2) != 0 ||
        expect_int("linked", editor_campaign_link_in_order(&es), 1) != 0)
        goto done;

    /* Changed during the save, after the temporary files were written. */
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_SOURCE_CHANGED);
    if (expect_int("late change refused", editor_campaign_save(&es), -1) != 0 ||
        expect_int("late change status", strstr(es.status_message, "changed on disk") != NULL, 1) != 0 ||
        expect_int("b untouched", strcmp(next_on_disk("levels/b.toml"), "levels/c.toml"), 0) != 0 ||
        expect_int("no b temporary", temp_left_for("levels/b.toml"), 0) != 0 ||
        expect_int("no manifest temporary", temp_left_for(CAMPAIGN_MANIFEST_PATH), 0) != 0)
        goto done;

    /* Changed by hand before Save: the manifest names the hand edit. */
    if (write_text_file(CAMPAIGN_MANIFEST_PATH, hand_edit) != 0 ||
        expect_int("hand edit refused", editor_campaign_save(&es), -1) != 0 ||
        expect_prefix("hand edit status", es.status_message,
                      "Campaign not saved: " CAMPAIGN_MANIFEST_PATH " changed on disk") != 0 ||
        expect_int("b still untouched", strcmp(next_on_disk("levels/b.toml"), "levels/c.toml"), 0) != 0 ||
        campaign_catalog_read(CAMPAIGN_MANIFEST_PATH, &reloaded) != 0 ||
        expect_int("hand edit kept", strcmp(campaign_order(&reloaded),
                                            "a.toml b.toml c.toml d.toml"), 0) != 0)
        goto done;
    result = 0;

done:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    campaign_catalog_cleanup(&reloaded);
    editor_campaign_free(&es);
    if (test_change_folder(cwd) != 0) result = 1;
    return result;
}
