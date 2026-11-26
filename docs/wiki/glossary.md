# Glossary

<a id="home"></a>

---

The words this manual and the code use in a specific sense, in alphabetical
order. Each entry says where to read the real thing.

| Term | Meaning | Where in the code |
|------|---------|-------------------|
| `AppSession` | The application runtime. It owns the one window, the audio device, the start menu or the running game, the shared assets and asset cache, the campaign catalog and the player profile, and turns each screen's route into the next screen. | `src/core/app_session.h`, `src/core/app_session.c`; [Architecture](../architecture/#app-session-and-game-frame) |
| Asset cache | Files loaded once and shared by counting who holds them (a level's images and music, the player sheet, the HUD icon and font). An entry nobody holds stays loaded until `asset_cache_trim` runs after the next level loads. | `src/shared/asset_cache.h`; [Architecture](../architecture/#the-asset-cache-files-that-outlive-a-game) |
| Borrowed pointer | A pointer to something another object owns and frees. The holder may read it while it lives but never frees it. The code says so in a comment, for example `cache` in `GameState` ("borrowed from AppSession"). | `src/game.h` (`GameState`, `GameScreen.profile`) |
| Campaign manifest | The ordered list of levels the start menu offers, `levels/campaigns/main.toml`. Each listed level's `next_phase` must point at the next entry. | `src/levels/campaign_catalog.c`; [Level Design](../level-design/) |
| Checkpoint | An authored respawn marker, a `[[checkpoints]]` record. Crossing it moves the respawn point. A level without any falls back to automatic screen-boundary respawns. | `src/core/game_checkpoint.c`; [Level Design](../level-design/) |
| Content hash | A hash of a level file's exact bytes (`source_level_hash`). A Continue point or ghost saved for one version of a level is refused by an edited one. | `src/game.h` (`GameWorld.source_level_hash`), `src/core/game_profile.h` |
| Continue point | The one saved place per profile where **Continue** (or `--continue`) picks a level up again: respawn point, score, lives, collected coins and timer, bound to the level's content hash. | `GameResume` in `src/core/game_profile.h`; `src/core/game_resume.c` |
| Experiment capture | A recording made in the debug inspector: the input and physics tuning in effect for each fixed step, from a restart of the level (F8) until you export it (F9). Loading it replays the same run. | `src/core/game_experiment.h`; [Sandbox School](../learning-path/) |
| Fixed step | One update of the simulation, always `GAME_FIXED_STEP` (1/60 s) long. Real frame time feeds an accumulator that runs as many fixed steps as are due, at most `GAME_MAX_STEPS_PER_FRAME`. Drawing happens once per frame, independently. | `src/core/game_timing.h`; [Architecture](../architecture/#fixed-time-step) |
| `GameState` parts | One game screen, in three by-value parts: `world` (`GameWorld`, the level being played), `screen` (`GameScreen`: HUD, overlays, routes, timing, input, replay, debug) and `assets` (`GameAssets`, the shared sprites and sounds), plus the borrowed `cache`. | `src/game.h`; [Architecture](../architecture/#gamestate-struct) |
| Ghost | A translucent replay of your best finished run of a level, drawn while you race it. It stores Mango's position and sprite frame for each fixed step, not inputs. | `src/core/game_ghost.h`, `src/core/game_ghost_file.c` |
| Lab | Two related meanings. The eight **labs** of [Sandbox School](../learning-path/) are lessons: read, change, observe. The six **lab levels** in `levels/labs/` are small standalone levels, each isolating one mechanism; together they are the [Mechanics Museum](../mechanics-museum/). Lab levels are opened with `--level` and are not part of the campaign. | `levels/labs/`; [Mechanics Museum](../mechanics-museum/) |
| `LevelDef` | The in-memory form of one level: every placement, layer and setting a TOML file can hold, in fixed-size arrays. The game and the editor both load into it and save from it. | `src/levels/level.h`; [Level Design](../level-design/) |
| Logical pixel | One pixel of the 400 × 300 canvas the game draws on (`GAME_W` × `GAME_H`). The canvas is scaled up to the window, 2× at the default 800 × 600, so every position and size in the code and the TOML files is in logical pixels. | `src/game_constants.h`; [Architecture](../architecture/#coordinate-system) |
| Memory backend | `RAYLIB_PLATFORM=memory`: raylib built with a software framebuffer and no audio device, for tests on machines without a display. Test-only. | `Makefile`; [Build System](../build-system/#builder-and-build-modes) |
| Phase, `next_phase` | A level in a chain of levels. `next_phase`, inside `[last_star]`, names the level that **Next Level** opens after this one is completed; the final level leaves it out. | `LevelDef.next_phase` in `src/levels/level.h`; [Level Design](../level-design/) |
| Render layer | One row of `s_render_layers`, drawn back to front. The manual's render order table lists the same rows. | `src/render/game_render.c`; [Architecture](../architecture/#render-order-back-to-front) |
| Route | A request to leave the current screen (Next Level, Replay, Level Select, Exit). A game sets `GameRoute`; the session reads it after the frame and opens the next screen. | `GameRoute` in `src/game.h`, `AppRoute` in `src/core/app_session.h` |
| Schema table | `ROOT_FIELDS`: every key a level file may contain, with its type, its default and the `LevelDef` member that holds it. The loader and the writer both walk it. | `src/shared/serializer_schema.c`; [C in This Codebase](../c-concepts/) |
| Smoke test | A short run of the real game or editor (`--smoke-test-frames`) that checks it starts, plays and closes cleanly. A scripted smoke test also replays recorded input (`--replay`). | `src/input/game_replay.c`; [Testing](../testing/) |
| Update step | One row of `s_update_steps`: one kind of thing updated during a fixed step, in a fixed order. | `src/core/game_update.c`; [Architecture](../architecture/#update-order-one-fixed-step) |
