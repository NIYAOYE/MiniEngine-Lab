import { CheckCircle2, XCircle, X } from 'lucide-react';
import { useEngine } from '@/state/engine';

/** Bottom-right toast stack reflecting ToolResult.ok of recent invocations. */
export function ToastHost() {
  const { toasts, dismissToast } = useEngine();
  return (
    <div className="pointer-events-none fixed bottom-12 right-4 z-50 flex flex-col gap-2 w-[320px]">
      {toasts.map((t) => (
        <div
          key={t.id}
          role="status"
          className={`pointer-events-auto flex items-start gap-2.5 rounded-panel border px-3 py-2.5 shadow-panel
            bg-p2/95 backdrop-blur ${t.ok ? 'border-ok/40' : 'border-danger/40'}`}
        >
          {t.ok ? (
            <CheckCircle2 size={16} className="text-ok mt-0.5 shrink-0" />
          ) : (
            <XCircle size={16} className="text-danger mt-0.5 shrink-0" />
          )}
          <div className="min-w-0 flex-1">
            <div className="text-[13px] font-semibold text-txt flex items-center gap-2">
              <span className="truncate">{t.title}</span>
              {t.code && t.code !== 'Ok' && (
                <span className="text-2xs mono text-danger shrink-0">{t.code}</span>
              )}
            </div>
            {t.detail && <div className="text-2xs text-sub mt-0.5 break-words">{t.detail}</div>}
          </div>
          <button
            aria-label="关闭提示"
            onClick={() => dismissToast(t.id)}
            className="text-sub hover:text-txt shrink-0"
          >
            <X size={14} />
          </button>
        </div>
      ))}
    </div>
  );
}
