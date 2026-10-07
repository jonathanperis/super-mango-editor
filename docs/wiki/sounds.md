# Sounds

<a id="home"></a>

---

All audio files live in `assets/sounds/`, organized into categorized subdirectories.
They are mono 16-bit 22050 Hz `.wav` files synthesized by `tools/gen_sounds.py`
(see [Generated Sounds](../assets/#generated-sounds)); run `make sounds` to
regenerate them. Regeneration skips files whose samples are within 1 LSB of the
new output, and `make docs-drift` runs `tools/gen_sounds.py --check` with the same
tolerance. raylib handles decoded `Sound` samples and streamed `Music`;
the project-owned `SoundEffect` and `MusicTrack` types manage their lifetimes.

---

## Sound Reference

The tables include runtime sounds and bundled theme tracks. A bundled track may
be available for level authors without being selected by a current campaign or lab.

### Player — `assets/sounds/player/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `player_jump.wav` | `SoundEffect` | `gs->audio.jump` | Rising square-wave blip; played when a jump starts, including buffered/coyote jumps and climb dismounts |
| `player_hit.wav` | `SoundEffect` | `gs->audio.hit` | Noisy descending thud; played when the player takes damage |

### Collectibles — `assets/sounds/collectibles/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `coin.wav` | `SoundEffect` | `gs->audio.coin` | Two-tone chime (B5 then E6); played when the player collects a coin |

### Entities — `assets/sounds/entities/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bird.wav` | `SoundEffect` | `gs->audio.flap` | Two soft band-passed noise puffs; played for each bird wing flap |
| `spider.wav` | `SoundEffect` | `gs->audio.spider_attack` | Hiss, rising zip and leg clicks; played when a jumping spider leaps at a gap |
| `fish.wav` | `SoundEffect` | `gs->audio.dive` | Filtered noise splash plus rising bubbles; played when the player falls into a water gap |

### Hazards — `assets/sounds/hazards/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `axe_trap.wav` | `SoundEffect` | `gs->audio.axe` | Filter-swept noise whoosh; played for axe trap swing |

### Surfaces — `assets/sounds/surfaces/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `bouncepad.wav` | `SoundEffect` | `gs->audio.spring` | Sine "boing" with fading vibrato; played when the player lands on a bouncepad |

### Screens — `assets/sounds/screens/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `confirm_ui.wav` | `SoundEffect` | `menu->snd_confirm` | Two quick square blips (E5, B5); played on menu confirmation |

### Levels — `assets/sounds/levels/`

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `water.wav` | `MusicTrack` | `gs->audio.music` | 6 s seamless loop: filtered-noise wash and surf with slow swells and bubbles; water-themed levels |
| `lava.wav` | `MusicTrack` | `gs->audio.music` | 6 s seamless loop: low brown-noise rumble, sizzle, slow bubbles and crackles; lava-themed levels |
| `winds.wav` | `MusicTrack` | `gs->audio.music` | 8 s seamless loop: band-limited noise with drifting band and gusts; available, no current level selects it |

---

## No Reserve Sounds

There is no `assets/sounds/unused/` directory: every WAV is generated and
loaded. `tools/gen_sounds.py --check` fails if a WAV appears that the generator
does not produce, so unlicensed media cannot slip back into releases.

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
| Simultaneous | One shared pool of eight independent raylib sound aliases | One active stream |
| Volume | Sample/user volume multiplied by each voice's distance volume | Authored level volume multiplied by user volume/mute |

Sound effects use raylib `LoadSound` and are fully decoded into memory. Music is selected
by each level's `music_path` and loaded through `LoadMusicStream`. Native decoding can
read incrementally from disk; WebAssembly preloads these WAV files into its
virtual filesystem, so streaming playback does not remove their download or
backing-storage cost. See [Asset Inventory](../asset-inventory/).

---

## Adding a New Sound Effect

1. Add a recipe and a `SOUNDS` row to `tools/gen_sounds.py`, then run `make sounds`
   to write `assets/sounds/<category>/<name>.wav`.
2. Add a `SoundEffect *<name>` field to `AudioResources` in `game.h`.
3. Add a row to the `s_optional_chunks` table in `src/core/game_resources.c`:

```c
{ CHUNK_FIELD(<name>), "assets/sounds/<category>/<name>.wav", "<name>.wav" },
```

   `game_resources_load` (called by `game_init`) loads every row through
   `sound_load`, printing a warning and leaving the slot `NULL` when a file is
   missing (non-fatal). `game_resources_cleanup` walks the same table in reverse
   with `FREE_CHUNK`, so no separate free call is needed.

4. Play it wherever the event occurs:

```c
sound_play(gs->audio.<name>, 128);
```

`sound_play` accepts an empty optional slot. Volume remains in the existing
0–128 authored units and is normalized at the raylib boundary. Voices have
independent playback/volume; aliases are stopped and unloaded before their sample.
If all eight voices are busy, a new effect is dropped rather than interrupting
one already playing. `sound_set_volume` updates a sample's multiplier and its
live aliases; the per-play multiplier remains independent.

The pinned dependency includes three documented audio fixes: alias converter
cache release, WAV decoder release, and miniaudio's zero-allocation passthrough
initialization. They are applied on every platform by `tools/build_raylib.py`;
see `vendor/raylib/README.md` and the [Build System](../build-system/).

---

## Adding a New Music Track

Ambient tracks are short loops: the generator crossfades each recording's tail
into its head so `MusicTrack` can repeat it without a click.

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
