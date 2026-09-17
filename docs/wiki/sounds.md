# Sounds

<a id="home"></a>

---

All audio files live in the `assets/sounds/` directory, organized into categorized subdirectories. All sound files use the `.wav` format. SDL2_mixer handles both short sound effects (`Mix_Chunk` via `Mix_LoadWAV`) and streaming music (`Mix_Music` via `Mix_LoadMUS`).

---

## Sound Reference

The tables include runtime sounds and bundled theme tracks. A bundled track may
be available for level authors without being selected by a current campaign or lab.

### Player — `assets/sounds/player/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `player_jump.wav` | `Mix_Chunk` | `gs->audio.jump` | Played when a jump starts, including buffered/coyote jumps and climb dismounts |
| `player_hit.wav` | `Mix_Chunk` | `gs->audio.hit` | Played when the player takes damage |

### Collectibles — `assets/sounds/collectibles/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `coin.wav` | `Mix_Chunk` | `gs->audio.coin` | Played when the player collects a coin |

### Entities — `assets/sounds/entities/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bird.wav` | `Mix_Chunk` | `gs->audio.flap` | Played for bird enemy wing flap |
| `spider.wav` | `Mix_Chunk` | `gs->audio.spider_attack` | Played when a jumping spider leaps at a gap |
| `fish.wav` | `Mix_Chunk` | `gs->audio.dive` | Played for fish enemy dive |

### Hazards — `assets/sounds/hazards/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `axe_trap.wav` | `Mix_Chunk` | `gs->audio.axe` | Played for axe trap swing |

### Surfaces — `assets/sounds/surfaces/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bouncepad.wav` | `Mix_Chunk` | `gs->audio.spring` | Played when the player lands on a bouncepad |

### Screens — `assets/sounds/screens/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `confirm_ui.wav` | `Mix_Chunk` | `menu->snd_confirm` | Played on menu confirmation |

### Levels — `assets/sounds/levels/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `water.wav` | `Mix_Music` | `gs->audio.music` | Background music for water-themed levels, loaded via `Mix_LoadMUS` (streaming) |
| `lava.wav` | `Mix_Music` | `gs->audio.music` | Background music for lava-themed levels, loaded via `Mix_LoadMUS` (streaming) |
| `winds.wav` | `Mix_Music` | `gs->audio.music` | Available theme track; no current campaign/lab level selects it |

---

## Unused Sounds

The following sounds are stored in `assets/sounds/unused/` and are not loaded by the game. They are available as reserves for future use.

| File | Description |
|------|-------------|
| `fireball.wav` | Projectile / fireball effect |
| `saw.wav` | Circular saw spinning |

---

## Audio Configuration

SDL2_mixer is opened in `main.c` with:

```c
Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
```

| Parameter | Value | Meaning |
|-----------|-------|---------|
| Frequency | `44100` Hz | CD quality |
| Format | `MIX_DEFAULT_FORMAT` | 16-bit signed samples |
| Channels | `2` | Stereo |
| Chunk size | `2048` samples | ~46 ms buffer at 44100 Hz |

---

## Sound Effects vs. Music

| Aspect | `Mix_Chunk` (sound effects) | `Mix_Music` (background music) |
|--------|----------------------------|-------------------------------|
| API | `Mix_LoadWAV`, `Mix_PlayChannel` | `Mix_LoadMUS`, `Mix_PlayMusic` |
| Loading | Fully decoded into RAM | Decoded on demand from the backing file/resource |
| Best for | Short, triggered sounds | Long looping tracks |
| Simultaneous | Multiple channels | One at a time |
| Volume | `Mix_VolumeChunk` | `Mix_VolumeMusic` |

Sound effects use `Mix_LoadWAV` and are fully loaded into memory. Music is selected
by each level's `music_path` and loaded through `Mix_LoadMUS`. Native decoding can
read incrementally from disk; WebAssembly preloads these WAV files into its
virtual filesystem, so streaming playback does not remove their download or
backing-storage cost. See [Asset Inventory](../asset-inventory/).

---

## Adding a New Sound Effect

1. Place the `.wav` file in the appropriate `assets/sounds/<category>/` subdirectory.
2. Add a `Mix_Chunk *<name>` field to `AudioResources` in `game.h`.
3. Load it in `src/core/game_resources.c`, called by `game_init`:

```c
gs->audio.<name> = Mix_LoadWAV("assets/sounds/<category>/<name>.wav");
if (!gs->audio.<name>) {
    fprintf(stderr, "Warning: failed to load <name>.wav: %s\n", Mix_GetError());
    /* Non-fatal — game continues without this sound */
}
```

4. Free it in the resource cleanup called by `game_cleanup`:

```c
FREE_CHUNK(gs->audio.<name>);
```

5. Play it wherever the event occurs:

```c
if (gs->audio.<name>) Mix_PlayChannel(-1, gs->audio.<name>, 0);
```

The `if` guard is important: if the WAV fails to load for any reason, the game continues without crashing.

---

## Adding a New Music Track

```c
// Load (streaming — not fully decoded into RAM)
gs->audio.music = Mix_LoadMUS("assets/sounds/levels/new_track.wav");
if (!gs->audio.music) { /* handle error */ }

// Start (loop forever)
Mix_PlayMusic(gs->audio.music, -1);

// Volume (0-128)
Mix_VolumeMusic(64);  // 50%

// Stop and free
Mix_HaltMusic();
Mix_FreeMusic(gs->audio.music);
gs->audio.music = NULL;
```

The normal session applies authored `music_volume` scaled by the player's saved
volume/mute preferences. Prefer that path to a hard-coded global music volume.
Media licenses are documented separately in [Asset Provenance](../asset-provenance/).
