import { useState } from 'react';
import { Sprout, Sun, Moon, Play, CalendarDays } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { Slider } from '@/components/ui/Slider';
import { Input } from '@/components/ui/Input';
import { ToolButton } from '@/components/ToolButton';
import { useEngine } from '@/state/engine';
import { SEASON_ZH } from '@/data/locale';

const MIN_ADVANCE = 5;
const MAX_ADVANCE = 720;
const DEFAULT_ADVANCE = 60;

const pad2 = (n: number) => String(n).padStart(2, '0');

/** Panel B (center top) — calendar/clock readout + time.advance control. */
export function TimeSeasonPanel() {
  const { time, run } = useEngine();
  const [minutes, setMinutes] = useState(DEFAULT_ADVANCE);

  const isNight = time ? time.hour < 6 || time.hour >= 19 : false;
  const seasonZh = time ? SEASON_ZH[time.seasonName] ?? time.seasonName : '—';

  return (
    <Panel
      badge="B"
      title="时间 / 季节"
      subtitle="游戏内日历与时钟"
      tool="time.get"
      bodyClassName="p-3 flex flex-col gap-3"
    >
      <div className="grid grid-cols-3 gap-2.5">
        {/* Season */}
        <div className="rounded-ctl bg-p2 border border-line p-2.5 flex items-center gap-2.5">
          <span className="grid place-items-center h-9 w-9 rounded-ctl bg-ok/15 text-ok shrink-0">
            <Sprout size={20} />
          </span>
          <div className="min-w-0">
            <div className="text-[15px] font-semibold text-txt leading-tight">{seasonZh}季</div>
            <div className="text-2xs text-sub">第 {time?.dayOfSeason ?? '—'} 天</div>
          </div>
        </div>

        {/* Year + clock */}
        <div className="rounded-ctl bg-p2 border border-line p-2.5 flex items-center gap-2.5">
          <span
            className={`grid place-items-center h-9 w-9 rounded-ctl shrink-0 ${
              isNight ? 'bg-grape/20 text-[#b794e8]' : 'bg-amber/15 text-amber'
            }`}
          >
            {isNight ? <Moon size={18} /> : <Sun size={20} />}
          </span>
          <div className="min-w-0">
            <div className="text-[15px] font-semibold text-txt leading-tight mono">
              {time ? `${pad2(time.hour)}:${pad2(time.minute)}` : '--:--'}
            </div>
            <div className="text-2xs text-sub">第 {time?.year ?? '—'} 年</div>
          </div>
        </div>

        {/* Engine-native season name */}
        <div className="rounded-ctl bg-p2 border border-line p-2.5 flex flex-col justify-center">
          <div className="text-2xs text-sub uppercase tracking-wide">seasonName</div>
          <div className="text-[15px] font-semibold text-accent leading-tight mono">
            {time?.seasonName ?? '—'}
          </div>
          <div className="text-2xs text-sub">dayOfSeason {time?.dayOfSeason ?? '—'}</div>
        </div>
      </div>

      {/* Advance control */}
      <div className="rounded-ctl bg-p2 border border-line p-2.5">
        <div className="flex items-center justify-between mb-2">
          <span className="text-xs font-medium text-sub">推进时间</span>
          <code className="text-2xs text-sub/80 mono">time.advance</code>
        </div>
        <div className="flex items-center gap-3">
          <Slider
            value={minutes}
            onValue={setMinutes}
            min={MIN_ADVANCE}
            max={MAX_ADVANCE}
            aria-label="推进分钟数"
            className="flex-1"
          />
          <Input
            type="number"
            min={MIN_ADVANCE}
            value={minutes}
            onChange={(e) =>
              setMinutes(Math.max(MIN_ADVANCE, parseInt(e.target.value || '0', 10) || MIN_ADVANCE))
            }
            aria-label="推进分钟数输入"
            className="w-[68px]"
          />
          <span className="text-xs text-sub shrink-0">分钟</span>
          <ToolButton
            tool="time.advance"
            tone="blue"
            icon={<Play size={14} />}
            onClick={() => void run('time.advance', { minutes })}
          >
            推进
          </ToolButton>
        </div>
        <div className="mt-1.5 flex items-center gap-1 text-2xs text-sub/80">
          <CalendarDays size={11} />
          1 天 = 1440 分钟,跨天自动滚动季节 / 年。
        </div>
      </div>
    </Panel>
  );
}
