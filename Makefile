# ── Compiler and pinned raylib build ─────────────────────────────────
# Make remains the application entry point; CMake builds the pinned dependency.
# Native and web libraries have separate build directories and never use SDL.
NODE ?= node

# A recipe that fails part-way can leave its target newer than its inputs:
# emcc writes the HTML, then tools/web_csp.py fails to pin its CSP.  Make
# would then treat that unpinned page as up to date.  This deletes the
# target of any failed recipe so the next run rebuilds it.
.DELETE_ON_ERROR:

ifeq ($(OS),Windows_NT)
CC      ?= /c/msys64/ucrt64/bin/clang.exe
else
CC      ?= clang
endif

ifeq ($(origin CC),default)
CC = clang
endif
BUILD_MODE ?= debug
MODE_FLAGS_debug = -g -O0
# Release builds add OS-supported exploit mitigations: stack canaries and
# fortified libc calls everywhere POSIX, plus PIE and full RELRO on Linux.
# -U first avoids a redefinition warning where the toolchain presets FORTIFY.
# MSYS2/MinGW keeps plain -O2: its FORTIFY support needs libssp linkage.
ifeq ($(OS),Windows_NT)
HARDEN_CFLAGS  =
HARDEN_LDFLAGS =
else ifeq ($(shell uname -s),Darwin)
HARDEN_CFLAGS  = -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
HARDEN_LDFLAGS =
else
HARDEN_CFLAGS  = -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fPIE
HARDEN_LDFLAGS = -pie -Wl,-z,relro,-z,now
endif
MODE_FLAGS_release = -O2 $(HARDEN_CFLAGS)
MODE_LDFLAGS_release = $(HARDEN_LDFLAGS)
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(MODE_FLAGS_$(BUILD_MODE)) -I$(RAYLIB_BUILD)/build/raylib/include $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_RAYLIB_MEMORY,) $(EXTRA_CFLAGS)
# Tests write scratch files under their own OUTDIR (tests/test_paths.h), so
# test, sanitize and coverage trees can run side by side in one checkout.
# MANGO_TESTING turns on the *_test_set_* seams (canned dialog answers,
# injected I/O failures); only test objects get it, never shipped binaries.
TEST_CFLAGS = $(CFLAGS) $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_MEMORY_TESTS,) \
              -DMANGO_TEST_OUTDIR='"$(OUTDIR)"' -DMANGO_TESTING
