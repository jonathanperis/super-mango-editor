export default function ConstantsSection({ visible }: { visible: boolean }) {
  return (
    <section id="constants-reference" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1 className="page-title">Constants Reference</h1>
      <p><a href="home">&#8592; Home</a></p>
      <hr />
      <p>A complete reference for every compile-time constant in the codebase.</p>
      <hr />
      <h2><code>game.h</code> Constants</h2>
      <p>These are available to every file that <code>#include &quot;game.h&quot;</code>.</p>
      <h3>Window</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>WINDOW_TITLE</code></td><td><code>&quot;Super Mango&quot;</code></td><td>Text shown in the OS title bar</td></tr>
          <tr><td><code>WINDOW_W</code></td><td><code>800</code></td><td>OS window width in physical pixels</td></tr>
          <tr><td><code>WINDOW_H</code></td><td><code>600</code></td><td>OS window height in physical pixels</td></tr>
        </tbody>
      </table>
      <blockquote><p><strong>Do not use <code>WINDOW_W</code> / <code>WINDOW_H</code> for game object math.</strong> All game objects live in logical space.</p></blockquote>
      <h3>Logical Canvas</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>GAME_W</code></td><td><code>400</code></td><td>Internal canvas width in logical pixels</td></tr>
          <tr><td><code>GAME_H</code></td><td><code>300</code></td><td>Internal canvas height in logical pixels</td></tr>
        </tbody>
      </table>
      <p><code>SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H)</code> makes SDL scale every draw call from 400x300 up to 800x600 automatically, producing a <strong>2x pixel scale</strong> (each logical pixel = 2x2 physical pixels).</p>
      <h3>Timing</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
        <tbody><tr><td><code>TARGET_FPS</code></td><td><code>60</code></td><td>Desired frames per second</td></tr></tbody>
      </table>
      <p>Used to compute <code>frame_ms = 1000 / TARGET_FPS</code> (approximately 16 ms), which is the manual frame-cap duration when VSync is unavailable.</p>
      <h3>Tiles and Floor</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Expression</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>TILE_SIZE</code></td><td><code>48</code></td><td>literal</td><td>Width and height of one grass tile (px)</td></tr>
          <tr><td><code>FLOOR_Y</code></td><td><code>252</code></td><td><code>GAME_H - TILE_SIZE</code></td><td>Y coordinate of the floor&#39;s top edge</td></tr>
        </tbody>
      </table>
      <p>The floor is drawn by repeating the 48x48 grass tile across the full <code>WORLD_W</code> at <code>y=FLOOR_Y</code>, with gaps cut out at each <code>floor_gaps[]</code> position.</p>
      <h3>Physics</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>GRAVITY</code></td><td><code>800.0f</code></td><td><code>float</code></td><td>Downward acceleration in px/s^2</td></tr>
          <tr><td><code>FLOOR_GAP_W</code></td><td><code>32</code></td><td><code>int</code></td><td>Width of each floor gap in logical pixels</td></tr>
          <tr><td><code>MAX_FLOOR_GAPS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum number of floor gaps per level</td></tr>
        </tbody>
      </table>
      <p>Every frame while airborne: <code>player-&gt;vy += GRAVITY * dt</code>.</p>
      <p>At 60 FPS (<code>dt</code> approximately 0.016s) gravity adds ~12.8 px/s per frame. The jump impulse (<code>-325.0f</code> px/s) produces a moderate arc.</p>
      <h3>Camera</h3>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>WORLD_W</code></td><td><code>1600</code></td><td><code>int</code></td><td>Total logical level width (4 x GAME_W)</td></tr>
          <tr><td><code>CAM_LOOKAHEAD_VX_FACTOR</code></td><td><code>0.20f</code></td><td><code>float</code></td><td>Pixels of lookahead per px/s of player velocity (dynamic lookahead)</td></tr>
          <tr><td><code>CAM_LOOKAHEAD_MAX</code></td><td><code>50.0f</code></td><td><code>float</code></td><td>Maximum forward-look offset in px</td></tr>
          <tr><td><code>CAM_SMOOTHING</code></td><td><code>8.0f</code></td><td><code>float</code></td><td>Lerp speed factor (per second); higher = snappier follow</td></tr>
          <tr><td><code>CAM_SNAP_THRESHOLD</code></td><td><code>0.5f</code></td><td><code>float</code></td><td>Sub-pixel distance at which the camera snaps exactly to target</td></tr>
        </tbody>
      </table>
      <p><code>WORLD_W</code> defines the full scrollable level width. The visible canvas is always <code>GAME_W</code> (400 px); the <code>Camera</code> struct tracks the left edge of the viewport in world coordinates.</p>
      <hr />
      <h2><code>player.c</code> Local Constants</h2>
      <p>These are <code>#define</code>s local to <code>player.c</code> (not visible to other files).</p>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>FRAME_W</code></td><td><code>48</code></td><td>Width of one sprite frame in the sheet (px)</td></tr>
          <tr><td><code>FRAME_H</code></td><td><code>48</code></td><td>Height of one sprite frame in the sheet (px)</td></tr>
          <tr><td><code>FLOOR_SINK</code></td><td><code>16</code></td><td>Visual overlap onto the floor tile to prevent floating feet</td></tr>
          <tr><td><code>PHYS_PAD_X</code></td><td><code>15</code></td><td>Pixels trimmed from each horizontal side of the frame for the physics box (physics width = 48 - 30 = 18 px)</td></tr>
          <tr><td><code>PHYS_PAD_TOP</code></td><td><code>18</code></td><td>Pixels trimmed from the top of the frame for the physics box (physics height = 48 - 18 - 16 = 14; combined with FLOOR_SINK gives a 30 px tall box)</td></tr>
        </tbody>
      </table>
      <h3>Why <code>FLOOR_SINK</code>?</h3>
      <p>The <code>player.png</code> sprite sheet has transparent padding at the <strong>bottom</strong> of each 48x48 frame. Without the sink offset, the physics floor edge (<code>y + h = FLOOR_Y</code>) would leave the character visually floating 16 px above the grass. <code>FLOOR_SINK</code> compensates:</p>
      <pre><code>{`floor_snap = FLOOR_Y - player->h + FLOOR_SINK
           = 252      - 48        + 16
           = 220`}</code></pre>
      <p>The character&#39;s sprite appears to rest naturally on the grass at that Y.</p>
      <hr />
      <h2>Animation Tables in <code>player.c</code></h2>
      <p>Static arrays indexed by <code>AnimState</code> (0 = <code>ANIM_IDLE</code>, 1 = <code>ANIM_WALK</code>, 2 = <code>ANIM_JUMP</code>, 3 = <code>ANIM_FALL</code>, 4 = <code>ANIM_CLIMB</code>):</p>
      <pre><code className="language-c">{`static const int ANIM_FRAME_COUNT[5] = { 4,   4,   2,   1,   2   };
static const int ANIM_FRAME_MS[5]    = { 150, 100, 150, 200, 100 };
static const int ANIM_ROW[5]         = { 0,   1,   2,   3,   4   };`}</code></pre>
      <table>
        <thead><tr><th>Index</th><th>State</th><th>Frames</th><th>ms/frame</th><th>Sheet row</th></tr></thead>
        <tbody>
          <tr><td>0</td><td><code>ANIM_IDLE</code></td><td>4</td><td>150</td><td>Row 0</td></tr>
          <tr><td>1</td><td><code>ANIM_WALK</code></td><td>4</td><td>100</td><td>Row 1</td></tr>
          <tr><td>2</td><td><code>ANIM_JUMP</code></td><td>2</td><td>150</td><td>Row 2</td></tr>
          <tr><td>3</td><td><code>ANIM_FALL</code></td><td>1</td><td>200</td><td>Row 3</td></tr>
          <tr><td>4</td><td><code>ANIM_CLIMB</code></td><td>2</td><td>100</td><td>Row 4</td></tr>
        </tbody>
      </table>
      <hr />
      <h2>Movement Constants in <code>player.c</code></h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>WALK_MAX_SPEED</code></td><td><code>100.0f</code></td><td>Maximum walking speed (px/s)</td></tr>
          <tr><td><code>RUN_MAX_SPEED</code></td><td><code>250.0f</code></td><td>Maximum running speed (px/s, Shift held)</td></tr>
          <tr><td><code>WALK_GROUND_ACCEL</code></td><td><code>750.0f</code></td><td>Ground acceleration while walking (px/s^2)</td></tr>
          <tr><td><code>RUN_GROUND_ACCEL</code></td><td><code>600.0f</code></td><td>Ground acceleration while running (px/s^2)</td></tr>
          <tr><td><code>GROUND_FRICTION</code></td><td><code>550.0f</code></td><td>Ground deceleration when no input (px/s^2)</td></tr>
          <tr><td><code>GROUND_COUNTER_ACCEL</code></td><td><code>100.0f</code></td><td>Extra deceleration when reversing direction (px/s^2)</td></tr>
          <tr><td><code>AIR_ACCEL_WALK</code></td><td><code>350.0f</code></td><td>Airborne acceleration while walking (px/s^2)</td></tr>
          <tr><td><code>AIR_ACCEL_RUN</code></td><td><code>180.0f</code></td><td>Airborne acceleration while running (px/s^2)</td></tr>
          <tr><td><code>AIR_FRICTION</code></td><td><code>80.0f</code></td><td>Airborne deceleration when no input (px/s^2)</td></tr>
          <tr><td><code>WALK_ANIM_MIN_VX</code></td><td><code>8.0f</code></td><td>Minimum horizontal speed to trigger walk animation (px/s)</td></tr>
        </tbody>
      </table>
      <p>The player uses an acceleration-based movement model. Hold Shift to run. Physics overrides for all these values can be configured per level in the TOML <code>[physics]</code> section.</p>
      <h2>Vine Climbing Constants in <code>player.c</code></h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>CLIMB_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Vertical climbing speed on vines (px/s)</td></tr>
          <tr><td><code>CLIMB_H_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Horizontal drift speed while on vine (px/s)</td></tr>
          <tr><td><code>VINE_GRAB_PAD</code></td><td><code>4</code></td><td><code>int</code></td><td>Extra pixels on each side of vine sprite that count as the grab zone (total grab width = VINE_W + 2 x 4 = 24 px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2>Audio Constants in <code>main.c</code></h2>
      <table>
        <thead><tr><th>Value</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>44100</code></td><td>Audio sample rate (Hz)</td></tr>
          <tr><td><code>MIX_DEFAULT_FORMAT</code></td><td>16-bit signed samples</td></tr>
          <tr><td><code>2</code></td><td>Stereo channels</td></tr>
          <tr><td><code>2048</code></td><td>Mixer buffer size (samples)</td></tr>
          <tr><td>per level</td><td>Music volume (0-128) configured via <code>music_volume</code> in level TOML (default 13, ~10%)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2>Derived Values Quick Reference</h2>
      <table>
        <thead><tr><th>Expression</th><th>Result</th><th>Meaning</th></tr></thead>
        <tbody>
          <tr><td><code>GAME_W / WINDOW_W</code></td><td><code>2x</code></td><td>Pixel scale factor</td></tr>
          <tr><td><code>GAME_H / WINDOW_H</code></td><td><code>2x</code></td><td>Pixel scale factor</td></tr>
          <tr><td><code>1000 / TARGET_FPS</code></td><td><code>~16 ms</code></td><td>Frame budget</td></tr>
          <tr><td><code>GAME_H - TILE_SIZE</code></td><td><code>252</code></td><td><code>FLOOR_Y</code></td></tr>
          <tr><td><code>FLOOR_Y - FRAME_H + FLOOR_SINK</code></td><td><code>220</code></td><td>Player start / floor snap Y</td></tr>
          <tr><td><code>GAME_W / TILE_SIZE</code></td><td><code>~8.3</code></td><td>Tiles needed to fill the floor</td></tr>
          <tr><td><code>WATER_FRAMES x WATER_ART_W</code></td><td><code>128</code></td><td><code>WATER_PERIOD</code> -- seamless repeat distance</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>platform.h</code> Constants</h2>
      <table><thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead><tbody><tr><td><code>MAX_PLATFORMS</code></td><td><code>32</code></td><td>Maximum number of platforms in the game</td></tr></tbody></table>
      <hr />
      <h2><code>water.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>WATER_FRAMES</code></td><td><code>8</code></td><td><code>int</code></td><td>Total animation frames in <code>water.png</code></td></tr>
          <tr><td><code>WATER_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Full slot width per frame in the sheet (px)</td></tr>
          <tr><td><code>WATER_ART_DX</code></td><td><code>16</code></td><td><code>int</code></td><td>Left offset to visible art within each slot</td></tr>
          <tr><td><code>WATER_ART_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of actual art pixels per frame</td></tr>
          <tr><td><code>WATER_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
          <tr><td><code>WATER_ART_H</code></td><td><code>31</code></td><td><code>int</code></td><td>Height of visible art pixels</td></tr>
          <tr><td><code>WATER_PERIOD</code></td><td><code>128</code></td><td><code>int</code></td><td>Pattern repeat distance: <code>WATER_FRAMES x WATER_ART_W</code></td></tr>
          <tr><td><code>WATER_SCROLL_SPEED</code></td><td><code>40.0f</code></td><td><code>float</code></td><td>Rightward scroll speed (px/s)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>spider.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_SPIDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous spider enemies</td></tr>
          <tr><td><code>SPIDER_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>spider.png</code> (192/64 = 3)</td></tr>
          <tr><td><code>SPIDER_FRAME_W</code></td><td><code>64</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>SPIDER_ART_X</code></td><td><code>20</code></td><td><code>int</code></td><td>First visible col within each frame slot</td></tr>
          <tr><td><code>SPIDER_ART_W</code></td><td><code>25</code></td><td><code>int</code></td><td>Width of visible art (cols 20-44)</td></tr>
          <tr><td><code>SPIDER_ART_Y</code></td><td><code>22</code></td><td><code>int</code></td><td>First visible row within each frame slot</td></tr>
          <tr><td><code>SPIDER_ART_H</code></td><td><code>10</code></td><td><code>int</code></td><td>Height of visible art (rows 22-31)</td></tr>
          <tr><td><code>SPIDER_SPEED</code></td><td><code>50.0f</code></td><td><code>float</code></td><td>Walk speed (px/s)</td></tr>
          <tr><td><code>SPIDER_FRAME_MS</code></td><td><code>150</code></td><td><code>int</code></td><td>Milliseconds each animation frame is held</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>fog.h</code> Constants</h2>
      <table><thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead><tbody><tr><td><code>FOG_TEX_COUNT</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of fog texture assets in rotation</td></tr><tr><td><code>FOG_MAX</code></td><td><code>4</code></td><td><code>int</code></td><td>Maximum concurrent fog instances</td></tr></tbody></table>
      <hr />
      <h2><code>parallax.h</code> Constants</h2>
      <table><thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead><tbody><tr><td><code>MAX_BACKGROUND_LAYERS</code></td><td><code>8</code></td><td><code>int</code></td><td>Maximum number of background layers the system can hold</td></tr></tbody></table>
      <hr />
      <h2><code>coin.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_COINS</code></td><td><code>64</code></td><td><code>int</code></td><td>Maximum simultaneous coins on screen</td></tr>
          <tr><td><code>COIN_DISPLAY_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Render width in logical pixels</td></tr>
          <tr><td><code>COIN_DISPLAY_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Render height in logical pixels</td></tr>
          <tr><td><code>COIN_SCORE</code></td><td><code>100</code></td><td><code>int</code></td><td>Score awarded per coin collected</td></tr>
          <tr><td><code>SCORE_PER_LIFE</code></td><td><code>1000</code></td><td><code>int</code></td><td>Score multiple that grants a bonus life</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>vine.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_VINES</code></td><td><code>24</code></td><td><code>int</code></td><td>Maximum number of vine instances</td></tr>
          <tr><td><code>VINE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Sprite width in logical pixels</td></tr>
          <tr><td><code>VINE_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Content height after removing transparent padding</td></tr>
          <tr><td><code>VINE_SRC_Y</code></td><td><code>8</code></td><td><code>int</code></td><td>First pixel row with content in vine.png</td></tr>
          <tr><td><code>VINE_SRC_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Height of content area in vine.png</td></tr>
          <tr><td><code>VINE_STEP</code></td><td><code>19</code></td><td><code>int</code></td><td>Vertical spacing between stacked tiles (px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>fish.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_FISH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous fish instances</td></tr>
          <tr><td><code>FISH_FRAMES</code></td><td><code>2</code></td><td><code>int</code></td><td>Horizontal frames in <code>fish.png</code> (96x48 sheet)</td></tr>
          <tr><td><code>FISH_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>FISH_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Height of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>FISH_RENDER_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen render width in logical pixels</td></tr>
          <tr><td><code>FISH_RENDER_H</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen render height in logical pixels</td></tr>
          <tr><td><code>FISH_SPEED</code></td><td><code>70.0f</code></td><td><code>float</code></td><td>Horizontal patrol speed (px/s)</td></tr>
          <tr><td><code>FISH_JUMP_VY</code></td><td><code>-280.0f</code></td><td><code>float</code></td><td>Upward jump impulse (px/s)</td></tr>
          <tr><td><code>FISH_JUMP_MIN</code></td><td><code>1.4f</code></td><td><code>float</code></td><td>Minimum seconds before next jump</td></tr>
          <tr><td><code>FISH_JUMP_MAX</code></td><td><code>3.0f</code></td><td><code>float</code></td><td>Maximum seconds before next jump</td></tr>
          <tr><td><code>FISH_HITBOX_PAD_X</code></td><td><code>16</code></td><td><code>int</code></td><td>Horizontal inset for fair AABB collision (hitbox width = 16 px)</td></tr>
          <tr><td><code>FISH_HITBOX_PAD_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>Vertical inset for fair AABB collision (hitbox height = 19 px)</td></tr>
          <tr><td><code>FISH_FRAME_MS</code></td><td><code>120</code></td><td><code>int</code></td><td>Milliseconds per swim animation frame</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>hud.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_HEARTS</code></td><td><code>3</code></td><td><code>int</code></td><td>Maximum hearts the player can have</td></tr>
          <tr><td><code>DEFAULT_LIVES</code></td><td><code>3</code></td><td><code>int</code></td><td>Lives the player starts with</td></tr>
          <tr><td><code>HUD_MARGIN</code></td><td><code>4</code></td><td><code>int</code></td><td>Pixel margin from screen edges</td></tr>
          <tr><td><code>HUD_HEART_SIZE</code></td><td><code>16</code></td><td><code>int</code></td><td>Display size of each heart icon (px)</td></tr>
          <tr><td><code>HUD_HEART_GAP</code></td><td><code>2</code></td><td><code>int</code></td><td>Horizontal gap between heart icons (px)</td></tr>
          <tr><td><code>HUD_ICON_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Display width of the player icon (px)</td></tr>
          <tr><td><code>HUD_ICON_H</code></td><td><code>13</code></td><td><code>int</code></td><td>Display height of the player icon (px)</td></tr>
          <tr><td><code>HUD_ROW_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Row height for text alignment (font px)</td></tr>
          <tr><td><code>HUD_COIN_ICON_SIZE</code></td><td><code>12</code></td><td><code>int</code></td><td>Display size of the coin count icon (px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>bouncepad.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_BOUNCEPADS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous bouncepad instances (per variant)</td></tr>
          <tr><td><code>BOUNCEPAD_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Display width of one bouncepad frame (px)</td></tr>
          <tr><td><code>BOUNCEPAD_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Display height of one bouncepad frame (px)</td></tr>
          <tr><td><code>BOUNCEPAD_VY_SMALL</code></td><td><code>-380.0f</code></td><td><code>float</code></td><td>Small bouncepad launch impulse (px/s)</td></tr>
          <tr><td><code>BOUNCEPAD_VY_MEDIUM</code></td><td><code>-536.25f</code></td><td><code>float</code></td><td>Medium bouncepad launch impulse (px/s)</td></tr>
          <tr><td><code>BOUNCEPAD_VY_HIGH</code></td><td><code>-700.0f</code></td><td><code>float</code></td><td>High bouncepad launch impulse (px/s)</td></tr>
          <tr><td><code>BOUNCEPAD_VY</code></td><td><code>-536.25f</code></td><td><code>float</code></td><td>Default launch impulse (alias for <code>BOUNCEPAD_VY_MEDIUM</code>)</td></tr>
          <tr><td><code>BOUNCEPAD_FRAME_MS</code></td><td><code>80</code></td><td><code>int</code></td><td>Milliseconds per animation frame during release</td></tr>
          <tr><td><code>BOUNCEPAD_SRC_Y</code></td><td><code>14</code></td><td><code>int</code></td><td>First non-transparent row in the frame</td></tr>
          <tr><td><code>BOUNCEPAD_SRC_H</code></td><td><code>18</code></td><td><code>int</code></td><td>Height of the art region (rows 14-31)</td></tr>
          <tr><td><code>BOUNCEPAD_ART_X</code></td><td><code>16</code></td><td><code>int</code></td><td>First non-transparent col in the frame</td></tr>
          <tr><td><code>BOUNCEPAD_ART_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of the art region (cols 16-31)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>rail.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>RAIL_N</code></td><td><code>1 &lt;&lt; 0</code></td><td>bitmask</td><td>Tile opens upward</td></tr>
          <tr><td><code>RAIL_E</code></td><td><code>1 &lt;&lt; 1</code></td><td>bitmask</td><td>Tile opens rightward</td></tr>
          <tr><td><code>RAIL_S</code></td><td><code>1 &lt;&lt; 2</code></td><td>bitmask</td><td>Tile opens downward</td></tr>
          <tr><td><code>RAIL_W</code></td><td><code>1 &lt;&lt; 3</code></td><td>bitmask</td><td>Tile opens leftward</td></tr>
          <tr><td><code>RAIL_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one tile in the sprite sheet (px)</td></tr>
          <tr><td><code>RAIL_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of one tile in the sprite sheet (px)</td></tr>
          <tr><td><code>MAX_RAIL_TILES</code></td><td><code>128</code></td><td><code>int</code></td><td>Maximum tiles in a single Rail path</td></tr>
          <tr><td><code>MAX_RAILS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum Rail instances per level</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>spike_block.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>SPIKE_DISPLAY_W</code></td><td><code>24</code></td><td><code>int</code></td><td>On-screen width in logical pixels (16x16 scaled up)</td></tr>
          <tr><td><code>SPIKE_DISPLAY_H</code></td><td><code>24</code></td><td><code>int</code></td><td>On-screen height in logical pixels</td></tr>
          <tr><td><code>SPIKE_SPIN_DEG_PER_SEC</code></td><td><code>360.0f</code></td><td><code>float</code></td><td>Rotation speed -- one full turn per second</td></tr>
          <tr><td><code>SPIKE_SPEED_SLOW</code></td><td><code>1.5f</code></td><td><code>float</code></td><td>Rail traversal: 1.5 tiles/s</td></tr>
          <tr><td><code>SPIKE_SPEED_NORMAL</code></td><td><code>3.0f</code></td><td><code>float</code></td><td>Rail traversal: 3.0 tiles/s</td></tr>
          <tr><td><code>SPIKE_SPEED_FAST</code></td><td><code>6.0f</code></td><td><code>float</code></td><td>Rail traversal: 6.0 tiles/s</td></tr>
          <tr><td><code>SPIKE_PUSH_SPEED</code></td><td><code>220.0f</code></td><td><code>float</code></td><td>Horizontal push impulse magnitude (px/s)</td></tr>
          <tr><td><code>SPIKE_PUSH_VY</code></td><td><code>-150.0f</code></td><td><code>float</code></td><td>Upward push component on collision (px/s)</td></tr>
          <tr><td><code>MAX_SPIKE_BLOCKS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike block instances per level</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>debug.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>DEBUG_LOG_MAX_ENTRIES</code></td><td><code>8</code></td><td><code>int</code></td><td>Maximum visible log messages</td></tr>
          <tr><td><code>DEBUG_LOG_MSG_LEN</code></td><td><code>64</code></td><td><code>int</code></td><td>Max characters per log message (incl. null)</td></tr>
          <tr><td><code>DEBUG_LOG_DISPLAY_SEC</code></td><td><code>4.0f</code></td><td><code>float</code></td><td>Seconds each log entry stays visible</td></tr>
          <tr><td><code>DEBUG_FPS_SAMPLE_MS</code></td><td><code>500</code></td><td><code>int</code></td><td>Milliseconds between FPS counter refreshes</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>jumping_spider.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_JUMPING_SPIDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous jumping spider instances</td></tr>
          <tr><td><code>JSPIDER_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>jumping_spider.png</code> (192/64 = 3)</td></tr>
          <tr><td><code>JSPIDER_FRAME_W</code></td><td><code>64</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>JSPIDER_ART_X</code></td><td><code>20</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
          <tr><td><code>JSPIDER_ART_W</code></td><td><code>25</code></td><td><code>int</code></td><td>Width of visible art (cols 20-44)</td></tr>
          <tr><td><code>JSPIDER_ART_Y</code></td><td><code>22</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
          <tr><td><code>JSPIDER_ART_H</code></td><td><code>10</code></td><td><code>int</code></td><td>Height of visible art (rows 22-31)</td></tr>
          <tr><td><code>JSPIDER_SPEED</code></td><td><code>55.0f</code></td><td><code>float</code></td><td>Walk speed (px/s)</td></tr>
          <tr><td><code>JSPIDER_FRAME_MS</code></td><td><code>150</code></td><td><code>int</code></td><td>Milliseconds per animation frame</td></tr>
          <tr><td><code>JSPIDER_JUMP_VY</code></td><td><code>-200.0f</code></td><td><code>float</code></td><td>Upward jump impulse (px/s)</td></tr>
          <tr><td><code>JSPIDER_GRAVITY</code></td><td><code>600.0f</code></td><td><code>float</code></td><td>Downward acceleration while airborne (px/s^2)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>bird.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_BIRDS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous bird instances</td></tr>
          <tr><td><code>BIRD_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>bird.png</code> (144/48 = 3)</td></tr>
          <tr><td><code>BIRD_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>BIRD_ART_X</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
          <tr><td><code>BIRD_ART_W</code></td><td><code>15</code></td><td><code>int</code></td><td>Width of visible art (cols 17-31)</td></tr>
          <tr><td><code>BIRD_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
          <tr><td><code>BIRD_ART_H</code></td><td><code>14</code></td><td><code>int</code></td><td>Height of visible art (rows 17-30)</td></tr>
          <tr><td><code>BIRD_SPEED</code></td><td><code>45.0f</code></td><td><code>float</code></td><td>Horizontal flight speed (px/s)</td></tr>
          <tr><td><code>BIRD_FRAME_MS</code></td><td><code>140</code></td><td><code>int</code></td><td>Milliseconds per wing animation frame</td></tr>
          <tr><td><code>BIRD_WAVE_AMP</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Sine-wave amplitude in logical pixels</td></tr>
          <tr><td><code>BIRD_WAVE_FREQ</code></td><td><code>0.015f</code></td><td><code>float</code></td><td>Sine cycles per pixel of horizontal travel</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>faster_bird.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_FASTER_BIRDS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous faster bird instances</td></tr>
          <tr><td><code>FBIRD_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>faster_bird.png</code> (144/48 = 3)</td></tr>
          <tr><td><code>FBIRD_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
          <tr><td><code>FBIRD_ART_X</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
          <tr><td><code>FBIRD_ART_W</code></td><td><code>15</code></td><td><code>int</code></td><td>Width of visible art (cols 17-31)</td></tr>
          <tr><td><code>FBIRD_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
          <tr><td><code>FBIRD_ART_H</code></td><td><code>14</code></td><td><code>int</code></td><td>Height of visible art (rows 17-30)</td></tr>
          <tr><td><code>FBIRD_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Horizontal speed -- nearly 2x the slow bird</td></tr>
          <tr><td><code>FBIRD_FRAME_MS</code></td><td><code>90</code></td><td><code>int</code></td><td>Faster wing animation (ms per frame)</td></tr>
          <tr><td><code>FBIRD_WAVE_AMP</code></td><td><code>15.0f</code></td><td><code>float</code></td><td>Tighter sine-wave amplitude (px)</td></tr>
          <tr><td><code>FBIRD_WAVE_FREQ</code></td><td><code>0.025f</code></td><td><code>float</code></td><td>Higher frequency -- more erratic curves</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>float_platform.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>FLOAT_PLATFORM_PIECE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of each 3-slice piece (px)</td></tr>
          <tr><td><code>FLOAT_PLATFORM_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of the platform sprite (px)</td></tr>
          <tr><td><code>MAX_FLOAT_PLATFORMS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum float platform instances per level</td></tr>
          <tr><td><code>CRUMBLE_STAND_LIMIT</code></td><td><code>0.75f</code></td><td><code>float</code></td><td>Seconds of standing before crumble-fall starts</td></tr>
          <tr><td><code>CRUMBLE_FALL_GRAVITY</code></td><td><code>250.0f</code></td><td><code>float</code></td><td>Downward acceleration during crumble fall (px/s^2)</td></tr>
          <tr><td><code>CRUMBLE_FALL_INITIAL_VY</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Initial downward velocity on crumble-start (px/s)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>bridge.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_BRIDGES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum bridge instances per level</td></tr>
          <tr><td><code>MAX_BRIDGE_BRICKS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum bricks in a single bridge</td></tr>
          <tr><td><code>BRIDGE_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one bridge.png tile (px)</td></tr>
          <tr><td><code>BRIDGE_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of one bridge.png tile (px)</td></tr>
          <tr><td><code>BRIDGE_FALL_DELAY</code></td><td><code>0.1f</code></td><td><code>float</code></td><td>Seconds between touch and first brick falling</td></tr>
          <tr><td><code>BRIDGE_CASCADE_DELAY</code></td><td><code>0.06f</code></td><td><code>float</code></td><td>Extra seconds between successive bricks cascading outward</td></tr>
          <tr><td><code>BRIDGE_FALL_GRAVITY</code></td><td><code>250.0f</code></td><td><code>float</code></td><td>Downward acceleration per brick during fall (px/s^2)</td></tr>
          <tr><td><code>BRIDGE_FALL_INITIAL_VY</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Initial downward velocity on fall-start (px/s)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>star_yellow.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_STAR_YELLOWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum star instances per color per level</td></tr>
          <tr><td><code>STAR_YELLOW_DISPLAY_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Display width (logical px)</td></tr>
          <tr><td><code>STAR_YELLOW_DISPLAY_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Display height (logical px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>last_star.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>LAST_STAR_DISPLAY_W</code></td><td><code>24</code></td><td><code>int</code></td><td>Display width (logical px)</td></tr>
          <tr><td><code>LAST_STAR_DISPLAY_H</code></td><td><code>24</code></td><td><code>int</code></td><td>Display height (logical px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>axe_trap.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>AXE_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Source sprite width (px)</td></tr>
          <tr><td><code>AXE_FRAME_H</code></td><td><code>64</code></td><td><code>int</code></td><td>Source sprite height (px)</td></tr>
          <tr><td><code>AXE_DISPLAY_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
          <tr><td><code>AXE_DISPLAY_H</code></td><td><code>64</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
          <tr><td><code>AXE_SWING_AMPLITUDE</code></td><td><code>60.0f</code></td><td><code>float</code></td><td>Maximum pendulum angle from vertical (degrees)</td></tr>
          <tr><td><code>AXE_SWING_PERIOD</code></td><td><code>2.0f</code></td><td><code>float</code></td><td>Time for one full pendulum cycle (s)</td></tr>
          <tr><td><code>AXE_SPIN_SPEED</code></td><td><code>180.0f</code></td><td><code>float</code></td><td>Rotation speed for spin variant (degrees/s)</td></tr>
          <tr><td><code>MAX_AXE_TRAPS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum axe trap instances per level</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>circular_saw.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>SAW_FRAME_W</code></td><td><code>32</code></td><td><code>int</code></td><td>Source sprite width (px)</td></tr>
          <tr><td><code>SAW_FRAME_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Source sprite height (px)</td></tr>
          <tr><td><code>SAW_DISPLAY_W</code></td><td><code>32</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
          <tr><td><code>SAW_DISPLAY_H</code></td><td><code>32</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
          <tr><td><code>SAW_SPIN_DEG_PER_SEC</code></td><td><code>720.0f</code></td><td><code>float</code></td><td>Rotation speed (degrees/s)</td></tr>
          <tr><td><code>SAW_PATROL_SPEED</code></td><td><code>180.0f</code></td><td><code>float</code></td><td>Horizontal patrol speed (px/s)</td></tr>
          <tr><td><code>SAW_PUSH_SPEED</code></td><td><code>220.0f</code></td><td><code>float</code></td><td>Push impulse magnitude (px/s)</td></tr>
          <tr><td><code>SAW_PUSH_VY</code></td><td><code>-150.0f</code></td><td><code>float</code></td><td>Upward bounce component on collision (px/s)</td></tr>
          <tr><td><code>MAX_CIRCULAR_SAWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum circular saw instances per level</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>blue_flame.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>BLUE_FLAME_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Animation frame width (px)</td></tr>
          <tr><td><code>BLUE_FLAME_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Animation frame height (px)</td></tr>
          <tr><td><code>BLUE_FLAME_DISPLAY_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
          <tr><td><code>BLUE_FLAME_DISPLAY_H</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
          <tr><td><code>BLUE_FLAME_FRAME_COUNT</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of animation frames</td></tr>
          <tr><td><code>BLUE_FLAME_ANIM_SPEED</code></td><td><code>0.1f</code></td><td><code>float</code></td><td>Seconds between frame advances</td></tr>
          <tr><td><code>BLUE_FLAME_LAUNCH_VY</code></td><td><code>-550.0f</code></td><td><code>float</code></td><td>Initial upward impulse (px/s)</td></tr>
          <tr><td><code>BLUE_FLAME_RISE_DECEL</code></td><td><code>800.0f</code></td><td><code>float</code></td><td>Deceleration during rise (px/s^2)</td></tr>
          <tr><td><code>BLUE_FLAME_APEX_Y</code></td><td><code>60.0f</code></td><td><code>float</code></td><td>World-space y coordinate at apex (px)</td></tr>
          <tr><td><code>BLUE_FLAME_FLIP_DURATION</code></td><td><code>0.12f</code></td><td><code>float</code></td><td>Time to rotate 180 degrees at apex (s)</td></tr>
          <tr><td><code>BLUE_FLAME_WAIT_DURATION</code></td><td><code>1.5f</code></td><td><code>float</code></td><td>Time hidden below floor before next eruption (s)</td></tr>
          <tr><td><code>MAX_BLUE_FLAMES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum blue/fire flame instances per level</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>faster_fish.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_FASTER_FISH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum faster fish instances per level</td></tr>
          <tr><td><code>FFISH_FRAMES</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of animation frames</td></tr>
          <tr><td><code>FFISH_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Frame width (px)</td></tr>
          <tr><td><code>FFISH_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Frame height (px)</td></tr>
          <tr><td><code>FFISH_RENDER_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Render width (logical px)</td></tr>
          <tr><td><code>FFISH_RENDER_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Render height (logical px)</td></tr>
          <tr><td><code>FFISH_SPEED</code></td><td><code>120.0f</code></td><td><code>float</code></td><td>Patrol speed (px/s)</td></tr>
          <tr><td><code>FFISH_JUMP_VY</code></td><td><code>-420.0f</code></td><td><code>float</code></td><td>Jump impulse (px/s)</td></tr>
          <tr><td><code>FFISH_JUMP_MIN</code></td><td><code>1.0f</code></td><td><code>float</code></td><td>Minimum delay between jumps (s)</td></tr>
          <tr><td><code>FFISH_JUMP_MAX</code></td><td><code>2.2f</code></td><td><code>float</code></td><td>Maximum delay between jumps (s)</td></tr>
          <tr><td><code>FFISH_HITBOX_PAD_X</code></td><td><code>16</code></td><td><code>int</code></td><td>Horizontal hitbox inset (px)</td></tr>
          <tr><td><code>FFISH_HITBOX_PAD_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>Vertical hitbox inset (px)</td></tr>
          <tr><td><code>FFISH_FRAME_MS</code></td><td><code>100</code></td><td><code>int</code></td><td>Frame animation duration (ms)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>spike.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_SPIKE_ROWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike row instances per level</td></tr>
          <tr><td><code>MAX_SPIKE_TILES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum tiles in a single spike row</td></tr>
          <tr><td><code>SPIKE_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Spike tile width (px)</td></tr>
          <tr><td><code>SPIKE_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Spike tile height (px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>spike_platform.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_SPIKE_PLATFORMS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike platform instances per level</td></tr>
          <tr><td><code>SPIKE_PLAT_PIECE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one 3-slice piece (px)</td></tr>
          <tr><td><code>SPIKE_PLAT_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Full frame height (px)</td></tr>
          <tr><td><code>SPIKE_PLAT_SRC_Y</code></td><td><code>5</code></td><td><code>int</code></td><td>First content row in each piece (px)</td></tr>
          <tr><td><code>SPIKE_PLAT_SRC_H</code></td><td><code>11</code></td><td><code>int</code></td><td>Content height (rows 5-15, px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>ladder.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_LADDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum ladder instances per level</td></tr>
          <tr><td><code>LADDER_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Sprite width (px)</td></tr>
          <tr><td><code>LADDER_H</code></td><td><code>22</code></td><td><code>int</code></td><td>Content height after cropping padding (px)</td></tr>
          <tr><td><code>LADDER_SRC_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>First pixel row with content</td></tr>
          <tr><td><code>LADDER_SRC_H</code></td><td><code>22</code></td><td><code>int</code></td><td>Height of content area (px)</td></tr>
          <tr><td><code>LADDER_STEP</code></td><td><code>8</code></td><td><code>int</code></td><td>Vertical overlap when tiling (px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>rope.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_ROPES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum rope instances per level</td></tr>
          <tr><td><code>ROPE_W</code></td><td><code>12</code></td><td><code>int</code></td><td>Display width with padding (px)</td></tr>
          <tr><td><code>ROPE_H</code></td><td><code>36</code></td><td><code>int</code></td><td>Display height with padding (px)</td></tr>
          <tr><td><code>ROPE_SRC_X</code></td><td><code>2</code></td><td><code>int</code></td><td>Source crop x offset (px)</td></tr>
          <tr><td><code>ROPE_SRC_Y</code></td><td><code>6</code></td><td><code>int</code></td><td>Source crop y offset (px)</td></tr>
          <tr><td><code>ROPE_SRC_W</code></td><td><code>12</code></td><td><code>int</code></td><td>Source crop width (px)</td></tr>
          <tr><td><code>ROPE_SRC_H</code></td><td><code>36</code></td><td><code>int</code></td><td>Source crop height (px)</td></tr>
          <tr><td><code>ROPE_STEP</code></td><td><code>34</code></td><td><code>int</code></td><td>Vertical spacing between stacked tiles (px)</td></tr>
        </tbody>
      </table>
      <hr />
      <h2><code>bouncepad_small.h</code> / <code>bouncepad_medium.h</code> / <code>bouncepad_high.h</code> Constants</h2>
      <table>
        <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
        <tbody>
          <tr><td><code>MAX_BOUNCEPADS_SMALL</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum small bouncepad instances</td></tr>
          <tr><td><code>MAX_BOUNCEPADS_MEDIUM</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum medium bouncepad instances</td></tr>
          <tr><td><code>MAX_BOUNCEPADS_HIGH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum high bouncepad instances</td></tr>
        </tbody>
      </table>
    </section>
  );
}
