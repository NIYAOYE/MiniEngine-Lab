import type { InputHTMLAttributes, ReactNode } from 'react';

interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
  leftIcon?: ReactNode;
}

/** Compact dark text/number input (26–32px tall per brief §十三). */
export function Input({ leftIcon, className = '', ...rest }: InputProps) {
  return (
    <div className={`relative flex items-center ${className}`}>
      {leftIcon && (
        <span className="pointer-events-none absolute left-2 text-sub flex items-center">
          {leftIcon}
        </span>
      )}
      <input
        className={`h-[28px] w-full rounded-ctl bg-field border border-line text-txt text-[13px]
          ${leftIcon ? 'pl-7' : 'pl-2.5'} pr-2.5 placeholder:text-sub/70
          focus:border-accent/60 focus:bg-[#26323f] outline-none transition-colors`}
        {...rest}
      />
    </div>
  );
}

/** Tiny labelled numeric field used by Transform / coordinate editors. */
export function NumberField({
  label,
  value,
  onValue,
  step = 1,
  className = '',
  ...rest
}: {
  label: string;
  value: number;
  onValue: (n: number) => void;
  step?: number;
  className?: string;
} & Omit<InputHTMLAttributes<HTMLInputElement>, 'value' | 'onChange' | 'className'>) {
  return (
    <label className={`flex flex-col gap-1 ${className}`}>
      <span className="text-2xs text-sub uppercase tracking-wide">{label}</span>
      <input
        type="number"
        step={step}
        value={Number.isFinite(value) ? value : 0}
        onChange={(e) => onValue(parseFloat(e.target.value))}
        className="h-[28px] w-full rounded-ctl bg-field border border-line text-txt text-[13px] px-2
          mono focus:border-accent/60 focus:bg-[#26323f] outline-none transition-colors"
        {...rest}
      />
    </label>
  );
}