LIBS    = $(RAYLIB_LIB) $(PLATFORM_LIBS) $(MODE_LDFLAGS_$(BUILD_MODE)) $(EXTRA_LDFLAGS)
OUTDIR  = out
# RAYLIB_AUDIO=null is a test-only variant: the real desktop GLFW/OpenGL
# backend with miniaudio's null playback device, for CI machines that have a
# display but no sound hardware. It builds into its own raylib directory.
RAYLIB_AUDIO ?= device
RAYLIB_BUILD ?= $(OUTDIR)/raylib$(if $(filter null,$(RAYLIB_AUDIO)),-nullaudio,)
RAYLIB_PLATFORM ?= native
RAYLIB_LIB = $(RAYLIB_BUILD)/build/raylib/libraylib.a
WEB_RAYLIB_BUILD = $(OUTDIR)/raylib-web
WEB_RAYLIB_LIB = $(WEB_RAYLIB_BUILD)/build/raylib/libraylib.a
# Optional shared, checksum-verified source archive (CI caches this one file
# instead of downloading it once per build directory).
RAYLIB_ARCHIVE ?=
RAYLIB_ARCHIVE_ARG = $(if $(RAYLIB_ARCHIVE),--archive "$(RAYLIB_ARCHIVE)",)
RELEASE_RAYLIB_BUILD = $(if $(filter command line environment,$(origin RAYLIB_BUILD)),$(RAYLIB_BUILD),$(OUTDIR)/release/raylib)
ifeq ($(OS),Windows_NT)
PLATFORM_LIBS = -lopengl32 -lgdi32 -lwinmm -lshell32 -lole32 -lpsapi -lbcrypt -lm
else ifeq ($(shell uname -s),Darwin)
PLATFORM_LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -lm
else
PLATFORM_LIBS = -lGL -lX11 -lpthread -ldl -lrt -lm
endif
OBJDIR  = $(OUTDIR)/obj
ifeq ($(RAYLIB_PLATFORM),memory)
ifeq ($(OS),Windows_NT)
PLATFORM_LIBS = -lshell32 -lole32 -lpsapi -lwinmm -lbcrypt -lm
else
PLATFORM_LIBS = -lm -lpthread
endif
ifneq ($(filter release dist-native,$(MAKECMDGOALS)),)
$(error Memory is a test backend; native releases require RAYLIB_PLATFORM=native)
endif
endif
ifeq ($(RAYLIB_AUDIO),null)
ifneq ($(filter release dist-native,$(MAKECMDGOALS)),)
$(error RAYLIB_AUDIO=null is a test build; native releases need a real audio device)
endif
endif
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
          $(wildcard $(SRCDIR)/shared/*.c) \
          vendor/tomlc17/tomlc17.c
OBJS    = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    = $(OBJS:.o=.d)
# The session test drives input and save failures through the MANGO_TESTING
# seams, so those two modules come from the test tree as well.
SESSION_RUNTIME_OBJS = $(filter-out $(OBJDIR)/src/main.o $(OBJDIR)/src/shared/audio.o \
                         $(OBJDIR)/src/core/app_session.o $(OBJDIR)/src/input/game_input.o \
                         $(OBJDIR)/src/shared/serializer_io.o,$(OBJS)) \
                       $(TEST_AUDIO_OBJ) $(TEST_SESSION_OBJ) $(TEST_GAME_INPUT_OBJ) \
                       $(TEST_OBJDIR)/$(SHARED_DIR)/serializer_io.o

# ── Editor (standalone level editor) ─────────────────────────────────
EDITOR_DIR    = src/editor
SHARED_DIR    = src/shared
VENDOR_DIR    = vendor/tomlc17
EDITOR_SRCS   = $(wildcard $(EDITOR_DIR)/*.c) $(wildcard $(SHARED_DIR)/*.c) $(VENDOR_DIR)/tomlc17.c \
                src/surfaces/rail.c src/levels/level_validate.c src/levels/level_ref.c \
                src/input/input_backend.c
EDITOR_OBJS   = $(patsubst %.c,$(OBJDIR)/%.o,$(EDITOR_SRCS))
EDITOR_DEPS   = $(EDITOR_OBJS:.o=.d)
EDITOR_TARGET = $(OUTDIR)/super-mango-editor
EDITOR_LIBS   = $(LIBS)
TEST_TARGETS  = $(OUTDIR)/level-serializer-test $(OUTDIR)/level-validate-test \
                 $(OUTDIR)/runtime-load-test \
                 $(OUTDIR)/rail-test $(OUTDIR)/entity-utils-test \
                 $(OUTDIR)/collision-test $(OUTDIR)/phase-transition-test \
                 $(OUTDIR)/editor-validation-test \
                 $(OUTDIR)/gameplay-damage-test $(OUTDIR)/gameplay-config-test \
                 $(OUTDIR)/gameplay-score-test \
                 $(OUTDIR)/game-overlay-test $(OUTDIR)/game-events-test \
                 $(OUTDIR)/session-test $(OUTDIR)/game-checkpoint-test
LEVEL_FILES   = $(wildcard levels/*.toml) $(wildcard levels/labs/*.toml)
SMOKE_LEVELS  = $(LEVEL_FILES)
SMOKE_FRAMES  ?= 5
SMOKE_SEED    ?= 1
SMOKE_SEEDS   ?= 1 7 23
# Tests recompile some game/editor sources with TEST_CFLAGS (a few also swap
# raylib calls for test doubles), so those objects live in their own tree that
# mirrors the source path: src/screens/hud.c -> $(TEST_OBJDIR)/src/screens/hud.o.
# One pattern rule (after the fuzz targets) builds every one of them; adding a
# test object is just a new variable here plus its use in a link rule below.
TEST_OBJDIR = $(OBJDIR)/tests
TEST_SERIALIZER_OBJS = $(patsubst %,$(TEST_OBJDIR)/$(SHARED_DIR)/%.o,serializer serializer_emit \
                       serializer_io serializer_load serializer_load_checkpoints \
                       serializer_load_climbables serializer_load_collectibles \
                       serializer_load_config serializer_load_enemies serializer_load_geometry \
                       serializer_load_hazards serializer_load_header serializer_load_layers \
                       serializer_load_surfaces serializer_parse serializer_save serializer_types)
TEST_VALIDATE_OBJ   = $(TEST_OBJDIR)/$(SRCDIR)/levels/level_validate.o
TEST_LEVEL_LOADER_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/levels/level_loader.o
TEST_TOMLC_OBJ      = $(TEST_OBJDIR)/$(VENDOR_DIR)/tomlc17.o
TEST_RAIL_OBJ       = $(TEST_OBJDIR)/$(SRCDIR)/surfaces/rail.o
TEST_ENTITY_UTILS_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/entity_utils.o
TEST_SPIKE_BLOCK_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/hazards/spike_block.o
TEST_SPIKE_PLATFORM_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/hazards/spike_platform.o
TEST_FISH_OBJ      = $(TEST_OBJDIR)/$(SRCDIR)/entities/fish.o
TEST_CIRCULAR_SAW_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/hazards/circular_saw.o
TEST_COLLISION_DAMAGE_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/collision/collision_damage.o
TEST_GAME_CAMERA_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/game_camera.o
TEST_GAME_SCORE_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/game_score.o
TEST_LEVEL_PHYSICS_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/levels/level_physics.o
TEST_PLAYER_LIFECYCLE_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/player/player_lifecycle.o
TEST_FLOAT_PLATFORM_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/surfaces/float_platform.o
TEST_BOUNCEPAD_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/surfaces/bouncepad.o
TEST_PHASE_OBJ      = $(TEST_OBJDIR)/$(SRCDIR)/levels/phase_transition.o
TEST_GAME_OVERLAY_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/game_overlay.o
TEST_GAME_EVENTS_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/game_events.o
TEST_GAME_INPUT_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/game_input.o
TEST_WEB_INPUT_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/game_web_input.o
TEST_BINDINGS_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/game_bindings.o
TEST_SETTINGS_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/screens/settings_menu.o
# These two need no test flags, so tests link the game's own objects.
TEST_GAME_TERMINAL_OBJ = $(OBJDIR)/src/core/game_terminal.o
TEST_GAME_RANDOM_OBJ = $(OBJDIR)/src/core/game_random.o
TEST_GAME_CHECKPOINT_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/game_checkpoint.o
TEST_HUD_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/screens/hud.o
TEST_EDITOR_UI_OBJ = $(TEST_OBJDIR)/$(SHARED_DIR)/ui.o
TEST_EDITOR_OBJS = $(patsubst %,$(TEST_OBJDIR)/$(EDITOR_DIR)/%.o,editor_validation editor_files \
                   editor_recovery editor_session editor_undo_apply entity_meta) \
                   $(TEST_EDITOR_UI_OBJ) \
                   $(patsubst %,$(TEST_OBJDIR)/$(EDITOR_DIR)/%.o,tools hit_test editor_clipboard \
                   editor_events canvas editor_panels editor_layout palette properties \
                   editor_playtest file_dialog undo)
TEST_AUDIO_OBJ     = $(TEST_OBJDIR)/$(SHARED_DIR)/audio.o
TEST_SESSION_OBJ   = $(TEST_OBJDIR)/$(SRCDIR)/core/app_session.o
TEST_INPUT_BACKEND_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/input_backend.o
TEST_LIBS           = $(LIBS)
PLATFORM_OBJS = $(addprefix $(OBJDIR)/src/shared/,audio.o graphics.o platform.o text.o) $(OBJDIR)/src/input/input_backend.o
# Every TEST_*_OBJ / TEST_*_OBJS variable above, so new ones need no extra list.
TEST_OBJECTS := $(foreach name,$(filter %_OBJ %_OBJS,$(filter TEST_%,$(.VARIABLES))),$($(name)))
TEST_DEPS           = $(patsubst %.o,%.d,$(filter $(TEST_OBJDIR)/%,$(TEST_OBJECTS)))
# UBSan only prints a report and carries on by default, so a test that hits
# undefined behaviour would still exit 0 and CI would stay green.
# -fno-sanitize-recover makes every UB check abort, as the fuzz builds do;
# UBSAN_OPTIONS also covers code built without it (the sanitized raylib) and
# prints a stack trace. ASan errors already abort. LeakSanitizer runs where the
# platform has it (Linux); Apple's ASan has no leak checker, so it is not set.
SANITIZE_CFLAGS     = -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
SANITIZE_LDFLAGS    = -fsanitize=address,undefined
SANITIZE_ENV        = UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1$${UBSAN_OPTIONS:+:$$UBSAN_OPTIONS}"

.PHONY: all clean run run-debug run-level run-level-debug web editor run-editor test validate-levels web-host-contract level-catalog overlay-snapshots docs-drift roadmap-quality smoke scripted-smoke sanitize sanitize-smoke dist-native dist-wasm compile-commands

all: $(OUTDIR) $(TARGET) ## Build: Compile the game into OUTDIR (default target)

$(RAYLIB_LIB): vendor/raylib/manifest.json vendor/raylib/patches.json tools/build_raylib.py Makefile
	python3 tools/build_raylib.py --build-dir "$(RAYLIB_BUILD)" --platform $(RAYLIB_PLATFORM) --cc "$(CC)" --mode $(BUILD_MODE) $(if $(findstring -fsanitize,$(CFLAGS)),--sanitize,) $(if $(filter null,$(RAYLIB_AUDIO)),--null-audio,) $(RAYLIB_ARCHIVE_ARG)

$(WEB_RAYLIB_LIB): vendor/raylib/manifest.json vendor/raylib/patches.json tools/build_raylib.py Makefile
	python3 tools/build_raylib.py --build-dir "$(WEB_RAYLIB_BUILD)" --platform web --mode release $(RAYLIB_ARCHIVE_ARG)

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
# Windows toolchain runtime DLLs must be on PATH for local builds.

ifeq ($(OS),Windows_NT)
RUNTIME_DLL_PATH ?= /c/msys64/ucrt64/bin
RUN_PREFIX = PATH="$(RUNTIME_DLL_PATH):$$PATH"
else
RUN_PREFIX =
endif

run: all ## Run: Build and play the campaign
	$(RUN_PREFIX) "$(abspath $(TARGET))"

run-debug: all ## Run: Play with the debug overlay
	$(RUN_PREFIX) "$(abspath $(TARGET))" --debug

run-level: all ## Run: Play one level: make run-level LEVEL=levels/<name>.toml
	$(RUN_PREFIX) "$(abspath $(TARGET))" --level "$(LEVEL)"

run-level-debug: all ## Run: run-level with the debug overlay
	$(RUN_PREFIX) "$(abspath $(TARGET))" --debug --level "$(LEVEL)"

# ── Editor targets ───────────────────────────────────────────────────
editor: $(OUTDIR) $(EDITOR_TARGET) ## Build: Compile the level editor into OUTDIR

$(EDITOR_TARGET): $(EDITOR_OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(EDITOR_LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

$(OBJDIR)/$(VENDOR_DIR)/%.o: $(VENDOR_DIR)/%.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

run-editor: all editor ## Run: Build and open the level editor
	$(RUN_PREFIX) "$(abspath $(EDITOR_TARGET))"

.PHONY: debug release builder
builder: all editor ## Build: Game and editor together
debug: ## Build: Game and editor with debug flags in OUTDIR/debug
	$(MAKE) builder BUILD_MODE=debug OUTDIR="$(OUTDIR)/debug"
release: ## Build: Game and editor, optimized and hardened, in OUTDIR/release
	$(MAKE) builder BUILD_MODE=release OUTDIR="$(OUTDIR)/release"

-include $(EDITOR_DEPS)
-include $(TEST_DEPS)

# ── Tests ────────────────────────────────────────────────────────────
test: $(OUTDIR) $(TEST_TARGETS) web-host-contract parser-allocation-probe parser-encoding-probe ## Test: Native test binaries plus the Python and Node host checks
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/level-serializer-test"
	python3 tests/validate_levels_test.py
	python3 tests/gen_sounds_test.py
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/level-validate-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/runtime-load-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/rail-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/entity-utils-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/collision-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/phase-transition-test"
	MANGO_TEST_WINDOW=1 $(RUN_PREFIX) "$(abspath $(OUTDIR))/editor-validation-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-damage-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-config-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/gameplay-score-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-overlay-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-events-test"
	MANGO_TEST_WINDOW=1 $(RUN_PREFIX) "$(abspath $(OUTDIR))/session-test"
	$(RUN_PREFIX) "$(abspath $(OUTDIR))/game-checkpoint-test"

$(TEST_TARGETS): | $(OUTDIR)
$(filter-out $(OUTDIR)/session-test $(OUTDIR)/game-events-test,$(TEST_TARGETS)): $(PLATFORM_OBJS)
$(OUTDIR)/game-events-test: $(filter-out $(OBJDIR)/src/input/input_backend.o,$(PLATFORM_OBJS)) $(TEST_INPUT_BACKEND_OBJ) tests/input_backend_test.c
$(OUTDIR)/editor-validation-test: $(OBJDIR)/src/editor/dialog_choice.o
# A rebuilt dependency must refresh consumers and relink executables. Exported
# headers retain upstream timestamps, so header mtimes alone are insufficient.
$(sort $(OBJS) $(EDITOR_OBJS) $(TEST_OBJECTS)): $(RAYLIB_LIB)

# Extra standalone parser probes; keep the 15-regression-binary inventory above.
.PHONY: parser-allocation-probe parser-encoding-probe
parser-allocation-probe: $(OUTDIR)/parser-allocation-probe
	$(RUN_PREFIX) "$(abspath $<)"

$(OUTDIR)/parser-allocation-probe: tests/parser_allocation_test.c $(VENDOR_DIR)/tomlc17.c $(VENDOR_DIR)/tomlc17.h Makefile | $(OUTDIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< -lm

parser-encoding-probe:
	python3 tests/parser_validator_test.py

# Levels are checked by the game's own C loader and validator (one source of
# truth, no window needed); the Python script adds the cross-file checks a
# single level cannot make: assets and next_phase targets exist, and the
# campaign manifest chains. The checker reuses the game's objects in $(OBJDIR).
LEVEL_CHECK = $(OUTDIR)/level-check
LEVEL_CHECK_OBJS = $(filter $(OBJDIR)/$(SHARED_DIR)/serializer%.o,$(OBJS)) \
                   $(OBJDIR)/$(SRCDIR)/levels/level_validate.o \
                   $(OBJDIR)/$(SRCDIR)/levels/level_ref.o $(OBJDIR)/$(VENDOR_DIR)/tomlc17.o

validate-levels: $(LEVEL_CHECK) ## Check: Every level, the campaign manifest and asset links
	$(RUN_PREFIX) "$(abspath $(LEVEL_CHECK))" $(LEVEL_FILES)
	python3 tools/validate_levels.py

$(LEVEL_CHECK): tools/level_check.c $(LEVEL_CHECK_OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ -lm

web-host-contract: ## Check: Web shell, storage and packaging contract tests
	python3 tools/check_web_boot_contract.py
	$(NODE) tests/web_host_test.cjs
	$(NODE) tests/profile_storage_test.cjs
	$(NODE) tests/touch_controls_test.cjs
	$(NODE) tests/keyboard_scope_test.cjs
	python3 tests/package_release_test.py
	python3 tests/filter_codeql_sarif_test.py

# clangd / IDE compile database uses the same pinned dependency headers.
compile-commands: ## Build: Write compile_commands.json for clangd and IDEs
	CC="$(CC)" RAYLIB_BUILD="$(RAYLIB_BUILD)" python3 tools/gen_compile_commands.py

level-catalog: ## Generate: Level catalog page from the campaign manifest
	python3 tools/generate_level_catalog.py

overlay-snapshots: ## Generate: Overlay text snapshots page
	python3 tools/generate_overlay_snapshots.py

# Regenerate every assets/sounds WAV from tools/gen_sounds.py recipes.
.PHONY: sounds
sounds: ## Generate: Every assets/sounds WAV from tools/gen_sounds.py
	python3 tools/gen_sounds.py

docs-drift: ## Check: Generated docs, sounds and manual facts match the code
	python3 tools/content_inventory.py --check
	python3 tools/gen_sounds.py --check
	python3 tools/generate_level_catalog.py --check
	python3 tools/generate_overlay_snapshots.py --check
	python3 tools/check_docs_drift.py
	python3 tools/check_roadmap_quality.py

roadmap-quality: ## Check: Roadmap entries are complete
	python3 tools/check_roadmap_quality.py

.PHONY: content-inventory asset-budget
content-inventory: ## Generate: Asset inventory page and docs project facts
	python3 tools/content_inventory.py
asset-budget: ## Check: Shipped assets stay within the size budget
	python3 tools/content_inventory.py --check

.PHONY: timing-lab
timing-lab: ## Run: Print the fixed-step vs variable-step physics study
	python3 tools/timing_lab.py

smoke: all editor ## Test: Boot every level and the editor for SMOKE_FRAMES frames
	@for level in $(SMOKE_LEVELS); do \
		echo "smoke: $$level"; \
		$(RUN_PREFIX) "$(abspath $(TARGET))" --level "$$level" --smoke-test-frames $(SMOKE_FRAMES) --seed $(SMOKE_SEED) || exit 1; \
	done
	$(RUN_PREFIX) "$(abspath $(EDITOR_TARGET))" --smoke-test

scripted-smoke: all editor ## Test: Replay scripted input scenarios for each of SMOKE_SEEDS
	python3 tools/run_scripted_smoke.py --binary $(TARGET) --editor $(EDITOR_TARGET) --frames $(SMOKE_FRAMES) --seeds $(SMOKE_SEEDS)

sanitize: ## Test: test and fuzz-corpus under ASan/UBSan in OUTDIR-sanitize
	$(SANITIZE_ENV) $(MAKE) all editor test OUTDIR="$(OUTDIR)-sanitize" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS) $(SANITIZE_CFLAGS)" \
		EXTRA_LDFLAGS="$(EXTRA_LDFLAGS) $(SANITIZE_LDFLAGS)"
	$(SANITIZE_ENV) $(MAKE) fuzz-corpus OUTDIR="$(OUTDIR)-sanitize"

sanitize-smoke: ## Test: smoke under ASan/UBSan in OUTDIR-sanitize
	$(SANITIZE_ENV) $(MAKE) smoke OUTDIR="$(OUTDIR)-sanitize" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS) $(SANITIZE_CFLAGS)" \
		EXTRA_LDFLAGS="$(EXTRA_LDFLAGS) $(SANITIZE_LDFLAGS)"

# ── Coverage (clang source-based) ───────────────────────────────────
# Rebuild the native tests with profile instrumentation in their own tree,
# run them, merge the per-process .profraw files and print a per-file
# summary.  Reuses the already-built raylib; vendor and test code are hidden.
COVERAGE_OUTDIR = $(OUTDIR)/coverage
COVERAGE_FLAGS = -fprofile-instr-generate -fcoverage-mapping
ifeq ($(shell uname -s),Darwin)
LLVM_PROFDATA ?= xcrun llvm-profdata
LLVM_COV ?= xcrun llvm-cov
else
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV ?= llvm-cov
endif
COVERAGE_BINS = $(patsubst $(OUTDIR)/%,$(COVERAGE_OUTDIR)/%,$(TEST_TARGETS)) \
                $(COVERAGE_OUTDIR)/parser-allocation-probe
COVERAGE_IGNORE = '(^|/)(vendor|tests|out)/'

.PHONY: coverage
coverage: $(RAYLIB_LIB) ## Test: Per-file line coverage of the native tests (clang)
	rm -rf "$(COVERAGE_OUTDIR)/profiles"
	mkdir -p "$(COVERAGE_OUTDIR)/profiles" "$(COVERAGE_OUTDIR)/obj/tests"
	LLVM_PROFILE_FILE="$(abspath $(COVERAGE_OUTDIR))/profiles/%p-%m.profraw" \
		$(MAKE) test OUTDIR="$(COVERAGE_OUTDIR)" RAYLIB_BUILD="$(RAYLIB_BUILD)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS) $(COVERAGE_FLAGS)" \
		EXTRA_LDFLAGS="$(EXTRA_LDFLAGS) -fprofile-instr-generate"
	$(LLVM_PROFDATA) merge -sparse "$(COVERAGE_OUTDIR)"/profiles/*.profraw \
		-o "$(COVERAGE_OUTDIR)/tests.profdata"
	$(LLVM_COV) report $(firstword $(COVERAGE_BINS)) \
		$(addprefix -object ,$(wordlist 2,$(words $(COVERAGE_BINS)),$(COVERAGE_BINS))) \
		-instr-profile="$(COVERAGE_OUTDIR)/tests.profdata" \
		-ignore-filename-regex=$(COVERAGE_IGNORE)
	@echo "coverage: per-line view: $(LLVM_COV) show $(firstword $(COVERAGE_BINS)) -instr-profile=$(COVERAGE_OUTDIR)/tests.profdata <file.c>"

# ── Fuzzing (POSIX) ──────────────────────────────────────────────────
# Each harness defines LLVMFuzzerTestOneInput.  fuzz-corpus links it with
# tests/fuzz_replay_main.c under ASan/UBSan and replays the seed inputs
# (FUZZ_MUTATIONS=N adds N blind mutations per seed).  fuzz links it with
# libFuzzer for coverage-guided search; Apple clang lacks libFuzzer, so
# point FUZZ_CC at Homebrew LLVM or a Linux clang.  Both honour
# EXTRA_CFLAGS, so CI's -Werror covers the harnesses under `make sanitize`.
FUZZ_FLAGS = -std=c11 -g -O1 -Wall -Wextra -Wpedantic -I$(SRCDIR) -I$(VENDOR_DIR) \
             -I$(RAYLIB_BUILD)/build/raylib/include \
             $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_RAYLIB_MEMORY,) \
             -fno-omit-frame-pointer -fsanitize=address,undefined \
             -fno-sanitize-recover=undefined $(EXTRA_CFLAGS)
FUZZ_LEVEL_SRCS = tests/fuzz_level_parse.c $(wildcard $(SHARED_DIR)/serializer*.c) \
                  src/levels/level_validate.c src/levels/level_ref.c $(VENDOR_DIR)/tomlc17.c
FUZZ_PROFILE_SRCS = tests/fuzz_profile_decode.c src/core/game_profile.c \
                    src/input/game_bindings.c src/input/input_backend.c \
                    src/levels/level_ref.c src/shared/serializer_io.c \
                    src/shared/platform.c $(VENDOR_DIR)/tomlc17.c
FUZZ_HEADERS = $(wildcard $(SRCDIR)/*.h $(SRCDIR)/*/*.h $(VENDOR_DIR)/*.h)
# Seeds are read in place: shipped levels, schema fixtures, extra edge cases.
FUZZ_LEVEL_SEEDS = levels levels/labs tests/fixtures/serializer_v1 \
                   tests/fixtures/runtime tests/fuzz/corpus/level
FUZZ_PROFILE_SEEDS = tests/fuzz/corpus/profile
FUZZ_MUTATIONS ?= 0
FUZZ_SECONDS ?= 60
FUZZ_CC ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang) clang)

.PHONY: fuzz-corpus fuzz
fuzz-corpus: $(OUTDIR)/fuzz-level-replay $(OUTDIR)/fuzz-profile-replay ## Test: Replay the fuzz seeds under ASan/UBSan (FUZZ_MUTATIONS=N)
	"$(abspath $(OUTDIR))/fuzz-level-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_LEVEL_SEEDS)
	"$(abspath $(OUTDIR))/fuzz-profile-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_PROFILE_SEEDS)

$(OUTDIR)/fuzz-level-replay: tests/fuzz_replay_main.c $(FUZZ_LEVEL_SRCS) $(FUZZ_HEADERS) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_LEVEL_SRCS) -lm

$(OUTDIR)/fuzz-profile-replay: tests/fuzz_replay_main.c $(FUZZ_PROFILE_SRCS) $(FUZZ_HEADERS) $(RAYLIB_LIB) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_PROFILE_SRCS) $(LIBS)

# -close_fd_mask=2 hides the loaders' per-input error lines; libFuzzer keeps
# its own and the sanitizer reports.  Crashes land in $(OUTDIR)/fuzz/ and
# replay with: $(OUTDIR)/fuzz-level-replay <crash-file>
fuzz: $(RAYLIB_LIB) | $(OUTDIR) ## Test: libFuzzer search for FUZZ_SECONDS (needs LLVM clang)
	@printf 'int LLVMFuzzerTestOneInput(const char *d, unsigned long n) { (void)d; (void)n; return 0; }\n' \
		| $(FUZZ_CC) -x c -fsanitize=fuzzer -o /dev/null - 2>/dev/null || { \
		echo "fuzz: '$(FUZZ_CC)' cannot link libFuzzer (Apple clang does not ship it)."; \
		echo "fuzz: install LLVM (brew install llvm) and run"; \
		echo "      make fuzz FUZZ_CC=\$$(brew --prefix llvm)/bin/clang"; \
		echo "fuzz: without libFuzzer, 'make fuzz-corpus FUZZ_MUTATIONS=500' runs blind mutations."; \
		exit 1; }
	mkdir -p $(OUTDIR)/fuzz/level $(OUTDIR)/fuzz/profile
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/level-fuzzer $(FUZZ_LEVEL_SRCS) -lm
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/profile-fuzzer $(FUZZ_PROFILE_SRCS) $(LIBS)
	"$(abspath $(OUTDIR))/fuzz/level-fuzzer" -max_total_time=$(FUZZ_SECONDS) -close_fd_mask=2 \
		-artifact_prefix=$(OUTDIR)/fuzz/level- $(OUTDIR)/fuzz/level $(FUZZ_LEVEL_SEEDS)
	"$(abspath $(OUTDIR))/fuzz/profile-fuzzer" -max_total_time=$(FUZZ_SECONDS) -close_fd_mask=2 \
		-artifact_prefix=$(OUTDIR)/fuzz/profile- $(OUTDIR)/fuzz/profile $(FUZZ_PROFILE_SEEDS)

# Test objects: one rule for every source. $(OUTDIR) is skipped once out/
# exists, so the recipe also creates its own directory; otherwise a deleted
# out/obj/tests would break every later `make test`.
TEST_OBJ_INCLUDES = -I$(SRCDIR) -I$(VENDOR_DIR)
$(TEST_OBJDIR)/%.o: %.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) $(TEST_OBJ_FLAGS) $(TEST_OBJ_INCLUDES) -MMD -MP -c -o $@ $<

# Per-object extras (target-specific variables). These rename raylib/GLFW
# calls so the tests can supply recording doubles in their place.
$(TEST_AUDIO_OBJ): TEST_OBJ_FLAGS = -DSetMusicVolume=test_SetMusicVolume \
	-DLoadSoundAlias=test_LoadSoundAlias -DSetSoundVolume=test_SetSoundVolume \
	-DUnloadSoundAlias=test_UnloadSoundAlias -DUnloadSound=test_UnloadSound
$(TEST_SESSION_OBJ): TEST_OBJ_FLAGS = -DSetWindowSize=test_SetWindowSize
$(TEST_INPUT_BACKEND_OBJ): TEST_OBJ_FLAGS = -UMANGO_RAYLIB_MEMORY \
	-DIsWindowReady=test_input_window_ready -DGetScreenWidth=test_input_screen_width -DGetScreenHeight=test_input_screen_height \
	-DglfwGetCurrentContext=test_input_current_context -DglfwGetCursorPos=test_input_cursor_pos \
	-DglfwSetKeyCallback=test_input_set_key -DglfwSetCharCallback=test_input_set_char \
	-DglfwSetMouseButtonCallback=test_input_set_button -DglfwSetCursorPosCallback=test_input_set_cursor \
	-DglfwSetScrollCallback=test_input_set_scroll
# Vendored tomlc17 builds without project include paths, as in the game.
$(TEST_TOMLC_OBJ): TEST_OBJ_INCLUDES =

$(OUTDIR)/level-serializer-test: tests/level_serializer_test.c $(TEST_SERIALIZER_OBJS) $(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/level-serializer-test: tests/parser_boundary_test.c

# level_validate.c shares the levels/<name>.toml rule from level_ref.c.
$(OUTDIR)/level-serializer-test $(OUTDIR)/level-validate-test \
$(OUTDIR)/runtime-load-test $(OUTDIR)/editor-validation-test: $(OBJDIR)/src/levels/level_ref.o

$(OUTDIR)/level-validate-test: tests/level_validate_test.c $(TEST_VALIDATE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/runtime-load-test: tests/runtime_load_test.c $(TEST_LEVEL_LOADER_OBJ) \
		$(TEST_GAME_RANDOM_OBJ) \
		$(TEST_VALIDATE_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_RAIL_OBJ) \
		$(TEST_SPIKE_BLOCK_OBJ) $(TEST_FLOAT_PLATFORM_OBJ) \
		$(TEST_BOUNCEPAD_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/rail-test: tests/rail_test.c $(TEST_RAIL_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/entity-utils-test: tests/entity_utils_test.c $(TEST_ENTITY_UTILS_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/collision-test: tests/collision_test.c $(TEST_SPIKE_PLATFORM_OBJ) \
		$(TEST_FISH_OBJ) $(TEST_CIRCULAR_SAW_OBJ) $(TEST_ENTITY_UTILS_OBJ) $(TEST_GAME_RANDOM_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/phase-transition-test: tests/phase_transition_test.c $(TEST_PHASE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/editor-validation-test: tests/editor_validation_test.c $(TEST_EDITOR_OBJS) $(TEST_RAIL_OBJ) $(TEST_SERIALIZER_OBJS) $(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(EDITOR_LIBS)

$(OUTDIR)/gameplay-damage-test: tests/gameplay_damage_test.c $(TEST_COLLISION_DAMAGE_OBJ) $(TEST_GAME_OVERLAY_OBJ) $(TEST_GAME_CHECKPOINT_OBJ) $(TEST_HUD_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/gameplay-config-test: tests/gameplay_config_test.c $(TEST_GAME_CAMERA_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/gameplay-score-test: tests/gameplay_score_test.c $(TEST_GAME_SCORE_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/game-overlay-test: tests/game_overlay_test.c $(TEST_GAME_OVERLAY_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

$(OUTDIR)/game-events-test: tests/game_events_test.c $(TEST_GAME_EVENTS_OBJ) $(TEST_GAME_INPUT_OBJ) $(TEST_WEB_INPUT_OBJ) $(TEST_GAME_OVERLAY_OBJ) $(TEST_GAME_TERMINAL_OBJ) $(TEST_SETTINGS_OBJ) $(TEST_BINDINGS_OBJ) $(TEST_EDITOR_UI_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(LIBS)

$(OUTDIR)/session-test: tests/session_test.c tests/game_profile_test.c tests/simulation_test.c tests/audio_contract_test.c tests/web_frame_pacing_test.c $(SESSION_RUNTIME_OBJS) levels/campaigns/main.toml
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $(filter %.c %.o,$^) $(LIBS)

$(OUTDIR)/game-checkpoint-test: tests/game_checkpoint_test.c $(TEST_GAME_CHECKPOINT_OBJ)
	$(CC) $(TEST_CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -o $@ $^ $(TEST_LIBS)

# Rebuild objects when their build recipes/flags change in this Makefile.
$(sort $(OBJS) $(EDITOR_OBJS) $(TEST_OBJECTS)): Makefile

# ── WebAssembly (Emscripten) ──────────────────────────────────────────
# Requires the Emscripten SDK (emcc on PATH).
# Produces out/super-mango.html, .js, .wasm, and .data (bundled assets).
#
# raylib is built with the same Emscripten toolchain as the application.
WEB_FLAGS = -s USE_GLFW=3 \
            --pre-js web/touch-controls.js \
            --pre-js web/keyboard-scope.js \
            -s ALLOW_MEMORY_GROWTH=1 \
            --preload-file assets \
            --exclude-file 'assets/sprites/unused/*' \
            --exclude-file '*/.DS_Store' \
            --preload-file levels \
            --shell-file web/shell.html
# Same warning set as native builds so Web-only code paths stay warning-free.
# Emscripten documents EM_JS(...); with a trailing semicolon, which pedantic C
# reports as an empty file-scope declaration; that one diagnostic is disabled.
WEB_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wno-extra-semi -O2 -D_GNU_SOURCE $(EXTRA_WEB_CFLAGS)
WEB_LINK_FLAGS = -s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain"]'
WEB_HTML = $(OUTDIR)/super-mango.html
WEB_DEBUG_HTML = $(OUTDIR)/super-mango-debug.html
# The .js/.wasm/.data siblings are emitted with each HTML file. Listing every
# input lets `web` (and dist-wasm through it) skip emcc only when fresh.
WEB_INPUTS = $(SRCS) $(wildcard $(SRCDIR)/*.h $(SRCDIR)/*/*.h) $(VENDOR_DIR)/tomlc17.h \
             $(wildcard web/*) $(wildcard assets/* assets/*/* assets/*/*/*) \
             $(wildcard levels/* levels/*/*) $(WEB_RAYLIB_LIB) tools/web_csp.py Makefile

web: $(WEB_HTML) $(WEB_DEBUG_HTML) ## Build: WebAssembly game with Emscripten (emcc on PATH)

$(WEB_HTML): $(WEB_INPUTS) | $(OUTDIR)
	emcc $(WEB_CFLAGS) -I$(WEB_RAYLIB_BUILD)/build/raylib/include -I$(SRCDIR) -I$(VENDOR_DIR) $(SRCS) $(WEB_RAYLIB_LIB) -o $@ $(WEB_FLAGS) \
		$(WEB_LINK_FLAGS)
	python3 tools/web_csp.py $@

$(WEB_DEBUG_HTML): $(WEB_INPUTS) | $(OUTDIR)
	emcc $(WEB_CFLAGS) -I$(WEB_RAYLIB_BUILD)/build/raylib/include -I$(SRCDIR) -I$(VENDOR_DIR) $(SRCS) $(WEB_RAYLIB_LIB) -o $@ $(WEB_FLAGS) \
		$(WEB_LINK_FLAGS) --post-js web/debug-boot.js
	python3 tools/web_csp.py $@

dist-native: release asset-budget ## Package: Native game and editor release zip in DISTDIR
	@if [ -n "$${RELEASE_DLL_DIR:-}" ]; then \
		python3 tools/package_release.py --platform "$${RELEASE_PLATFORM:-super-mango-native}" --binary "$(OUTDIR)/release/super-mango" --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-native}.zip" --dll-dir "$${RELEASE_DLL_DIR}" --raylib-build "$(RELEASE_RAYLIB_BUILD)"; \
	else \
		python3 tools/package_release.py --platform "$${RELEASE_PLATFORM:-super-mango-native}" --binary "$(OUTDIR)/release/super-mango" --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-native}.zip" --raylib-build "$(RELEASE_RAYLIB_BUILD)"; \
	fi

