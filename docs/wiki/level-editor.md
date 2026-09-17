# Level Editor

<a id="home"></a>

---

Super Mango includes a standalone visual level editor (`out/super-mango-editor`) that lets designers place, move, and configure entities on a scrollable canvas, then save the result as a TOML level file that the game engine loads directly.

```sh
make editor        # build the editor binary → out/super-mango-editor
make run-editor    # build game + editor, then launch editor
```

---

## Window Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Toolbar (32px)                                   │
├──────────────────────────────────────────────┬──────────────────────────┤
│                                              │                          │
│              Canvas  (896 × 656 px)          │    Panel  (384 × 656 px) │
│                                              │                          │
│   Scrollable level preview — WYSIWYG with    │  ┌────────────────────┐  │
│   the running game. Entities drawn at their  │  │   Entity Palette   │  │
│   exact game sizes and positions.            │  ├────────────────────┤  │
│                                              │  │   Properties       │  │
│                                              │  │   Inspector        │  │
│                                              │  ├────────────────────┤  │
│                                              │  │   Level Config     │  │
│                                              │  └────────────────────┘  │
├──────────────────────────────────────────────┴──────────────────────────┤
│                         Status Bar (32px)                                │
└─────────────────────────────────────────────────────────────────────────┘
  Total: 1280 × 720 px
```

| Area | Size | Contents |
|------|------|----------|
| Toolbar | 1280 × 32 px | Tool selector (Select / Place / Delete), File buttons (New, Open, Save, Save As), Play button |
| Canvas | 896 × 656 px | Scrollable level view with zoom. All entity types drawn at game-accurate sizes |
| Panel | 384 × 656 px | Entity palette, properties inspector, level config (collapsible sections) |
| Status bar | 1280 × 32 px | Cursor world coordinates, zoom level, current filename, modified indicator |

---

## Tools

Three interaction modes are available via the toolbar or keyboard shortcuts:

| Tool | Key | Behaviour |
|------|-----|-----------|
| **Select** | `1` | Click an entity on the canvas to select it. Drag to reposition. Selected entity appears in the Properties panel. |
| **Place** | `2` | Click empty canvas space to stamp a new entity of the type chosen in the palette. |
| **Delete** | `3` | Click an entity to remove it from the level immediately. |

Delete removes the selection; right-click quick-deletes an entity. Esc cancels a
field edit, returns to Select, or clears the selection. Printable shortcuts do
not switch tools while a text field is active. File/history shortcuts accept
Command on macOS as well as Ctrl.

---

## Entity Palette

The right panel lists all **31 placeable entity types** (`ENT_COUNT`), grouped by category:

| Category | Entities |
|----------|----------|
| World | Player Spawn, Floor Gap, Checkpoint, Rail |
| Terrain | Platform, Float Platform, Bridge |
| Collectibles | Coin, Star Yellow, Star Green, Star Red, Last Star |
| Enemies | Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish |
| Hazards | Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block, Blue Flame, Fire Flame |
| Surfaces | Bouncepad Small, Bouncepad Medium, Bouncepad High, Vine, Ladder, Rope |

Sprite-backed types use their in-game thumbnails. Checkpoints use the editor's primitive marker instead, because they have no sprite asset.

---

## Properties Inspector

When an entity is selected with the Select tool, the Properties panel displays its editable fields. All fields match the TOML schema exactly — what you see in the inspector is what gets written to the file.

**Example — Spider:**
- `x`, `vx`, `patrol_x0`, `patrol_x1`, `frame_index`

**Example — Float Platform:**
- `mode` (dropdown: STATIC / CRUMBLE / RAIL)
- `x`, `y`, `tile_count`, `rail_index`, `t_offset`, `speed`

**Example — Axe Trap:**
- `pillar_x`, `y`
- `mode` (dropdown: PENDULUM / SPIN)

**Example — Checkpoint:**
- `x`, `y`
- `screen` is derived from `x`; the inspector labels its runtime purpose: “Respawn when crossed.”

Changes take effect immediately on the canvas (WYSIWYG).

### Checkpoint Workflow and Feedback

1. Open the **World** palette category, choose **Checkpoint**, select **Place**, and click the intended respawn point.
2. Use **Select** to drag it or edit its `x` and `y` fields. Delete, copy/paste, undo, and redo use the same workflow as other non-singleton placement records.
3. Save or playtest only after validation succeeds. A checkpoint must be after the effective player start, must have a unique in-world `x`, and must have an in-world `y`; invalid records block save, autosave, and playtest.

The canvas draws a labelled `CP n` marker. Valid markers are amber, hovered markers brighten, the selected marker is blue, and invalid markers are red. The status bar reports **Checkpoint placed**, **Checkpoint moved**, or **Checkpoint deleted**. In the running game, crossing a valid marker produces a brief `CHECKPOINT CP n` HUD notice; the active checkpoint stays visible as `CP n`, and a death respawn is labelled `RESPAWN CP n`.

---

## Level Config Panel

The Level Config section in the right panel exposes the top-level TOML scalars:

- `name`, `description`, `generated_by`
- `screen_count` (world width = screen_count × 400 px)
- `player_start_x`, `player_start_y`
- `music_path`, `music_volume`
- `floor_tile_path`
- `initial_hearts`, `initial_lives`
- `score_per_life`, `coin_score`
- `next_phase` path (saved under `[last_star]` in TOML)
- `floor_gaps` list (add / remove entries)

---

## Camera Controls

| Action | Input |
|--------|-------|
| Pan left / right | Mouse wheel over the canvas |
| Cycle zoom | Toolbar dropdown or `Ctrl + Mouse Wheel` (1×, 2×, 3×, 5×) |
| Snap a dragged entity | Hold Shift while dragging (48px grid) |
| Toggle grid | `G` |

The canvas renders the level in WYSIWYG — entity positions and sizes match the game exactly at zoom 1.0 (logical pixel = 1 canvas pixel). At zoom 2.0 each logical pixel maps to 2 canvas pixels.

---

## Undo / Redo

The editor maintains a full undo stack for all placement, deletion, and property changes.

| Action | Shortcut |
|--------|----------|
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` (or `Ctrl+Shift+Z`) |

