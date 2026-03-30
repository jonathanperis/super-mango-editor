---
description: "Audit the codebase and update wiki + README to match the current state of the code."
---

# Sync Documentation

Scan the entire codebase and verify that all documentation (wiki pages and README.md) accurately reflects the current state. Fix any drift found.

## Steps

### 1. Gather current code state

Collect the ground truth from source code:

- **All source modules:** list every `.c` and `.h` file in `src/`
- **All assets:** list every file in `assets/` (exclude `assets/unused/`)
- **All sounds:** list every file in `sounds/` (exclude `sounds/unused/`)
- **GameState struct:** read `src/game.h` — every field, every `#include`, every `#define` constant
- **Texture loads:** grep `IMG_LoadTexture` in `src/game.c` and all component `.c` files
- **Sound loads:** grep `Mix_LoadWAV` and `Mix_LoadMUS` in `src/game.c`
- **Entity init calls:** grep `_init(` in `src/game.c` and `src/sandbox.c`
- **Render order:** read the render section of `src/game.c` (look for `SDL_RenderCopy` and `_render(` calls in order)
- **Build targets:** read `Makefile` for all targets
- **Constants:** grep `#define` across all `.h` files in `src/`
- **Player constants:** read `src/player.h` and `src/player.c` for all `#define` values

### 2. Cross-reference each documentation file

For each file below, compare the documented content against the code state gathered above. Track every discrepancy.

| File | What to verify |
|------|---------------|
| `README.md` | Feature list, project structure (all modules listed), asset/sound filenames, render order, controls, build commands |
| `wiki/home.md` | Entity description completeness, navigation links, repo structure diagram, source file listing |
| `wiki/architecture.md` | Startup sequence (all texture + sound loads), game loop phases, render order (all layers), GameState struct fields, cleanup sequence |
| `wiki/build_system.md` | Build targets, compiler/linker flags, output structure (all .o files), prerequisites |
| `wiki/source_files.md` | Every module listed with correct description, GameState includes list, asset filenames |
| `wiki/assets.md` | Every used asset listed with correct filename and component mapping, unused assets list |
| `wiki/sounds.md` | Every used sound listed with correct filename, GameState field, and format, unused sounds list |
| `wiki/developer_guide.md` | Entity creation workflow, render layer order, script paths, naming conventions, checklist |
| `wiki/player_module.md` | Player struct fields, physics constants, animation table, asset filenames |
| `wiki/constants_reference.md` | Every `#define` from every `.h` file documented with correct values |

### 3. Apply fixes

For each discrepancy found:
- Update the documentation file to match the code (code is always the source of truth)
- Use the current naming conventions: snake_case filenames, snake_case wiki links like `(home)`, `(architecture)`, etc.
- Do NOT modify any source code — only fix documentation

### 4. Report

After all fixes are applied, output a summary:
- Number of files checked
- Number of discrepancies found and fixed
- List of specific changes made (grouped by file)
- Confirmation that zero stale references remain (verify with grep for old PascalCase asset names, old sound names, old `.github/skills` paths)
