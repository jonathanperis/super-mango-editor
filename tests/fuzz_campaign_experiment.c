/*
 * fuzz_campaign_experiment.c — Fuzz the campaign manifest loader and the
 * experiment capture decoder.
 *
 * Both read TOML that someone else may have written: a campaign manifest
 * (levels/campaigns/main.toml, or one opened in the editor's Campaign
 * view) and a capture exported with F9 and replayed with --experiment.
 * Each input is tried as both, as the profile harness tries its input as a
 * profile and as a ghost. Rejecting the input is the expected outcome; the
 * harness fails only on a crash, a sanitizer report, or an accepted result
 * that breaks its own limits.
 *
 * The campaign loader looks for each listed level on disk. A mutated path
 * almost never names a real file, so nearly every entry is "level file not
 * found", which is the path a damaged manifest takes in the game too.
 *
 * See fuzz_replay_main.c for the standalone driver.  POSIX only.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_experiment.h"
#include "levels/campaign_catalog.h"
#include "levels/level_ref.h"

/*
 * The level hash a capture must name to be decoded. The seed captures in
 * tests/fuzz/corpus/experiment use the same value, so the decoder gets
 * past the hash check and into the frame rows.
 */
#define FUZZ_LEVEL_HASH UINT64_C(0x0123456789abcdef)

static void broken(const char *what)
{
    fprintf(stderr, "fuzz_campaign_experiment: %s\n", what);
    abort();
}

static void check_campaign(const uint8_t *data, size_t size)
{
    CampaignCatalog catalog = {0};

    if (campaign_catalog_load_text((const char *)data, size, "fuzz input", &catalog) != 0) {
        if (catalog.levels != NULL || catalog.count != 0)
            broken("a refused manifest left a catalog behind");
        return;
    }
    /* Accepted: within the limit, at least one playable level, and every
     * entry a valid level reference. */
    if (catalog.count == 0 || catalog.count > CAMPAIGN_MAX_LEVELS)
        broken("an accepted manifest has an impossible level count");
    if (campaign_first_available(&catalog) < 0)
        broken("an accepted manifest has no playable level");
    for (size_t i = 0; i < catalog.count; i++) {
        const char *path = catalog.levels[i].path;
        if (!level_ref_valid(path, strlen(path)))
            broken("an accepted manifest lists an invalid level reference");
    }
    campaign_catalog_cleanup(&catalog);
}

static void check_experiment(const uint8_t *data, size_t size)
{
    GameExperiment tape;

    if (game_experiment_decode((const char *)data, size, "fuzz input",
                               FUZZ_LEVEL_HASH, &tape) != 0) {
        if (tape.frames != NULL || tape.count != 0)
            broken("a refused capture left frames behind");
        return;
    }
    if (tape.count <= 0 || tape.count > EXPERIMENT_MAX_FRAMES || !tape.frames)
        broken("an accepted capture has an impossible frame count");
    for (int i = 0; i < tape.count; i++) {
        if (tape.frames[i].input > 63) broken("an accepted frame has unknown input bits");
        for (int j = 0; j < INSPECTOR_PHYSICS_COUNT; j++) {
            float value = tape.frames[i].physics[j];
            /* `!(a && b)` also catches NaN, which fails every comparison. */
            if (!(value >= 0.0f && value <= (float)MAX_LEVEL_MOTION))
                broken("an accepted frame has a physics value out of range");
        }
    }
    free(tape.frames);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    check_campaign(data, size);
    check_experiment(data, size);
    return 0;
}
