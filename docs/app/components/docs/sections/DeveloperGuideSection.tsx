export default function DeveloperGuideSection({ visible }: { visible: boolean }) {
  return (
    <section id="developer-guide" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1 className="page-title">Developer Guide</h1>
      <p><a href="home">&#8592; Home</a></p>
      <hr />
      <p>This guide covers the patterns and conventions used in Super Mango and explains how to extend the game safely and consistently.</p>
      <hr />
      <h2>Coding Conventions</h2>
      <h3>Language and Standard</h3>
      <ul>
        <li><strong>C11</strong> (<code>-std=c11</code>)</li>
        <li>Compiler: <code>clang</code> (default), <code>gcc</code> compatible</li>
      </ul>
      <h3>Naming</h3>
      <table>
        <thead><tr><th>Category</th><th>Convention</th><th>Example</th></tr></thead>
        <tbody>
          <tr><td>Files</td><td><code>snake_case</code></td><td><code>player.c</code>, <code>coin.h</code></td></tr>
          <tr><td>Functions</td><td><code>module_verb</code></td><td><code>player_init</code>, <code>coin_update</code></td></tr>
          <tr><td>Struct types</td><td><code>PascalCase</code> via <code>typedef</code></td><td><code>Player</code>, <code>GameState</code>, <code>Coin</code></td></tr>
          <tr><td>Enum values</td><td><code>UPPER_SNAKE_CASE</code></td><td><code>ANIM_IDLE</code>, <code>ANIM_WALK</code></td></tr>
          <tr><td>Constants (<code>#define</code>)</td><td><code>UPPER_SNAKE_CASE</code></td><td><code>FLOOR_Y</code>, <code>TILE_SIZE</code></td></tr>
          <tr><td>Local variables</td><td><code>snake_case</code></td><td><code>dt</code>, <code>frame_ms</code>, <code>elapsed</code></td></tr>
          <tr><td>Assets</td><td><code>snake_case</code></td><td><code>player.png</code>, <code>coin.png</code>, <code>spider.png</code></td></tr>
          <tr><td>Sounds</td><td><code>component_descriptor.wav</code></td><td><code>player_jump.wav</code>, <code>coin.wav</code>, <code>bird.wav</code></td></tr>
        </tbody>
      </table>
      <h3>Memory and Safety Rules</h3>
      <ul>
        <li>Every pointer must be set to <code>NULL</code> <strong>immediately after freeing</strong>. (<code>SDL_Destroy*</code> and <code>free()</code> on <code>NULL</code> are no-ops, preventing double-free crashes.)</li>
        <li>Error paths call <code>SDL_GetError()</code> / <code>IMG_GetError()</code> / <code>Mix_GetError()</code> and write to <code>stderr</code>.</li>
        <li>Resources are <strong>always freed in reverse init order</strong>.</li>
        <li>Use <code>float</code> for positions and velocities; cast to <code>int</code> only at render time (<code>SDL_Rect</code> fields are <code>int</code>).</li>
      </ul>
      <h3>Coordinate System</h3>
      <p>All game-object positions and sizes live in <strong>logical space (400x300)</strong>. Never use <code>WINDOW_W</code> / <code>WINDOW_H</code> for game math -- SDL scales the logical canvas to the OS window automatically.</p>
      <p>See <a href="constants_reference">Constants Reference</a> for all defined constants.</p>
      <hr />
      <h2>Adding a New Entity</h2>
      <p>Every entity follows the same lifecycle pattern:</p>
      <pre><code>{`entity_init    -> load texture, set initial state
entity_update  -> move, apply physics, detect events
entity_render  -> draw to renderer
entity_cleanup -> SDL_DestroyTexture, set to NULL`}</code></pre>
      <p>And optionally:</p>
      <pre><code>{`entity_handle_input   -> if player-controlled
entity_animate        -> static helper, called from entity_update`}</code></pre>
      <h3>Step-by-Step</h3>
      <h4>1. Create the header -- <code>src/collectibles/coin.h</code></h4>
      <pre><code className="language-c">{`#pragma once
#include <SDL.h>

typedef struct {
    float        x, y;    /* logical position (top-left) */
    int          w, h;    /* display size in logical px  */
    int          active;  /* 1 = visible, 0 = collected  */
    SDL_Texture *texture;
} Coin;

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y);
void coin_update(Coin *coin, float dt);
void coin_render(Coin *coin, SDL_Renderer *renderer);
void coin_cleanup(Coin *coin);`}</code></pre>
      <h4>2. Create the implementation -- <code>src/collectibles/coin.c</code></h4>
      <pre><code className="language-c">{`#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "coin.h"

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y) {
    coin->texture = IMG_LoadTexture(renderer, "assets/sprites/collectibles/coin.png");
    if (!coin->texture) {
        fprintf(stderr, "Failed to load coin.png: %s\\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }
    coin->x = x;
    coin->y = y;
    coin->w = 48;
    coin->h = 48;
    coin->active = 1;
}

void coin_render(Coin *coin, SDL_Renderer *renderer) {
    if (!coin->active) return;
    SDL_Rect dst = { (int)coin->x, (int)coin->y, coin->w, coin->h };
    SDL_RenderCopy(renderer, coin->texture, NULL, &dst);
}

void coin_cleanup(Coin *coin) {
    if (coin->texture) {
        SDL_DestroyTexture(coin->texture);
        coin->texture = NULL;
    }
}`}</code></pre>
      <p>The Makefile picks up <code>coin.c</code> automatically -- <strong>no Makefile changes needed</strong>.</p>
      <h4>3. Add texture to <code>GameState</code> in <code>game.h</code></h4>
      <p>Textures are loaded in <code>game_init()</code> and stored in <code>GameState</code>. The entity array and count also live in <code>GameState</code>:</p>
      <pre><code className="language-c">{`#include "coin.h"

typedef struct {
    // ... existing fields ...
    SDL_Texture *tex_coin;    /* GPU texture, loaded in game_init */
    Coin coins[32];           /* fixed-size array -- simple and cache-friendly */
    int  coin_count;          /* how many are currently active */
} GameState;`}</code></pre>
      <h4>4. Wire up in <code>game.c</code></h4>
      <pre><code className="language-c">{`// game_init -- load texture and init entities:
gs->tex_coin = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
coin_init(&gs->coins[0], gs->tex_coin, 200.0f, 100.0f);
gs->coin_count = 1;

// game_loop update section:
for (int i = 0; i < gs->coin_count; i++)
    coin_update(&gs->coins[i], dt);

// game_loop render section (correct layer order):
for (int i = 0; i < gs->coin_count; i++)
    coin_render(&gs->coins[i], gs->renderer);

// game_cleanup (before SDL_DestroyRenderer):
for (int i = 0; i < gs->coin_count; i++)
    coin_cleanup(&gs->coins[i]);`}</code></pre>
      <h4>5. Define entity placements in level TOML</h4>
      <p>Entity spawn positions are defined in level TOML files (e.g. <code>levels/00_sandbox_01.toml</code>). Add your entity placements there, or use the visual editor (<code>make run-editor</code>) to place entities graphically.</p>
      <h4>6. Add debug hitbox -- <code>src/core/debug.c</code></h4>
      <p>Every entity must have hitbox visualization in <code>debug.c</code>:</p>
      <pre><code className="language-c">{`// In debug_render:
for (int i = 0; i < gs->coin_count; i++) {
    if (!gs->coins[i].active) continue;
    SDL_Rect hb = { (int)gs->coins[i].x, (int)gs->coins[i].y,
                     gs->coins[i].w, gs->coins[i].h };
    SDL_SetRenderDrawColor(gs->renderer, 255, 255, 0, 128);
    SDL_RenderDrawRect(gs->renderer, &hb);
}`}</code></pre>
      <p>Also add <code>debug_log</code> calls in <code>game.c</code> for any significant entity events (collection, destruction, spawn).</p>
      <hr />
      <h2>Adding Physics to an Entity</h2>
      <p>Use the same pattern as <code>player_update</code>:</p>
      <pre><code className="language-c">{`/* Apply gravity while airborne */
if (!entity->on_ground) {
    entity->vy += GRAVITY * dt;
}

/* Integrate position */
entity->x += entity->vx * dt;
entity->y += entity->vy * dt;

/* Floor collision */
if (entity->y + entity->h >= FLOOR_Y) {
    entity->y        = (float)(FLOOR_Y - entity->h);
    entity->vy       = 0.0f;
    entity->on_ground = 1;
} else {
    entity->on_ground = 0;
}

/* Horizontal clamp */
if (entity->x < 0.0f)                entity->x = 0.0f;
if (entity->x > GAME_W - entity->w)  entity->x = (float)(GAME_W - entity->w);`}</code></pre>
      <p><code>GRAVITY</code>, <code>FLOOR_Y</code>, <code>GAME_W</code>, and <code>GAME_H</code> are all defined in <code>game.h</code> and available to any file that includes it. See <a href="constants_reference">Constants Reference</a> for values.</p>
      <hr />
      <h2>Adding a New Sound Effect</h2>
      <p>All sound files are <code>.wav</code> format, named with the convention <code>component_descriptor.wav</code>:</p>
      <table>
        <thead><tr><th>Sound</th><th>File</th></tr></thead>
        <tbody>
          <tr><td>Player jump</td><td><code>player_jump.wav</code></td></tr>
          <tr><td>Player hit</td><td><code>player_hit.wav</code></td></tr>
          <tr><td>Coin collect</td><td><code>coin.wav</code></td></tr>
          <tr><td>Bouncepad</td><td><code>bouncepad.wav</code></td></tr>
          <tr><td>Bird</td><td><code>bird.wav</code></td></tr>
          <tr><td>Fish</td><td><code>fish.wav</code></td></tr>
          <tr><td>Spider</td><td><code>spider.wav</code></td></tr>
          <tr><td>Axe trap</td><td><code>axe_trap.wav</code></td></tr>
        </tbody>
      </table>
      <p>Steps to add a new sound:</p>
      <ol>
        <li>Place <code>.wav</code> in <code>assets/sounds/&lt;category&gt;/</code>.</li>
        <li>Add <code>Mix_Chunk *snd_&lt;name&gt;;</code> to <code>GameState</code> in <code>game.h</code>.</li>
        <li>Load in <code>game_init</code> (non-fatal -- warn but continue):</li>
      </ol>
      <pre><code className="language-c">{`gs->snd_<name> = Mix_LoadWAV("assets/sounds/<category>/<name>.wav");
if (!gs->snd_<name>) {
    fprintf(stderr, "Warning: could not load <name>.wav: %s\\n", Mix_GetError());
}`}</code></pre>
      <ol start={4}><li>Free in <code>game_cleanup</code>:</li></ol>
      <pre><code className="language-c">{`if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }`}</code></pre>
      <ol start={5}><li>Play wherever needed:</li></ol>
      <pre><code className="language-c">{`if (gs->snd_<name>) Mix_PlayChannel(-1, gs->snd_<name>, 0);`}</code></pre>
      <p>See <a href="sounds">Sounds</a> for the full list of available sound files.</p>
      <hr />
      <h2>Adding Background Music</h2>
      <p>Background music is loaded via <code>Mix_LoadMUS</code> (not <code>Mix_LoadWAV</code>). The track path is configured per level via <code>music_path</code> in the TOML file:</p>
      <pre><code className="language-c">{`// Load (path from level TOML music_path field)
gs->music = Mix_LoadMUS(level->music_path);

// Play (looping)
Mix_PlayMusic(gs->music, -1);
Mix_VolumeMusic(level->music_volume);  // 0-128, configured per level

// Cleanup
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;`}</code></pre>
      <hr />
      <h2>Adding HUD / Text Rendering</h2>
      <p><code>SDL2_ttf</code> is already initialized in <code>main.c</code>. The font <code>round9x13.ttf</code> is in <code>assets/fonts/</code>.</p>
      <pre><code className="language-c">{`// Load font
TTF_Font *font = TTF_OpenFont("assets/fonts/round9x13.ttf", 13);
if (!font) { fprintf(stderr, "TTF_OpenFont: %s\\n", TTF_GetError()); }

// Render text to a surface, then upload to a texture
SDL_Color white = {255, 255, 255, 255};
SDL_Surface *surf = TTF_RenderText_Solid(font, "Score: 0", white);
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FreeSurface(surf);

// Draw the texture
SDL_Rect dst = {10, 10, surf->w, surf->h};
SDL_RenderCopy(renderer, tex, NULL, &dst);

// Cleanup
SDL_DestroyTexture(tex);
TTF_CloseFont(font);`}</code></pre>
      <p>The HUD renders hearts (lives), life counter, and score. It is drawn after all game entities so it always appears on top.</p>
      <hr />
      <h2>Render Layer Order</h2>
      <p>Always draw in painter&#39;s algorithm order (back to front). The game currently uses 35 layers:</p>
      <pre><code>{` 1. Parallax background    (up to 8 layers from assets/sprites/backgrounds/, per level)
 2. Platforms              (platform.png 9-slice pillars)
 3. Floor tiles            (per-level tileset at FLOOR_Y, with floor-gap openings)
 4. Float platforms        (float_platform.png 3-slice hovering surfaces)
 5. Spike rows             (spike.png ground-level spike strips)
 6. Spike platforms        (spike_platform.png elevated spike hazards)
 7. Bridges                (bridge.png tiled crumble walkways)
 8. Bouncepads medium      (bouncepad_medium.png standard spring pads)
 9. Bouncepads small       (bouncepad_small.png low spring pads)
10. Bouncepads high        (bouncepad_high.png tall spring pads)
11. Rails                  (rail.png bitmask tile tracks)
12. Vines                  (vine.png climbable)
13. Ladders                (ladder.png climbable)
14. Ropes                  (rope.png climbable)
15. Coins                  (coin.png collectibles)
16. Star yellows           (star_yellow.png health pickups)
17. Star greens            (star_green.png health pickups)
18. Star reds              (star_red.png health pickups)
19. Last star              (end-of-level star using HUD star sprite)
20. Blue flames            (blue_flame.png erupting from floor gaps)
21. Fire flames            (fire_flame.png fire variant erupting from floor gaps)
22. Fish                   (fish.png jumping water enemies)
23. Faster fish            (faster_fish.png fast jumping enemies)
24. Water                  (water.png animated strip)
25. Spike blocks           (spike_block.png rail-riding hazards)
26. Axe traps              (axe_trap.png swinging hazards)
27. Circular saws          (circular_saw.png patrol hazards)
28. Spiders                (spider.png ground patrol)
29. Jumping spiders        (jumping_spider.png jumping patrol)
30. Birds                  (bird.png slow sine-wave)
31. Faster birds           (faster_bird.png fast sine-wave)
32. Player                 (player.png animated)
33. Fog                    (fog_background_1/2.png sliding overlay)
34. HUD                    (hearts, lives, score -- always on top)
35. Debug overlay          (FPS, hitboxes, event log -- when --debug)`}</code></pre>
      <p>See <a href="architecture">Architecture</a> for details on the render pipeline.</p>
      <hr />
      <h2>Sprite Sheet Workflow</h2>
      <p>To analyze a new sprite sheet:</p>
      <pre><code className="language-sh">{`python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png`}</code></pre>
      <p>Frame math:</p>
      <pre><code>{`source_x = (frame_index % num_cols) * frame_w
source_y = (frame_index / num_cols) * frame_h`}</code></pre>
      <p>Standard animation row layout (most assets in this pack):</p>
      <table>
        <thead><tr><th>Row</th><th>Animation</th><th>Notes</th></tr></thead>
        <tbody>
          <tr><td>0</td><td>Idle</td><td>1-4 frames, subtle</td></tr>
          <tr><td>1</td><td>Walk / Run</td><td>6-8 frames, looping</td></tr>
          <tr><td>2</td><td>Jump (up)</td><td>2-4 frames, one-shot</td></tr>
          <tr><td>3</td><td>Fall / Land</td><td>2-4 frames</td></tr>
          <tr><td>4</td><td>Attack</td><td>4-8 frames, one-shot</td></tr>
          <tr><td>5</td><td>Death / Hurt</td><td>4-6 frames, one-shot</td></tr>
        </tbody>
      </table>
      <p>See <a href="assets">Assets</a> for sprite sheet dimensions and <a href="player_module">Player Module</a> for animation state machine details.</p>
      <hr />
      <h2>Checklist: Adding a New Entity</h2>
      <ul>
        <li><input type="checkbox" disabled /> Create <code>src/&lt;entity&gt;.h</code> with struct and function declarations</li>
        <li><input type="checkbox" disabled /> Create <code>src/&lt;entity&gt;.c</code> with init, update, render, cleanup</li>
        <li><input type="checkbox" disabled /> Add <code>#include &quot;&lt;entity&gt;.h&quot;</code> to <code>game.h</code></li>
        <li><input type="checkbox" disabled /> Add texture pointer, entity array, and count to <code>GameState</code> (by value, not pointer)</li>
        <li><input type="checkbox" disabled /> Load texture in <code>game_init</code> in <code>game.c</code></li>
        <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_init</code> in <code>game_init</code></li>
        <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_update</code> in <code>game_loop</code> update section</li>
        <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_render</code> in <code>game_loop</code> render section (correct layer order)</li>
        <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_cleanup</code> in <code>game_cleanup</code> (before <code>SDL_DestroyRenderer</code>)</li>
        <li><input type="checkbox" disabled /> Set all freed pointers to <code>NULL</code></li>
        <li><input type="checkbox" disabled /> Define entity spawn positions in a level TOML file or use the visual editor</li>
        <li><input type="checkbox" disabled /> Add hitbox visualization in <code>debug.c</code></li>
        <li><input type="checkbox" disabled /> Add <code>debug_log</code> calls in <code>game.c</code> for significant entity events</li>
        <li><input type="checkbox" disabled /> Build with <code>make</code> -- no Makefile changes needed</li>
        <li><input type="checkbox" disabled /> Test with <code>--debug</code> flag to verify hitboxes render correctly</li>
      </ul>
      <hr />
      <h2>Related Pages</h2>
      <ul>
        <li><a href="home">Home</a> -- project overview</li>
        <li><a href="architecture">Architecture</a> -- system design and game loop</li>
        <li><a href="build_system">Build System</a> -- compiling and running</li>
        <li><a href="source_files">Source Files</a> -- module-by-module reference</li>
        <li><a href="assets">Assets</a> -- sprite sheets and textures</li>
        <li><a href="sounds">Sounds</a> -- audio files and music</li>
        <li><a href="player_module">Player Module</a> -- player-specific details</li>
        <li><a href="constants_reference">Constants Reference</a> -- all defined constants</li>
      </ul>
    </section>
  );
}
