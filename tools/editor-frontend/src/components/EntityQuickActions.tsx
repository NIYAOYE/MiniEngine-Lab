import { Wrench, Crosshair, Trash2 } from 'lucide-react';
import { Panel } from '@/components/ui/Panel';
import { ToolButton } from '@/components/ToolButton';
import { useEngine } from '@/state/engine';
import { entityTag } from '@/data/labels';

/** Card B — large permission-gated shortcuts for the selected entity. */
export function EntityQuickActions() {
  const { selectedEntityId, selectedDetail, selectEntity, run } = useEngine();
  const none = selectedEntityId === null;
  const reason = none ? '未选中实体' : undefined;

  return (
    <Panel
      title="实体快捷操作"
      subtitle={none ? '未选中实体' : `选中:${entityTag(selectedEntityId)}`}
      bodyClassName="flex flex-col gap-2 p-2.5"
    >
      <ToolButton
        tool="scene.get_entity"
        tone="blue"
        variant="soft"
        block
        icon={<Wrench size={16} />}
        sublabel="scene.get_entity"
        disabledReason={reason}
        onClick={() => !none && selectEntity(selectedEntityId)}
      >
        查看详情
      </ToolButton>

      <ToolButton
        tool="entity.set_transform"
        tone="green"
        variant="soft"
        block
        icon={<Crosshair size={16} />}
        sublabel="entity.set_transform"
        disabledReason={reason}
        onClick={() => {
          if (none || !selectedDetail) return;
          // Re-applies current transform; the Inspector is the full editor.
          void run('entity.set_transform', {
            id: selectedEntityId,
            position: selectedDetail.position,
            rotation: selectedDetail.rotation,
            scale: selectedDetail.scale,
          });
        }}
      >
        设置变换
      </ToolButton>

      <ToolButton
        tool="scene.destroy_entity"
        tone="red"
        variant="soft"
        block
        icon={<Trash2 size={16} />}
        sublabel="scene.destroy_entity"
        disabledReason={reason}
        onClick={() => !none && void run('scene.destroy_entity', { id: selectedEntityId })}
      >
        删除实体
      </ToolButton>
    </Panel>
  );
}
