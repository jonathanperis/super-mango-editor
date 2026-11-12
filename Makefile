# ── Compiler and pinned raylib build ─────────────────────────────────
# Make remains the application entry point; CMake builds the pinned dependency.
# Native and web libraries have separate build directories and never use SDL.
NODE ?= node

# A recipe that fails part-way can leave its target newer than its inputs:
# emcc writes the HTML, then tools/web_csp.py fails to pin its CSP.  Make
# would then treat that unpinned page as up to date.  This deletes the
# target of any failed recipe so the next run rebuilds it.
.DELETE_ON_ERROR:

# GNU make predefines CC as `cc`, so `CC ?= clang` would never take effect:
# CC is never empty. $(origin CC) is "default" exactly when neither the
# command line nor the environment chose a compiler, so only then do we pick
# ours. On Windows that is the MSYS2 UCRT64 Clang (the same toolchain as
# RUNTIME_DLL_PATH below); `make CC=gcc` and friends still win.
ifeq ($(origin CC),default)
ifeq ($(OS),Windows_NT)
CC = /c/msys64/ucrt64/bin/clang.exe
else
CC = clang
endif
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
# Release builds also leave out code and data that nothing uses, and do not
# record where on the builder's disk the sources were:
#   -ffunction-sections -fdata-sections  put every function and every global
#       in a section of its own, so that
#   -Wl,--gc-sections                    the linker can drop the ones no
#       code reaches (GNU ld and lld; Linux and MinGW).
#   -Wl,-dead_strip                      Apple's linker does the same per
#       symbol and needs no special compile flags.
#   -ffile-prefix-map=<this folder>=.    rewrites the build path inside
#       __FILE__ strings, so two checkouts in different folders build the
#       same bytes and no home directory ends up in a shipped binary.
# No link-time optimisation (LTO): it would make release builds differ more
# from the debug builds that the tests run.
# A folder name with a space in it would split the flag in two, so such a
# checkout builds without it rather than failing.
PATH_MAP_CFLAGS = $(if $(word 2,$(CURDIR)),,-ffile-prefix-map=$(CURDIR)=.)
ifeq ($(shell uname -s),Darwin)
GC_CFLAGS  =
GC_LDFLAGS = -Wl,-dead_strip
else
GC_CFLAGS  = -ffunction-sections -fdata-sections
GC_LDFLAGS = -Wl,--gc-sections
endif
MODE_FLAGS_release = -O2 $(HARDEN_CFLAGS) $(GC_CFLAGS) $(PATH_MAP_CFLAGS)
# Release binaries ship without a symbol table (-s): nothing reads it, and
# it was about 12% of each executable (230 KB). On macOS the linker
# deprecates -s, so -Wl,-x drops the local (file-private) symbols instead,
# while it links. That has to happen in the link: the ad-hoc signature is
# made right after it, and a later strip would invalidate that signature.
ifeq ($(shell uname -s),Darwin)
STRIP_LDFLAGS = -Wl,-x
else
STRIP_LDFLAGS = -s
endif
MODE_LDFLAGS_release = $(HARDEN_LDFLAGS) $(GC_LDFLAGS) $(STRIP_LDFLAGS)
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(MODE_FLAGS_$(BUILD_MODE)) -I$(RAYLIB_BUILD)/build/raylib/include $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_RAYLIB_MEMORY,) $(EXTRA_CFLAGS)
# Tests write scratch files under their own OUTDIR (tests/test_paths.h), so
# test, sanitize and coverage trees can run side by side in one checkout.
# MANGO_TESTING turns on the *_test_set_* seams (canned dialog answers,
# injected I/O failures); only test objects get it, never shipped binaries.
TEST_CFLAGS = $(CFLAGS) $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_MEMORY_TESTS,) \
              -DMANGO_TEST_OUTDIR='"$(OUTDIR)"' -DMANGO_TESTING
LIBS    = $(RAYLIB_LIB) $(PLATFORM_LIBS) $(MODE_LDFLAGS_$(BUILD_MODE)) $(EXTRA_LDFLAGS)
# Every flag a compile or link recipe passes lives in a named variable that a
# flag stamp records (see "Flag stamps" near the end), never as literal recipe
# text: Make cannot see a flag edited inside a recipe, so nothing would rebuild.
PROJECT_INCLUDES = -I$(SRCDIR) -I$(VENDOR_DIR)
MATH_LIBS = -lm
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
PLATFORM_LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -framework QuartzCore -lm
else
PLATFORM_LIBS = -lGL -lX11 -lpthread -ldl -lrt -lm
endif
OBJDIR  = $(OUTDIR)/obj
# Flag stamps (see "Flag stamps" near the end): the settings each tree was
# built with. Named here because rule prerequisites are expanded as soon as
# Make reads them, so these names must exist before the first rule uses them.
BUILD_FLAGS_STAMP    = $(OBJDIR)/build-flags.txt
TEST_FLAGS_STAMP     = $(OBJDIR)/test-flags.txt
LINK_FLAGS_STAMP     = $(OBJDIR)/link-flags.txt
FUZZ_FLAGS_STAMP     = $(OBJDIR)/fuzz-flags.txt
RAYLIB_OPTIONS_STAMP = $(RAYLIB_BUILD)/make-options.txt
WEB_RAYLIB_OPTIONS_STAMP = $(WEB_RAYLIB_BUILD)/make-options.txt
WEB_OBJDIR           = $(OBJDIR)/web
WEB_FLAGS_STAMP      = $(WEB_OBJDIR)/build-flags.txt
WEB_LINK_FLAGS_STAMP = $(WEB_OBJDIR)/link-flags.txt
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
# seams, so those two modules come from the test tree as well. So do the
# texture, sound, music and font loaders, whose test builds count loads.
SESSION_RUNTIME_OBJS = $(filter-out $(OBJDIR)/src/main.o $(OBJDIR)/src/shared/audio.o \
                         $(OBJDIR)/src/core/app_session.o $(OBJDIR)/src/input/game_input.o \
                         $(OBJDIR)/src/shared/serializer_io.o $(OBJDIR)/src/shared/texture.o \
                         $(OBJDIR)/src/shared/text.o,$(OBJS)) \
                       $(TEST_AUDIO_OBJ) $(TEST_SESSION_OBJ) $(TEST_GAME_INPUT_OBJ) \
                       $(TEST_TEXTURE_OBJ) $(TEST_TEXT_OBJ) \
                       $(TEST_OBJDIR)/$(SHARED_DIR)/serializer_io.o

