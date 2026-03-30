# Sounds

← [Home](home)

---

All audio files live in the `sounds/` directory. All sound files use the `.wav` format. SDL2_mixer handles both short sound effects (`Mix_Chunk` via `Mix_LoadWAV`) and streaming music (`Mix_Music` via `Mix_LoadMUS`).

---

## Currently Used Sounds

| File | Type | GameState Field | Description |
|------|------|-----------------|-------------|
| `player_jump.wav` | `Mix_Chunk` | `gs->snd_jump` | Played on Space press when `on_ground == 1` |
| `coin.wav` | `Mix_Chunk` | `gs->snd_coin` | Played when the player collects a coin |
| `player_hit.wav` | `Mix_Chunk` | `gs->snd_hit` | Played when the player takes damage |
| `bouncepad.wav` | `Mix_Chunk` | `gs->snd_spring` | Played when the player lands on a bouncepad |
| `bird.wav` | `Mix_Chunk` | `gs->snd_flap` | Played for bird enemy wing flap |
| `spider.wav` | `Mix_Chunk` | `gs->snd_spider_attack` | Played for spider enemy attack |
| `fish.wav` | `Mix_Chunk` | `gs->snd_dive` | Played for fish enemy dive |
| `axe_trap.wav` | `Mix_Chunk` | `gs->snd_axe` | Played for axe trap swing |
| `game_music.wav` | `Mix_Music` | `gs->music` | Background music loop, loaded via `Mix_LoadMUS` (streaming) |

---

## Unused Sounds

The following sounds are stored in `sounds/unused/` and are not loaded by the game. They are available as reserves for future use.

| File | Description |
|------|-------------|
| `confirm_ui.wav` | Menu confirmation / level complete |
| `fireball.wav` | Projectile / fireball effect |
| `lava.wav` | Player touches lava |
| `saw.wav` | Circular saw spinning |
| `strong_wind.wav` | Wind gust / outdoor ambience |

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
| Loading | Fully decoded into RAM | Streamed from disk |
| Best for | Short, triggered sounds | Long looping tracks |
| Simultaneous | Multiple channels | One at a time |
| Volume | `Mix_VolumeChunk` | `Mix_VolumeMusic` |

All eight sound effects use `Mix_LoadWAV` and are fully loaded into memory. The background music (`game_music.wav`) uses `Mix_LoadMUS` which streams from disk, keeping memory usage low.

---

## Adding a New Sound Effect

1. Place the `.wav` file in `sounds/`.
2. Add a `Mix_Chunk *snd_<name>` field to `GameState` in `game.h`.
3. Load it in `game_init`:

```c
gs->snd_<name> = Mix_LoadWAV("sounds/<name>.wav");
if (!gs->snd_<name>) {
    fprintf(stderr, "Warning: failed to load <name>.wav: %s\n", Mix_GetError());
    /* Non-fatal — game continues without this sound */
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
// Load (streaming — not fully decoded into RAM)
gs->music = Mix_LoadMUS("sounds/new_track.wav");
if (!gs->music) { /* handle error */ }

// Start (loop forever)
Mix_PlayMusic(gs->music, -1);

// Volume (0-128)
Mix_VolumeMusic(64);  // 50%

// Stop and free
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;
```

`Mix_Music` **streams** from disk; it does not load the entire file into RAM. This keeps memory usage low for large audio files.
