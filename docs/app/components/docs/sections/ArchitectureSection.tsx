export default function ArchitectureSection({ visible }: { visible: boolean }) {
  return (
    <section id="architecture" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1 className="page-title">Architecture</h1>
      <p><a href="home">&#8592; Home</a></p>
      <hr />
      <h2>Overview</h2>
      <p>Super Mango follows a classic <strong>init &#8594; loop &#8594; cleanup</strong> pattern. A single <code>GameState</code> struct is the owner of every resource in the game and is passed by pointer to every function that needs to read or modify it.</p>
      <hr />
      <h2>Startup Sequence</h2>
      <pre><code>{`main()
  \u251C\u2500\u2500 SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  \u251C\u2500\u2500 IMG_Init(IMG_INIT_PNG)
  \u251C\u2500\u2500 TTF_Init()
  \u251C\u2500\u2500 Mix_OpenAudio(44100, stereo, 2048 buffer)
  \u2502
  \u251C\u2500\u2500 game_init(&gs)
  \u2502     \u251C\u2500\u2500 SDL_CreateWindow  \u2192 gs.window
  \u2502     \u251C\u2500\u2500 SDL_CreateRenderer \u2192 gs.renderer
  \u2502     \u251C\u2500\u2500 SDL_RenderSetLogicalSize(GAME_W, GAME_H)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Load all textures (engine resources) \u2500\u2500
  \u2502     \u251C\u2500\u2500 parallax_init(&gs.parallax, gs.renderer)  (multi-layer background, configured per level)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.floor_tile        (sprites/levels/grass_tileset.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.platform_tex      (sprites/surfaces/Platform.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 water_init(&gs.water, gs.renderer)      (sprites/foregrounds/water.png)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spider_tex        (sprites/entities/spider.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.jumping_spider_tex (sprites/entities/jumping_spider.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bird_tex          (sprites/entities/bird.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.faster_bird_tex   (sprites/entities/faster_bird.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.fish_tex          (sprites/entities/fish.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.coin_tex          (sprites/collectibles/coin.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_medium_tex (sprites/surfaces/bouncepad_medium.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.vine_tex          (sprites/surfaces/vine.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.ladder_tex        (sprites/surfaces/ladder.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.rope_tex          (sprites/surfaces/rope.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_small_tex  (sprites/surfaces/bouncepad_small.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_high_tex   (sprites/surfaces/bouncepad_high.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.rail_tex          (sprites/surfaces/rail.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_block_tex   (sprites/hazards/spike_block.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.float_platform_tex (sprites/surfaces/float_platform.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bridge_tex        (sprites/surfaces/bridge.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_yellow_tex   (sprites/collectibles/star_yellow.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_green_tex    (sprites/collectibles/star_green.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_red_tex      (sprites/collectibles/star_red.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.axe_trap_tex      (sprites/hazards/axe_trap.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.circular_saw_tex  (sprites/hazards/circular_saw.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.blue_flame_tex    (sprites/hazards/blue_flame.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.fire_flame_tex    (sprites/hazards/fire_flame.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.faster_fish_tex   (sprites/entities/faster_fish.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_tex         (sprites/hazards/spike.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_platform_tex (sprites/hazards/spike_platform.png \u2014 non-fatal)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Load all sound effects \u2500\u2500
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_spring        (sounds/surfaces/bouncepad.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_axe           (sounds/hazards/axe_trap.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_flap          (sounds/entities/bird.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_spider_attack (sounds/entities/spider.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_dive          (sounds/entities/fish.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_jump          (sounds/player/player_jump.wav \u2014 fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_coin          (sounds/collectibles/coin.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_hit           (sounds/player/player_hit.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadMUS     \u2192 gs.music             (per-level music_path \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_PlayMusic(-1)                      (loop forever, per-level volume)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Initialise game objects \u2500\u2500
  \u2502     \u251C\u2500\u2500 player_init(&gs.player, gs.renderer)
  \u2502     \u251C\u2500\u2500 fog_init(&gs.fog, gs.renderer)         (fog_background_1.png, fog_background_2.png)
  \u2502     \u251C\u2500\u2500 hud_init(&gs.hud, gs.renderer)
  \u2502     \u251C\u2500\u2500 if (debug_mode) debug_init(&gs.debug)
  \u2502     \u251C\u2500\u2500 level_loader_load(&gs)                 (load level from TOML, entity inits + floor gap positions)
  \u2502     \u251C\u2500\u2500 hearts/lives/score/score_life_next initialisation
  \u2502     \u251C\u2500\u2500 ctrl_pending_init = 1 \u2014 deferred gamepad init (avoids antivirus/HID delays)
  \u2514\u2500\u2500 gamepad subsystem initializes on first rendered frame via background thread
  \u2502
  \u251C\u2500\u2500 game_loop(&gs)          \u2190 see Game Loop section below
  \u2502
  \u2514\u2500\u2500 game_cleanup(&gs)       \u2190 reverse init order
        \u251C\u2500\u2500 SDL_GameControllerClose(gs->controller)  \u2190 if non-NULL
        \u251C\u2500\u2500 SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER)
        \u251C\u2500\u2500 hud_cleanup
        \u251C\u2500\u2500 fog_cleanup
        \u251C\u2500\u2500 player_cleanup
        \u251C\u2500\u2500 Mix_HaltMusic + Mix_FreeMusic
        \u251C\u2500\u2500 Mix_FreeChunk (snd_jump)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_coin)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_hit)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_spring)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_axe)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_flap)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_spider_attack)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_dive)
        \u251C\u2500\u2500 water_cleanup
        \u251C\u2500\u2500 SDL_DestroyTexture (fire_flame_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (blue_flame_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (axe_trap_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (circular_saw_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_block_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bridge_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (float_platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (rail_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_high_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_medium_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_small_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (rope_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (ladder_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (vine_tex)
        \u251C\u2500\u2500 last_star_cleanup
        \u251C\u2500\u2500 SDL_DestroyTexture (star_red_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (star_green_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (star_yellow_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (coin_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (faster_fish_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (fish_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (faster_bird_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bird_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (jumping_spider_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spider_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (floor_tile)
        \u251C\u2500\u2500 parallax_cleanup
        \u251C\u2500\u2500 SDL_DestroyRenderer
        \u2514\u2500\u2500 SDL_DestroyWindow
  \u2502
  \u251C\u2500\u2500 Mix_CloseAudio
  \u251C\u2500\u2500 TTF_Quit
  \u251C\u2500\u2500 IMG_Quit
  \u2514\u2500\u2500 SDL_Quit`}</code></pre>
      <hr />
      <h2>Game Loop</h2>
      <p>The loop runs at <strong>60 FPS</strong>, capped via VSync + a manual <code>SDL_Delay</code> fallback. Each frame has four distinct phases:</p>
      <pre><code>{`while (gs.running) {
  1. Delta Time   \u2014 measure ms since last frame \u2192 dt (seconds)
  2. Events       \u2014 SDL_PollEvent (quit / ESC key)
                    SDL_CONTROLLERDEVICEADDED   \u2014 opens a newly plugged-in controller
                    SDL_CONTROLLERDEVICEREMOVED \u2014 closes and NULLs gs->controller when unplugged
                    SDL_CONTROLLERBUTTONDOWN (START) \u2014 sets gs->running = 0 to quit
  3. Update       \u2014 player_handle_input \u2192 player_update (incl. bouncepad, float-platform, bridge landing)
                    \u2192 bouncepad response (animation + spring sound)
                    \u2192 spiders_update \u2192 jumping_spiders_update \u2192 birds_update \u2192 faster_birds_update
                    \u2192 fish_update \u2192 faster_fish_update \u2192 spike_blocks_update \u2192 spikes_update
                    \u2192 spike_platforms_update \u2192 circular_saws_update \u2192 axe_traps_update
                    \u2192 blue_flames_update \u2192 fire_flames_update \u2192 float_platforms_update \u2192 bridges_update
                    \u2192 spider collision \u2192 jumping_spider collision \u2192 bird collision \u2192 faster_bird collision
                    \u2192 fish collision \u2192 faster_fish collision \u2192 spike_block collision (+ push impulse)
                    \u2192 spike collision \u2192 spike_platform collision \u2192 circular_saw collision
                    \u2192 axe_trap collision \u2192 blue_flame collision \u2192 fire_flame collision
                    \u2192 floor gap fall detection (instant death)
                    \u2192 coin\u2013player collision \u2192 star_yellow\u2013player collision
                    \u2192 star_green\u2013player collision \u2192 star_red\u2013player collision
                    \u2192 last_star\u2013player collision
                    \u2192 heart/lives/score_life_next logic
                    \u2192 water_update \u2192 fog_update \u2192 bouncepads_update (small, medium, high)
                    \u2192 debug_update (if --debug)
  4. Render       \u2014 clear \u2192 parallax background \u2192 platforms \u2192 floor tiles
                    \u2192 float platforms \u2192 spike rows \u2192 spike platforms \u2192 bridges
                    \u2192 bouncepads (medium, small, high) \u2192 rails
                    \u2192 vines \u2192 ladders \u2192 ropes \u2192 coins \u2192 yellow stars
                    \u2192 green stars \u2192 red stars \u2192 last star
                    \u2192 blue flames \u2192 fire flames \u2192 fish \u2192 faster fish \u2192 water
                    \u2192 spike blocks \u2192 axe traps \u2192 circular saws
                    \u2192 spiders \u2192 jumping spiders \u2192 birds \u2192 faster birds
                    \u2192 player \u2192 fog \u2192 hud
                    \u2192 debug overlay (if --debug) \u2192 present
}`}</code></pre>
      <h3>Delta Time</h3>
      <pre><code className="language-c">{`Uint64 now = SDL_GetTicks64();
float  dt  = (float)(now - prev) / 1000.0f;
prev = now;`}</code></pre>
      <p>All velocities are expressed in <strong>pixels per second</strong>. Multiplying by <code>dt</code> (seconds) gives the correct displacement per frame regardless of the actual frame rate.</p>
      <h3>Render Order (back to front)</h3>
      <table>
        <thead><tr><th>Layer</th><th>What</th><th>How</th></tr></thead>
        <tbody>
          <tr><td>1</td><td>Background</td><td>Up to 8 layers from <code>assets/sprites/backgrounds/</code> configured per level via <code>[[background_layers]]</code> in TOML, tiled horizontally, each scrolling at a different speed fraction of <code>cam_x</code></td></tr>
          <tr><td>2</td><td>Platforms</td><td><code>platform.png</code> 9-slice tiled pillar stacks (drawn before floor so pillars sink into ground)</td></tr>
          <tr><td>3</td><td>Floor</td><td><code>grass_tileset.png</code> 9-slice tiled across world width at <code>FLOOR_Y</code>, with floor-gap openings</td></tr>
          <tr><td>4</td><td>Float platforms</td><td><code>float_platform.png</code> 3-slice hovering surfaces (static, crumble, rail modes)</td></tr>
          <tr><td>5</td><td>Spike rows</td><td><code>spike.png</code> ground-level spike strips on the floor surface</td></tr>
          <tr><td>6</td><td>Spike platforms</td><td><code>spike_platform.png</code> elevated spike hazard surfaces</td></tr>
          <tr><td>7</td><td>Bridges</td><td><code>bridge.png</code> tiled crumble walkways</td></tr>
          <tr><td>8</td><td>Bouncepads (medium)</td><td><code>bouncepad_medium.png</code> standard-launch spring pads</td></tr>
          <tr><td>9</td><td>Bouncepads (small)</td><td><code>bouncepad_small.png</code> low-launch spring pads</td></tr>
          <tr><td>10</td><td>Bouncepads (high)</td><td><code>bouncepad_high.png</code> high-launch spring pads</td></tr>
          <tr><td>11</td><td>Rails</td><td><code>rail.png</code> bitmask tile tracks for spike blocks and float platforms</td></tr>
          <tr><td>12</td><td>Vines</td><td><code>vine.png</code> climbable plant decorations hanging from platforms</td></tr>
          <tr><td>13</td><td>Ladders</td><td><code>ladder.png</code> climbable ladder structures</td></tr>
          <tr><td>14</td><td>Ropes</td><td><code>rope.png</code> climbable rope segments</td></tr>
          <tr><td>15</td><td>Coins</td><td><code>coin.png</code> collectible sprites drawn on top of platforms</td></tr>
          <tr><td>16</td><td>Star yellows</td><td><code>star_yellow.png</code> collectible star pickups</td></tr>
          <tr><td>17</td><td>Star greens</td><td><code>star_green.png</code> collectible star pickups</td></tr>
          <tr><td>18</td><td>Star reds</td><td><code>star_red.png</code> collectible star pickups</td></tr>
          <tr><td>19</td><td>Last star</td><td>end-of-level star collectible (uses HUD star sprite)</td></tr>
          <tr><td>20</td><td>Blue flames</td><td><code>blue_flame.png</code> animated flame hazards erupting from floor gaps</td></tr>
          <tr><td>21</td><td>Fire flames</td><td><code>fire_flame.png</code> animated fire variant flame hazards erupting from floor gaps</td></tr>
          <tr><td>22</td><td>Fish</td><td><code>fish.png</code> animated jumping enemies, drawn before water for submerged look</td></tr>
          <tr><td>23</td><td>Faster fish</td><td><code>faster_fish.png</code> fast aggressive jumping fish enemies</td></tr>
          <tr><td>24</td><td>Water</td><td><code>water.png</code> animated scrolling strip at the bottom</td></tr>
          <tr><td>25</td><td>Spike blocks</td><td><code>spike_block.png</code> rotating rail-riding hazards</td></tr>
          <tr><td>26</td><td>Axe traps</td><td><code>axe_trap.png</code> swinging axe hazards</td></tr>
          <tr><td>27</td><td>Circular saws</td><td><code>circular_saw.png</code> spinning blade hazards</td></tr>
          <tr><td>28</td><td>Spiders</td><td><code>spider.png</code> animated ground patrol enemies</td></tr>
          <tr><td>29</td><td>Jumping spiders</td><td><code>jumping_spider.png</code> animated jumping patrol enemies</td></tr>
          <tr><td>30</td><td>Birds</td><td><code>bird.png</code> slow sine-wave sky patrol enemies</td></tr>
          <tr><td>31</td><td>Faster birds</td><td><code>faster_bird.png</code> fast aggressive sky patrol enemies</td></tr>
          <tr><td>32</td><td>Player</td><td>Animated sprite sheet, drawn on top of environment</td></tr>
          <tr><td>33</td><td>Fog</td><td><code>fog_background_1.png</code> / <code>fog_background_2.png</code> semi-transparent sliding overlay</td></tr>
          <tr><td>34</td><td>HUD</td><td><code>hud_render</code>: hearts, lives, score -- always drawn on top</td></tr>
          <tr><td>35</td><td>Debug</td><td><code>debug_render</code>: FPS counter, collision boxes, event log -- when <code>--debug</code> active</td></tr>
        </tbody>
      </table>
      <hr />
      <h2>Coordinate System</h2>
      <p>SDL&#39;s Y-axis increases <strong>downward</strong>. The origin (0, 0) is at the <strong>top-left</strong> of the logical canvas.</p>
      <pre><code>{`(0,0) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u25BA x  (GAME_W = 400)
  \u2502
  \u2502   LOGICAL CANVAS (400 \u00d7 300)
  \u2502
  \u25BC
  y
(GAME_H = 300)
              \u250C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510
              \u2502 \u2190\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500 GAME_W (400 px) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u25BA \u2502
  FLOOR_Y \u2500\u2500\u25BA\u2502\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2502
  (300-48=252)\u2502          Grass Tileset (48px tall)        \u2502
              \u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518`}</code></pre>
      <p><code>SDL_RenderSetLogicalSize(renderer, 400, 300)</code> makes SDL scale this canvas <strong>2x</strong> to fill the 800x600 OS window automatically, giving the chunky pixel-art look with no changes to game logic.</p>
      <hr />
      <h2>GameState Struct</h2>
      <p>Defined in <code>game.h</code>. The <strong>single container</strong> for every runtime resource.</p>
      <pre><code className="language-c">{`typedef struct {
    SDL_Window         *window;      // OS window handle
    SDL_Renderer       *renderer;    // GPU drawing context
    SDL_GameController *controller;  // first connected gamepad; NULL = none
    ParallaxSystem      parallax;    // multi-layer scrolling background

    SDL_Texture   *floor_tile;       // grass_tileset.png (GPU)
    SDL_Texture   *platform_tex;     // Shared tile for platform pillars (GPU)

    SDL_Texture   *spider_tex;       // Shared texture for all spiders (GPU)
    Spider         spiders[MAX_SPIDERS];
    int            spider_count;

    SDL_Texture   *jumping_spider_tex;  // Shared texture for jumping spiders (GPU)
    JumpingSpider  jumping_spiders[MAX_JUMPING_SPIDERS];
    int            jumping_spider_count;

    SDL_Texture   *bird_tex;         // Shared texture for Bird enemies (GPU)
    Bird           birds[MAX_BIRDS];
    int            bird_count;

    SDL_Texture   *faster_bird_tex;  // Shared texture for FasterBird (GPU)
    FasterBird     faster_birds[MAX_FASTER_BIRDS];
    int            faster_bird_count;

    SDL_Texture   *fish_tex;         // Shared texture for all fish enemies (GPU)
    Fish           fish[MAX_FISH];
    int            fish_count;

    SDL_Texture   *faster_fish_tex;  // Shared texture for faster fish enemies (GPU)
    FasterFish     faster_fish[MAX_FASTER_FISH];
    int            faster_fish_count;

    SDL_Texture   *coin_tex;         // Shared texture for all coin collectibles (GPU)
    Coin           coins[MAX_COINS];
    int            coin_count;

    SDL_Texture   *star_yellow_tex;  // Shared texture for star yellow pickups (GPU)
    StarYellow     star_yellows[MAX_STAR_YELLOWS];
    int            star_yellow_count;

    SDL_Texture   *star_green_tex;   // Shared texture for star green pickups (GPU)
    StarYellow     star_greens[MAX_STAR_YELLOWS];
    int            star_green_count;

    SDL_Texture   *star_red_tex;     // Shared texture for star red pickups (GPU)
    StarYellow     star_reds[MAX_STAR_YELLOWS];
    int            star_red_count;

    LastStar       last_star;        // Special end-of-level star collectible

    SDL_Texture   *vine_tex;         // Shared texture for vine decorations (GPU)
    VineDecor      vines[MAX_VINES];
    int            vine_count;

    SDL_Texture   *ladder_tex;       // Shared texture for ladders (GPU)
    LadderDecor    ladders[MAX_LADDERS];
    int            ladder_count;

    SDL_Texture   *rope_tex;         // Shared texture for ropes (GPU)
    RopeDecor      ropes[MAX_ROPES];
    int            rope_count;

    SDL_Texture   *bouncepad_small_tex;    // Shared texture for small bouncepads (GPU)
    Bouncepad      bouncepads_small[MAX_BOUNCEPADS_SMALL];
    int            bouncepad_small_count;

    SDL_Texture   *bouncepad_medium_tex;   // Shared texture for medium bouncepads (GPU)
    Bouncepad      bouncepads_medium[MAX_BOUNCEPADS_MEDIUM];
    int            bouncepad_medium_count;

    SDL_Texture   *bouncepad_high_tex;     // Shared texture for high bouncepads (GPU)
    Bouncepad      bouncepads_high[MAX_BOUNCEPADS_HIGH];
    int            bouncepad_high_count;

    SDL_Texture   *rail_tex;         // Shared texture for all rail tiles (GPU)
    Rail           rails[MAX_RAILS];
    int            rail_count;

    SDL_Texture   *spike_block_tex;  // Shared texture for spike blocks (GPU)
    SpikeBlock     spike_blocks[MAX_SPIKE_BLOCKS];
    int            spike_block_count;

    SDL_Texture   *spike_tex;        // Shared texture for ground spikes (GPU)
    SpikeRow       spike_rows[MAX_SPIKE_ROWS];
    int            spike_row_count;

    SDL_Texture   *spike_platform_tex;  // Shared texture for spike platforms (GPU)
    SpikePlatform  spike_platforms[MAX_SPIKE_PLATFORMS];
    int            spike_platform_count;

    SDL_Texture   *circular_saw_tex;    // Shared texture for circular saws (GPU)
    CircularSaw    circular_saws[MAX_CIRCULAR_SAWS];
    int            circular_saw_count;

    SDL_Texture   *axe_trap_tex;        // Shared texture for axe traps (GPU)
    AxeTrap        axe_traps[MAX_AXE_TRAPS];
    int            axe_trap_count;

    SDL_Texture   *blue_flame_tex;      // Shared texture for blue flames (GPU)
    BlueFlame      blue_flames[MAX_BLUE_FLAMES];
    int            blue_flame_count;

    SDL_Texture   *fire_flame_tex;      // Shared texture for fire flames (GPU)
    BlueFlame      fire_flames[MAX_BLUE_FLAMES];
    int            fire_flame_count;

    SDL_Texture   *float_platform_tex;  // float_platform.png 3-slice (GPU)
    FloatPlatform  float_platforms[MAX_FLOAT_PLATFORMS];
    int            float_platform_count;

    SDL_Texture   *bridge_tex;       // bridge.png tile (GPU)
    Bridge         bridges[MAX_BRIDGES];
    int            bridge_count;

    Mix_Chunk     *snd_jump;         // Player jump sound effect (WAV)
    Mix_Chunk     *snd_coin;         // Coin collect sound effect (WAV)
    Mix_Chunk     *snd_hit;          // Player hurt sound effect (WAV)
    Mix_Chunk     *snd_spring;       // Bouncepad spring sound effect (WAV)
    Mix_Chunk     *snd_axe;          // Axe trap swing sound effect (WAV)
    Mix_Chunk     *snd_flap;         // Bird flap sound effect (WAV)
    Mix_Chunk     *snd_spider_attack;// Spider attack sound effect (WAV)
    Mix_Chunk     *snd_dive;         // Fish dive sound effect (WAV)
    Mix_Music     *music;            // Background music stream (WAV)

    Player         player;           // Player data \u2014 stored by value
    Platform       platforms[MAX_PLATFORMS];
    int            platform_count;
    Water          water;            // Animated water strip at the bottom
    FogSystem      fog;              // Atmospheric fog overlay \u2014 topmost layer

    int            floor_gaps[MAX_FLOOR_GAPS];
    int            floor_gap_count;

    Hud            hud;              // HUD display: hearts, lives, score
    int            hearts;           // Current hit points (0-MAX_HEARTS)
    int            lives;            // Remaining lives; <0 triggers game over
    int            score;            // Cumulative score from collecting coins/stars
    int            score_life_next;  // Score threshold for next bonus life

    Camera         camera;           // Viewport scroll position; updated every frame
    int            running;          // Loop flag: 1 = keep going, 0 = quit
    int            paused;           // 1 = window lost focus; physics/music frozen
    int            debug_mode;       // 1 = debug overlays active (--debug flag)
    DebugOverlay   debug;            // FPS counter, collision vis, event log

    char           level_path[256];  // Path to loaded TOML level file
    const void    *current_level;    // Pointer to active LevelDef
    int            fog_enabled;      // 1 = fog rendering active
    int            water_enabled;    // 1 = water strip rendered
    int            world_w;          // Dynamic level width (set per level)
    int            score_per_life;   // Per-level score threshold for bonus life
    int            coin_score;       // Per-level points per coin

    // ---- Gamepad lazy init (deferred to avoid antivirus/HID delays) ----
    int            ctrl_pending_init;   // 0=idle, 1=first frame, 2=thread running
    SDL_Thread    *ctrl_init_thread;    // Background init thread
    volatile int   ctrl_init_done;     // Thread completion flag
    SDL_Texture   *ctrl_init_msg_tex;  // Cached HUD "Initializing gamepad..." texture

    // ---- Loop state (persists across frames for emscripten callback) ----
    Uint64         loop_prev_ticks;  // timestamp of previous frame
    int            fp_prev_riding;   // float platform player stood on last frame
} GameState;`}</code></pre>
      <p><strong>Key design decisions:</strong></p>
      <ul>
        <li><code>Player</code> is <strong>embedded by value</strong>, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to <code>Platform</code>, <code>Water</code>, <code>FogSystem</code>, and all entity arrays.</li>
        <li>Every pointer is set to <code>NULL</code> after freeing, making accidental double-frees safe.</li>
        <li>Initialised with <code>GameState gs = {'{'}0{'}'}</code> so every field starts as <code>0</code> / <code>NULL</code>.</li>
      </ul>
      <hr />
      <h2>Error Handling Strategy</h2>
      <table>
        <thead><tr><th>Situation</th><th>Action</th></tr></thead>
        <tbody>
          <tr><td>SDL subsystem init failure (in <code>main</code>)</td><td><code>fprintf(stderr, ...)</code> &#8594; clean up already-inited subsystems &#8594; <code>return EXIT_FAILURE</code></td></tr>
          <tr><td>Resource load failure (in <code>game_init</code>)</td><td><code>fprintf(stderr, ...)</code> &#8594; destroy already-created resources &#8594; <code>exit(EXIT_FAILURE)</code></td></tr>
          <tr><td>Sound load failure (non-fatal pattern)</td><td><code>fprintf(stderr, ...)</code> then continue -- play is guarded by <code>if (snd_jump)</code></td></tr>
          <tr><td>Optional texture load failure (non-fatal)</td><td><code>fprintf(stderr, ...)</code> then continue -- render is guarded by <code>if (texture)</code></td></tr>
        </tbody>
      </table>
      <p>All SDL error strings are retrieved with <code>SDL_GetError()</code>, <code>IMG_GetError()</code>, or <code>Mix_GetError()</code> and printed to <code>stderr</code>.</p>
    </section>
  );
}
