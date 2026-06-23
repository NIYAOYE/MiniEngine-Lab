import type { ButtonHTMLAttributes, ReactNode } from 'react';

export type ButtonTone = 'neutral' | 'blue' | 'green' | 'red' | 'purple' | 'orange';
export type ButtonVariant = 'solid' | 'soft' | 'ghost';
export type ButtonSize = 'sm' | 'md';

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  tone?: ButtonTone;
  variant?: ButtonVariant;
  size?: ButtonSize;
  icon?: ReactNode;
  /** Optional sub-label rendered under the main label (used by quick actions). */
  sublabel?: string;
  block?: boolean;
}

const SOLID: Record<ButtonTone, string> = {
  neutral: 'bg-field text-txt hover:bg-[#26323f] active:bg-[#2c3a48] border border-line',
  blue: 'bg-accent text-white hover:bg-[#4c8ef7] active:bg-[#2f72e0] border border-[#2f6fd6]',
  green: 'bg-ok text-[#0c1a0c] hover:bg-[#79c95f] active:bg-[#5aa843] border border-[#4f9a3a]',
  red: 'bg-danger text-white hover:bg-[#e2685f] active:bg-[#bf463f] border border-[#b8403a]',
  purple: 'bg-grape text-white hover:bg-[#8c5cd6] active:bg-[#6a3eaf] border border-[#5e379b]',
  orange: 'bg-amber text-[#1c1206] hover:bg-[#d99a3e] active:bg-[#b3781f] border border-[#9c6a23]',
};

const SOFT: Record<ButtonTone, string> = {
  neutral: 'bg-field/70 text-sub hover:text-txt hover:bg-field border border-line',
  blue: 'bg-accent/12 text-accent hover:bg-accent/20 border border-accent/30',
  green: 'bg-ok/12 text-ok hover:bg-ok/20 border border-ok/30',
  red: 'bg-danger/12 text-danger hover:bg-danger/20 border border-danger/30',
  purple: 'bg-grape/15 text-[#b794e8] hover:bg-grape/25 border border-grape/40',
  orange: 'bg-amber/12 text-amber hover:bg-amber/20 border border-amber/35',
};

const GHOST: Record<ButtonTone, string> = {
  neutral: 'text-sub hover:text-txt hover:bg-white/5 border border-transparent',
  blue: 'text-accent hover:bg-accent/10 border border-transparent',
  green: 'text-ok hover:bg-ok/10 border border-transparent',
  red: 'text-danger hover:bg-danger/10 border border-transparent',
  purple: 'text-[#b794e8] hover:bg-grape/10 border border-transparent',
  orange: 'text-amber hover:bg-amber/10 border border-transparent',
};

const SIZE: Record<ButtonSize, string> = {
  sm: 'h-[26px] px-2 text-xs gap-1.5',
  md: 'h-[30px] px-3 text-[13px] gap-2',
};

/** Themed button with tone/variant/size, icon + optional sublabel, a11y title. */
export function Button({
  tone = 'neutral',
  variant = 'solid',
  size = 'md',
  icon,
  sublabel,
  block,
  className = '',
  children,
  disabled,
  ...rest
}: ButtonProps) {
  const palette = variant === 'solid' ? SOLID : variant === 'soft' ? SOFT : GHOST;
  const base =
    'inline-flex items-center justify-center rounded-ctl font-medium select-none transition-colors duration-100 ' +
    'disabled:opacity-40 disabled:cursor-not-allowed disabled:hover:bg-inherit focus-visible:outline-2';

  if (sublabel) {
    return (
      <button
        className={`${base} ${palette[tone]} ${block ? 'w-full' : ''} h-auto px-3 py-2 text-left ${className}`}
        disabled={disabled}
        {...rest}
      >
        <span className="flex items-center gap-2.5 w-full">
          {icon && <span className="shrink-0 opacity-90">{icon}</span>}
          <span className="flex flex-col leading-tight min-w-0">
            <span className="text-[13px] font-semibold truncate">{children}</span>
            <span className="text-2xs font-normal opacity-70 mono truncate">{sublabel}</span>
          </span>
        </span>
      </button>
    );
  }

  return (
    <button
      className={`${base} ${palette[tone]} ${SIZE[size]} ${block ? 'w-full' : ''} ${className}`}
      disabled={disabled}
      {...rest}
    >
      {icon}
      {children && <span className="truncate">{children}</span>}
    </button>
  );
}
