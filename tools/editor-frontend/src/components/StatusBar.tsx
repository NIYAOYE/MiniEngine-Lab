import { useEngine } from '@/state/engine';

/** Bottom status bar (≈36px, fixed). */
export function StatusBar() {
  const { busy } = useEngine();
  return (
    <footer className="h-[36px] shrink-0 flex items-center gap-4 px-3 bg-topbar border-t border-line text-2xs text-sub">
      <span className="inline-flex items-center gap-1.5">
        <span
          className={`h-2 w-2 rounded-full ${busy ? 'bg-accent animate-pulse' : 'bg-ok shadow-[0_0_6px_#69b84f]'}`}
        />
        连接状态:{busy ? '调用中…' : '已连接(mock)'}
      </span>
      <span className="h-3 w-px bg-line" />
      <span>项目:<span className="text-txt mono">demo_farm</span></span>
      <span className="h-3 w-px bg-line" />
      <span>版本:<span className="text-txt mono">0.1.0</span></span>
      <span className="ml-auto opacity-80">Powered by Tool Contract (13 Tools)</span>
    </footer>
  );
}
