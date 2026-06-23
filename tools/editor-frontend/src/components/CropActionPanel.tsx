import { useState } from 'react';
import { Leaf, Droplets, CalendarRange, Wheat } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { Select } from '@/components/ui/Select';
import { Input } from '@/components/ui/Input';
import { ToolButton } from '@/components/ToolButton';
import { useEngine } from '@/state/engine';
import { CROP_DB } from '@/lib/toolClient';
import { cropName } from '@/data/locale';

const tileKey = (x: number, y: number) => `${x},${y}`;
const DEFAULT_DAYS = 1;

/** Small key/value row for the selected-tile readout. */
function Field({ label, value, tone }: { label: string; value: string; tone?: string }) {
  return (
    <div className="flex items-center justify-between text-xs py-0.5">
      <span className="text-sub">{label}</span>
      <span className={`mono ${tone ?? 'text-txt'}`}>{value}</span>
    </div>
  );
}

/** Crop operations for the selected tile (plant / water / advance days / harvest). */
export function CropActionPanel() {
  const { fieldByKey, selectedTile, run } = useEngine();
  const [plantCrop, setPlantCrop] = useState('parsnip');
  const [days, setDays] = useState(DEFAULT_DAYS);

  const { x, y } = selectedTile;
  const tile = fieldByKey[tileKey(x, y)];
  const empty = !tile;

  return (
    <Panel
      title="作物操作"
      subtitle={`选中瓦片 (${x}, ${y})`}
      bodyClassName="flex flex-col gap-3 p-2.5"
    >
      {/* selected tile readout */}
      <div className="rounded-ctl bg-p2 border border-line p-2.5">
        {empty ? (
          <p className="text-xs text-sub">该瓦片未种植。选择作物后可种下。</p>
        ) : (
          <>
            <Field label="作物 cropId" value={`${cropName(tile.cropId)} · ${tile.cropId}`} />
            <Field label="阶段 stageName" value={`${tile.stageName} (s${tile.stage})`} tone="text-accent" />
            <Field label="本阶段天数" value={String(tile.daysInStage)} />
            <Field label="已浇水" value={tile.watered ? '是' : '否'} tone={tile.watered ? 'text-accent' : 'text-sub'} />
            <Field label="是否成熟" value={tile.mature ? '是(可收获)' : '否'} tone={tile.mature ? 'text-amber' : 'text-sub'} />
          </>
        )}
      </div>

      {/* plant row */}
      <div className="flex items-center gap-2">
        <Select
          value={plantCrop}
          onValue={setPlantCrop}
          aria-label="选择作物"
          options={Object.keys(CROP_DB).map((id) => ({ value: id, label: `${cropName(id)} (${id})` }))}
          className="flex-1"
        />
        <ToolButton
          tool="crop.plant"
          tone="green"
          icon={<Leaf size={14} />}
          disabledReason={!empty ? '瓦片已占用' : undefined}
          onClick={() => void run('crop.plant', { tileX: x, tileY: y, cropId: plantCrop })}
        >
          种植
        </ToolButton>
      </div>

      {/* water */}
      <ToolButton
        tool="crop.water"
        tone="blue"
        block
        icon={<Droplets size={15} />}
        disabledReason={empty ? '空瓦片无法浇水' : undefined}
        onClick={() => void run('crop.water', { tileX: x, tileY: y })}
      >
        浇水 (crop.water)
      </ToolButton>

      {/* advance days — whole field */}
      <div className="rounded-ctl bg-grape/10 border border-grape/30 p-2.5">
        <div className="flex items-center gap-1.5 mb-2 text-[#b794e8]">
          <CalendarRange size={14} />
          <span className="text-xs font-medium">推进天数</span>
          <code className="text-2xs mono opacity-80 ml-auto">crop.advance_days</code>
        </div>
        <div className="flex items-center gap-2">
          <Input
            type="number"
            min={1}
            value={days}
            onChange={(e) => setDays(Math.max(1, parseInt(e.target.value || '1', 10) || 1))}
            aria-label="推进天数"
            className="w-[64px]"
          />
          <span className="text-xs text-sub">天</span>
          <ToolButton
            tool="crop.advance_days"
            tone="purple"
            className="ml-auto"
            onClick={() => void run('crop.advance_days', { days })}
          >
            推进生长
          </ToolButton>
        </div>
        <p className="mt-1.5 text-2xs text-[#b794e8]/80">作用于整片农田,非单格。仅当天已浇水的作物前进一格。</p>
      </div>

      {/* harvest */}
      <ToolButton
        tool="crop.harvest"
        tone="orange"
        block
        icon={<Wheat size={15} />}
        disabledReason={!tile?.mature ? '仅成熟作物可收获' : undefined}
        onClick={() => void run('crop.harvest', { tileX: x, tileY: y })}
      >
        收获 (crop.harvest)
      </ToolButton>
    </Panel>
  );
}
