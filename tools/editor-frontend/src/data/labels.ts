/**
 * FRONT-END LOCAL entity labels (future).
 *
 * The engine contract has NO entity names (brief §0.4) — entities are only
 * id + transform + hierarchy. These friendly names live entirely in the editor
 * and are shown alongside the authoritative `ent_NNNN` id. When `entity.set_name`
 * eventually lands in the contract, this map gets replaced by real engine data.
 */
export const LOCAL_LABELS: Record<number, string> = {
  1: 'World',
  2: 'Player',
  3: 'Barn',
  4: 'Barn_Door',
  5: 'Silo',
  6: 'FarmField',
};

/** Format an entity id as its authoritative engine label, e.g. ent_0012. */
export function entityTag(id: number): string {
  return `ent_${String(id).padStart(4, '0')}`;
}
