interface SwitchProps {
  checked: boolean;
  onChange?: (checked: boolean) => void;
  disabled?: boolean;
  label?: string;
  /** future flag — renders muted with a tooltip and stays non-interactive. */
  future?: boolean;
  title?: string;
}

/** Small toggle switch. When `future`/`disabled`, it is non-interactive. */
export function Switch({ checked, onChange, disabled, label, future, title }: SwitchProps) {
  const off = disabled || future;
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      title={title ?? (future ? 'future:契约未暴露' : undefined)}
      disabled={off}
      onClick={() => !off && onChange?.(!checked)}
      className={`inline-flex items-center gap-2 ${off ? 'opacity-45 cursor-not-allowed' : ''}`}
    >
      <span
        className={`relative h-[16px] w-[28px] rounded-full transition-colors duration-150
          ${checked ? 'bg-accent' : 'bg-field border border-line'}`}
      >
        <span
          className={`absolute top-[2px] h-[12px] w-[12px] rounded-full bg-white transition-all duration-150
            ${checked ? 'left-[14px]' : 'left-[2px]'}`}
        />
      </span>
      {label && <span className="text-xs text-sub">{label}</span>}
    </button>
  );
}