# ── Editor (standalone level editor) ─────────────────────────────────
EDITOR_DIR    = src/editor
SHARED_DIR    = src/shared
VENDOR_DIR    = vendor/tomlc17
EDITOR_SRCS   = $(wildcard $(EDITOR_DIR)/*.c) $(wildcard $(SHARED_DIR)/*.c) $(VENDOR_DIR)/tomlc17.c \
                src/surfaces/rail.c src/levels/level_validate.c src/levels/level_ref.c \
                src/levels/level_start.c src/levels/campaign_catalog.c src/levels/level_path.c \
                src/input/input_backend.c
EDITOR_OBJS   = $(patsubst %.c,$(OBJDIR)/%.o,$(EDITOR_SRCS))
EDITOR_DEPS   = $(EDITOR_OBJS:.o=.d)
EDITOR_TARGET = $(OUTDIR)/super-mango-editor
EDITOR_LIBS   = $(LIBS)
TEST_TARGETS  = $(OUTDIR)/level-serializer-test $(OUTDIR)/level-validate-test \
                 $(OUTDIR)/runtime-load-test \
                 $(OUTDIR)/rail-test $(OUTDIR)/entity-utils-test \
                 $(OUTDIR)/collision-test $(OUTDIR)/phase-transition-test \
                 $(OUTDIR)/editor-validation-test $(OUTDIR)/editor-undo-recovery-test \
                 $(OUTDIR)/gameplay-damage-test $(OUTDIR)/gameplay-config-test \
                 $(OUTDIR)/gameplay-score-test \
                 $(OUTDIR)/game-overlay-test $(OUTDIR)/game-events-test \
                 $(OUTDIR)/session-test $(OUTDIR)/game-checkpoint-test \
                 $(OUTDIR)/gameplay-mechanics-test $(OUTDIR)/editor-ui-test
LEVEL_FILES   = $(wildcard levels/*.toml) $(wildcard levels/labs/*.toml)
SMOKE_LEVELS  = $(LEVEL_FILES)
SMOKE_FRAMES  ?= 5
SMOKE_SEED    ?= 1
SMOKE_SEEDS   ?= 1 7 23
# Tests recompile some game/editor sources with TEST_CFLAGS (a few also swap
# raylib calls for test doubles), so those objects live in their own tree that
# mirrors the source path: src/screens/hud.c -> $(TEST_OBJDIR)/src/screens/hud.o.
# One pattern rule (after the fuzz targets) builds every one of them; adding a
# test object is just a new variable here plus its use on the program's line in
# the "Native test programs" section below.
TEST_OBJDIR = $(OBJDIR)/tests
TEST_SERIALIZER_OBJS = $(patsubst %,$(TEST_OBJDIR)/$(SHARED_DIR)/%.o,serializer serializer_emit \
                       serializer_io serializer_load serializer_load_header \
                       serializer_parse serializer_save serializer_types)
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
# These need no test flags, so tests link the game's own objects.
TEST_GAME_TERMINAL_OBJ = $(OBJDIR)/src/core/game_terminal.o
TEST_GAME_RANDOM_OBJ = $(OBJDIR)/src/core/game_random.o
TEST_GAME_GHOST_OBJ = $(OBJDIR)/src/core/game_ghost.o
TEST_GAME_CHECKPOINT_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/core/game_checkpoint.o
TEST_HUD_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/screens/hud.o
TEST_EDITOR_UI_OBJ = $(TEST_OBJDIR)/$(SHARED_DIR)/ui.o \
                   $(TEST_OBJDIR)/$(SHARED_DIR)/ui_widgets.o \
                   $(TEST_OBJDIR)/$(SHARED_DIR)/ui_scroll.o
TEST_EDITOR_OBJS = $(patsubst %,$(TEST_OBJDIR)/$(EDITOR_DIR)/%.o,editor_validation editor_files \
                   editor_recovery editor_session editor_undo_apply snapshot_equal entity_meta editor_campaign) \
                   $(TEST_EDITOR_UI_OBJ) \
                   $(patsubst %,$(TEST_OBJDIR)/$(EDITOR_DIR)/%.o,tools hit_test editor_clipboard \
                   editor_events canvas editor_panels editor_layout palette properties asset_choices \
                   editor_textures \
                   editor_playtest editor_shortcuts file_dialog dialog_choice undo \
                   properties_entities properties_config \
                   tools_bounds tools_place tools_drag tools_nudge \
                   editor_handles editor_entity_list)
# The editor's own window, chrome and frame, for editor-ui-test.
TEST_EDITOR_FRAME_OBJS = $(patsubst %,$(TEST_OBJDIR)/$(EDITOR_DIR)/%.o,editor editor_chrome \
                   editor_frame editor_minimap)
TEST_AUDIO_OBJ     = $(TEST_OBJDIR)/$(SHARED_DIR)/audio.o
TEST_TEXTURE_OBJ   = $(TEST_OBJDIR)/$(SHARED_DIR)/texture.o
TEST_TEXT_OBJ      = $(TEST_OBJDIR)/$(SHARED_DIR)/text.o
TEST_SESSION_OBJ   = $(TEST_OBJDIR)/$(SRCDIR)/core/app_session.o
TEST_INPUT_BACKEND_OBJ = $(TEST_OBJDIR)/$(SRCDIR)/input/input_backend.o
TEST_LIBS           = $(LIBS)
# Each test's own .c files (tests/rail_test.c and friends) compile through the
# same test-object rule, into $(TEST_OBJDIR)/tests/, so their -MMD dependency
# files notice edits to shared test headers such as tests/test_paths.h. The
# fuzz harnesses and the parser probe build separately (see their rules).
TEST_SOURCE_DIR  = $(TEST_OBJDIR)/tests
TEST_SOURCE_OBJS = $(patsubst %.c,$(TEST_OBJDIR)/%.o,$(filter-out tests/fuzz_%.c tests/parser_allocation_test.c,$(wildcard tests/*.c)))
# tools/level_check.c (validate-levels) is compiled the same way, with CFLAGS.
TOOL_OBJS = $(OBJDIR)/tools/level_check.o
PLATFORM_OBJS = $(addprefix $(OBJDIR)/src/shared/,asset_cache.o audio.o graphics.o platform.o text.o texture.o) $(OBJDIR)/src/input/input_backend.o
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

# raylib is rebuilt only when something that shapes it changes: the pin, the
# patches, the gamepad database, the build script, or the options below (kept in a flags stamp; see
# "Flag stamps" further down). build_raylib.py always re-runs CMake, and CMake
# leaves libraylib.a untouched when nothing needed recompiling, so objects
# that depend on the library rebuild only when the library really changed.
# The rule therefore builds a separate "done" file, touched after every
# successful run; a deleted library forces a run even when that file is fresh.
RAYLIB_INPUTS = vendor/raylib/manifest.json vendor/raylib/patches.json tools/build_raylib.py \
                vendor/raylib/gamecontrollerdb/gamecontrollerdb.txt
RAYLIB_OPTIONS = --platform $(RAYLIB_PLATFORM) --cc "$(CC)" --mode $(BUILD_MODE) \
                 $(if $(findstring -fsanitize,$(CFLAGS)),--sanitize,) $(if $(filter null,$(RAYLIB_AUDIO)),--null-audio,)
WEB_RAYLIB_OPTIONS = --platform web --mode release
RAYLIB_DONE = $(RAYLIB_BUILD)/build-done.stamp
WEB_RAYLIB_DONE = $(WEB_RAYLIB_BUILD)/build-done.stamp

# mark_raylib_done touches the "done" file, then, when the library ($(1)) is
# newer than every input, gives the done file the library's own timestamp.
# That keeps `make -n` honest: it only lists objects as stale when the library
# really is older than the done file (CMake found nothing to rebuild).
define mark_raylib_done
@touch $@; for input in $(filter-out FORCE,$^); do \
	[ "$(1)" -nt "$$input" ] || exit 0; done; touch -r "$(1)" $@
endef

$(RAYLIB_DONE): $(RAYLIB_INPUTS) $(RAYLIB_OPTIONS_STAMP) $(if $(wildcard $(RAYLIB_LIB)),,FORCE)
	python3 tools/build_raylib.py --build-dir "$(RAYLIB_BUILD)" $(RAYLIB_OPTIONS) $(RAYLIB_ARCHIVE_ARG)
	$(call mark_raylib_done,$(RAYLIB_LIB))

$(WEB_RAYLIB_DONE): $(RAYLIB_INPUTS) $(WEB_RAYLIB_OPTIONS_STAMP) $(if $(wildcard $(WEB_RAYLIB_LIB)),,FORCE) | web-toolchain
	python3 tools/build_raylib.py --build-dir "$(WEB_RAYLIB_BUILD)" $(WEB_RAYLIB_OPTIONS) $(RAYLIB_ARCHIVE_ARG)
	$(call mark_raylib_done,$(WEB_RAYLIB_LIB))

# The libraries are made by the rules above; these empty recipes (the ";")
# only tell Make so. Make re-reads a library's timestamp afterwards, so its
# users rebuild only if CMake actually rewrote it.
$(RAYLIB_LIB): $(RAYLIB_DONE) ;
$(WEB_RAYLIB_LIB): $(WEB_RAYLIB_DONE) ;

# CI keys its compiled-raylib caches on this text: the option definitions as
# written, not their per-run values (the compiler is fingerprinted separately).
# Other Makefile edits then keep the caches. A restored tree stays correct
# either way, because its make-options.txt stamp still triggers a rebuild
# when the options a run passes differ from the ones it was built with.
.PHONY: raylib-options
raylib-options:
	@printf '%s\n' $(call shell_quote,$(value RAYLIB_OPTIONS)) $(call shell_quote,$(value WEB_RAYLIB_OPTIONS))

$(OUTDIR):
	mkdir -p $(OUTDIR) $(OBJDIR) $(OBJDIR)/tests

$(TARGET): $(OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

$(OBJDIR)/$(SRCDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PROJECT_INCLUDES) -MMD -MP -c -o $@ $<

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
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(EDITOR_LIBS)
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
# Adding a native test takes one name in TEST_TARGETS (near the top) and, when
# it links more than its own tests/<name>.c, one line of extra objects in the
# "Native test programs" section below. The run line here and the link rule
# there are generated from TEST_TARGETS.
#
# TEST_ENV_<program> is extra environment for one program's run line. These
# two open a real window on the desktop backend (MANGO_TEST_WINDOW=1); the
# others never need one.
TEST_ENV_editor-validation-test = MANGO_TEST_WINDOW=1
TEST_ENV_session-test = MANGO_TEST_WINDOW=1
# A variable holding one line break. $(foreach) joins its results with
# spaces; ending each result with this makes every test its own recipe
# line, so Make stops at the first one that fails.
define newline


endef
run_native_test = $(strip $(TEST_ENV_$(notdir $(1))) $(RUN_PREFIX) "$(abspath $(1))")
# Python tests of the build and docs tools. They need no compiler, window or
# network. tools/check_docs_drift.py compares this list (and NODE_TESTS
# below) with the files in tests/ and with the counts the docs quote.
PYTHON_TESTS = tests/validate_levels_test.py tests/gen_sounds_test.py \
               tests/fuzz_dict_test.py tests/coverage_baseline_test.py \
               tests/clang_tidy_report_test.py tests/doctor_test.py \
               tests/build_raylib_test.py tests/gen_compile_commands_test.py \
               tests/roadmap_quality_test.py tests/import_raygui_icons_test.py

test: $(OUTDIR) $(TEST_TARGETS) web-host-contract parser-allocation-probe parser-encoding-probe ## Test: Native test binaries plus the Python and Node host checks
	$(foreach script,$(PYTHON_TESTS),python3 $(script)$(newline))
	$(foreach program,$(TEST_TARGETS),$(call run_native_test,$(program))$(newline))

$(TEST_TARGETS): | $(OUTDIR)
$(filter-out $(OUTDIR)/session-test $(OUTDIR)/game-events-test,$(TEST_TARGETS)): $(PLATFORM_OBJS)
$(OUTDIR)/game-events-test: $(filter-out $(OBJDIR)/src/input/input_backend.o,$(PLATFORM_OBJS)) $(TEST_INPUT_BACKEND_OBJ) $(TEST_SOURCE_DIR)/input_backend_test.o
# A rebuilt dependency must refresh consumers and relink executables. Exported
# headers retain upstream timestamps, so header mtimes alone are insufficient.
# The flag stamps recompile the objects whose compile flags changed (game and
# test copies separately) and relink every program when a link flag changed;
# link recipes pass only $(filter %.o,$^), so the stamp never reaches the linker.
$(sort $(OBJS) $(EDITOR_OBJS) $(TEST_OBJECTS) $(TOOL_OBJS)): $(RAYLIB_LIB)
$(filter-out $(TEST_OBJDIR)/%,$(sort $(OBJS) $(EDITOR_OBJS) $(TEST_OBJECTS) $(TOOL_OBJS))): $(BUILD_FLAGS_STAMP)
$(filter $(TEST_OBJDIR)/%,$(TEST_OBJECTS)): $(TEST_FLAGS_STAMP)
$(TARGET) $(EDITOR_TARGET) $(TEST_TARGETS): $(LINK_FLAGS_STAMP)

# Extra standalone parser probes; keep the 18-regression-binary inventory above.
.PHONY: parser-allocation-probe parser-encoding-probe
parser-allocation-probe: $(OUTDIR)/parser-allocation-probe
	$(RUN_PREFIX) "$(abspath $<)"

$(OUTDIR)/parser-allocation-probe: tests/parser_allocation_test.c $(VENDOR_DIR)/tomlc17.c $(VENDOR_DIR)/tomlc17.h \
		$(TEST_FLAGS_STAMP) $(LINK_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(MATH_LIBS)

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
	python3 tests/schema_parity_test.py "$(abspath $(LEVEL_CHECK))"

$(LEVEL_CHECK): $(TOOL_OBJS) $(LEVEL_CHECK_OBJS) $(LINK_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(MATH_LIBS)

$(OBJDIR)/tools/%.o: tools/%.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PROJECT_INCLUDES) -MMD -MP -c -o $@ $<

-include $(TOOL_OBJS:.o=.d)

# The browser-side scripts, run under Node with a stand-in document; no
# browser and no network. boot_test covers web/boot.js on its own,
# web_host_test how the two pages use it.
NODE_TESTS = tests/boot_test.cjs tests/web_host_test.cjs tests/profile_storage_test.cjs \
             tests/touch_controls_test.cjs tests/keyboard_scope_test.cjs \
             tests/audio_unlock_test.cjs
# Python tests of the Web page tools and of packaging.
WEB_PYTHON_TESTS = tests/web_boot_contract_test.py tests/web_shell_test.py \
                   tests/web_debug_page_test.py tests/wasm_artifacts_test.py \
                   tests/package_release_test.py tests/filter_codeql_sarif_test.py
web-host-contract: ## Check: Web shell, storage and packaging contract tests
	python3 tools/check_web_boot_contract.py
	$(foreach script,$(NODE_TESTS),$(NODE) $(script)$(newline))
	$(foreach script,$(WEB_PYTHON_TESTS),python3 $(script)$(newline))

# clangd / IDE / clang-tidy compile database. The script is handed the flags
# this Makefile compiles with (mode flags, the Memory backend define,
# EXTRA_CFLAGS, the raylib include path of this OUTDIR, the test flags and the
# per-object test renames), so tools analyse the build you would get from the
# same command line: `make compile-commands RAYLIB_PLATFORM=memory OUTDIR=...`
# describes that tree. --flag=VALUE keeps argparse from reading a value that
# starts with "-" as another option.
# COMPILE_DB_TEST_SOURCES: the game sources that are also compiled with the
# test flags (every object under $(TEST_OBJDIR) that is not a tests/*.c file).
COMPILE_DB_TEST_SOURCES = $(patsubst $(TEST_OBJDIR)/%.o,%.c,$(filter-out $(TEST_SOURCE_DIR)/%,$(filter $(TEST_OBJDIR)/%,$(sort $(TEST_OBJECTS)))))
compile-commands: ## Build: Write compile_commands.json for clangd and IDEs
	python3 tools/gen_compile_commands.py --cc "$(CC)" --objdir "$(OBJDIR)" \
		--cflags=$(call shell_quote,$(CFLAGS)) \
		--includes=$(call shell_quote,$(PROJECT_INCLUDES)) \
		--test-cflags=$(call shell_quote,$(TEST_CFLAGS)) \
		--test-sources=$(call shell_quote,$(COMPILE_DB_TEST_SOURCES)) \
		$(foreach v,$(sort $(filter TEST_OBJ_FLAGS_%,$(.VARIABLES))),--object-flags $(call shell_quote,$(patsubst TEST_OBJ_FLAGS_%,%,$(v))=$($(v))))

# clang-tidy over src/ and tools/ with the checks in .clang-tidy; prints the
# findings and exits 0 (CI's static-analysis.yml shows them in its summary).
# CLANG_TIDY=/path/to/clang-tidy picks another version.
.PHONY: tidy
tidy: compile-commands ## Check: clang-tidy findings in src/ and tools/ (report only)
	python3 tools/run_clang_tidy.py

# Reports missing host tools. Optional tools (zenity, Bun, emcc, audio) warn
# and do not fail, so a native build machine can still be "ready".
.PHONY: doctor
doctor: ## Check: Host compilers, libraries and optional tools
	python3 tools/doctor.py

level-catalog: ## Generate: Level catalog page from the campaign manifest
	python3 tools/generate_level_catalog.py

overlay-snapshots: ## Generate: Overlay text snapshots page
	python3 tools/generate_overlay_snapshots.py

# Regenerate vendor/raygui/raygui_icons.h, the editor's toolbar icons, from
# the pinned raygui.h. Needed after adding an icon to the ICONS table in the
# script or after changing the pin in vendor/raygui/manifest.json (see
# vendor/raygui/README.md). It downloads raygui.h and checks its SHA-256;
# pass RAYGUI_SOURCE=<file> to read a copy you already have instead.
.PHONY: raygui-icons
RAYGUI_SOURCE ?=
raygui-icons: ## Generate: Editor icon bitmaps from the pinned raygui.h
	python3 tools/import_raygui_icons.py $(if $(RAYGUI_SOURCE),--source "$(RAYGUI_SOURCE)",)

# Regenerate every assets/sounds QOA file from tools/gen_sounds.py recipes.
.PHONY: sounds
sounds: ## Generate: Every assets/sounds QOA file from tools/gen_sounds.py
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
	python3 tools/run_scripted_smoke.py --binary $(TARGET) --editor $(EDITOR_TARGET) --frames $(SMOKE_FRAMES) --seeds $(SMOKE_SEEDS) --replay-dir $(OUTDIR)/replays-smoke

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
# Optional floor for the TOTAL line coverage, in percent (CI sets one). Empty
# means "report only". The summary is also kept in summary.txt for CI.
COVERAGE_MIN ?=
# Each file's line coverage is also compared with its number in
# COVERAGE_BASELINE (tools/check_coverage_baseline.py). COVERAGE_TOLERANCE=N
# fails the run when a file is more than N points below it (CI sets 2);
# empty means "report only". `make coverage-baseline` rewrites the file.
COVERAGE_BASELINE = tools/coverage_baseline.txt
COVERAGE_TOLERANCE ?=

.PHONY: coverage coverage-baseline
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
		-ignore-filename-regex=$(COVERAGE_IGNORE) > "$(COVERAGE_OUTDIR)/summary.txt"
	@cat "$(COVERAGE_OUTDIR)/summary.txt"
	@echo "coverage: per-line view: $(LLVM_COV) show $(firstword $(COVERAGE_BINS)) -instr-profile=$(COVERAGE_OUTDIR)/tests.profdata <file.c>"
# The TOTAL row lists region, function and line coverage (then branches, on
# newer llvm-cov) as percentages, so the third NN.NN% field is the line total.
	@if [ -n "$(COVERAGE_MIN)" ]; then \
		awk -v min="$(COVERAGE_MIN)" ' \
			$$1 == "TOTAL" { for (i = 2; i <= NF; i++) if ($$i ~ /%$$/ && ++n == 3) lines = $$i } \
			END { if (lines == "") { print "coverage: no TOTAL line coverage in summary.txt"; exit 1 } \
			      sub(/%$$/, "", lines); \
			      if (lines + 0 < min + 0) { printf "coverage: line coverage %s%% is below the %s%% floor\n", lines, min; exit 1 } \
			      printf "coverage: line coverage %s%% meets the %s%% floor\n", lines, min }' \
			"$(COVERAGE_OUTDIR)/summary.txt"; \
	fi
	python3 tools/check_coverage_baseline.py "$(COVERAGE_OUTDIR)/summary.txt" --baseline $(COVERAGE_BASELINE) \
		$(if $(COVERAGE_TOLERANCE),--tolerance $(COVERAGE_TOLERANCE),)

# Run coverage, then record every file's line coverage as the new baseline.
# Use the CI configuration (Linux, clang, Memory backend), which the baseline
# describes: make coverage-baseline CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
coverage-baseline: coverage ## Generate: Refresh tools/coverage_baseline.txt from a coverage run
	python3 tools/check_coverage_baseline.py "$(COVERAGE_OUTDIR)/summary.txt" --baseline $(COVERAGE_BASELINE) --write

# ── Fuzzing (POSIX) ──────────────────────────────────────────────────
# Each harness defines LLVMFuzzerTestOneInput.  fuzz-corpus links it with
# tests/fuzz_replay_main.c under ASan/UBSan and replays the seed inputs
# (FUZZ_MUTATIONS=N adds N blind mutations per seed; the sanitizer CI job
# passes a small N).  fuzz links it with libFuzzer for coverage-guided
# search; Apple clang lacks libFuzzer, so
# point FUZZ_CC at Homebrew LLVM or a Linux clang.  Both honour
# EXTRA_CFLAGS, so CI's -Werror covers the harnesses under `make sanitize`.
FUZZ_FLAGS = -std=c11 -g -O1 -Wall -Wextra -Wpedantic $(PROJECT_INCLUDES) \
             -I$(RAYLIB_BUILD)/build/raylib/include \
             $(if $(filter memory,$(RAYLIB_PLATFORM)),-DMANGO_RAYLIB_MEMORY,) \
             -fno-omit-frame-pointer -fsanitize=address,undefined \
             -fno-sanitize-recover=undefined $(EXTRA_CFLAGS)
# The level loader and saver, without a harness: both level harnesses use it.
FUZZ_SERIALIZER_SRCS = $(wildcard $(SHARED_DIR)/serializer*.c) \
                       src/levels/level_validate.c src/levels/level_ref.c $(VENDOR_DIR)/tomlc17.c
FUZZ_LEVEL_SRCS = tests/fuzz_level_parse.c $(FUZZ_SERIALIZER_SRCS)
# The editor's save -> load round trip (tests/fuzz_editor_roundtrip.c).
FUZZ_EDITOR_SRCS = tests/fuzz_editor_roundtrip.c $(FUZZ_SERIALIZER_SRCS)
# input_backend.c maps the pointer through graphics.c's display_viewport.
FUZZ_PROFILE_SRCS = tests/fuzz_profile_decode.c src/core/game_profile.c \
                    src/core/game_ghost.c src/core/game_ghost_file.c \
                    src/input/game_bindings.c src/input/input_backend.c \
                    src/shared/graphics.c \
                    src/levels/level_ref.c src/shared/serializer_io.c \
                    src/shared/platform.c $(VENDOR_DIR)/tomlc17.c
# The campaign manifest loader and the experiment capture decoder
# (tests/fuzz_campaign_experiment.c). The campaign loads each listed level,
# so it needs the level loader too.
FUZZ_CAMPAIGN_SRCS = tests/fuzz_campaign_experiment.c src/levels/campaign_catalog.c \
                     src/levels/level_path.c src/core/game_experiment_file.c \
                     src/shared/platform.c $(FUZZ_SERIALIZER_SRCS)
FUZZ_HEADERS = $(wildcard $(SRCDIR)/*.h $(SRCDIR)/*/*.h $(VENDOR_DIR)/*.h)
# Seeds are read in place: shipped levels, schema fixtures, extra edge cases.
FUZZ_LEVEL_SEEDS = levels levels/labs tests/fixtures/serializer_v1 \
                   tests/fixtures/runtime tests/fuzz/corpus/level
FUZZ_PROFILE_SEEDS = tests/fuzz/corpus/profile
# Every level seed is an editor seed too; its own folder adds levels that
# use many fields at once, so a field the saver drops shows up quickly.
FUZZ_EDITOR_SEEDS = $(FUZZ_LEVEL_SEEDS) tests/fuzz/corpus/editor
# Manifests (shipped, broken fixtures, edge cases) and captures.
FUZZ_CAMPAIGN_SEEDS = levels/campaigns tests/fixtures/campaign_manifest \
                      tests/fuzz/corpus/campaign tests/fuzz/corpus/experiment
FUZZ_MUTATIONS ?= 0
FUZZ_SECONDS ?= 60
# Words libFuzzer may splice into an input, and the longest input it builds:
# 64 KiB is several times the largest shipped level, and inputs past the
# readers' own limits are refused before any parsing.
FUZZ_DICT = tests/fuzz/toml.dict
FUZZ_RUN_FLAGS = -max_total_time=$(FUZZ_SECONDS) -close_fd_mask=2 \
                 -dict=$(FUZZ_DICT) -max_len=65536
FUZZ_CC ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang) clang)

.PHONY: fuzz-corpus fuzz
fuzz-corpus: $(OUTDIR)/fuzz-level-replay $(OUTDIR)/fuzz-profile-replay $(OUTDIR)/fuzz-editor-replay $(OUTDIR)/fuzz-campaign-replay ## Test: Replay the fuzz seeds under ASan/UBSan (FUZZ_MUTATIONS=N)
	"$(abspath $(OUTDIR))/fuzz-level-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_LEVEL_SEEDS)
	"$(abspath $(OUTDIR))/fuzz-profile-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_PROFILE_SEEDS)
	"$(abspath $(OUTDIR))/fuzz-editor-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_EDITOR_SEEDS)
	"$(abspath $(OUTDIR))/fuzz-campaign-replay" -mutate=$(FUZZ_MUTATIONS) $(FUZZ_CAMPAIGN_SEEDS)

# level.h includes raylib's headers, so every replay waits for raylib's build
# even where it links no raylib code.
$(OUTDIR)/fuzz-level-replay: tests/fuzz_replay_main.c $(FUZZ_LEVEL_SRCS) $(FUZZ_HEADERS) $(RAYLIB_LIB) $(FUZZ_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_LEVEL_SRCS) $(MATH_LIBS)

$(OUTDIR)/fuzz-editor-replay: tests/fuzz_replay_main.c $(FUZZ_EDITOR_SRCS) $(FUZZ_HEADERS) $(RAYLIB_LIB) $(FUZZ_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_EDITOR_SRCS) $(MATH_LIBS)

$(OUTDIR)/fuzz-profile-replay: tests/fuzz_replay_main.c $(FUZZ_PROFILE_SRCS) $(FUZZ_HEADERS) $(RAYLIB_LIB) $(FUZZ_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_PROFILE_SRCS) $(LIBS)

$(OUTDIR)/fuzz-campaign-replay: tests/fuzz_replay_main.c $(FUZZ_CAMPAIGN_SRCS) $(FUZZ_HEADERS) $(RAYLIB_LIB) $(FUZZ_FLAGS_STAMP) | $(OUTDIR)
	$(CC) $(FUZZ_FLAGS) -o $@ tests/fuzz_replay_main.c $(FUZZ_CAMPAIGN_SRCS) $(MATH_LIBS)

# -close_fd_mask=2 hides the loaders' per-input error lines; libFuzzer keeps
# its own and the sanitizer reports.  Crashes land in $(OUTDIR)/fuzz/ and
# replay with the matching program: $(OUTDIR)/fuzz-level-replay <crash-file>
# (or fuzz-profile-replay, fuzz-editor-replay, fuzz-campaign-replay).
#
# Each harness gets its own folder under $(OUTDIR)/fuzz/. libFuzzer writes
# every input that reached new code there, and reads them back on the next
# run, so a folder kept between runs (the weekly CI job caches it) lets the
# search carry on from where it stopped instead of starting from the seeds.
fuzz: $(RAYLIB_LIB) | $(OUTDIR) ## Test: libFuzzer search for FUZZ_SECONDS (needs LLVM clang)
	@printf 'int LLVMFuzzerTestOneInput(const char *d, unsigned long n) { (void)d; (void)n; return 0; }\n' \
		| $(FUZZ_CC) -x c -fsanitize=fuzzer -o /dev/null - 2>/dev/null || { \
		echo "fuzz: '$(FUZZ_CC)' cannot link libFuzzer (Apple clang does not ship it)."; \
		echo "fuzz: install LLVM (brew install llvm) and run"; \
		echo "      make fuzz FUZZ_CC=\$$(brew --prefix llvm)/bin/clang"; \
		echo "fuzz: without libFuzzer, 'make fuzz-corpus FUZZ_MUTATIONS=500' runs blind mutations."; \
		exit 1; }
	mkdir -p $(OUTDIR)/fuzz/level $(OUTDIR)/fuzz/profile $(OUTDIR)/fuzz/editor $(OUTDIR)/fuzz/campaign
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/level-fuzzer $(FUZZ_LEVEL_SRCS) $(MATH_LIBS)
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/profile-fuzzer $(FUZZ_PROFILE_SRCS) $(LIBS)
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/editor-fuzzer $(FUZZ_EDITOR_SRCS) $(MATH_LIBS)
	$(FUZZ_CC) $(FUZZ_FLAGS) -fsanitize=fuzzer -o $(OUTDIR)/fuzz/campaign-fuzzer $(FUZZ_CAMPAIGN_SRCS) $(MATH_LIBS)
	"$(abspath $(OUTDIR))/fuzz/level-fuzzer" $(FUZZ_RUN_FLAGS) \
		-artifact_prefix=$(OUTDIR)/fuzz/level- $(OUTDIR)/fuzz/level $(FUZZ_LEVEL_SEEDS)
	"$(abspath $(OUTDIR))/fuzz/profile-fuzzer" $(FUZZ_RUN_FLAGS) \
		-artifact_prefix=$(OUTDIR)/fuzz/profile- $(OUTDIR)/fuzz/profile $(FUZZ_PROFILE_SEEDS)
	"$(abspath $(OUTDIR))/fuzz/editor-fuzzer" $(FUZZ_RUN_FLAGS) \
		-artifact_prefix=$(OUTDIR)/fuzz/editor- $(OUTDIR)/fuzz/editor $(FUZZ_EDITOR_SEEDS)
	"$(abspath $(OUTDIR))/fuzz/campaign-fuzzer" $(FUZZ_RUN_FLAGS) \
		-artifact_prefix=$(OUTDIR)/fuzz/campaign- $(OUTDIR)/fuzz/campaign $(FUZZ_CAMPAIGN_SEEDS)

# Test objects: one rule for every source. $(OUTDIR) is skipped once out/
# exists, so the recipe also creates its own directory; otherwise a deleted
# out/obj/tests would break every later `make test`.
# Vendored tomlc17 builds without project include paths, as in the game.
TEST_OBJ_INCLUDES = $(if $(filter $(VENDOR_DIR)/%,$<),,$(PROJECT_INCLUDES))
# Per-object extras: the rule picks TEST_OBJ_FLAGS_<source path without .c>.
# The test stamp records every TEST_OBJ_FLAGS_* variable and the text of these
# two lookups, so adding, changing or retargeting an extra rebuilds the tests.
TEST_OBJ_FLAGS = $(TEST_OBJ_FLAGS_$(<:.c=))
$(TEST_OBJDIR)/%.o: %.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) $(TEST_OBJ_FLAGS) $(TEST_OBJ_INCLUDES) -MMD -MP -c -o $@ $<

# These rename raylib/GLFW calls so the tests can supply recording doubles in
# their place.
TEST_OBJ_FLAGS_src/shared/audio = -DSetMusicVolume=test_SetMusicVolume \
	-DLoadSoundAlias=test_LoadSoundAlias -DSetSoundVolume=test_SetSoundVolume \
	-DUnloadSoundAlias=test_UnloadSoundAlias -DUnloadSound=test_UnloadSound \
	-DSetSoundPan=test_SetSoundPan -DSetSoundPitch=test_SetSoundPitch
TEST_OBJ_FLAGS_src/core/app_session = -DSetWindowSize=test_SetWindowSize \
	-DToggleBorderlessWindowed=test_ToggleBorderlessWindowed -DIsWindowState=test_IsWindowState
TEST_OBJ_FLAGS_src/input/input_backend = -UMANGO_RAYLIB_MEMORY \
	-DIsWindowReady=test_input_window_ready -DGetScreenWidth=test_input_screen_width -DGetScreenHeight=test_input_screen_height \
	-DglfwGetCurrentContext=test_input_current_context -DglfwGetCursorPos=test_input_cursor_pos \
	-DglfwSetKeyCallback=test_input_set_key -DglfwSetCharCallback=test_input_set_char \
	-DglfwSetMouseButtonCallback=test_input_set_button -DglfwSetCursorPosCallback=test_input_set_cursor \
	-DglfwSetScrollCallback=test_input_set_scroll \
	-DIsGamepadAvailable=test_input_gamepad_available -DglfwJoystickIsGamepad=test_input_joystick_is_gamepad \
	-DGetGamepadName=test_input_gamepad_name

# ── Native test programs ─────────────────────────────────────────────
# Every program in TEST_TARGETS is linked by the one rule this template
# writes: out/<name>-test is made from tests/<name>_test.c (the same name
# with "_" for "-") plus whatever extra objects the lines further down list
# for it, with the same command for all of them. $(eval) reads the template
# once per program, as if the rule had been typed out by hand; the doubled
# $$ keep $@ and $^ for the moment the rule runs.
#
# The editor tests link the editor's libraries, the others the game's (the
# two lists are the same today).
TEST_LINK_LIBS = $(if $(filter editor-%,$(notdir $@)),$(EDITOR_LIBS),$(TEST_LIBS))
define native_test_rule
$(1): $$(TEST_SOURCE_DIR)/$(subst -,_,$(notdir $(1))).o
	$$(CC) $$(TEST_CFLAGS) -o $$@ $$(filter %.o,$$^) $$(TEST_LINK_LIBS)
endef
$(foreach program,$(TEST_TARGETS),$(eval $(call native_test_rule,$(program))))

# Extra objects, one line per program. A line with no recipe only adds
# prerequisites to the rule above; a program that needs nothing beyond its
# own tests/<name>_test.c has no line here at all.
$(OUTDIR)/level-serializer-test: $(TEST_SOURCE_DIR)/parser_boundary_test.o \
		$(TEST_SERIALIZER_OBJS) $(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)

# level_validate.c shares the levels/<name>.toml rule from level_ref.c.
$(OUTDIR)/level-serializer-test $(OUTDIR)/level-validate-test \
$(OUTDIR)/runtime-load-test $(OUTDIR)/editor-validation-test \
$(OUTDIR)/editor-undo-recovery-test \
$(OUTDIR)/editor-ui-test: $(OBJDIR)/src/levels/level_ref.o

# "Playtest from here" checks a start point with the game's own rules, and
# the Campaign view checks the manifest with them.
$(OUTDIR)/editor-validation-test $(OUTDIR)/editor-undo-recovery-test \
$(OUTDIR)/editor-ui-test: $(OBJDIR)/src/levels/level_start.o \
		$(OBJDIR)/src/levels/campaign_catalog.o $(OBJDIR)/src/levels/level_path.o

# The editor knows every document by its absolute path (editor_absolute_path).
$(OUTDIR)/editor-validation-test $(OUTDIR)/editor-undo-recovery-test \
$(OUTDIR)/editor-ui-test: $(OBJDIR)/src/shared/asset_root.o

$(OUTDIR)/level-validate-test: $(TEST_VALIDATE_OBJ)

$(OUTDIR)/runtime-load-test: $(TEST_LEVEL_LOADER_OBJ) \
		$(TEST_GAME_RANDOM_OBJ) \
		$(TEST_VALIDATE_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_RAIL_OBJ) \
		$(TEST_SPIKE_BLOCK_OBJ) $(TEST_FLOAT_PLATFORM_OBJ) \
		$(TEST_BOUNCEPAD_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)

$(OUTDIR)/rail-test: $(TEST_RAIL_OBJ)

$(OUTDIR)/entity-utils-test: $(TEST_ENTITY_UTILS_OBJ)

$(OUTDIR)/collision-test: $(TEST_SPIKE_PLATFORM_OBJ) \
		$(TEST_FISH_OBJ) $(TEST_CIRCULAR_SAW_OBJ) $(TEST_ENTITY_UTILS_OBJ) $(TEST_GAME_RANDOM_OBJ)

$(OUTDIR)/phase-transition-test: $(TEST_PHASE_OBJ)

# The three editor programs share one set of editor objects. editor-validation
# drives the validator and the save paths; editor-undo-recovery calls
# undo/redo, the clipboard and crash recovery directly on a bare EditorState
# (no window); editor-ui drives the editor as a designer does (events in,
# document/undo state out) and so also needs the window, chrome and frame.
$(OUTDIR)/editor-validation-test $(OUTDIR)/editor-undo-recovery-test \
$(OUTDIR)/editor-ui-test: $(TEST_EDITOR_OBJS) $(TEST_RAIL_OBJ) $(TEST_SERIALIZER_OBJS) \
		$(TEST_VALIDATE_OBJ) $(TEST_TOMLC_OBJ)
$(OUTDIR)/editor-ui-test: $(TEST_EDITOR_FRAME_OBJS)

$(OUTDIR)/gameplay-damage-test: $(TEST_COLLISION_DAMAGE_OBJ) $(TEST_GAME_OVERLAY_OBJ) \
		$(TEST_GAME_CHECKPOINT_OBJ) $(TEST_HUD_OBJ) $(TEST_GAME_GHOST_OBJ)

$(OUTDIR)/gameplay-config-test: $(TEST_GAME_CAMERA_OBJ) $(TEST_LEVEL_PHYSICS_OBJ) $(TEST_PLAYER_LIFECYCLE_OBJ)

$(OUTDIR)/gameplay-score-test: $(TEST_GAME_SCORE_OBJ)

$(OUTDIR)/game-overlay-test: $(TEST_GAME_OVERLAY_OBJ)

$(OUTDIR)/game-events-test: $(TEST_GAME_EVENTS_OBJ) $(TEST_GAME_INPUT_OBJ) $(TEST_WEB_INPUT_OBJ) \
		$(TEST_GAME_OVERLAY_OBJ) $(TEST_GAME_TERMINAL_OBJ) $(TEST_SETTINGS_OBJ) \
		$(TEST_BINDINGS_OBJ) $(TEST_EDITOR_UI_OBJ)

# session-test is several test files in one program. The manifest is listed
# so the program is rebuilt, and so re-run, when the campaign changes; the
# link command only takes the .o files.
$(OUTDIR)/session-test: $(TEST_SOURCE_DIR)/game_profile_test.o $(TEST_SOURCE_DIR)/simulation_test.o \
		$(TEST_SOURCE_DIR)/audio_contract_test.o $(TEST_SOURCE_DIR)/web_frame_pacing_test.o \
		$(TEST_SOURCE_DIR)/asset_cache_test.o $(SESSION_RUNTIME_OBJS) levels/campaigns/main.toml

$(OUTDIR)/game-checkpoint-test: $(TEST_GAME_CHECKPOINT_OBJ)

# Every game object except main.o: the mechanics test drives whole levels
# through game_init/game_update_active/game_frame, as the game does. Like
# session-test it links the test build of game_input.o, the copy that
# compiles the MANGO_TESTING input seams.
GAMEPLAY_TEST_OBJS = $(filter-out $(OBJDIR)/src/main.o $(OBJDIR)/src/input/game_input.o,$(OBJS)) \
                     $(TEST_GAME_INPUT_OBJ)
$(OUTDIR)/gameplay-mechanics-test: $(TEST_SOURCE_DIR)/game_replay_test.o \
		$(TEST_SOURCE_DIR)/creature_update_test.o $(TEST_SOURCE_DIR)/player_climb_test.o \
		$(TEST_SOURCE_DIR)/thin_coverage_test.o \
		$(GAMEPLAY_TEST_OBJS)

# ── WebAssembly (Emscripten) ──────────────────────────────────────────
# Requires the Emscripten SDK (emcc on PATH).
# Produces out/super-mango.html, .js, .wasm, and .data (bundled assets).
#
# raylib is built with the same Emscripten toolchain as the application.
# CI pins this Emscripten release (.github/workflows/build.yml). `make web`
# stops early with a clear message when emcc reports another version, since
# a different SDK can change warnings and generated code. Pass
# EMSCRIPTEN_VERSION=<x.y.z> to expect another release, or leave it empty
# (EMSCRIPTEN_VERSION=) to skip the check.
# renovate: datasource=github-tags depName=emscripten-core/emsdk
EMSCRIPTEN_VERSION ?= 6.0.13
WEB_FLAGS = -s USE_GLFW=3 \
            --pre-js web/touch-controls.js \
            --pre-js web/keyboard-scope.js \
            --pre-js web/audio-unlock.js \
            -s ALLOW_MEMORY_GROWTH=1 \
            --preload-file assets \
            --exclude-file 'assets/sprites/unused/*' \
            --exclude-file '*/.DS_Store' \
            --preload-file levels \
            --shell-file $(WEB_SHELL)
# The page's own inline script cannot be linked into super-mango.js: it has
# to run first, to tell Emscripten how to start. tools/web_shell.py writes
# the shell that emcc uses from web/shell.html, pasting in web/boot.js (the
# boot sequence shared with the website's home page) and the file version.
WEB_SHELL = $(WEB_OBJDIR)/shell.html
# WEB_ASSET_VERSION is added to the address of every game file the page
# loads (super-mango.js?v=..., .wasm, .data), so a browser that cached an
# older deploy cannot pair an old file with a new one. CI passes the commit
# being built. Empty, the default for local builds, keeps the plain names.
WEB_ASSET_VERSION ?=
# Same warning set as native builds so Web-only code paths stay warning-free.
# Emscripten documents EM_JS(...); with a trailing semicolon, which pedantic C
# reports as an empty file-scope declaration; that one diagnostic is disabled.
# -Os, and raylib's Web library at -Os too (tools/build_raylib.py): the game
# is light on CPU and WebGL draws the frames, so code size is what players
# wait for. Against -O2 the .wasm went from 469 KB to 371 KB (190 KB to
# 166 KB gzipped).
WEB_OPT = -Os
WEB_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wno-extra-semi $(WEB_OPT) -D_GNU_SOURCE $(EXTRA_WEB_CFLAGS)
# The page only runs in browsers; Node merely syntax-checks the .js and
# compiles the .wasm (tools/check_wasm_artifacts.py), so drop Node support.
WEB_LINK_FLAGS = -s ENVIRONMENT=web -s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain"]'
WEB_HTML = $(OUTDIR)/super-mango.html
# The debug page runs the same program as the normal page and differs only by
# one inline marker script (web/debug-boot.js), so it is not linked at all:
# tools/web_debug_page.py derives it from the built normal page, and it loads
# that page's super-mango.js, .wasm and .data instead of identical copies.
WEB_DEBUG_HTML = $(OUTDIR)/super-mango-debug.html
# Every source compiles once into $(WEB_OBJDIR). The .d files track headers.
WEB_OBJS = $(patsubst %.c,$(WEB_OBJDIR)/%.o,$(SRCS))
WEB_INCLUDES = -I$(WEB_RAYLIB_BUILD)/build/raylib/include $(PROJECT_INCLUDES)
# The .js/.wasm/.data siblings are emitted with the HTML file. Listing every
# link input lets `web` (and dist-wasm through it) skip emcc only when fresh.
WEB_LINK_INPUTS = $(WEB_OBJS) $(WEB_RAYLIB_LIB) $(WEB_LINK_FLAGS_STAMP) \
                  $(filter-out web/debug-boot.js,$(wildcard web/*)) \
                  $(filter-out assets/sprites/unused/%,$(wildcard assets/* assets/*/* assets/*/*/*)) \
                  $(wildcard levels/* levels/*/*) tools/web_csp.py $(WEB_SHELL)

web: $(WEB_HTML) $(WEB_DEBUG_HTML) ## Build: WebAssembly game with Emscripten (emcc on PATH)

# Order-only (after the |): it runs first but never makes anything stale.
.PHONY: web-toolchain
web-toolchain:
	@if [ -n "$(EMSCRIPTEN_VERSION)" ]; then \
		found="$$(emcc --version 2>/dev/null | head -n 1)"; \
		case " $$found " in \
		*" $(EMSCRIPTEN_VERSION) "*) ;; \
		*) echo "web: expected Emscripten $(EMSCRIPTEN_VERSION) (the CI pin), found: $${found:-no emcc on PATH}"; \
		   echo "web: activate that SDK (emsdk install/activate $(EMSCRIPTEN_VERSION)), or pass"; \
		   echo "     EMSCRIPTEN_VERSION=<version> to expect another one (empty skips this check)"; \
		   exit 1 ;; \
		esac; \
	fi

