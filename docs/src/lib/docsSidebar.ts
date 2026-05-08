export const SECTION_CATEGORIES = [
  { label: "", ids: ["home"] },
  { label: "Engine & Code", ids: ["architecture", "source-files", "player-module", "constants-reference"] },
  { label: "Content & Assets", ids: ["entities-and-hazards", "collectibles-and-surfaces", "assets", "sounds"] },
  { label: "Building & Contributing", ids: ["build-system", "level-design", "level-editor", "developer-guide"] },
] as const;

export const SECTION_ORDER = SECTION_CATEGORIES.flatMap(({ ids }) => ids);
