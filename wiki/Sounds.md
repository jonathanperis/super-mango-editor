# Sounds

← [Home](Home)

---

All audio files live in the `sounds/` directory. SDL2_mixer handles both short sound effects (`Mix_Chunk` / WAV) and long streaming music (`Mix_Music` / OGG/MP3).

---

## Currently Used Sounds

| File | Type | Used By | Description |
|------|------|---------|-------------|
| `jump.wav` | `Mix_Chunk` | `gs->snd_jump` | Played on Space press when `on_ground == 1` |
| `water-ambience.ogg` | `Mix_Music` | `gs->music` | Background ambient loop, volume ~10% |

---

## All Available Sound Files

### Sound Effects (WAV — `Mix_Chunk`)

| File | Suggested Use |
|------|---------------|
| `jump.wav` | ✅ In use — player jump |
| `coin.wav` | Coin collected |
| `confirm-ui.wav` | Menu confirmation / level complete |
| `dive.wav` | Dive or fast-fall move |
| `fireball.wav` | Projectile / fireball fired |
| `flapping.wav` | Bird enemy wing flap |
| `hit.wav` | Player or enemy takes damage |
| `lava.wav` | Player touches lava |
| `saw.wav` | Circular saw spinning |
| `strong-wind.wav` | Wind gust / outdoor ambience |

### Music Tracks (OGG/MP3 — `Mix_Music`)

| File | Suggested Use |
|------|---------------|
| `water-ambience.ogg` | ✅ In use — ambient background loop |
| `spider-attack.mp3` | Tense combat / chase music |
| `swinging-axe.mp3` | Trap-heavy stage music |

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

Background music volume is set to `13` out of `128` (`MIX_MAX_VOLUME`), approximately 10%.

---

## Adding a New Sound Effect

1. Place the `.wav` file in `sounds/`.
2. Add a `Mix_Chunk *snd_<name>` field to `GameState` in `game.h`.
3. Load it in `game_init`:

```c
gs->snd_<name> = Mix_LoadWAV("sounds/<name>.wav");
if (!gs->snd_<name>) {
    fprintf(stderr, "Failed to load <name>.wav: %s\n", Mix_GetError());
    // clean up previously loaded resources, then exit
}
```

4. Free it in `game_cleanup` (before `SDL_DestroyRenderer`):

```c
if (gs->snd_<name>) {
    Mix_FreeChunk(gs->snd_<name>);
    gs->snd_<name> = NULL;
}
```

5. Play it wherever the event occurs:

```c
if (gs->snd_<name>) Mix_PlayChannel(-1, gs->snd_<name>, 0);
```

The `if` guard is important: if the WAV fails to load for any reason, the game continues without crashing.

---

## Adding a New Music Track

```c
// Load
gs->music = Mix_LoadMUS("sounds/new-track.mp3");
if (!gs->music) { /* handle error */ }

// Start (loop forever)
Mix_PlayMusic(gs->music, -1);

// Volume (0–128)
Mix_VolumeMusic(64);  // 50%

// Stop and free
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;
```

`Mix_Music` **streams** from disk; it does not load the entire file into RAM. This keeps memory usage low for large MP3/OGG files.

---

## Sound vs. Music Types

| Aspect | `Mix_Chunk` (sound effects) | `Mix_Music` (background music) |
|--------|----------------------------|-------------------------------|
| API | `Mix_LoadWAV`, `Mix_PlayChannel` | `Mix_LoadMUS`, `Mix_PlayMusic` |
| Loading | Fully decoded into RAM | Streamed from disk |
| Best for | Short, triggered sounds | Long looping tracks |
| Simultaneous | Multiple channels | One at a time |
| Volume | `Mix_VolumeChunk` | `Mix_VolumeMusic` |