$(WEB_OBJDIR)/%.o: %.c | $(OUTDIR) web-toolchain
	@mkdir -p $(@D)
	emcc $(WEB_CFLAGS) $(WEB_INCLUDES) -MMD -MP -c -o $@ $<

$(WEB_OBJS): $(WEB_RAYLIB_LIB) $(WEB_FLAGS_STAMP)
-include $(WEB_OBJS:.o=.d)

# Linking needs the optimisation level again (it drives wasm-opt) and the
# extra flags, so CI's -Werror also covers link-time warnings.
$(WEB_SHELL): web/shell.html web/boot.js tools/web_shell.py $(WEB_LINK_FLAGS_STAMP)
	python3 tools/web_shell.py template --version "$(WEB_ASSET_VERSION)" --output $@

# After the link: version the script tag emcc wrote, then pin the page's
# Content-Security-Policy to the final (minified) inline script.
$(WEB_HTML): $(WEB_LINK_INPUTS) | $(OUTDIR) web-toolchain
	emcc $(WEB_OPT) $(EXTRA_WEB_CFLAGS) $(WEB_OBJS) $(WEB_RAYLIB_LIB) -o $@ $(WEB_FLAGS) \
		$(WEB_LINK_FLAGS)
	python3 tools/web_shell.py stamp --version "$(WEB_ASSET_VERSION)" $@
	python3 tools/web_csp.py $@

