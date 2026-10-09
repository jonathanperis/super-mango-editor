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
| Toolbar | 1280 × 32 px | Tool selector (Select / Place / Delete), Grid and Debug toggles, zoom dropdown, file buttons (New, Open, Save As, Save), Play (Stop while a playtest runs) |
| Canvas | 896 × 656 px | Scrollable level view with zoom. All entity types drawn at game-accurate sizes |
| Panel | 384 × 656 px | Entity palette, properties inspector, level config (collapsible sections) |
| Status bar | 1280 × 32 px | Cursor world coordinates, snap-to-grid state, active tool, validation summary, entity count (every placement except the player spawn), current filename with ` *` when modified, latest status message |

---

## Tools

Three interaction modes are available via the toolbar or keyboard shortcuts:

| Tool | Key | Behaviour |
|------|-----|-----------|
| **Select** | `1` | Click an entity on the canvas to select it. Drag to reposition. Selected entity appears in the Properties panel. Clicking empty space clears the selection; dragging from empty space draws a selection box. |
| **Place** | `2` | Click the canvas to stamp a new entity of the type chosen in the palette. For the two singletons (Player Spawn, Last Star) the click moves the existing one. A new spike block attaches to the rail nearest the click. |
| **Delete** | `3` | Click an entity to remove it from the level immediately. Clicking a member of a multi-selection removes the whole selection. |

### Selecting Several Entities

Several entities can be selected at once (up to 64, `EDITOR_MAX_SELECTION`):

| Action | Input |
|--------|-------|
| Box select | Press on empty canvas and drag: every entity the box touches is selected |
| Add to the selection with a box | `Shift` + drag from empty canvas |
| Add / remove one entity | `Shift + click` it |
| Select just one member again | Click it without dragging |
| Clear the selection | `Esc`, or a click on empty canvas |

Dragging any selected entity moves the whole group (the grabbed one snaps to
the grid; the others keep their distance), and the arrow keys nudge it. Delete
or Backspace, `Ctrl+C` / `Ctrl+V` and `Ctrl+D` act on every selected entity.
Each of these is a single undo step, however many entities it touched, and
redoing a paste or duplicate selects all the copies again. The Delete tool and
right-click also act on the whole selection when the entity they hit is part
of it (on anything else they delete just that entity). A rail
deleted together with the spike blocks and platforms riding it goes after them,
so the pair can be deleted (alone, a ridden rail is refused). The Properties
panel shows `N selected` with a count per type instead of fields.

Dragging keeps the point you grabbed under the cursor and starts only after the
cursor moves 3 canvas pixels, so a plain click selects without moving anything.
Patrolling enemies and saws carry their patrol range with them, and positions
are clamped so a drag cannot produce a level that fails validation. While the
button is held, keyboard commands (Undo, Delete, Paste…) and right-click
quick-delete wait; Esc cancels the move and puts the entity back. Axe traps and
saws store `y = 0` for "default height", so dragging one to the very top row
stores 1 px instead.

Spike blocks and rail-mode float platforms refer to rails by position, so the
editor refuses to delete a rail that one of them rides (the status bar names
what still uses it). Deleting an unused rail renumbers the references to later
rails, and undo restores the original numbering.

The `Delete` key, or `Backspace` (laptops without a Delete key), removes the
selection; inside a text field both edit text instead. The arrow keys nudge the
selection 1 px, or 16 px with `Shift`, following the same rules as a drag
(floor-bound entities move only sideways, rail riders stay on their rail, and
nothing leaves the world or makes the level invalid). A quick run of nudges of
the same selection is one undo step; after a one-second pause the next nudge
starts a new step. Right-click quick-deletes an entity in
any tool (ignored while a drag is in progress). Clicks pick the entity drawn on
top: the hit test (`src/editor/hit_test.c`) walks the exact reverse of the
canvas draw order, so enemies and hazards win over collectibles, and those over
surfaces and ground pillars. To reach an entity hidden underneath, `Alt+click`
the spot, or click the very same spot again without moving: each such click
selects the next entity down and wraps back to the top. The status bar says
which one is selected (`Selected Coin (2 of 2 here)`). (Some Linux window
managers take `Alt+click` for themselves; the second click works everywhere.) Esc cancels a field edit, returns to Select, or
clears the selection. Printable shortcuts do not switch tools while a text field
is active; Ctrl+C/Ctrl+V inside a text field copy and paste text. File/history
shortcuts accept Command on macOS as well as Ctrl.

