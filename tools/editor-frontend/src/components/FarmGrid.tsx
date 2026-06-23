import { useState, type ReactNode } from 'react';
import { Panel } from '@/components/ui/Panel';
import { Select } from '@/components/ui/Select';
import { Switch } from '@/components/ui/Switch';
import { FarmTile } from '@/components/FarmTile';
import { useEngine, FIELD_COLS, FIELD_ROWS } from '@/state/engine';
import { STAGE_COLORS } from '@/data/locale';

const tileKey = (x: number, y: number) => `${x},${y}`;

/** Legend swatch. */
function Swatch({ color, label, ring }: { color: string; label: string; ring?: boolean }) {
  return (
    <span className="inline-flex items-center gap-1.5 text-2xs text-sub whitespace-nowrap">
      <span
        className="h-3 w-3 rounded-sm border border-black/40"
        style={{ background: color, boxShadow: ring ? '0 0 0 1.5px #c9892f inset' : undefined }}
      />
      {label}
    </span>
  );
}

/** Panel C — the 3×3 farm grid driven by crop.get_field. */
export function FarmGrid() {
  const { fieldByKey, selectedTile, selectTile } = useEngine();
  const [showGrid, setShowGrid] = useState(true);
  const [colorBy, setColorBy] = useState<'stage' | 'crop'>('stage');

  return (
    <Panel
      badge="C"
      title="作物 / 农田"
      subtitle="农田瓦片视图"
      tool="crop.get_field"
      className="flex-1"
      bodyClassName="flex flex-col p-3 gap-3 min-h-0"
    >
      {/* toolbar */}
      <div className="flex flex-wrap items-center gap-x-4 gap-y-2">
        <div className="flex items-center gap-2">
          <span className="text-2xs text-sub">作物视图</span>
          <Select
            value={colorBy}
            onValue={(v) => setColorBy(v as 'stage' | 'crop')}
            options={[
              { value: 'stage', label: '按阶段着色' },
              { value: 'crop', label: '按作物着色' },
            ]}
            className="w-[120px]"
          />
        </div>
        <Switch checked={showGrid} onChange={setShowGrid} label="显示网格" />
        <div className="flex flex-wrap items-center gap-x-3 gap-y-1 ml-auto">
          <Swatch color="#3a2c20" label="未种植" />
          {STAGE_COLORS.map((c, i) => (
            <Swatch key={i} color={c} label={`阶段 ${i}`} />
          ))}
          <Swatch color="#69b84f" label="可收获" ring />
        </div>
      </div>

      {/* grid with row/col headers */}
      <div className="overflow-auto min-h-0">
        <div
          className="grid w-full min-w-[360px] gap-px bg-line rounded-ctl overflow-hidden"
          style={{ gridTemplateColumns: `22px repeat(${FIELD_COLS}, minmax(0,1fr))` }}
        >
          {/* header row: corner + column numbers */}
          <div className="bg-p1" />
          {Array.from({ length: FIELD_COLS }, (_, x) => (
            <div key={`c${x}`} className="bg-p1 text-center text-2xs text-sub mono py-1">
              {x}
            </div>
          ))}

          {/* tile rows */}
          {Array.from({ length: FIELD_ROWS }, (_, y) => (
            <Row key={`r${y}`}>
              <div className="bg-p1 grid place-items-center text-2xs text-sub mono">{y}</div>
              {Array.from({ length: FIELD_COLS }, (_, x) => (
                <FarmTile
                  key={tileKey(x, y)}
                  x={x}
                  y={y}
                  tile={fieldByKey[tileKey(x, y)]}
                  selected={selectedTile.x === x && selectedTile.y === y}
                  showGrid={showGrid}
                  colorBy={colorBy}
                  onSelect={(sx, sy) => selectTile({ x: sx, y: sy })}
                />
              ))}
            </Row>
          ))}
        </div>
      </div>
    </Panel>
  );
}

/** Fragment wrapper so each grid row's cells stay contiguous in the grid flow. */
function Row({ children }: { children: ReactNode }) {
  return <>{children}</>;
}
