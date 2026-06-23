import { ChevronDown } from 'lucide-react';
import type { SelectHTMLAttributes } from 'react';

interface Option {
  value: string;
  label: string;
  disabled?: boolean;
}

interface SelectProps extends Omit<SelectHTMLAttributes<HTMLSelectElement>, 'onChange'> {
  options: Option[];
  onValue: (value: string) => void;
}

/** Compact dark <select> with a custom chevron. */
export function Select({ options, onValue, className = '', ...rest }: SelectProps) {
  return (
    <div className={`relative inline-flex items-center ${className}`}>
      <select
        className="h-[28px] w-full appearance-none rounded-ctl bg-field border border-line text-txt
          text-[13px] pl-2.5 pr-7 outline-none focus:border-accent/60 focus:bg-[#26323f]
          transition-colors cursor-pointer"
        onChange={(e) => onValue(e.target.value)}
        {...rest}
      >
        {options.map((o) => (
          <option key={o.value} value={o.value} disabled={o.disabled} className="bg-p2 text-txt">
            {o.label}
          </option>
        ))}
      </select>
      <ChevronDown size={14} className="pointer-events-none absolute right-2 text-sub" />
    </div>
  );
}
