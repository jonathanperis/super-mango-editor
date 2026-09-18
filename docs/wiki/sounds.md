# Sounds

<a id="home"></a>

---

All audio files live in `assets/sounds/`, organized into categorized subdirectories.
The assets use `.wav`. raylib handles decoded `Sound` samples and streamed `Music`;
the project-owned `SoundEffect` and `MusicTrack` types manage their lifetimes.

---

## Sound Reference

The tables include runtime sounds and bundled theme tracks. A bundled track may
be available for level authors without being selected by a current campaign or lab.

### Player — `assets/sounds/player/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `player_jump.wav` | `SoundEffect` | `gs->audio.jump` | Played when a jump starts, including buffered/coyote jumps and climb dismounts |
| `player_hit.wav` | `SoundEffect` | `gs->audio.hit` | Played when the player takes damage |

### Collectibles — `assets/sounds/collectibles/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `coin.wav` | `SoundEffect` | `gs->audio.coin` | Played when the player collects a coin |

### Entities — `assets/sounds/entities/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bird.wav` | `SoundEffect` | `gs->audio.flap` | Played for bird enemy wing flap |
| `spider.wav` | `SoundEffect` | `gs->audio.spider_attack` | Played when a jumping spider leaps at a gap |
| `fish.wav` | `SoundEffect` | `gs->audio.dive` | Played for fish enemy dive |

### Hazards — `assets/sounds/hazards/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `axe_trap.wav` | `SoundEffect` | `gs->audio.axe` | Played for axe trap swing |

### Surfaces — `assets/sounds/surfaces/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bouncepad.wav` | `SoundEffect` | `gs->audio.spring` | Played when the player lands on a bouncepad |

### Screens — `assets/sounds/screens/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `confirm_ui.wav` | `SoundEffect` | `menu->snd_confirm` | Played on menu confirmation |

### Levels — `assets/sounds/levels/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `water.wav` | `MusicTrack` | `gs->audio.music` | Streamed background music for water-themed levels |
| `lava.wav` | `MusicTrack` | `gs->audio.music` | Streamed background music for lava-themed levels |
| `winds.wav` | `MusicTrack` | `gs->audio.music` | Available theme track; no current campaign/lab level selects it |

---

## Unused Sounds

The following sounds are stored in `assets/sounds/unused/` and are not loaded by the game. They are available as reserves for future use.

| File | Description |
|------|-------------|
| `fireball.wav` | Projectile / fireball effect |
| `saw.wav` | Circular saw spinning |

---

## Audio Configuration

`AppSession` opens raylib's audio device through `audio_open`:

```c
InitAudioDevice();
if (!IsAudioDeviceReady()) { /* fail startup and clean up */ }
```

raylib/miniaudio negotiates the device format and sample rate. Game device-open
failure is fatal; missing optional sound files warn and leave empty slots. The
editor does not open an audio device. CI can use a virtual sink; the explicit
Memory test build uses miniaudio's null backend and does not prove audible output.

---

## Sound Effects vs. Music

| Aspect | `SoundEffect` (effects) | `MusicTrack` (background music) |
|--------|----------------------------|-------------------------------|
| Project API | `sound_load`, `sound_play` | `music_load`, `music_play`, `music_update` |
| Loading | Fully decoded into RAM | Decoded on demand from the backing file/resource |
| Best for | Short, triggered sounds | Long looping tracks |
| Simultaneous | Eight independent raylib sound aliases | One active stream |
| Volume | Sample/user volume multiplied by each voice's distance volume | Authored level volume multiplied by user volume/mute |

Sound effects use raylib `LoadSound` and are fully decoded into memory. Music is selected
by each level's `music_path` and loaded through `LoadMusicStream`. Native decoding can
read incrementally from disk; WebAssembly preloads these WAV files into its
virtual filesystem, so streaming playback does not remove their download or
backing-storage cost. See [Asset Inventory](../asset-inventory/).

---

## Adding a New Sound Effect

1. Place the `.wav` file in the appropriate `assets/sounds/<category>/` subdirectory.
2. Add a `SoundEffect *<name>` field to `AudioResources` in `game.h`.
3. Load it in `src/core/game_resources.c`, called by `game_init`:

```c
gs->audio.<name> = sound_load("assets/sounds/<category>/<name>.wav");
if (!gs->audio.<name>) {
    fprintf(stderr, "Warning: failed to load assets/sounds/<category>/<name>.wav\n");
    /* Non-fatal — game continues without this sound */
}
```

4. Free it in the resource cleanup called by `game_cleanup`:

```c
FREE_CHUNK(gs->audio.<name>);
```

5. Play it wherever the event occurs:

```c
sound_play(gs->audio.<name>, 128);
```

`sound_play` accepts an empty optional slot. Volume remains in the existing
0–128 authored units and is normalized at the raylib boundary. Voices have
independent playback/volume; aliases are stopped and unloaded before their sample.

---

## Adding a New Music Track

```c
// Load (streaming — not fully decoded into RAM)
gs->audio.music = music_load("assets/sounds/levels/new_track.wav");
if (!gs->audio.music) { /* handle error */ }

// Start (loop forever)
music_play(gs->audio.music);

// Volume (0-128)
music_set_volume(64); // 50%

// AppSession pumps this once per frame, including overlay frames
music_update();

// Stop and free
music_unload(gs->audio.music);
gs->audio.music = NULL;
```

The normal session applies authored `music_volume` scaled by the player's saved
volume/mute preferences. Prefer that path to a hard-coded global music volume.
Media licenses are documented separately in [Asset Provenance](../asset-provenance/).
