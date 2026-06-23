import { useState, type ReactNode } from 'react';
import { ChevronRight, Copy, Check } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { TransformSection } from '@/components/TransformSection';
import { ComponentSection } from '@/components/ComponentSection';
import { useEngine } from '@/state/engine';
import { entityTag } from '@/data/labels';

/** Collapsible sub-section used inside the Inspector. */
function Collapsible({
  title,
  defaultOpen = true,
  children,
}: {
  title: ReactNode;
  defaultOpen?: boolean;
  children: ReactNode;
}) {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <div className="border-t border-line">
      <button
        type="button"
        onClick={() => setOpen((o) => !o)}
        aria-expanded={open}
        className="flex items-center gap-1.5 w-full h-[32px] px-3 text-xs font-semibold text-txt hover:bg-white/[0.03]"
      >
        <ChevronRight size={13} className={`text-sub transition-transform ${open ? 'rotate-90' : ''}`} />
        {title}
      </button>
      {open && <div className="px-3 pb-3">{children}</div>}
    </div>
  );
}

/** Read-only labelled value with optional copy button. */
function ReadField({ label, value, onCopy }: { label: string; value: string; onCopy?: () => void }) {
  return (
    <div className="flex items-center justify-between gap-2 text-xs py-1">
      <span className="text-sub shrink-0">{label}</span>
      <span className="flex items-center gap-1.5 min-w-0">
        <span className="mono text-txt truncate">{value}</span>
        {onCopy && (
          <CopyButton onCopy={onCopy} />
        )}
      </span>
    </div>
  );
}

function CopyButton({ onCopy }: { onCopy: () => void }) {
  const [done, setDone] = useState(false);
  return (
    <button
      type="button"
      aria-label="复制 ID"
      onClick={() => {
        onCopy();
        setDone(true);
        window.setTimeout(() => setDone(false), 1200);
      }}
      className="text-sub hover:text-txt shrink-0"
    >
      {done ? <Check size={13} className="text-ok" /> : <Copy size={13} />}
    </button>
  );
}

/** Right column — entity Inspector (scene.get_entity + entity.set_transform). */
export function InspectorPanel() {
  const { selectedDetail, selectedEntityId } = useEngine();

  const copyId = (id: number) => {
    void navigator.clipboard?.writeText(entityTag(id)).catch(() => {});
  };

  return (
    <Panel
      title="Inspector"
      subtitle="实体属性"
      tool="scene.get_entity"
      className="w-[330px] shrink-0"
      bodyClassName="flex flex-col overflow-auto min-h-0"
    >
      {selectedDetail === null ? (
        <div className="p-4 text-xs text-sub">
          {selectedEntityId === null ? '在左侧选中一个实体以查看属性。' : '正在读取实体…'}
        </div>
      ) : (
        <>
          {/* identity */}
          <div className="px-3 py-2.5">
            <ReadField
              label="实体 ID"
              value={entityTag(selectedDetail.id)}
              onCopy={() => copyId(selectedDetail.id)}
            />
            <ReadField
              label="parentId"
              value={selectedDetail.parentId === null ? '— (根)' : entityTag(selectedDetail.parentId)}
            />
            <ReadField
              label="children"
              value={
                selectedDetail.children.length
                  ? `${selectedDetail.children.length} · ${selectedDetail.children.map(entityTag).join(', ')}`
                  : '0'
              }
            />
          </div>

          <Collapsible title="Transform">
            <TransformSection detail={selectedDetail} />
          </Collapsible>

          <Collapsible title="组件 (Components)" defaultOpen={false}>
            <ComponentSection />
          </Collapsible>
        </>
      )}
    </Panel>
  );
}
