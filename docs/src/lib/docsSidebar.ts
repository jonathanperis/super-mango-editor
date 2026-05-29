export type DocsSectionId =
  | 'home'
  | 'developer-guide'
  | 'controls'
  | 'testing'
  | 'architecture'
  | 'source-files'
  | 'player-module'
  | 'constants-reference'
  | 'entities-and-hazards'
  | 'collectibles-and-surfaces'
  | 'level-design'
  | 'level-editor'
  | 'assets'
  | 'sounds'
  | 'level-catalog'
  | 'overlay-snapshots'
  | 'build-system'
  | 'release-checklist';

export type DocsPageMeta = {
  id: DocsSectionId;
  label: string;
  shortLabel?: string;
  description: string;
  route: string;
  marker: string;
};

export type DocsCategory = {
  label: string;
  kicker: string;
  description: string;
  ids: DocsSectionId[];
};

export const DOCS_META: Record<DocsSectionId, DocsPageMeta> = {
  home: {
    id: 'home',
    label: 'Overview',
    description: 'Project map, quick start, product facts, and first routes through the manual.',
    route: '/docs/',
    marker: 'START',
  },
  'developer-guide': {
    id: 'developer-guide',
    label: 'Developer Guide',
    description: 'Coding conventions, safe extension patterns, entity workflow, and contribution rules.',
    route: '/docs/developer-guide/',
    marker: 'GUIDE',
  },
  controls: {
    id: 'controls',
    label: 'Controls & Input',
    shortLabel: 'Controls',
    description: 'Keyboard, gamepad, browser, replay, smoke, and runtime flag reference.',
    route: '/docs/controls/',
    marker: 'INPUT',
  },
  testing: {
    id: 'testing',
    label: 'Testing & Smoke Matrix',
    shortLabel: 'Testing',
    description: 'Which native, smoke, docs, WebAssembly, and release checks to run for each change.',
    route: '/docs/testing/',
    marker: 'TEST',
  },
  architecture: {
    id: 'architecture',
    label: 'Architecture',
    description: 'Init, loop, cleanup, GameState ownership, render order, and runtime flow.',
    route: '/docs/architecture/',
    marker: 'ENGINE',
  },
  'source-files': {
    id: 'source-files',
    label: 'Source Files',
    description: 'Module-by-module reference for every core C and header file in the codebase.',
    route: '/docs/source-files/',
    marker: 'FILES',
  },
  'player-module': {
    id: 'player-module',
    label: 'Player Module',
    description: 'Input, physics, animation, collisions, and player lifecycle in player.c.',
    route: '/docs/player-module/',
    marker: 'PLAYER',
  },
  'constants-reference': {
    id: 'constants-reference',
    label: 'Constants Reference',
    shortLabel: 'Constants',
    description: 'Named limits, dimensions, scores, timings, and gameplay constants.',
    route: '/docs/constants-reference/',
    marker: 'CONST',
  },
  'entities-and-hazards': {
    id: 'entities-and-hazards',
    label: 'Entities & Hazards',
    description: 'Enemy and hazard behaviours, TOML placement, and gameplay effects.',
    route: '/docs/entities-and-hazards/',
    marker: 'DANGER',
  },
  'collectibles-and-surfaces': {
    id: 'collectibles-and-surfaces',
    label: 'Collectibles & Surfaces',
    shortLabel: 'Collectibles',
    description: 'Coins, stars, bouncepads, rails, float platforms, ropes, ladders, and vines.',
    route: '/docs/collectibles-and-surfaces/',
    marker: 'ITEMS',
  },
  'level-design': {
    id: 'level-design',
    label: 'Level Design',
    description: 'TOML schema, minimum level template, and authoring rules for worlds.',
    route: '/docs/level-design/',
    marker: 'TOML',
  },
  'level-editor': {
    id: 'level-editor',
    label: 'Level Editor',
    description: 'Visual editor canvas, palette, properties, undo, validation, and play-test flow.',
    route: '/docs/level-editor/',
    marker: 'EDITOR',
  },
  assets: {
    id: 'assets',
    label: 'Assets',
    description: 'Sprite sheets, tilesets, fonts, folder rules, and visual resource notes.',
    route: '/docs/assets/',
    marker: 'ART',
  },
  sounds: {
    id: 'sounds',
    label: 'Sounds',
    description: 'Audio files, categories, naming rules, and game sound reference.',
    route: '/docs/sounds/',
    marker: 'AUDIO',
  },
  'level-catalog': {
    id: 'level-catalog',
    label: 'Level Catalog',
    description: 'Generated inventory of each TOML level, screens, counts, and progression links.',
    route: '/docs/level-catalog/',
    marker: 'LEVELS',
  },
  'overlay-snapshots': {
    id: 'overlay-snapshots',
    label: 'Overlay Snapshots',
    shortLabel: 'Overlays',
    description: 'Text snapshots for pause, game-over, completion, and terminal overlay states.',
    route: '/docs/overlay-snapshots/',
    marker: 'UI',
  },
  'build-system': {
    id: 'build-system',
    label: 'Build System',
    description: 'Make targets, compiler flags, platform prerequisites, and WebAssembly build flow.',
    route: '/docs/build-system/',
    marker: 'BUILD',
  },
  'release-checklist': {
    id: 'release-checklist',
    label: 'Release Checklist',
    shortLabel: 'Release',
    description: 'Pre-release source, docs, WebAssembly, archive, CI, and Pages verification gates.',
    route: '/docs/release-checklist/',
    marker: 'SHIP',
  },
};

export const SECTION_CATEGORIES: DocsCategory[] = [
  {
    label: 'Start Here',
    kicker: 'MANUAL_01',
    description: 'New to the cabinet? Start with the map, developer conventions, controls, tests, and architecture route.',
    ids: ['home', 'developer-guide', 'controls', 'testing', 'architecture'],
  },
  {
    label: 'Engine & Code',
    kicker: 'MANUAL_02',
    description: 'Follow the C11 and SDL2 runtime, player module, constants, and source ownership.',
    ids: ['source-files', 'player-module', 'constants-reference'],
  },
  {
    label: 'World Builder',
    kicker: 'MANUAL_03',
    description: 'Build TOML levels, inspect content systems, and use the standalone level editor.',
    ids: ['level-design', 'level-editor', 'entities-and-hazards', 'collectibles-and-surfaces'],
  },
  {
    label: 'Assets & Builds',
    kicker: 'MANUAL_04',
    description: 'Audit resources, generated catalogs, overlays, sounds, and platform build targets.',
    ids: ['assets', 'sounds', 'level-catalog', 'overlay-snapshots', 'build-system', 'release-checklist'],
  },
];

export const SECTION_ORDER = SECTION_CATEGORIES.flatMap(({ ids }) => ids);