The undo stack is in-memory only — it is cleared when a new file is opened or created.

The editor also keeps recent files and writes recovery snapshots for modified valid levels in its platform preference directory (`SDL_GetPrefPath("Super Mango", "Editor")`).

---

## Copy / Paste

| Action | Shortcut |
|--------|----------|
| Copy selected entity | `Ctrl+C` |
| Paste (offset from original) | `Ctrl+V` |

Only one entity can be in the clipboard at a time. The pasted entity appears offset from the original so it does not overlap.

---

## Play-Test Integration

The **Play** button or **F5** validates the active `LevelDef`, serializes it to a private TOML snapshot in the editor preference directory, then launches the sibling game executable with `--no-save --level <snapshot>`. It does not overwrite the open source file or use your personal game profile. Validation errors block playtest and appear in the validation summary/status feedback. Clicking **Stop** (or closing the game window) returns to the editor and removes the temporary playtest file.

```sh
# Run an already-saved level with the same profile isolation:
./out/super-mango --no-save --level levels/your_level.toml
```

Enabling **Debug Mode** in the toolbar adds `--debug` to the game launch, showing collision boxes, FPS counter, and the event log.

---

## File Operations

| Operation | Shortcut / Button | Notes |
|-----------|-------------------|-------|
| New | `Ctrl+N` / New button | Prompts to save if modified |
| Open | `Ctrl+O` / Open button | Opens a native file picker |
| Save | `Ctrl+S` / Save button | Overwrites the current file |
| Save As | `Ctrl+Shift+S` / Save As button | Native file picker for new path |
| Recover autosave | `Ctrl+R` | Select an available recovery snapshot |
| Recent file | `Ctrl+1` through `Ctrl+5` | Open a recent file |

The title bar shows an asterisk (`*`) after the filename when there are unsaved changes. The editor prompts to save on quit if the level has been modified.

Saved files are plain TOML — they can be edited in any text editor and immediately reloaded in the editor or game.

Save, autosave, and Play run `editor_validate_level()` first. Errors such as bad counts, invalid paths, missing `next_phase` files, invalid `screen_count`, or invalid checkpoints block persistence and playtest so the editor does not write or launch levels known to be unsafe. The Level Config panel and status bar show the current validation summary.

CI can initialize the editor, render five bounded frames, and exit with:

```sh
./out/super-mango-editor --smoke-test
```

---

## Architecture

The editor uses focused modules in `src/editor/` and shared persistence/UI code in `src/shared/`:

| File | Responsibility |
|------|---------------|
| `editor_main.c` | Entry point — SDL init, `EditorState` lifecycle |
| `editor.c` / `editor.h` | Core state struct, init/loop/cleanup, `EntityType` enum (31 types), `EditorTool`, `EditorCamera`, `Selection` |
| `editor_validation.c` / `editor_validation.h` | In-memory level validation report used by status, save, autosave, and playtest |
| `canvas.c` / `canvas.h` | Level preview rendering, `canvas_screen_to_world`, grid overlay |
| `palette.c` / `palette.h` | Entity palette panel — thumbnails, type selection |
| `properties.c` / `properties.h` | Property inspector panel — per-type field editing |
| `tools.c` / `tools.h` | Mouse interaction for Select / Place / Delete tools |
| `src/shared/ui.c` / `ui.h` | Immediate-mode UI widget library shared with game settings |
| `undo.c` / `undo.h` | Undo stack and `PlacementData` clipboard union |
| `src/shared/serializer.h`, `serializer_load.c`, `serializer_load_*.c` | Public TOML API and staged parsing into `LevelDef`, including strict checkpoints |
| `src/shared/serializer_save.c`, `serializer_emit.c`, `serializer_io.c` | TOML emission and atomic file persistence |
| `file_dialog.c` / `file_dialog.h` | Native OS file picker (macOS / Linux / Windows) |

The `EditorState` struct mirrors the game's `GameState` design: one container passed by pointer to every function, owning the SDL window, renderer, font, entity textures, level data, camera, tool state, undo stack, and UI state.

---

## Relationship to the Game Engine

The editor and game share `LevelDef`, the serializer and level validation. Runtime object loading in `src/levels/level_loader.c` belongs to the game. This means:

- Both applications use the same file schema. Runtime texture availability and playability still need a playtest.
- A new entity needs coordinated changes to shared schema/load/save modules, runtime and editor integrations; see [Entity Walkthrough](../entity-walkthrough/).
- The editor's canvas draws entities using the same sprite paths as the game — adding a new entity type requires adding its texture to `EntityTextures` and a render call in `canvas_render`.

See [Level Design — TOML Reference](../level-design/) for the full schema of every entity type.