# Depends on web so a stale or missing WASM build is rebuilt, never packaged.
dist-wasm: web asset-budget ## Package: WebAssembly release zip in DISTDIR
	python3 tools/package_release.py --wasm --out-dir "$(OUTDIR)" --platform "$${RELEASE_PLATFORM:-super-mango-wasm}" --output "$(DISTDIR)/$${RELEASE_PLATFORM:-super-mango-wasm}.zip" --raylib-build "$(WEB_RAYLIB_BUILD)"

# Every build product lives under OUTDIR/DISTDIR (objects in $(OBJDIR)), plus
# the sibling sanitizer tree that `make sanitize` creates.
clean: ## Other: Remove OUTDIR, OUTDIR-sanitize and DISTDIR
	rm -rf $(OUTDIR) $(OUTDIR)-sanitize $(DISTDIR)

# ── Help ─────────────────────────────────────────────────────────────
# `make help` lists every target whose rule line ends in "## Group: text",
# grouped in the order each group first appears in this file. To document a
# new target, add that comment to its rule line. Plain POSIX awk, so it runs
# the same with BSD, GNU and MSYS2 tools.
.PHONY: help
help: ## Other: List these targets
	@awk -F ' ## ' '/^[a-z][a-z0-9-]*:[^=]*## / { \
		split($$1, rule, ":"); group = $$2; sub(/:.*/, "", group); \
		text = $$2; sub(/^[^:]*: /, "", text); \
		if (!(group in rows)) order[++groups] = group; \
		rows[group] = rows[group] sprintf("  %-18s %s\n", rule[1], text) } \
		END { for (i = 1; i <= groups; i++) printf "%s\n%s\n", order[i], rows[order[i]] }' \
		$(firstword $(MAKEFILE_LIST))
	@echo "Common variables: OUTDIR=out/headless RAYLIB_PLATFORM=memory (no window or GPU),"
	@echo "  BUILD_MODE=release, CC=gcc, EXTRA_CFLAGS=-Werror, RAYLIB_AUDIO=null (no sound device)."
	@echo "Manual art tools (python3 tools/<name>.py, not make targets): analyze_sprite"
	@echo "  (sprite frame grid), gen_fire_sprites (fire palette), generate_favicon (site icons)."