$(WEB_DEBUG_HTML): $(WEB_HTML) web/debug-boot.js tools/web_debug_page.py tools/web_csp.py
	python3 tools/web_debug_page.py $(WEB_HTML) $@

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

# ── Flag stamps ──────────────────────────────────────────────────────
# Make decides what to rebuild from file timestamps alone, so by itself it
# cannot see that a flag changed: after a debug build, `make BUILD_MODE=release`
# would link the old -O0 objects. A stamp file closes that gap. It holds the
# exact settings its outputs were built with, and those outputs list it as a
# prerequisite. Compile and link settings have separate stamps, so a link-only
# change (a library, an Emscripten --pre-js) relinks without recompiling:
#
#   build-flags.txt       game, editor and tool objects   CC CFLAGS PROJECT_INCLUDES
#   test-flags.txt        test copies of objects          TEST_CFLAGS and per-object extras
#   link-flags.txt        every native program            LIBS and the other link inputs
#   fuzz-flags.txt        the fuzz replay programs        FUZZ_FLAGS (they compile and link at once)
#   web/build-flags.txt   Emscripten objects              WEB_CFLAGS WEB_INCLUDES
#   web/link-flags.txt    the normal Web page             WEB_OPT WEB_FLAGS WEB_LINK_FLAGS ...
#   raylib*/make-options.txt  raylib's "done" file        build_raylib.py options
#
# Each stamp is read back while Make parses this file. Only when the text
# differs, or the file is missing, does the stamp's rule depend on FORCE and
# rewrite the file; that newer file makes its outputs out of date. When the
# text matches, the file is left alone and nothing rebuilds. Editing this
# Makefile therefore rebuilds only what a changed setting affects, provided
# every recipe flag sits in a variable listed below (see PROJECT_INCLUDES).
# Every stamp is a single line, so reading it back with $(shell cat) is exact.
#
# shell_quote wraps a value in '...' for the shell, turning each ' inside it
# into '"'"' (close quote, a quoted ', reopen), so TEST_CFLAGS's
# -DMANGO_TEST_OUTDIR='"out"' is written out unchanged.
shell_quote = '$(subst ','"'"',$(1))'
# NAME=value for each listed variable. stamp_text records a variable's own
# text instead, for lookups that only resolve inside a recipe ($<).
stamp_vars = $(foreach v,$(1),$(v)=$($(v)))
stamp_text = $(foreach v,$(1),$(v)=$(value $(v)))

