import type { ReactNode } from 'react';

interface PanelProps {
  /** Section letter badge, e.g. "A", "B" (brief uses A/B/C/D). */
  badge?: string;
  title: ReactNode;
  subtitle?: ReactNode;
  /** Tool name shown subtly in the header, e.g. scene.list_entities. */
  tool?: string;
  actions?: ReactNode;
  children: ReactNode;
  className?: string;
  bodyClassName?: string;
  scroll?: boolean;
}

/**
 * Standard editor panel: layered dark surface, 1px border, header with optional
 * section badge + tool name, and a (optionally scrollable) body.
 */
export function Panel({
  badge,
  title,
  subtitle,
  tool,
  actions,
  children,
  className = '',
  bodyClassName = '',
  scroll = false,
}: PanelProps) {
  return (
    <section
      className={`flex flex-col rounded-panel border border-line bg-p1 shadow-panel min-h-0 ${className}`}
    >
      <header className="flex items-center gap-2 px-3 h-[38px] shrink-0 border-b border-line bg-gradient-to-b from-white/[0.015] to-transparent">
        {badge && (
          <span className="grid place-items-center h-[18px] w-[18px] rounded bg-accent/15 text-accent text-2xs font-bold shrink-0">
            {badge}
          </span>
        )}
        <div className="flex flex-col min-w-0">
          <h2 className="text-[13px] font-semibold text-txt leading-tight truncate">{title}</h2>
          {subtitle && <span className="text-2xs text-sub leading-tight truncate">{subtitle}</span>}
        </div>
        {tool && (
          <code className="ml-1 text-2xs text-sub/80 mono hidden xl:inline truncate">{tool}</code>
        )}
        {actions && <div className="ml-auto flex items-center gap-1.5 shrink-0">{actions}</div>}
      </header>
      <div className={`min-h-0 ${scroll ? 'overflow-auto' : ''} ${bodyClassName}`}>{children}</div>
    </section>
  );
}