If a hand-edited file or a property field leaves the level invalid, the canvas
pauses: clicks report `Canvas paused: <error>` in the status bar until you fix
the field in the side panel or press Ctrl+Z.

---

## Entity Palette

The right panel lists all **31 placeable entity types** (`ENT_COUNT`) in collapsible categories. Names, categories and order come from one table in `src/editor/entity_meta.c`:

| Category | Entities |
|----------|----------|
| World | Player Spawn, Floor Gap, Checkpoint, Rail |
| Collectibles | Coin, Star Yellow, Star Green, Star Red, Last Star |
| Enemies | Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish |
| Hazards | Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block, Blue Flame, Fire Flame |
| Surfaces | Platform, Float Platform, Bridge, Bouncepad Small, Bouncepad Medium, Bouncepad High |
| Decorations | Vine, Ladder, Rope |

Palette rows are text labels; clicking one selects that type and switches to the Place tool. On the canvas, the placement ghost under the cursor uses the in-game sprite; which sprite, which part of its sheet and what size come from the same table row. Checkpoints use the editor's primitive marker instead, because they have no sprite asset.

---

## Properties Inspector

When one entity is selected with the Select tool, the Properties panel displays its editable fields (with several selected it shows how many of each type instead). All fields match the TOML schema exactly — what you see in the inspector is what gets written to the file.

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
3. Save or playtest only after validation succeeds. A checkpoint must be after the effective player start, must have a unique in-world `x`, and must have an in-world `y`; invalid records block save and playtest (autosave keeps the last valid version).

The canvas draws a labelled `CP n` marker. Valid markers are amber, hovered markers brighten, the selected marker is blue, and invalid markers are red. The status bar reports **Checkpoint placed**, **Checkpoint moved**, or **Checkpoint deleted**. In the running game, crossing a valid marker produces a brief `CHECKPOINT CP n` HUD notice; a death respawn is labelled `RESPAWN CP n`. The debug inspector exposes the current checkpoint index after those temporary notices expire.

---

## Level Config Panel

