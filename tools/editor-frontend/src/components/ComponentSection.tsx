import { Plus, Lock, Grid3x3, Sprout, Image, Square } from 'lucide-react';
import { Switch } from '@/components/ui/Switch';

/**
 * Components section — FUTURE placeholder.
 *
 * The current Tool contract exposes NO component read/write (brief §0.4 / §七).
 * This region exists only to match the editor layout; every control is disabled
 * and clearly marked `future`. It must never pretend to edit soil/moisture.
 */
const FUTURE_COMPONENTS = [
  { icon: Grid3x3, name: 'TileGrid' },
  { icon: Sprout, name: 'CropField' },
  { icon: Image, name: 'Renderable' },
  { icon: Square, name: 'Collider' },
];

export function ComponentSection() {
  return (
    <div className="flex flex-col gap-2">
      <div className="flex items-center justify-between">
        <span className="text-xs font-medium text-sub">组件 (Components)</span>
        <button
          type="button"
          disabled
          title="future:当前 Tool 契约未暴露组件读写"
          className="inline-flex items-center gap-1 h-[24px] px-2 text-2xs rounded-ctl border border-line
            text-sub opacity-50 cursor-not-allowed"
        >
          <Plus size={12} />
          添加组件
        </button>
      </div>

      <div className="rounded-ctl border border-dashed border-line bg-p2/40 p-2 flex flex-col gap-1.5">
        {FUTURE_COMPONENTS.map(({ icon: Icon, name }) => (
          <div
            key={name}
            className="flex items-center gap-2 h-[28px] px-2 rounded-ctl bg-p1/60 opacity-55 select-none"
            title="future:契约未暴露"
          >
            <Icon size={14} className="text-sub" />
            <span className="text-xs text-sub mono">{name}</span>
            <Lock size={11} className="ml-auto text-sub" />
          </div>
        ))}
        <p className="text-2xs text-sub/70 px-0.5 pt-0.5">
          future:当前 Tool 契约未暴露组件读写,以上为布局占位,不可编辑。
        </p>
      </div>

      <div className="flex items-center justify-between pt-1">
        <Switch checked={false} future label="Renderable" />
        <Switch checked={false} future label="Collider" />
      </div>
    </div>
  );
}