BUILD_FLAGS      = $(call stamp_vars,CC CFLAGS PROJECT_INCLUDES)
TEST_BUILD_FLAGS = $(call stamp_vars,CC TEST_CFLAGS PROJECT_INCLUDES \
                     $(sort $(filter TEST_OBJ_FLAGS_%,$(.VARIABLES)))) \
                   $(call stamp_text,TEST_OBJ_FLAGS TEST_OBJ_INCLUDES)
LINK_BUILD_FLAGS = $(call stamp_vars,CC CFLAGS TEST_CFLAGS LIBS EDITOR_LIBS TEST_LIBS MATH_LIBS)
FUZZ_BUILD_FLAGS = $(call stamp_vars,CC FUZZ_FLAGS LIBS MATH_LIBS)
WEB_BUILD_FLAGS  = $(call stamp_vars,EMSCRIPTEN_VERSION WEB_CFLAGS WEB_INCLUDES)
WEB_LINK_BUILD_FLAGS = $(call stamp_vars,EMSCRIPTEN_VERSION WEB_OPT EXTRA_WEB_CFLAGS WEB_FLAGS \
                         WEB_LINK_FLAGS WEB_ASSET_VERSION)
WEB_RAYLIB_BUILD_OPTIONS = $(call stamp_vars,EMSCRIPTEN_VERSION WEB_RAYLIB_OPTIONS)