The Level Config section in the right panel starts with the validation summary
and its messages, then the recent-files list (`Ctrl+1` to `Ctrl+5`), then the
level-wide TOML fields. Each validation message is clickable (it highlights
under the cursor): a message about an entity, such as `checkpoints[0].x is ...`,
selects that entity (checkpoints, the Player Spawn and the Last Star included),
switches to Select, pans the canvas to centre it and focuses the property the
message names (`spiders[2].vx` puts the caret in that spider's `vx` field; a
dropdown such as an axe's `mode` opens its list); a message about a Level
Config value focuses that field, unfolding its group and scrolling the panel
to it (a layer's asset dropdown opens, `fog_layers[1].speed` takes the
caret). Clicking the `Validation: ...` summary in the status bar does the same
for the first message. The level-wide fields are:

- `name`, `description`, `generated_by`
- `screen_count` (1–99; world width = screen_count × 400 px)
- `next_phase` path (saved under `[last_star]` in TOML; it must follow the [level-reference rule](../level-design/#level-references))
- `music_path` (dropdown: none, water, lava, winds) and `music_volume` (0–128)
- `floor_tile_path` (dropdown)
- `initial_hearts` (1–3), `initial_lives`
- `score_per_life`, `coin_score`
- A collapsible Movement Physics group for the optional `[physics]` overrides

The player start and the floor gaps are canvas entities: move the Player Spawn
marker, and place or delete Floor Gap entities. A floor gap's `x` field rounds
to the 16 px floor-piece grid.

Inside an active field, `Left` / `Right` move the caret one character,
`Home` / `End` jump to either end, typing inserts at the caret, and `Backspace`
/ `Delete` remove the character before / after it (whole UTF-8 characters, so
`é` never splits in half). `Tab` applies the field and moves to the next field
of the side panel, `Shift+Tab` to the previous one (wrapping at either end); a
value that cannot be stored keeps the focus where it is.

Leaving a field applies what you typed: Return, a click on the canvas or
another field, or any command (Save, Undo, a shortcut) stores a valid value as
its own undo step, with no prompt. Only a value that cannot be stored (`12a`,
a lone `-`) asks **Keep Editing / Discard**; Keep Editing leaves the field open
and the command waits. Esc always discards the typed text. Number fields with a
range clamp the value however the edit ends. A bouncepad's `launch_vy`
is kept at least as strong as a jump, a rail rider's `speed` above 0 and at
most the rail speed limit, and an enemy's patrol range at least as wide as its
sprite. An enemy's `vx` is limited to `MAX_PATROL_SPEED` (960 px/s) either
way, and a typed `0`, which would freeze the enemy, is not stored: the field
stays open like any value that cannot be stored. When a field limits or refuses
what you typed, the status bar says so (`-99999 is below -960, the smallest this
field takes; it was limited`). Newly placed enemies start at their usual speeds, which are valid. Switching a float platform to Rail gives it a speed of 3 if it had
none, and needs a rail in the level. A Static or Crumble platform keeps its old
`rail_index`, which deleting rails does not renumber; if that number names no
rail any more, the switch attaches the platform to the rail nearest it. Either
way the status bar names the rail it now rides. In Rail mode the `rail_index`
field only accepts existing rails.

A dropdown whose current value is not one of its options (a path typed into
the TOML by hand) shows `---`, and any option can then be picked. While a
dropdown list is open, the next click only picks an option or closes the list;
it never reaches the canvas or the fields under the list.

---

## Camera Controls

| Action | Input |
|--------|-------|
| Pan left / right | Mouse wheel over the canvas, or a trackpad's sideways swipe |
| Pan up / down | `Shift + Mouse Wheel` over the canvas (at 3× and 5× the 300 px world is taller than the canvas); macOS reports Shift+wheel as horizontal scroll, which also pans up/down here |
| Change zoom | Toolbar dropdown (zooms around the canvas centre) or `Ctrl + Mouse Wheel` (zooms around the cursor; 1×, 2×, 3×, 5×, stopping at 1× and 5×) |
| Snap to grid on / off | `S` (status bar shows `Snap: on` / `Snap: off`). Placing and dragging then put the entity's corner on the 48 px grid |
| Snap just this once (or not) | Hold `Shift` while placing or dragging: it does the opposite of the `S` setting |
| Toggle grid | `G` |

The canvas renders the level in WYSIWYG — entity positions and sizes match the game exactly at zoom 1.0 (logical pixel = 1 canvas pixel). At zoom 2.0 each logical pixel maps to 2 canvas pixels. One wheel notch pans 48 canvas pixels and scrolls a side panel 20 px. Trackpads report fractions of a notch; the editor adds them up, so small swipes scroll the side panels too, and a `Ctrl` pinch or swipe changes zoom once per whole notch.

---

## Undo / Redo

The editor keeps an undo stack for placement, movement, deletion, property and Level Config changes. It holds the latest 256 steps (`UNDO_MAX`); older ones are dropped. An action on several entities at once (a nudge, move, delete, paste or duplicate of a multi-selection) records one entry per entity, up to 64 (`UNDO_GROUP_MAX`), in a single step: it costs one of the 256 slots, undoes and redoes in one go, and the oldest step is always dropped whole. If the editor cannot get the memory to record such an action, it refuses the action and the level is left unchanged. A new edit clears the redo stack.

| Action | Shortcut |
|--------|----------|
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` (or `Ctrl+Shift+Z`) |

The undo stack is in-memory only — it is cleared when a new file is opened or created. Undoing back to the saved contents clears the modified marker.

The editor keeps recent files (the last 5) and recovery snapshots for modified levels in its OS preference directory, retaining the `Super Mango/Editor/` organization/application suffix. Autosave runs every 30 seconds while the level is modified. Recovery snapshots must load through the same validation as any level, so while the current level has validation errors autosave writes the most recent valid version of the document instead ("Autosaved last valid version"); if nothing newer than the saved file is valid, it skips that round. Every attempt, successful or not, restarts the 30-second timer, so a failing autosave reports `Autosave failed; retrying in 30 s` at most once per interval.

The folder holds at most 32 recovery copies. When leftover copies fill it, autosave pauses with `Autosave paused: 32 old recovery copies fill the folder; Ctrl+R to recover or discard them` instead of failing silently. `Ctrl+R` shows one copy at a time (source file and time) with **Cancel / Recover / Discard**; with several copies the third button is **More...**, which offers **Back / Discard / Next**. Discard deletes that copy's files. Each copy records the process id of the editor that wrote it: a copy whose editor is still running (a second editor window, say) is that editor's live work and is never offered or deleted by another one. A snapshot file that lost its `.meta` description can never be offered, so the editor deletes such orphans when it starts.

---

## Copy / Paste

| Action | Shortcut |
|--------|----------|
| Copy the selection | `Ctrl+C` |
| Paste (offset from original) | `Ctrl+V` |
| Duplicate the selection (offset, clipboard untouched) | `Ctrl+D` |

`Ctrl+C` copies the whole selection. Each pasted entity is moved a little so it does not hide the original (24 px right, and down for free-floating things; along the rail for rail riders; one gap width for floor gaps), and the pasted copies become the selection. A rail copied together with its riders pastes as a new rail carrying the copied riders. A paste is all or nothing: if one copy cannot be added, none is. Each further
`Ctrl+V` steps one more offset from the previous copy, so repeated pastes lay
out a row instead of stacking on one spot; a new `Ctrl+C` starts again from the
copied entity.

`Ctrl+D` duplicates the selection without touching the clipboard: the copy moves
by the same offset a paste uses and becomes the selection, so pressing `Ctrl+D`
again keeps stepping. A rail rider stays on its own rail. The Player Spawn and
the Last Star exist once per level and are not duplicated.

Place, drag and Paste clamp entities into the world (patrol ranges included, and a
pasted spike block's `t_offset` wraps onto its rail). Paste is refused while the
level has validation errors. A copied rail rider remembers the rail it rode, so
a paste re-attaches it to that rail even after the rail was moved or other rails
were deleted, and never to an identical rail next to it. Pasted into another
level, it attaches to a rail with the same shape and position. When an entity
cannot be added — its array is full, its rail is not in this level (or was deleted),
or the result would fail validation, such as a checkpoint behind the player
start — the status bar explains why and the level is left unchanged.

---

## Play-Test Integration

The **Play** button or **F5** validates the active `LevelDef`, serializes it to a private TOML snapshot in the editor preference directory, then launches the sibling game executable with `--no-save --level <snapshot>`. It does not overwrite the open source file or use your personal game profile. Validation errors block playtest and appear in the validation summary/status feedback. While the game runs, the editor draws only a "Playing" overlay with a Stop button: the canvas, panels and keyboard shortcuts are inactive, so the document cannot change under the running snapshot. Clicking **Stop** (or quitting the editor) asks the game to exit, waits up to one second, then force-stops it and reaps the process; closing the game window also returns to the editor. Either way the temporary playtest file is removed.

```sh
# Run an already-saved level with the same profile isolation:
./out/super-mango --no-save --level levels/your_level.toml
```

Enabling **Debug Mode** in the toolbar adds `--debug` to the game launch, showing collision boxes, FPS counter, and the event log.

### Playtest From Here

**Shift+F5** playtests from a point instead of the level's start. With exactly one checkpoint selected, the game starts on that checkpoint (`--start-checkpoint <n>`); otherwise it starts with the player centred on the world x under the mouse (`--start-x <px>`), standing on the highest surface there: the ground floor, a pillar, a bridge, or a fixed or crumbling float platform. The selected checkpoint's properties also have a **Playtest from here** button. The point is checked with the game's own rules first, so pointing over a floor gap with nothing above it, or away from the canvas with no checkpoint selected, is refused in the status bar (`Playtest from here: nothing to stand on at x 416 (a floor gap)`) and nothing is launched. In the game the start point is also where a lost life comes back to, until a later checkpoint is crossed; Retry after Game Over starts the level from its own start (see the [runtime flags](../controls/#runtime-flags-for-input-and-ci)).

---

## Campaign View

**Ctrl+M** or the toolbar's **Campaign** button opens `levels/campaigns/main.toml`, the list of levels the game's start menu offers, in place of the canvas. Each row shows the level file, its `name` (the menu's label, editable in place) and the game's verdict on it: `ok`, or why the start menu would grey it out (`next_phase is out of campaign order`, `final level has a next_phase`, `level file is invalid`...). A problem with the campaign as a whole (a repeated level, nothing playable) shows in red above the list. The verdicts come from the same C rules the game applies when it loads the manifest (`campaign_catalog_check` in `src/levels/campaign_catalog.c`), rerun after every edit.

| Button | Action |
|--------|--------|
| Up / Down | Move the selected row (click a row to select it) |
| Remove | Take the selected level out of the campaign; its file is not touched |
| Add level... | Pick a level file with the native picker; it must sit directly in `levels/` (the [level-reference rule](../level-design/#level-references)) and not be listed already |
| Link in order | Set every listed level's `[last_star].next_phase` to the row below it, and clear the last one's, so the chain matches the order shown |
| Save (`Ctrl+S`) | Write each level whose name or `next_phase` changed, and the manifest, to temporary files first; replace the real files only once all of them are written |
| Close (`Esc`) | Back to the level; with unsaved campaign changes the first Close (or quit) only warns, the second discards them |

Save is refused while the rules find something this view can fix: a campaign-wide problem, or a level that is out of order or has no name (Link in order fixes the order). A listed file that does not load is the file's own problem and may stay listed; the menu shows it disabled. Before writing anything, and again right before replacing the files, Save checks that every file it would rewrite (each changed level and the manifest itself) still holds what the view read when it opened, so a file changed by another program is never overwritten; it also refuses to rewrite the level open in the editor while that level has unsaved changes, and reloads it afterwards when it was clean. A failure while writing the temporary files changes nothing on disk. The replacing itself is one quick rename per file, so it can fail half-way only if the system refuses a rename (a full disk, a file locked by another program on Windows); the status bar then says how many files were written and which one failed (`Campaign partly saved: 2 of 4 files written; writing levels/b.toml failed`), and the next Save writes the rest. If Windows moved a file aside and then could not move the new one in, the temporary file is the only complete copy: it is kept, and the status bar names it (`Campaign partly saved (1 of 4 files written). Save incomplete: the new levels/b.toml is safe in <path>`). While the view is open, clicks on it never reach the canvas and level shortcuts are ignored.

---

## File Operations

| Operation | Shortcut / Button | Notes |
|-----------|-------------------|-------|
| New | `Ctrl+N` / New button | Prompts to save if modified |
| Open | `Ctrl+O` / Open button | Opens a native file picker |
| Save | `Ctrl+S` / Save button | Overwrites the current file |
| Save As | `Ctrl+Shift+S` / Save As button | Native file picker for new path |
| Recover autosave | `Ctrl+R` | Recover or discard a leftover recovery snapshot |
| Recent file | `Ctrl+1` through `Ctrl+5` | Open a recent file |
| Campaign view | `Ctrl+M` / Campaign button | Edit the campaign manifest (see [Campaign View](#campaign-view)) |

The title bar and status bar show an asterisk (`*`) after the filename when there are unsaved changes. New, Open, a recent file, recovery and quit first ask **Save / Discard / Cancel** when the level has been modified; Save continues only if the save succeeds. On Linux these three-button prompts use zenity's extra button.

Saved files are plain TOML — they can be edited in any text editor and immediately reloaded in the editor or game.

Saves write a sibling temporary file, flush it, then atomically rename it over the destination and (on macOS/Linux) sync the containing directory so the new entry survives a crash. On macOS/Linux a newly created level gets ordinary document permissions (`0666` minus your umask, usually `0644`), while recovery snapshots and playtest copies stay private to you (`0600`); replacing an existing file keeps its permissions, and its owner and group where the system allows it. On Windows the replace can stop halfway (the old file already moved aside, the new one not yet in place, for example while a virus scanner holds the file); the editor then retries with a plain move, and if that fails too it keeps the temporary file, never deletes it, and names it in the status bar: `Save incomplete: your level is safe in <path>`. Before replacing a file, Save checks that it still holds what the editor last read or wrote; if another program changed it, Save asks **Cancel / Replace / Save As**. If the file changes in the moment between that question and the write, nothing is written and the status bar says `Save stopped: <path> changed on disk; nothing was written. Save again to replace it or Save As`. **Save** on a symlinked level updates the file the link points to and keeps the link, but only while the link still points at the file the editor opened; otherwise it asks you to use Save As. **Save As** refuses a destination that is a symbolic link (`Save failed: <path> is a symbolic link; choose another name`) and refuses the editor's private recovery and playtest files.

Save, autosave, and Play run `editor_validate_level()` first. Errors include everything `level_validate_runtime()` rejects (bad counts, out-of-world placements, invalid checkpoints, unsafe paths, a `next_phase` that breaks the level-reference rule), a `screen_count` below 1, and asset or `next_phase` files that do not exist. They block persistence and playtest, and the status bar reports `Save blocked: <first error>`. An empty level name or a Last Star left at the origin are warnings only. The status bar shows `Validation: OK` or `Validation: N error(s), M warning(s)`; the Level Config panel lists the messages. Every broken rule gets its own message (the game itself stops at the first), so a level with a bad coin and a frozen spider shows both at once; the panel lists up to 16 and ends with `... and N more` when there are others.

A file that will not open is not loaded (the current document stays), and the status bar says `Load failed: <file>: <first problem>`. The Level Config panel then starts with `Could not open <file>`, listing why: a TOML syntax error with its line (`line 3: TOML syntax: ...`), a schema error with the line of the bad value (`line 9: root.coins[1].x has type string, expected finite number`), or every runtime rule the file breaks, each with its line. Clicking that heading hides the list; opening a level or starting a new one clears it.

CI can initialize the editor, render five bounded frames, and exit with:

```sh
./out/super-mango-editor --smoke-test
```

---

## Architecture

The editor uses focused modules in `src/editor/` and shared persistence/UI code in `src/shared/`:

| File | Responsibility |
|------|---------------|
| `editor_main.c` | Entry point — raylib-backed `EditorState` lifecycle, `--smoke-test` |
| `editor.c` / `editor.h` | Core state struct, init/loop/cleanup, `EntityType` enum (31 types), `EditorTool`, `EditorCamera`, `Selection` |
| `editor_frame.c`, `editor_events.c` | One frame (validation, autosave, drawing) and keyboard/mouse/wheel event routing, including every shortcut |
| `editor_chrome.c`, `editor_panels.c`, `editor_layout.c` | Toolbar, status bar and side-panel layout |
| `editor_files.c`, `editor_session.c` | Open/save/Save As, playtest copies and recent files; dirty tracking, document hash and confirmation prompts |
| `editor_recovery.c` | Autosave every 30 s while dirty, and crash recovery: finding leftover snapshots, the Recover picker, loading and retiring them |
| `editor_playtest.c` | Launching, stopping and reaping the playtest game process |
| `editor_campaign.c` | The Campaign view: editing `levels/campaigns/main.toml` with the game's campaign rules (`src/levels/campaign_catalog.c`) |
| `editor_clipboard.c`, `editor_undo_apply.c` | Copy/paste, and applying undo/redo commands to the level |
| `editor_validation.c` / `editor_validation.h` | In-memory level validation report used by status, save, autosave, and playtest |
| `canvas.c` / `canvas.h` | Level preview rendering, `canvas_screen_to_world`, grid overlay |
| `palette.c` / `palette.h` | Entity palette panel — category rows, type selection |
| `properties.c` / `properties.h` | Property inspector panel — one `draw_<type>_properties()` function per entity type, chosen from the `s_property_panels` table, plus Level Config drawn by one `config_*` helper per settings group |
| `tools.c` / `tools.h` | Mouse interaction for Select / Place / Delete tools |
| `hit_test.c` / `hit_test.h` | Entity rectangles shared by click hit-testing and the selection outline; reverse-draw-order hit test |
| `entity_meta.c` / `entity_meta.h` | The per-type table (names, category, capacity, where the placements live in `LevelDef`, the placement-ghost sprite) and the shared read/insert/remove helpers used by tools, undo and paste |
| `src/shared/ui.c` / `ui.h` | Immediate-mode UI widget library shared with game settings |
| `undo.c` / `undo.h` | Undo stack and `PlacementData` clipboard union |
| `src/shared/serializer.h`, `serializer_load.c`, `serializer_load_*.c` | Public TOML API and staged parsing into `LevelDef`, including strict checkpoints |
| `src/shared/serializer_save.c`, `serializer_emit.c`, `serializer_io.c` | TOML emission and atomic file persistence |
| `file_dialog.c`, `dialog_choice.c` | Native OS file picker and button prompts (macOS `osascript`, Linux `zenity`, Windows PowerShell) |

The `EditorState` struct mirrors the game's `GameState` design: one container passed by pointer to every function, owning its raylib render target, textures, level data, camera, tools, undo history and UI state. Text uses raylib's built-in font, which the UI borrows and never unloads. The standalone editor owns its process window; native file pickers and confirmations use the existing OS-dialog boundary.

---

## Relationship to the Game Engine

The editor and game share `LevelDef`, the serializer and level validation. Runtime object loading in `src/levels/level_loader.c` belongs to the game. This means:

- Both applications use the same file schema. Runtime texture availability and playability still need a playtest.
- A new entity needs coordinated changes to shared schema/load/save modules, runtime and editor integrations; see [Entity Walkthrough](../entity-walkthrough/).
- The editor's canvas draws entities using the same sprite paths as the game — adding a new entity type requires adding its texture to `EntityTextures` and a render call in `canvas_render`.

See [Level Design — TOML Reference](../level-design/) for the full schema of every entity type.
