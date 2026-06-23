import { useState } from 'react';
import { ChevronRight, Box, MoreVertical } from 'lucide-react';
import type { EntityNode } from '@/types';
import { entityTag } from '@/data/labels';

interface Props {
  node: EntityNode;
  depth: number;
  selectedId: number | null;
  onSelect: (id: number) => void;
  filter: string;
}

/** Does this node, or any descendant, match the search filter? */
function matches(node: EntityNode, filter: string): boolean {
  if (!filter) return true;
  const f = filter.toLowerCase();
  const self =
    entityTag(node.id).includes(f) ||
    String(node.id).includes(f) ||
    (node.label?.toLowerCase().includes(f) ?? false);
  return self || node.childNodes.some((c) => matches(c, filter));
}

/** One row in the real parent/child entity tree (id is the authoritative label). */
export function EntityTreeItem({ node, depth, selectedId, onSelect, filter }: Props) {
  const [open, setOpen] = useState(true);
  if (!matches(node, filter)) return null;

  const hasChildren = node.childNodes.length > 0;
  const selected = node.id === selectedId;

  return (
    <div>
      <div
        role="treeitem"
        aria-selected={selected}
        aria-expanded={hasChildren ? open : undefined}
        tabIndex={0}
        onClick={() => onSelect(node.id)}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            onSelect(node.id);
          }
        }}
        className={`group flex items-center gap-1 h-[28px] rounded-ctl cursor-pointer pr-1
          ${selected ? 'bg-accent/15 ring-1 ring-accent/30' : 'hover:bg-white/[0.04]'}`}
        style={{ paddingLeft: 6 + depth * 14 }}
      >
        <button
          aria-label={open ? '收起' : '展开'}
          onClick={(e) => {
            e.stopPropagation();
            setOpen((o) => !o);
          }}
          className={`grid place-items-center h-4 w-4 shrink-0 text-sub transition-transform
            ${hasChildren ? 'hover:text-txt' : 'invisible'} ${open ? 'rotate-90' : ''}`}
        >
          <ChevronRight size={13} />
        </button>
        <Box size={14} className={`shrink-0 ${selected ? 'text-accent' : 'text-sub'}`} />
        <span className="mono text-[12px] text-txt truncate">{entityTag(node.id)}</span>
        {node.label && (
          <span
            className="text-2xs text-sub/80 truncate"
            title="前端本地标注(future:引擎契约无实体名)"
          >
            {node.label}
          </span>
        )}
        <button
          aria-label="更多操作"
          onClick={(e) => e.stopPropagation()}
          className="ml-auto opacity-0 group-hover:opacity-100 text-sub hover:text-txt shrink-0"
        >
          <MoreVertical size={14} />
        </button>
      </div>
      {hasChildren && open && (
        <div role="group">
          {node.childNodes.map((c) => (
            <EntityTreeItem
              key={c.id}
              node={c}
              depth={depth + 1}
              selectedId={selectedId}
              onSelect={onSelect}
              filter={filter}
            />
          ))}
        </div>
      )}
    </div>
  );
}