# $(call flag_stamp,FILE,VARIABLE): FILE holds VARIABLE's text, rewritten only
# when that text changed.
define flag_stamp
ifneq ($$(strip $$($(2))),$$(strip $$(shell cat "$(1)" 2>/dev/null)))
$(1): FORCE
endif
$(1):
	@mkdir -p $$(@D)
	@printf '%s\n' $$(call shell_quote,$$(strip $$($(2)))) > $$@
endef
$(eval $(call flag_stamp,$(BUILD_FLAGS_STAMP),BUILD_FLAGS))
$(eval $(call flag_stamp,$(TEST_FLAGS_STAMP),TEST_BUILD_FLAGS))
$(eval $(call flag_stamp,$(LINK_FLAGS_STAMP),LINK_BUILD_FLAGS))
$(eval $(call flag_stamp,$(FUZZ_FLAGS_STAMP),FUZZ_BUILD_FLAGS))
$(eval $(call flag_stamp,$(RAYLIB_OPTIONS_STAMP),RAYLIB_OPTIONS))
$(eval $(call flag_stamp,$(WEB_FLAGS_STAMP),WEB_BUILD_FLAGS))
$(eval $(call flag_stamp,$(WEB_LINK_FLAGS_STAMP),WEB_LINK_BUILD_FLAGS))
$(eval $(call flag_stamp,$(WEB_RAYLIB_OPTIONS_STAMP),WEB_RAYLIB_BUILD_OPTIONS))

# FORCE has no rule and no file, so anything that depends on it always runs.
.PHONY: FORCE
FORCE:

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
	@echo "  (sprite frame grid), gen_fire_sprites (fire palette), generate_favicon (site icons),"
	@echo "  render_og_image (site link preview, needs Chrome)."
