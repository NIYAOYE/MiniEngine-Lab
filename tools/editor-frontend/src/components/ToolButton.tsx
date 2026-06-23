import type { ReactNode } from 'react';
import { Button, type ButtonTone, type ButtonSize } from '@/components/ui/Button';
import { TOOLS, canCall, requiredRoleLabel } from '@/lib/toolClient';
import { useEngine } from '@/state/engine';

const PERM_BY_NAME = new Map(TOOLS.map((t) => [t.name, t.permission]));

interface ToolButtonProps {
  tool: string;
  tone?: ButtonTone;
  size?: ButtonSize;
  variant?: 'solid' | 'soft' | 'ghost';
  icon?: ReactNode;
  sublabel?: string;
  block?: boolean;
  /** Extra (non-permission) disable reason, e.g. "需选中实体". */
  disabledReason?: string;
  onClick?: () => void;
  children?: ReactNode;
  title?: string;
  className?: string;
}

/**
 * Button bound to a Tool. Greys out + shows a tooltip when the current role is
 * below the Tool's permission (brief §0.2 permission ladder). The `sublabel`
 * is conventionally the Tool name so the contract is visible in the UI.
 */
export function ToolButton({
  tool,
  tone = 'neutral',
  size = 'md',
  variant = 'solid',
  icon,
  sublabel,
  block,
  disabledReason,
  onClick,
  children,
  title,
  className,
}: ToolButtonProps) {
  const { role } = useEngine();
  const permission = PERM_BY_NAME.get(tool);
  const allowed = permission ? canCall(role, permission) : false;
  const disabled = !allowed || !!disabledReason;

  const tip = !allowed
    ? permission
      ? requiredRoleLabel(permission)
      : '未知 Tool'
    : disabledReason ?? title ?? tool;

  return (
    <Button
      tone={tone}
      size={size}
      variant={variant}
      icon={icon}
      sublabel={sublabel}
      block={block}
      disabled={disabled}
      title={tip}
      className={className}
      aria-label={typeof children === 'string' ? `${children} (${tool})` : tool}
      onClick={onClick}
    >
      {children}
    </Button>
  );
}
