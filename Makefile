# ── Compiler + SDL2 detection ────────────────────────────────────────
# Both CC and SDL2CFG can be overridden on the command line:
#   make CC=gcc
#   make CC=gcc SDL2CFG=/custom/sdl2-config
#
# SDL2CFG always resolves from PATH so it works in any MSYS2/UCRT64
# shell (local or CI) without needing a hardcoded install location.
#
# CC defaults to the MSYS2 UCRT64 clang when running on Windows outside
# an MSYS2 shell (e.g. Git Bash), where the wrong clang from Git for
# Windows would otherwise be picked up first.  Inside an MSYS2 shell
# (CI: shell: msys2 {0}, or local: launched from MSYS2 terminal) the
# right gcc/clang is already first on PATH, so the ?= default is ignored
# and only the SDL2CFG path matters — which is just "sdl2-config".

SDL2CFG ?= sdl2-config
NODE ?= node

ifeq ($(OS),Windows_NT)
CC      ?= /c/msys64/ucrt64/bin/clang.exe
else
CC      ?= clang
endif

CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell $(SDL2CFG) --cflags)
# Tests provide their own main(), so stop SDL from remapping it to SDL_main on Windows.
TEST_CFLAGS = $(filter-out -Dmain=SDL_main,$(CFLAGS)) -DSDL_MAIN_HANDLED
LIBS    = $(shell $(SDL2CFG) --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
OUTDIR  = out
OBJDIR  = $(OUTDIR)/obj
DISTDIR = dist
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c) \
          $(wildcard $(SRCDIR)/collectibles/*.c) \
          $(wildcard $(SRCDIR)/collision/*.c) \
          $(wildcard $(SRCDIR)/core/*.c) \
          $(wildcard $(SRCDIR)/effects/*.c) \
          $(wildcard $(SRCDIR)/entities/*.c) \
          $(wildcard $(SRCDIR)/hazards/*.c) \
          $(wildcard $(SRCDIR)/input/*.c) \
          $(wildcard $(SRCDIR)/levels/*.c) \
          $(wildcard $(SRCDIR)/player/*.c) \
          $(wildcard $(SRCDIR)/render/*.c) \
          $(wildcard $(SRCDIR)/screens/*.c) \
          $(wildcard $(SRCDIR)/surfaces/*.c) \
          $(SRCDIR)/editor/serializer.c \
          $(SRCDIR)/editor/serializer_emit.c \
          $(SRCDIR)/editor/serializer_io.c \
          $(SRCDIR)/editor/serializer_load.c \
          $(SRCDIR)/editor/serializer_load_checkpoints.c \
          $(SRCDIR)/editor/serializer_load_climbables.c \
          $(SRCDIR)/editor/serializer_load_collectibles.c \
          $(SRCDIR)/editor/serializer_load_config.c \
          $(SRCDIR)/editor/serializer_load_enemies.c \
          $(SRCDIR)/editor/serializer_load_geometry.c \
          $(SRCDIR)/editor/serializer_load_hazards.c \
          $(SRCDIR)/editor/serializer_load_header.c \
          $(SRCDIR)/editor/serializer_load_layers.c \
          $(SRCDIR)/editor/serializer_load_surfaces.c \
          $(SRCDIR)/editor/serializer_parse.c \
          $(SRCDIR)/editor/serializer_save.c \
          $(SRCDIR)/editor/serializer_types.c \
          $(SRCDIR)/editor/ui.c \
          vendor/tomlc17/tomlc17.c
OBJS    = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    = $(OBJS:.o=.d)
SESSION_RUNTIME_OBJS = $(filter-out $(OBJDIR)/src/main.o,$(OBJS))

# ── Editor (standalone level editor) ─────────────────────────────────
EDITOR_DIR    = src/editor
VENDOR_DIR    = vendor/tomlc17
EDITOR_SRCS   = $(wildcard $(EDITOR_DIR)/*.c) $(VENDOR_DIR)/tomlc17.c \
                src/surfaces/rail.c src/levels/level_validate.c
EDITOR_OBJS   = $(patsubst %.c,$(OBJDIR)/%.o,$(EDITOR_SRCS))
EDITOR_DEPS   = $(EDITOR_OBJS:.o=.d)
EDITOR_TARGET = $(OUTDIR)/super-mango-editor
EDITOR_LIBS   = $(shell $(SDL2CFG) --libs) -lSDL2_image -lSDL2_ttf -lm
TEST_TARGETS  = $(OUTDIR)/level-serializer-test $(OUTDIR)/level-validate-test \
                 $(OUTDIR)/runtime-load-test \
                 $(OUTDIR)/rail-test $(OUTDIR)/entity-utils-test \
                 $(OUTDIR)/collision-test $(OUTDIR)/phase-transition-test \
                 $(OUTDIR)/editor-validation-test \
                 $(OUTDIR)/gameplay-damage-test $(OUTDIR)/gameplay-config-test \
                 $(OUTDIR)/gameplay-score-test \
                 $(OUTDIR)/game-overlay-test $(OUTDIR)/game-events-test \
                 $(OUTDIR)/session-test $(OUTDIR)/game-checkpoint-test
SMOKE_LEVELS  = $(wildcard levels/*.toml)
SMOKE_FRAMES  ?= 5
SMOKE_SEED    ?= 1
SMOKE_SEEDS   ?= 1 7 23
TEST_SERIALIZER_OBJ = $(OBJDIR)/tests/test-serializer.o
TEST_SERIALIZER_EMIT_OBJ = $(OBJDIR)/tests/test-serializer-emit.o
TEST_SERIALIZER_IO_OBJ = $(OBJDIR)/tests/test-serializer-io.o
TEST_SERIALIZER_LOAD_OBJ = $(OBJDIR)/tests/test-serializer-load.o
TEST_SERIALIZER_LOAD_CHECKPOINTS_OBJ = $(OBJDIR)/tests/test-serializer-load-checkpoints.o
TEST_SERIALIZER_LOAD_CLIMBABLES_OBJ = $(OBJDIR)/tests/test-serializer-load-climbables.o
TEST_SERIALIZER_LOAD_COLLECTIBLES_OBJ = $(OBJDIR)/tests/test-serializer-load-collectibles.o
TEST_SERIALIZER_LOAD_CONFIG_OBJ = $(OBJDIR)/tests/test-serializer-load-config.o
TEST_SERIALIZER_LOAD_ENEMIES_OBJ = $(OBJDIR)/tests/test-serializer-load-enemies.o
TEST_SERIALIZER_LOAD_GEOMETRY_OBJ = $(OBJDIR)/tests/test-serializer-load-geometry.o
TEST_SERIALIZER_LOAD_HAZARDS_OBJ = $(OBJDIR)/tests/test-serializer-load-hazards.o
TEST_SERIALIZER_LOAD_HEADER_OBJ = $(OBJDIR)/tests/test-serializer-load-header.o
TEST_SERIALIZER_LOAD_LAYERS_OBJ = $(OBJDIR)/tests/test-serializer-load-layers.o
TEST_SERIALIZER_LOAD_SURFACES_OBJ = $(OBJDIR)/tests/test-serializer-load-surfaces.o
TEST_SERIALIZER_PARSE_OBJ = $(OBJDIR)/tests/test-serializer-parse.o
TEST_SERIALIZER_SAVE_OBJ = $(OBJDIR)/tests/test-serializer-save.o
TEST_SERIALIZER_TYPES_OBJ = $(OBJDIR)/tests/test-serializer-types.o
TEST_VALIDATE_OBJ   = $(OBJDIR)/tests/test-level-validate.o
TEST_LEVEL_LOADER_OBJ = $(OBJDIR)/tests/test-level-loader.o
TEST_TOMLC_OBJ      = $(OBJDIR)/tests/test-tomlc17.o
TEST_RAIL_OBJ       = $(OBJDIR)/tests/test-rail.o
TEST_ENTITY_UTILS_OBJ = $(OBJDIR)/tests/test-entity-utils.o
TEST_SPIKE_BLOCK_OBJ = $(OBJDIR)/tests/test-spike-block.o
TEST_SPIKE_PLATFORM_OBJ = $(OBJDIR)/tests/test-spike-platform.o
TEST_FISH_OBJ      = $(OBJDIR)/tests/test-fish.o
TEST_CIRCULAR_SAW_OBJ = $(OBJDIR)/tests/test-circular-saw.o
TEST_COLLISION_DAMAGE_OBJ = $(OBJDIR)/tests/test-collision-damage.o
TEST_GAME_CAMERA_OBJ = $(OBJDIR)/tests/test-game-camera.o
TEST_GAME_SCORE_OBJ = $(OBJDIR)/tests/test-game-score.o
TEST_LEVEL_PHYSICS_OBJ = $(OBJDIR)/tests/test-level-physics.o
TEST_PLAYER_LIFECYCLE_OBJ = $(OBJDIR)/tests/test-player-lifecycle.o
TEST_FLOAT_PLATFORM_OBJ = $(OBJDIR)/tests/test-float-platform.o
TEST_BOUNCEPAD_OBJ = $(OBJDIR)/tests/test-bouncepad.o
TEST_PHASE_OBJ      = $(OBJDIR)/tests/test-phase-transition.o
TEST_GAME_OVERLAY_OBJ = $(OBJDIR)/tests/test-game-overlay.o
TEST_GAME_EVENTS_OBJ = $(OBJDIR)/tests/test-game-events.o
TEST_GAME_INPUT_OBJ = $(OBJDIR)/tests/test-game-input.o
TEST_BINDINGS_OBJ = $(OBJDIR)/tests/test-game-bindings.o
TEST_SETTINGS_OBJ = $(OBJDIR)/tests/test-settings-menu.o
TEST_GAME_TERMINAL_OBJ = $(OBJDIR)/src/core/game_terminal.o
TEST_GAME_CHECKPOINT_OBJ = $(OBJDIR)/tests/test-game-checkpoint.o
TEST_HUD_OBJ = $(OBJDIR)/tests/test-hud.o
TEST_EDITOR_VALIDATION_OBJ = $(OBJDIR)/tests/test-editor-validation.o
TEST_EDITOR_FILES_OBJ = $(OBJDIR)/tests/test-editor-files.o
TEST_EDITOR_SESSION_OBJ = $(OBJDIR)/tests/test-editor-session.o
TEST_EDITOR_UNDO_APPLY_OBJ = $(OBJDIR)/tests/test-editor-undo-apply.o
TEST_EDITOR_ENTITY_META_OBJ = $(OBJDIR)/tests/test-editor-entity-meta.o
TEST_EDITOR_UI_OBJ = $(OBJDIR)/tests/test-editor-ui.o
TEST_EDITOR_TOOLS_OBJ = $(OBJDIR)/tests/test-editor-tools.o
TEST_EDITOR_CLIPBOARD_OBJ = $(OBJDIR)/tests/test-editor-clipboard.o
TEST_EDITOR_EVENTS_OBJ = $(OBJDIR)/tests/test-editor-events.o
TEST_EDITOR_CANVAS_OBJ = $(OBJDIR)/tests/test-editor-canvas.o
TEST_EDITOR_PANELS_OBJ = $(OBJDIR)/tests/test-editor-panels.o
TEST_EDITOR_LAYOUT_OBJ = $(OBJDIR)/tests/test-editor-layout.o
TEST_EDITOR_PALETTE_OBJ = $(OBJDIR)/tests/test-editor-palette.o
TEST_EDITOR_PROPERTIES_OBJ = $(OBJDIR)/tests/test-editor-properties.o
TEST_EDITOR_PLAYTEST_OBJ = $(OBJDIR)/tests/test-editor-playtest.o
TEST_FILE_DIALOG_OBJ = $(OBJDIR)/tests/test-file-dialog.o
TEST_UNDO_OBJ      = $(OBJDIR)/tests/test-undo.o
TEST_LIBS           = $(shell $(SDL2CFG) --libs) -lm
TEST_DEPS           = $(wildcard $(OBJDIR)/tests/*.d)
# Test objects have explicit recipes; order their directory creation too,
# including when an individual test is built in parallel from a fresh OUTDIR.
TEST_OBJECTS := $(foreach name,$(filter %_OBJ,$(filter TEST_%,$(.VARIABLES))),$($(name)))
SANITIZE_CFLAGS     = -fsanitize=address,undefined -fno-omit-frame-pointer
SANITIZE_LDFLAGS    = -fsanitize=address,undefined

.PHONY: all clean run run-debug run-level run-level-debug web editor run-editor test validate-levels web-host-contract level-catalog overlay-snapshots docs-drift roadmap-quality smoke scripted-smoke sanitize sanitize-smoke dist-native dist-wasm compile-commands

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR) $(OBJDIR) $(OBJDIR)/tests

$(TARGET): $(OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

$(OBJDIR)/$(SRCDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

-include $(DEPS)

# ── Run targets (cross-platform) ─────────────────────────────────────
# Windows needs SDL DLL path in PATH; Linux/macOS use system libs.

ifeq ($(OS),Windows_NT)
SDL_DLL_PATH ?= /c/msys64/ucrt64/bin
RUN_PREFIX = PATH="$(SDL_DLL_PATH):$$PATH"
else
RUN_PREFIX =
endif

run: all
	$(RUN_PREFIX) "$(abspath $(TARGET))"

run-debug: all
	$(RUN_PREFIX) "$(abspath $(TARGET))" --debug

run-level: all
	$(RUN_PREFIX) "$(abspath $(TARGET))" --level "$(LEVEL)"

run-level-debug: all
	$(RUN_PREFIX) "$(abspath $(TARGET))" --debug --level "$(LEVEL)"

# ── Editor targets ───────────────────────────────────────────────────
editor: $(OUTDIR) $(EDITOR_TARGET)

$(EDITOR_TARGET): $(EDITOR_OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(EDITOR_LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

$(OBJDIR)/$(VENDOR_DIR)/%.o: $(VENDOR_DIR)/%.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

run-editor: editor
	$(RUN_PREFIX) "$(abspath $(EDITOR_TARGET))"

-include $(EDITOR_DEPS)
-include $(TEST_DEPS)

# ── Tests ────────────────────────────────────────────────────────────
test: $(OUTDIR) $(TEST_TARGETS) web-host-contract
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/level-serializer-test"
	python3 tests/validate_levels_test.py
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/level-validate-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/runtime-load-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/rail-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/entity-utils-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/collision-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/phase-transition-test"
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(RUN_PREFIX) "$(abspath $(OUTDIR))/editor-validation-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-damage-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-config-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-score-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-overlay-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-events-test"
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(RUN_PREFIX) "$(abspath $(OUTDIR))/session-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-checkpoint-test"

$(TEST_TARGETS): | $(OUTDIR)
$(TEST_OBJECTS): | $(OUTDIR)

validate-levels:
	python3 tools/validate_levels.py

web-host-contract:
	python3 tools/check_web_boot_contract.py
	$(NODE) tests/web_host_test.cjs
	python3 tests/package_release_test.py

# clangd / IDE compile database from the same flags make uses (sdl2-config).
# Output is machine-local — gitignored. Re-run after SDL install path changes.
compile-commands:
	CC="$(CC)" SDL2CFG="$(SDL2CFG)" python3 tools/gen_compile_commands.py

level-catalog:
	python3 tools/generate_level_catalog.py

overlay-snapshots:
	python3 tools/generate_overlay_snapshots.py

docs-drift:
	python3 tools/generate_level_catalog.py --check
	python3 tools/generate_overlay_snapshots.py --check
	python3 tools/check_docs_drift.py
	python3 tools/check_roadmap_quality.py

roadmap-quality:
	python3 tools/check_roadmap_quality.py

smoke: all editor
	@for level in $(SMOKE_LEVELS); do \
		echo "smoke: $$level"; \
		SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(RUN_PREFIX) "$(abspath $(TARGET))" --level "$$level" --smoke-test-frames $(SMOKE_FRAMES) --seed $(SMOKE_SEED) || exit 1; \
	done
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(RUN_PREFIX) "$(abspath $(EDITOR_TARGET))" --smoke-test

scripted-smoke: all editor
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy python3 tools/run_scripted_smoke.py --binary $(TARGET) --editor $(EDITOR_TARGET) --frames $(SMOKE_FRAMES) --seeds $(SMOKE_SEEDS)

sanitize:
	$(MAKE) all editor test OUTDIR="$(OUTDIR)-sanitize" \
		CFLAGS="$(CFLAGS) $(SANITIZE_CFLAGS)" \
		TEST_CFLAGS="$(TEST_CFLAGS) $(SANITIZE_CFLAGS)" \
		LIBS="$(LIBS) $(SANITIZE_LDFLAGS)" \
		TEST_LIBS="$(TEST_LIBS) $(SANITIZE_LDFLAGS)" \
		EDITOR_LIBS="$(EDITOR_LIBS) $(SANITIZE_LDFLAGS)"

sanitize-smoke:
	$(MAKE) smoke OUTDIR="$(OUTDIR)-sanitize" \
		CFLAGS="$(CFLAGS) $(SANITIZE_CFLAGS)" \
		LIBS="$(LIBS) $(SANITIZE_LDFLAGS)" \
		EDITOR_LIBS="$(EDITOR_LIBS) $(SANITIZE_LDFLAGS)"

$(TEST_SERIALIZER_OBJ): $(EDITOR_DIR)/serializer.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_EMIT_OBJ): $(EDITOR_DIR)/serializer_emit.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_IO_OBJ): $(EDITOR_DIR)/serializer_io.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_OBJ): $(EDITOR_DIR)/serializer_load.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_CHECKPOINTS_OBJ): $(EDITOR_DIR)/serializer_load_checkpoints.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_CLIMBABLES_OBJ): $(EDITOR_DIR)/serializer_load_climbables.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_COLLECTIBLES_OBJ): $(EDITOR_DIR)/serializer_load_collectibles.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_CONFIG_OBJ): $(EDITOR_DIR)/serializer_load_config.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_ENEMIES_OBJ): $(EDITOR_DIR)/serializer_load_enemies.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_GEOMETRY_OBJ): $(EDITOR_DIR)/serializer_load_geometry.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_HAZARDS_OBJ): $(EDITOR_DIR)/serializer_load_hazards.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_HEADER_OBJ): $(EDITOR_DIR)/serializer_load_header.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_LAYERS_OBJ): $(EDITOR_DIR)/serializer_load_layers.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_LOAD_SURFACES_OBJ): $(EDITOR_DIR)/serializer_load_surfaces.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_PARSE_OBJ): $(EDITOR_DIR)/serializer_parse.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_SAVE_OBJ): $(EDITOR_DIR)/serializer_save.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SERIALIZER_TYPES_OBJ): $(EDITOR_DIR)/serializer_types.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_VALIDATE_OBJ): $(SRCDIR)/levels/level_validate.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_LEVEL_LOADER_OBJ): $(SRCDIR)/levels/level_loader.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_RAIL_OBJ): $(SRCDIR)/surfaces/rail.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_ENTITY_UTILS_OBJ): $(SRCDIR)/core/entity_utils.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SPIKE_BLOCK_OBJ): $(SRCDIR)/hazards/spike_block.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SPIKE_PLATFORM_OBJ): $(SRCDIR)/hazards/spike_platform.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_FISH_OBJ): $(SRCDIR)/entities/fish.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_CIRCULAR_SAW_OBJ): $(SRCDIR)/hazards/circular_saw.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_COLLISION_DAMAGE_OBJ): $(SRCDIR)/collision/collision_damage.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_CAMERA_OBJ): $(SRCDIR)/core/game_camera.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_SCORE_OBJ): $(SRCDIR)/core/game_score.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_LEVEL_PHYSICS_OBJ): $(SRCDIR)/levels/level_physics.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_PLAYER_LIFECYCLE_OBJ): $(SRCDIR)/player/player_lifecycle.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_FLOAT_PLATFORM_OBJ): $(SRCDIR)/surfaces/float_platform.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_BOUNCEPAD_OBJ): $(SRCDIR)/surfaces/bouncepad.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_PHASE_OBJ): $(SRCDIR)/levels/phase_transition.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_OVERLAY_OBJ): $(SRCDIR)/core/game_overlay.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_EVENTS_OBJ): $(SRCDIR)/input/game_events.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_CHECKPOINT_OBJ): $(SRCDIR)/core/game_checkpoint.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_HUD_OBJ): $(SRCDIR)/screens/hud.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_GAME_INPUT_OBJ): $(SRCDIR)/input/game_input.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_BINDINGS_OBJ): $(SRCDIR)/input/game_bindings.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_SETTINGS_OBJ): $(SRCDIR)/screens/settings_menu.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_VALIDATION_OBJ): $(EDITOR_DIR)/editor_validation.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_FILES_OBJ): $(EDITOR_DIR)/editor_files.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_SESSION_OBJ): $(EDITOR_DIR)/editor_session.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_UNDO_APPLY_OBJ): $(EDITOR_DIR)/editor_undo_apply.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_ENTITY_META_OBJ): $(EDITOR_DIR)/entity_meta.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_UI_OBJ): $(EDITOR_DIR)/ui.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_TOOLS_OBJ): $(EDITOR_DIR)/tools.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_CLIPBOARD_OBJ): $(EDITOR_DIR)/editor_clipboard.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_EVENTS_OBJ): $(EDITOR_DIR)/editor_events.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_CANVAS_OBJ): $(EDITOR_DIR)/canvas.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_PANELS_OBJ): $(EDITOR_DIR)/editor_panels.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_LAYOUT_OBJ): $(EDITOR_DIR)/editor_layout.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_PALETTE_OBJ): $(EDITOR_DIR)/palette.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_PROPERTIES_OBJ): $(EDITOR_DIR)/properties.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_EDITOR_PLAYTEST_OBJ): $(EDITOR_DIR)/editor_playtest.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_FILE_DIALOG_OBJ): $(EDITOR_DIR)/file_dialog.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_UNDO_OBJ): $(EDITOR_DIR)/undo.c
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(TEST_TOMLC_OBJ): $(VENDOR_DIR)/tomlc17.c
	$(CC) $(TEST_CFLAGS) -MMD -MP -c -o $@ $<

$(OUTDIR)/level-serializer-test: tests/level_serializer_test.c $(TEST_SERIALIZER_OBJ) $(TEST_SERIALIZER_EMIT_OBJ) $(TEST_SERIALIZER_IO_OBJ) $(TEST_SERIALIZER_LOAD_OBJ) $(TEST_SERIALIZER_LOAD_CHECKPOINTS_OBJ) $(TEST_SERIALIZER_LOAD_CLIMBABLES_OBJ) $(TEST_SERIALIZER_LOAD_COLLECTIBLES_OBJ) $(TEST_SERIALIZER_LOAD_CONFIG_OBJ) $(TEST_SERIALIZER_LOAD_ENEMIES_OBJ) $(TEST_SERIALIZER_LOAD_GEOMETRY_OBJ) $(TEST_SERIALIZER_LOAD_HAZARDS_OBJ) $(TEST_SERIALIZER_LOAD_HEADER_OBJ) $(TEST_SERIALIZER_LOAD_LAYERS_OBJ) $(TEST_SERIALIZER_LOAD_SURFACES_OBJ) $(TEST_SERIALIZER_PARSE_OBJ) $(TEST_SERIALIZER_SAVE_OBJ) $(TEST_SERIALIZER_TYPES_OBJ) $(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ -lm

$(OUTDIR)/level-serializer-test: tests/parser_boundary_test.c

$(OUTDIR)/level-validate-test: tests/level_validate_test.c $(TEST_VALIDATE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^

$(OUTDIR)/runtime-load-test: tests/runtime_load_test.c $(TEST_LEVEL_LOADER_OBJ) \
		$(TEST_VALIDATE_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_RAIL_OBJ) \
		$(TEST_SPIKE_BLOCK_OBJ) $(TEST_FLOAT_PLATFORM_OBJ) \
		$(TEST_BOUNCEPAD_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/rail-test: tests/rail_test.c $(TEST_RAIL_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/entity-utils-test: tests/entity_utils_test.c $(TEST_ENTITY_UTILS_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^

$(OUTDIR)/collision-test: tests/collision_test.c $(TEST_SPIKE_PLATFORM_OBJ) \
		$(TEST_FISH_OBJ) $(TEST_CIRCULAR_SAW_OBJ) $(TEST_ENTITY_UTILS_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/phase-transition-test: tests/phase_transition_test.c $(TEST_PHASE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/editor-validation-test: tests/editor_validation_test.c $(TEST_EDITOR_VALIDATION_OBJ) $(TEST_EDITOR_FILES_OBJ) $(TEST_EDITOR_SESSION_OBJ) $(TEST_EDITOR_UNDO_APPLY_OBJ) $(TEST_EDITOR_ENTITY_META_OBJ) $(TEST_EDITOR_UI_OBJ) $(TEST_EDITOR_TOOLS_OBJ) $(TEST_EDITOR_CLIPBOARD_OBJ) $(TEST_EDITOR_EVENTS_OBJ) $(TEST_EDITOR_CANVAS_OBJ) $(TEST_EDITOR_PANELS_OBJ) $(TEST_EDITOR_LAYOUT_OBJ) $(TEST_EDITOR_PALETTE_OBJ) $(TEST_EDITOR_PROPERTIES_OBJ) $(TEST_EDITOR_PLAYTEST_OBJ) $(TEST_FILE_DIALOG_OBJ) $(TEST_UNDO_OBJ) $(TEST_RAIL_OBJ) $(TEST_SERIALIZER_OBJ) $(TEST_SERIALIZER_EMIT_OBJ) $(TEST_SERIALIZER_IO_OBJ) $(TEST_SERIALIZER_LOAD_OBJ) $(TEST_SERIALIZER_LOAD_CHECKPOINTS_OBJ) $(TEST_SERIALIZER_LOAD_CLIMBABLES_OBJ) $(TEST_SERIALIZER_LOAD_COLLECTIBLES_OBJ) $(TEST_SERIALIZER_LOAD_CONFIG_OBJ) $(TEST_SERIALIZER_LOAD_ENEMIES_OBJ) $(TEST_SERIALIZER_LOAD_GEOMETRY_OBJ) $(TEST_SERIALIZER_LOAD_HAZARDS_OBJ) $(TEST_SERIALIZER_LOAD_HEADER_OBJ) $(TEST_SERIALIZER_LOAD_LAYERS_OBJ) $(TEST_SERIALIZER_LOAD_SURFACES_OBJ) $(TEST_SERIALIZER_PARSE_OBJ) $(TEST_SERIALIZER_SAVE_OBJ) $(TEST_SERIALIZER_TYPES_OBJ) $(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(EDITOR_LIBS)

$(OUTDIR)/gameplay-damage-test: tests/gameplay_damage_test.c $(TEST_COLLISION_DAMAGE_OBJ) $(TEST_GAME_OVERLAY_OBJ) $(TEST_GAME_CHECKPOINT_OBJ) $(TEST_HUD_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/gameplay-config-test: tests/gameplay_config_test.c $(TEST_GAME_CAMERA_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/gameplay-score-test: tests/gameplay_score_test.c $(TEST_GAME_SCORE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/game-overlay-test: tests/game_overlay_test.c $(TEST_GAME_OVERLAY_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/game-events-test: tests/game_events_test.c $(TEST_GAME_EVENTS_OBJ) $(TEST_GAME_INPUT_OBJ) $(TEST_GAME_OVERLAY_OBJ) $(TEST_GAME_TERMINAL_OBJ) $(TEST_SETTINGS_OBJ) $(TEST_BINDINGS_OBJ) $(TEST_EDITOR_UI_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/session-test: tests/session_test.c tests/game_profile_test.c $(SESSION_RUNTIME_OBJS) levels/campaigns/main.toml
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $(filter %.c %.o,$^) $(LIBS)

$(OUTDIR)/game-checkpoint-test: tests/game_checkpoint_test.c $(TEST_GAME_CHECKPOINT_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

# Rebuild objects when their build recipes/flags change in this Makefile.
$(sort $(OBJS) $(EDITOR_OBJS) $(TEST_OBJECTS)): Makefile

# ── WebAssembly (Emscripten) ──────────────────────────────────────────
# Requires the Emscripten SDK (emcc on PATH).
# Produces out/super-mango.html, .js, .wasm, and .data (bundled assets).
#
# SDL2 ports are compiled from source by Emscripten on first build;
# subsequent builds reuse the cached port libraries.
WEB_FLAGS = -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' \
            -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 \
            -s SDL2_MIXER_FORMATS='["wav"]' \
            -s ALLOW_MEMORY_GROWTH=1 \
            --preload-file assets \
            --preload-file levels \
            --shell-file web/shell.html
WEB_CFLAGS = -D_GNU_SOURCE

web: $(OUTDIR)
	emcc -std=c11 -O2 $(WEB_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) $(SRCS) -o $(OUTDIR)/super-mango.html $(WEB_FLAGS) \
		-s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain"]'
	emcc -std=c11 -O2 $(WEB_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) $(SRCS) -o $(OUTDIR)/super-mango-debug.html $(WEB_FLAGS) \
		-s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain"]' \
		--post-js web/debug-boot.js

dist-native: all
	@if [ -n "$${RELEASE_DLL_DIR:-}" ]; then \
		python3 tools/package_release.py --platform "$${RELEASE_PLATFORM:-super-mango-native}" --binary $(TARGET) --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-native}.zip" --dll-dir "$${RELEASE_DLL_DIR}"; \
	else \
		python3 tools/package_release.py --platform "$${RELEASE_PLATFORM:-super-mango-native}" --binary $(TARGET) --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-native}.zip"; \
	fi

dist-wasm: web
	python3 tools/package_release.py --wasm --out-dir "$(OUTDIR)" --platform "$${RELEASE_PLATFORM:-super-mango-wasm}" --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-wasm}.zip"

clean:
	rm -f $(SRCDIR)/*.o $(SRCDIR)/*.d
	rm -f $(SRCDIR)/collectibles/*.o $(SRCDIR)/collectibles/*.d
	rm -f $(SRCDIR)/collision/*.o $(SRCDIR)/collision/*.d
	rm -f $(SRCDIR)/core/*.o $(SRCDIR)/core/*.d
	rm -f $(SRCDIR)/effects/*.o $(SRCDIR)/effects/*.d
	rm -f $(SRCDIR)/entities/*.o $(SRCDIR)/entities/*.d
	rm -f $(SRCDIR)/hazards/*.o $(SRCDIR)/hazards/*.d
	rm -f $(SRCDIR)/input/*.o $(SRCDIR)/input/*.d
	rm -f $(SRCDIR)/levels/*.o $(SRCDIR)/levels/*.d
	rm -f $(SRCDIR)/player/*.o $(SRCDIR)/player/*.d
	rm -f $(SRCDIR)/render/*.o $(SRCDIR)/render/*.d
	rm -f $(SRCDIR)/screens/*.o $(SRCDIR)/screens/*.d
	rm -f $(SRCDIR)/surfaces/*.o $(SRCDIR)/surfaces/*.d
	rm -f $(EDITOR_DIR)/*.o $(EDITOR_DIR)/*.d
	rm -f $(VENDOR_DIR)/*.o $(VENDOR_DIR)/*.d
	rm -rf $(OUTDIR) $(DISTDIR)
