import { useMemo, useState } from 'react';
import {
  Search,
  List,
  Eye,
  ScrollText,
  Sprout,
  Clock,
  PlusSquare,
  Move,
  FastForward,
  Leaf,
  Droplets,
  CalendarRange,
  Trash2,
  Wheat,
  ShieldCheck,
  ShieldAlert,
  Lock,
} from 'lucide-react';
import type { LucideIcon } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { Input } from '@/components/ui/Input';
import { useEngine } from '@/state/engine';
import { canCall } from '@/lib/toolClient';
import type { Permission, ToolDefinition } from '@/types';

const TOOL_ICON: Record<string, LucideIcon> = {
  'scene.list_entities': List,
  'scene.get_entity': Eye,
  'log.read': ScrollText,
  'crop.get_field': Sprout,
  'time.get': Clock,
  'scene.create_entity': PlusSquare,
  'entity.set_transform': Move,
  'time.advance': FastForward,
  'crop.plant': Leaf,
  'crop.water': Droplets,
  'crop.advance_days': CalendarRange,
  'scene.destroy_entity': Trash2,
  'crop.harvest': Wheat,
};

const PERM_BADGE: Record<Permission, { label: string; cls: string; Icon: LucideIcon }> = {
  AgentAllowed: { label: 'AgentAllowed', cls: 'text-ok bg-ok/12 border-ok/25', Icon: ShieldCheck },
  Automation: { label: 'Automation', cls: 'text-accent bg-accent/12 border-accent/25', Icon: ShieldAlert },
  EditorOnly: { label: 'EditorOnly', cls: 'text-amber bg-amber/12 border-amber/25', Icon: Lock },
};

type Filter = 'all' | Permission;

/** Bottom-right — the 13-Tool registry (listTools), linked to the role view. */
export function ToolOverviewPanel() {
  const { tools, role } = useEngine();
  const [query, setQuery] = useState('');
  const [filter, setFilter] = useState<Filter>('all');

  const filtered = useMemo(
    () =>
      tools.filter(
        (t) =>
          (filter === 'all' || t.permission === filter) &&
          (query ? t.name.toLowerCase().includes(query.toLowerCase()) : true),
      ),
    [tools, filter, query],
  );

  const tabs: { key: Filter; label: string }[] = [
    { key: 'all', label: `全部 ${tools.length}` },
    { key: 'AgentAllowed', label: 'AgentAllowed' },
    { key: 'Automation', label: 'Automation' },
    { key: 'EditorOnly', label: 'EditorOnly' },
  ];

  return (
    <Panel
      title={`工具 (${tools.length}) 总览`}
      subtitle="自描述契约 (listTools)"
      className="w-[330px] shrink-0 min-h-0"
      bodyClassName="flex flex-col min-h-0"
    >
      <div className="p-2.5 flex flex-col gap-2 border-b border-line">
        <Input
          leftIcon={<Search size={13} />}
          placeholder="搜索工具..."
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          aria-label="搜索工具"
        />
        <div className="flex flex-wrap gap-1">
          {tabs.map((t) => (
            <button
              key={t.key}
              onClick={() => setFilter(t.key)}
              className={`px-2 h-[24px] rounded-ctl text-2xs border transition-colors ${
                filter === t.key
                  ? 'bg-accent/15 border-accent/40 text-accent'
                  : 'border-line text-sub hover:text-txt hover:bg-white/5'
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>
      </div>

      <ul className="overflow-auto min-h-0 p-1.5 flex flex-col gap-1">
        {filtered.map((t) => (
          <ToolRow key={t.name} tool={t} callable={canCall(role, t.permission)} />
        ))}
        {filtered.length === 0 && (
          <li className="px-2 py-4 text-center text-2xs text-sub">无匹配工具。</li>
        )}
      </ul>
    </Panel>
  );
}

function ToolRow({ tool, callable }: { tool: ToolDefinition; callable: boolean }) {
  const Icon = TOOL_ICON[tool.name] ?? List;
  const badge = PERM_BADGE[tool.permission];
  return (
    <li
      title={`${tool.description}${callable ? '' : ' — 当前角色不可调用'}`}
      className={`flex items-center gap-2 h-[34px] px-2 rounded-ctl border border-transparent
        hover:border-line hover:bg-white/[0.03] ${callable ? '' : 'opacity-45'}`}
    >
      <Icon size={15} className={tool.category === 'Query' ? 'text-sub' : 'text-accent'} />
      <div className="min-w-0 flex-1">
        <div className="mono text-[12px] text-txt truncate">{tool.name}</div>
        <div className="text-2xs text-sub leading-none">{tool.category}</div>
      </div>
      <span className={`inline-flex items-center gap-1 px-1.5 h-[18px] rounded border text-2xs ${badge.cls}`}>
        <badge.Icon size={10} />
        {badge.label}
      </span>
    </li>
  );
}
