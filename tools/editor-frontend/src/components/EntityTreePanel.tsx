import { useState } from 'react';
import { Plus, Trash2, Search } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { Input } from '@/components/ui/Input';
import { ToolButton } from '@/components/ToolButton';
import { EntityTreeItem } from '@/components/EntityTreeItem';
import { useEngine } from '@/state/engine';

/** Card A — the live scene hierarchy (scene.list_entities + per-node get_entity). */
export function EntityTreePanel() {
  const { tree, selectedEntityId, selectEntity, run } = useEngine();
  const [filter, setFilter] = useState('');

  return (
    <Panel
      badge="A"
      title="场景 / 实体"
      subtitle="实体层级 (scene.list_entities)"
      className="flex-1"
      bodyClassName="flex flex-col p-2.5 gap-2.5"
    >
      <div className="flex gap-1.5">
        <ToolButton
          tool="scene.create_entity"
          tone="blue"
          size="sm"
          icon={<Plus size={14} />}
          onClick={() => void run('scene.create_entity', {})}
        >
          创建实体
        </ToolButton>
        <ToolButton
          tool="scene.destroy_entity"
          tone="red"
          size="sm"
          icon={<Trash2 size={14} />}
          disabledReason={selectedEntityId === null ? '未选中实体' : undefined}
          onClick={() =>
            selectedEntityId !== null && void run('scene.destroy_entity', { id: selectedEntityId })
          }
        >
          删除
        </ToolButton>
      </div>

      <Input
        leftIcon={<Search size={13} />}
        placeholder="搜索实体..."
        value={filter}
        onChange={(e) => setFilter(e.target.value)}
        aria-label="搜索实体"
      />

      <div role="tree" className="flex-1 overflow-auto -mx-1 px-1 min-h-0">
        {tree.length === 0 ? (
          <p className="text-2xs text-sub px-2 py-3">场景为空。点击「创建实体」开始。</p>
        ) : (
          tree.map((n) => (
            <EntityTreeItem
              key={n.id}
              node={n}
              depth={0}
              selectedId={selectedEntityId}
              onSelect={selectEntity}
              filter={filter}
            />
          ))
        )}
      </div>
    </Panel>
  );
}
