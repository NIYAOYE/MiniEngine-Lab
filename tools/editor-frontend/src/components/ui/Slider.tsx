import type { InputHTMLAttributes } from 'react';

interface SliderProps
  extends Omit<InputHTMLAttributes<HTMLInputElement>, 'onChange' | 'value' | 'type'> {
  value: number;
  onValue: (n: number) => void;
}

/**
 * Range slider with a filled track. The fill is a CSS gradient painted on the
 * input's own background (the most cross-browser-stable approach); the native
 * tracks are made transparent so only our gradient shows through.
 */
export function Slider({ value, onValue, min = 0, max = 100, className = '', ...rest }: SliderProps) {
  const lo = Number(min);
  const hi = Number(max);
  const pct = hi > lo ? Math.max(0, Math.min(100, ((value - lo) / (hi - lo)) * 100)) : 0;
  return (
    <input
      type="range"
      min={min}
      max={max}
      value={value}
      onChange={(e) => onValue(parseInt(e.target.value, 10))}
      style={{
        background: `linear-gradient(90deg, #3b82f6 ${pct}%, #202c39 ${pct}%)`,
        borderRadius: '999px',
        height: '6px',
      }}
      className={`my-[11px] w-full cursor-pointer appearance-none border border-line
        [&::-webkit-slider-runnable-track]:bg-transparent
        [&::-moz-range-track]:bg-transparent
        [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-3.5
        [&::-webkit-slider-thumb]:w-3.5 [&::-webkit-slider-thumb]:rounded-full
        [&::-webkit-slider-thumb]:bg-accent [&::-webkit-slider-thumb]:-mt-[5px]
        [&::-webkit-slider-thumb]:border-2 [&::-webkit-slider-thumb]:border-[#0e1722]
        [&::-moz-range-thumb]:h-3.5 [&::-moz-range-thumb]:w-3.5
        [&::-moz-range-thumb]:rounded-full [&::-moz-range-thumb]:bg-accent
        [&::-moz-range-thumb]:border-2 [&::-moz-range-thumb]:border-[#0e1722] ${className}`}
      {...rest}
    />
  );
}
