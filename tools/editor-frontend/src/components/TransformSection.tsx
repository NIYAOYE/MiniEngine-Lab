import { useEffect, useState } from 'react';
import { Check, RotateCcw } from 'lucide-react';
import { NumberField } from '@/components/ui/Input';
import { ToolButton } from '@/components/ToolButton';
import { useEngine } from '@/state/engine';
import type { EntityDetail } from '@/types';

interface Draft {
  px: number;
  py: number;
  rot: number;
  sx: number;
  sy: number;
}

function draftFrom(d: EntityDetail): Draft {
  return { px: d.position.x, py: d.position.y, rot: d.rotation, sx: d.scale.x, sy: d.scale.y };
}

/** Editable Transform — the only truly mutable Inspector section (entity.set_transform). */
export function TransformSection({ detail }: { detail: EntityDetail }) {
  const { run } = useEngine();
  const [draft, setDraft] = useState<Draft>(() => draftFrom(detail));

  // Resync when the selected entity (or its server-side transform) changes.
  useEffect(() => {
    setDraft(draftFrom(detail));
  }, [detail]);

  const dirty =
    draft.px !== detail.position.x ||
    draft.py !== detail.position.y ||
    draft.rot !== detail.rotation ||
    draft.sx !== detail.scale.x ||
    draft.sy !== detail.scale.y;

  const set = (patch: Partial<Draft>) => setDraft((d) => ({ ...d, ...patch }));

  const apply = () =>
    void run('entity.set_transform', {
      id: detail.id,
      position: { x: draft.px, y: draft.py },
      rotation: draft.rot,
      scale: { x: draft.sx, y: draft.sy },
    });

  return (
    <div className="flex flex-col gap-2.5">
      <div>
        <div className="text-2xs text-sub uppercase tracking-wide mb-1">Position</div>
        <div className="grid grid-cols-2 gap-2">
          <NumberField label="X" value={draft.px} onValue={(n) => set({ px: n })} step={0.5} />
          <NumberField label="Y" value={draft.py} onValue={(n) => set({ py: n })} step={0.5} />
        </div>
      </div>
      <div>
        <div className="text-2xs text-sub uppercase tracking-wide mb-1">Rotation (°)</div>
        <NumberField label="rot" value={draft.rot} onValue={(n) => set({ rot: n })} step={5} />
      </div>
      <div>
        <div className="text-2xs text-sub uppercase tracking-wide mb-1">Scale</div>
        <div className="grid grid-cols-2 gap-2">
          <NumberField label="X" value={draft.sx} onValue={(n) => set({ sx: n })} step={0.1} />
          <NumberField label="Y" value={draft.sy} onValue={(n) => set({ sy: n })} step={0.1} />
        </div>
      </div>
      <div className="flex gap-2 pt-0.5">
        <ToolButton
          tool="entity.set_transform"
          tone="green"
          size="sm"
          icon={<Check size={14} />}
          disabledReason={!dirty ? '无改动' : undefined}
          onClick={apply}
        >
          应用变换
        </ToolButton>
        <button
          type="button"
          onClick={() => setDraft(draftFrom(detail))}
          disabled={!dirty}
          className="inline-flex items-center gap-1.5 h-[26px] px-2 text-xs text-sub hover:text-txt rounded-ctl
            disabled:opacity-40 disabled:cursor-not-allowed"
          title="还原为当前引擎值"
        >
          <RotateCcw size={13} />
          还原
        </button>
      </div>
    </div>
  );
}
