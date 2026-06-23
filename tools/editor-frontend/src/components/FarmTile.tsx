import { Droplets, Sparkles } from 'lucide-react';
import type { CropTile } from '@/types';
import { CropSprite } from '@/components/CropSprite';
import { cropName, stageColor } from '@/data/locale';

interface Props {
  x: number;
  y: number;
  tile: CropTile | undefined;
  selected: boolean;
  showGrid: boolean;
  colorBy: 'stage' | 'crop';
  onSelect: (x: number, y: number) => void;
}

const CROP_HUE: Record<string, string> = {
  parsnip: '#c9892f',
  cauliflower: '#7c4cc4',
};

/** One farm cell. Empty tiles render as bare soil ("未种植"). */
export function FarmTile({ x, y, tile, selected, showGrid, colorBy, onSelect }: Props) {
  const empty = !tile;
  const accent = tile
    ? colorBy === 'crop'
      ? CROP_HUE[tile.cropId] ?? stageColor(tile.stage)
      : stageColor(tile.stage)
    : 'transparent';

  return (
    <button
      type="button"
      onClick={() => onSelect(x, y)}
      aria-label={`瓦片 ${x},${y}${tile ? ` ${tile.cropId} ${tile.stageName}` : ' 未种植'}`}
      aria-pressed={selected}
      className={`group relative overflow-hidden text-left soil
        ${showGrid ? 'ring-1 ring-black/40' : ''}
        ${selected ? 'outline outline-2 outline-accent z-10' : ''}
        ${tile?.mature && !selected ? 'outline outline-2 outline-amber/80' : ''}
        focus-visible:outline focus-visible:outline-2 focus-visible:outline-accent`}
      style={{ minHeight: 105 }}
    >
      {/* stage/crop tint wash */}
      {!empty && (
        <span
          className="absolute inset-0 opacity-25 group-hover:opacity-35 transition-opacity"
          style={{ background: `linear-gradient(160deg, ${accent}55, transparent 70%)` }}
        />
      )}

      {/* plant */}
      <span className="absolute inset-0 flex items-end justify-center pb-3">
        {empty ? (
          <span className="text-2xs text-sub/60 mb-7 tracking-wide">未种植</span>
        ) : (
          <CropSprite cropId={tile.cropId} stage={tile.stage} />
        )}
      </span>

      {/* mature sparkle */}
      {tile?.mature && (
        <span className="absolute top-1.5 left-1.5 text-amber" title="可收获">
          <Sparkles size={14} />
        </span>
      )}

      {/* watered droplet */}
      {tile?.watered && (
        <span
          className="absolute bottom-1.5 right-1.5 text-accent drop-shadow"
          title="已浇水"
        >
          <Droplets size={15} />
        </span>
      )}

      {/* label footer */}
      {!empty && (
        <span className="absolute inset-x-0 bottom-0 px-2 py-1 bg-gradient-to-t from-black/80 to-transparent">
          <span className="block text-[12px] font-semibold text-txt leading-tight">
            {cropName(tile.cropId)}
            <span className="text-2xs text-sub mono ml-1">{tile.cropId}</span>
          </span>
          <span className="block text-2xs text-sub mono leading-tight">
            {tile.stageName} · s{tile.stage}
          </span>
        </span>
      )}
    </button>
  );
}
