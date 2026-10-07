/*
 * The website reuses the game's own pixel art straight from assets/sprites,
 * so the site and the game can never drift apart. `?url` makes Vite copy each
 * PNG unchanged into the build (no resampling: pixels stay crisp when the CSS
 * scales them with image-rendering: pixelated).
 *
 * Sheet sizes are the PNG dimensions in pixels; frame sizes match the game
 * code that animates them (src/player/player_animation.c, src/entities/*.h).
 */
import logo from '../../../assets/sprites/screens/start_menu_logo.png?url';
import player from '../../../assets/sprites/player/player.png?url';
import coin from '../../../assets/sprites/collectibles/coin.png?url';
import starYellow from '../../../assets/sprites/collectibles/star_yellow.png?url';
import lastStar from '../../../assets/sprites/collectibles/last_star.png?url';
import spider from '../../../assets/sprites/entities/spider.png?url';
import bird from '../../../assets/sprites/entities/bird.png?url';
import fish from '../../../assets/sprites/entities/fish.png?url';
import saw from '../../../assets/sprites/hazards/circular_saw.png?url';
import axe from '../../../assets/sprites/hazards/axe_trap.png?url';
import flame from '../../../assets/sprites/hazards/blue_flame.png?url';
import grassTile from '../../../assets/sprites/levels/grass_tileset.png?url';
import sky from '../../../assets/sprites/backgrounds/sky_blue.png?url';
import clouds from '../../../assets/sprites/backgrounds/clouds_bg.png?url';
import cloudsNear from '../../../assets/sprites/backgrounds/clouds_mg_1.png?url';
import mountains from '../../../assets/sprites/backgrounds/glacial_mountains.png?url';
import water from '../../../assets/sprites/foregrounds/water.png?url';

export type SpriteSheet = {
    src: string;
    sheetW: number;
    sheetH: number;
    frameW: number;
    frameH: number;
};

const sheet = (src: string, sheetW: number, sheetH: number, frameW = sheetW, frameH = sheetH): SpriteSheet =>
    ({ src, sheetW, sheetH, frameW, frameH });

export const art = {
    logo: sheet(logo, 64, 48),
    player: sheet(player, 192, 288, 48, 48),   // 4 x 6 frames; row 0 idle, row 1 walk
    coin: sheet(coin, 16, 16),
    starYellow: sheet(starYellow, 16, 16),
    lastStar: sheet(lastStar, 16, 16),
    spider: sheet(spider, 192, 48, 64, 48),    // 3 frames
    bird: sheet(bird, 144, 48, 48, 48),        // 3 frames
    fish: sheet(fish, 96, 48, 48, 48),         // 2 frames
    saw: sheet(saw, 32, 32),
    axe: sheet(axe, 48, 64),
    flame: sheet(flame, 96, 48, 48, 48),       // 2 frames
    grassTile: sheet(grassTile, 48, 48),
    sky: sheet(sky, 384, 216),
    clouds: sheet(clouds, 384, 216),
    cloudsNear: sheet(cloudsNear, 384, 216),
    mountains: sheet(mountains, 384, 216),
    water: sheet(water, 384, 64),
};
