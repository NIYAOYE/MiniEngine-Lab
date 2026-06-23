import { Sprout, Undo2, Redo2, Settings } from 'lucide-react';
import { RolePicker } from '@/components/RolePicker';

const NAV_TABS = [
  { id: 'section-scene', label: '场景' },
  { id: 'section-time', label: '时间' },
  { id: 'section-crop', label: '作物' },
  { id: 'section-audit', label: '审计·历史' },
  { id: 'section-tools', label: '工具 (13)' },
];

interface Props {
  active: string;
  onNavigate: (id: string) => void;
}

/** Top bar (≈44px): brand + section tabs + role view + disabled undo/redo. */
export function TopNavigation({ active, onNavigate }: Props) {
  return (
    <header className="h-[44px] shrink-0 flex items-center gap-3 px-3 bg-topbar border-b border-line">
      {/* brand */}
      <div className="flex items-center gap-2 shrink-0">
        <span className="grid place-items-center h-6 w-6 rounded bg-ok/15 text-ok">
          <Sprout size={16} />
        </span>
        <span className="font-semibold text-txt text-[14px]">Farm Editor</span>
        <span className="px-1.5 h-[18px] grid place-items-center rounded bg-ok/15 text-ok text-2xs font-semibold mono">
          M8.2
        </span>
      </div>

      {/* section tabs */}
      <nav className="flex items-center gap-0.5 ml-2">
        {NAV_TABS.map((t) => (
          <button
            key={t.id}
            onClick={() => onNavigate(t.id)}
            className={`px-2.5 h-[28px] rounded-ctl text-[13px] transition-colors ${
              active === t.id
                ? 'bg-white/[0.06] text-txt'
                : 'text-sub hover:text-txt hover:bg-white/[0.03]'
            }`}
          >
            {t.label}
          </button>
        ))}
      </nav>

      {/* right cluster */}
      <div className="ml-auto flex items-center gap-2.5">
        <span className="inline-flex items-center gap-1.5 text-2xs text-sub">
          <span className="h-2 w-2 rounded-full bg-ok shadow-[0_0_6px_#69b84f]" />
        </span>
        <RolePicker />
        <span className="h-5 w-px bg-line" />
        <button
          disabled
          title="future:edit.undo 待引擎暴露"
          aria-label="撤销 (future)"
          className="grid place-items-center h-[28px] w-[28px] rounded-ctl text-sub opacity-40 cursor-not-allowed"
        >
          <Undo2 size={15} />
        </button>
        <button
          disabled
          title="future:edit.redo 待引擎暴露"
          aria-label="重做 (future)"
          className="grid place-items-center h-[28px] w-[28px] rounded-ctl text-sub opacity-40 cursor-not-allowed"
        >
          <Redo2 size={15} />
        </button>
        <button
          aria-label="设置"
          title="设置"
          className="grid place-items-center h-[28px] w-[28px] rounded-ctl text-sub hover:text-txt hover:bg-white/5"
        >
          <Settings size={15} />
        </button>
      </div>
    </header>
  );
}
