/**
 * UI localization + visual mappings.
 *
 * IMPORTANT: The engine speaks English (seasonName "Spring", cropId "parsnip",
 * stageName "sprout"). These maps are FRONT-END display only — never sent back
 * to the engine and never assumed by the contract.
 */

/** Engine seasonName (English) → Chinese label for the UI. */
export const SEASON_ZH: Record<string, string> = {
  Spring: '春',
  Summer: '夏',
  Fall: '秋',
  Winter: '冬',
};

/** Engine cropId → Chinese display name. */
export const CROP_ZH: Record<string, string> = {
  parsnip: '防风草',
  cauliflower: '花椰菜',
};

export function cropName(cropId: string): string {
  return CROP_ZH[cropId] ?? cropId;
}

/**
 * Stage colour ramp (by stage index). Earthy → green → ripe so the field reads
 * like real growth, not arbitrary categories. Indices beyond the array clamp.
 */
export const STAGE_COLORS = [
  '#6b5640', // seed — soil brown
  '#7faf5a', // sprout — pale green
  '#5fa244', // growing / leafy
  '#46924f', // heading — deeper green
  '#69b84f', // mature — ripe green (also highlighted)
] as const;

export function stageColor(stage: number): string {
  const i = Math.max(0, Math.min(stage, STAGE_COLORS.length - 1));
  return STAGE_COLORS[i];
}

/** Mature/harvestable highlight colour. */
export const MATURE_COLOR = '#c9892f';
