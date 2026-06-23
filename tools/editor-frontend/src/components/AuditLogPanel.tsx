import { useMemo, useState } from 'react';
import { CheckCircle2, XCircle, RefreshCw } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { Select } from '@/components/ui/Select';
import { Input } from '@/components/ui/Input';
import { Button } from '@/components/ui/Button';
import { useEngine } from '@/state/engine';
import type { AuditEntry, Role } from '@/types';

const ROLE_COLOR: Record<Role, string> = {
  Agent: 'text-[#b794e8]',
  Automation: 'text-accent',
  Editor: 'text-ok',
};

function clockTime(ts: number): string {
  const d = new Date(ts);
  const p = (n: number) => String(n).padStart(2, '0');
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function compactJson(value: unknown, max = 80): string {
  let s: string;
  try {
    s = JSON.stringify(value);
  } catch {
    s = String(value);
  }
  if (!s) return '∅';
  return s.length > max ? `${s.slice(0, max - 1)}…` : s;
}

/**
 * Panel D — audit / history.
 * Data source is the FRONT-END call history (every column is honest), not the
 * engine's log.read (which exposes only a subset — see footnote / brief §0.5).
 */
export function AuditLogPanel() {
  const { history } = useEngine();
  const [toolFilter, setToolFilter] = useState('');
  const [statusFilter, setStatusFilter] = useState<'all' | 'ok' | 'fail'>('all');
  const [rangeMin, setRangeMin] = useState<'all' | '1' | '5'>('all');
  const [nonce, setNonce] = useState(0); // manual refresh tick

  const rows = useMemo(() => {
    const now = Date.now();
    void nonce;
    return history
      .filter((h) => (toolFilter ? h.tool.toLowerCase().includes(toolFilter.toLowerCase()) : true))
      .filter((h) => (statusFilter === 'all' ? true : statusFilter === 'ok' ? h.ok : !h.ok))
      .filter((h) =>
        rangeMin === 'all' ? true : now - h.timestamp <= Number(rangeMin) * 60 * 1000,
      )
      .slice()
      .reverse();
  }, [history, toolFilter, statusFilter, rangeMin, nonce]);

  return (
    <Panel
      badge="D"
      title="审计 / 历史"
      subtitle="数据 = 前端调用历史(每列均诚实)"
      tool="log.read"
      className="flex-1 min-w-0"
      bodyClassName="flex flex-col min-h-0"
      actions={
        <Button
          tone="blue"
          size="sm"
          variant="soft"
          icon={<RefreshCw size={13} />}
          onClick={() => setNonce((n) => n + 1)}
        >
          刷新
        </Button>
      }
    >
      {/* filters */}
      <div className="flex flex-wrap items-center gap-2 px-3 py-2 border-b border-line">
        <Select
          value={rangeMin}
          onValue={(v) => setRangeMin(v as 'all' | '1' | '5')}
          options={[
            { value: 'all', label: '时间范围:全部' },
            { value: '5', label: '最近 5 分钟' },
            { value: '1', label: '最近 1 分钟' },
          ]}
          className="w-[140px]"
        />
        <Input
          placeholder="工具筛选..."
          value={toolFilter}
          onChange={(e) => setToolFilter(e.target.value)}
          aria-label="工具筛选"
          className="w-[150px]"
        />
        <Select
          value={statusFilter}
          onValue={(v) => setStatusFilter(v as 'all' | 'ok' | 'fail')}
          options={[
            { value: 'all', label: '状态:全部' },
            { value: 'ok', label: '仅成功' },
            { value: 'fail', label: '仅失败' },
          ]}
          className="w-[110px]"
        />
        <span className="ml-auto text-2xs text-sub">{rows.length} 条</span>
      </div>

      {/* table */}
      <div className="overflow-auto min-h-0">
        <table className="w-full text-xs border-collapse">
          <thead className="sticky top-0 bg-p1 z-10">
            <tr className="text-2xs text-sub text-left">
              <th className="font-medium px-2 py-1.5 w-8"></th>
              <th className="font-medium px-2 py-1.5">时间(客户端)</th>
              <th className="font-medium px-2 py-1.5">工具</th>
              <th className="font-medium px-2 py-1.5">角色</th>
              <th className="font-medium px-2 py-1.5">参数 (JSON)</th>
              <th className="font-medium px-2 py-1.5">结果 (JSON)</th>
              <th className="font-medium px-2 py-1.5">状态</th>
              <th className="font-medium px-2 py-1.5 text-right">耗时</th>
            </tr>
          </thead>
          <tbody>
            {rows.length === 0 ? (
              <tr>
                <td colSpan={8} className="px-3 py-6 text-center text-sub text-xs">
                  无匹配记录。
                </td>
              </tr>
            ) : (
              rows.map((h) => <AuditRow key={h.id} entry={h} />)
            )}
          </tbody>
        </table>
      </div>

      <p className="px-3 py-1.5 border-t border-line text-2xs text-sub/80 shrink-0">
        注:引擎 <code className="mono">log.read</code> 为权威子集(无时间 / 角色 / 结果 / 耗时);完整可观测性由前端调用历史补足。
      </p>
    </Panel>
  );
}

function AuditRow({ entry }: { entry: AuditEntry }) {
  return (
    <tr className="border-t border-line/70 hover:bg-white/[0.03] align-top">
      <td className="px-2 py-1.5">
        {entry.ok ? (
          <CheckCircle2 size={14} className="text-ok" />
        ) : (
          <XCircle size={14} className="text-danger" />
        )}
      </td>
      <td className="px-2 py-1.5 mono text-sub whitespace-nowrap">{clockTime(entry.timestamp)}</td>
      <td className="px-2 py-1.5 mono text-txt whitespace-nowrap">{entry.tool}</td>
      <td className={`px-2 py-1.5 mono whitespace-nowrap ${ROLE_COLOR[entry.role]}`}>{entry.role}</td>
      <td className="px-2 py-1.5 mono text-sub max-w-[200px] truncate" title={JSON.stringify(entry.params)}>
        {compactJson(entry.params)}
      </td>
      <td
        className="px-2 py-1.5 mono text-sub max-w-[240px] truncate"
        title={JSON.stringify(entry.result)}
      >
        {compactJson(entry.result.data)}
      </td>
      <td className="px-2 py-1.5 whitespace-nowrap">
        <span className={`mono ${entry.ok ? 'text-ok' : 'text-danger'}`}>{entry.code}</span>
      </td>
      <td className="px-2 py-1.5 mono text-sub text-right whitespace-nowrap">{entry.durationMs}ms</td>
    </tr>
  );
}
