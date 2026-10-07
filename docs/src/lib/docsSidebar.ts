export type DocsSectionId =
  | 'home'
  | 'learning-path'
  | 'debugging-c'
  | 'c-concepts'
  | 'mechanics-museum'
  | 'entity-walkthrough'
  | 'asset-inventory'
  | 'asset-provenance'
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
};

export type DocsCategory = {
  label: string;
  description: string;
  ids: DocsSectionId[];
};

export const DOCS_META: Record<DocsSectionId, DocsPageMeta> = {
  'learning-path': { id: 'learning-path', label: 'Sandbox School', description: 'Eight guided experiments from first frame to reproducible replay.', route: '/docs/learning-path/' },
  'debugging-c': { id: 'debugging-c', label: 'Debugging C', description: 'Find crashes and memory bugs with sanitizers, lldb or gdb, and a quick profiling pass.', route: '/docs/debugging-c/' },
  'c-concepts': { id: 'c-concepts', label: 'C in This Codebase', shortLabel: 'C Concepts', description: 'Each C idea the game relies on, mapped to the file and function where you can read it.', route: '/docs/c-concepts/' },
  'mechanics-museum': { id: 'mechanics-museum', label: 'Mechanics Museum', description: 'Six focused levels and the simulation inspector.', route: '/docs/mechanics-museum/' },
  'entity-walkthrough': { id: 'entity-walkthrough', label: 'Entity Walkthrough', description: 'Trace a collectible through parser, runtime, editor, undo and tests.', route: '/docs/entity-walkthrough/' },
  'asset-inventory': { id: 'asset-inventory', label: 'Asset Inventory', description: 'Generated playable asset sizes and bundle budget.', route: '/docs/asset-inventory/' },
  'asset-provenance': { id: 'asset-provenance', label: 'Asset Provenance', description: 'Sources, third-party notices and media license records.', route: '/docs/asset-provenance/' },
  home: {
    id: 'home',
    label: 'Overview',
    description: 'Project map, quick start, product facts, and first routes through the manual.',
    route: '/docs/',
  },
  'developer-guide': {
    id: 'developer-guide',
    label: 'Developer Guide',
    description: 'Coding conventions, safe extension patterns, entity workflow, and contribution rules.',
    route: '/docs/developer-guide/',
  },
  controls: {
    id: 'controls',
    label: 'Controls & Input',
    shortLabel: 'Controls',
    description: 'Keyboard, gamepad, browser, replay, smoke, and runtime flag reference.',
    route: '/docs/controls/',
  },
  testing: {
    id: 'testing',
    label: 'Testing & Smoke Matrix',
    shortLabel: 'Testing',
    description: 'Which native, smoke, docs, WebAssembly, and release checks to run for each change.',
    route: '/docs/testing/',
  },
  architecture: {
    id: 'architecture',
    label: 'Architecture',
    description: 'Init, loop, cleanup, GameState ownership, render order, and runtime flow.',
    route: '/docs/architecture/',
  },
  'source-files': {
    id: 'source-files',
    label: 'Source Files',
    description: 'Module-by-module reference for every core C and header file in the codebase.',
    route: '/docs/source-files/',
  },
  'player-module': {
    id: 'player-module',
    label: 'Player Module',
    description: 'Sampled input, physics, animation, collisions, and lifecycle across src/player/.',
    route: '/docs/player-module/',
  },
  'constants-reference': {
    id: 'constants-reference',
    label: 'Constants Reference',
    shortLabel: 'Constants',
    description: 'Named limits, dimensions, scores, timings, and gameplay constants.',
    route: '/docs/constants-reference/',
  },
  'entities-and-hazards': {
    id: 'entities-and-hazards',
    label: 'Entities & Hazards',
    description: 'Enemy and hazard behaviours, TOML placement, and gameplay effects.',
    route: '/docs/entities-and-hazards/',
  },
  'collectibles-and-surfaces': {
    id: 'collectibles-and-surfaces',
    label: 'Collectibles & Surfaces',
    shortLabel: 'Collectibles',
    description: 'Coins, stars, bouncepads, rails, float platforms, ropes, ladders, and vines.',
    route: '/docs/collectibles-and-surfaces/',
  },
  'level-design': {
    id: 'level-design',
    label: 'Level Design',
    description: 'TOML schema, minimum level template, and authoring rules for worlds.',
    route: '/docs/level-design/',
  },
  'level-editor': {
    id: 'level-editor',
    label: 'Level Editor',
    description: 'Visual editor canvas, palette, properties, undo, validation, and play-test flow.',
    route: '/docs/level-editor/',
  },
  assets: {
    id: 'assets',
    label: 'Assets',
    description: 'Sprite sheets, tilesets, fonts, folder rules, and visual resource notes.',
    route: '/docs/assets/',
  },
  sounds: {
    id: 'sounds',
    label: 'Sounds',
    description: 'Audio files, categories, naming rules, and game sound reference.',
    route: '/docs/sounds/',
  },
  'level-catalog': {
    id: 'level-catalog',
    label: 'Level Catalog',
    description: 'Generated campaign inventory: screens, content counts, and progression links.',
    route: '/docs/level-catalog/',
  },
  'overlay-snapshots': {
    id: 'overlay-snapshots',
    label: 'Overlay Snapshots',
    shortLabel: 'Overlays',
    description: 'Text snapshots for pause, game-over, completion, and terminal overlay states.',
    route: '/docs/overlay-snapshots/',
  },
  'build-system': {
    id: 'build-system',
    label: 'Build System',
    description: 'Make targets, compiler flags, platform prerequisites, and WebAssembly build flow.',
    route: '/docs/build-system/',
  },
  'release-checklist': {
    id: 'release-checklist',
    label: 'Release Checklist',
    shortLabel: 'Release',
    description: 'Pre-release source, docs, WebAssembly, archive, CI, and Pages verification gates.',
    route: '/docs/release-checklist/',
  },
};

export const SECTION_CATEGORIES: DocsCategory[] = [
  {
    label: 'Start Here',
    description: 'Where to begin: the lessons, the controls, how the code is organised and how to test a change.',
    ids: ['home', 'learning-path', 'debugging-c', 'mechanics-museum', 'developer-guide', 'controls', 'testing', 'architecture'],
  },
  {
    label: 'Engine & Code',
    description: 'How the game works inside: every source file, the player, the numbers that tune it.',
    ids: ['c-concepts', 'source-files', 'entity-walkthrough', 'player-module', 'constants-reference'],
  },
  {
    label: 'World Builder',
    description: 'Make your own levels: the TOML format, the editor, and what every enemy and surface does.',
    ids: ['level-design', 'level-editor', 'entities-and-hazards', 'collectibles-and-surfaces'],
  },
  {
    label: 'Assets & Builds',
    description: 'Art, sound, the generated catalogs, and how builds and releases are made.',
    ids: ['assets', 'asset-inventory', 'asset-provenance', 'sounds', 'level-catalog', 'overlay-snapshots', 'build-system', 'release-checklist'],
  },
];

export const SECTION_ORDER = SECTION_CATEGORIES.flatMap(({ ids }) => ids);
