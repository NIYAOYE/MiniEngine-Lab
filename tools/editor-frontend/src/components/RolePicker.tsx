import { ChevronDown, UserCog } from 'lucide-react';
import { useEngine } from '@/state/engine';
import type { Role } from '@/types';

const ROLES: Role[] = ['Agent', 'Automation', 'Editor'];

/**
 * Role selector that drives global permission gating. Changing it re-evaluates
 * every ToolButton's enabled state (brief §三 / §0.2).
 */
export function RolePicker() {
  const { role, setRole } = useEngine();
  return (
    <label className="inline-flex items-center gap-2 text-xs text-sub">
      <span className="hidden sm:inline">权限视图:</span>
      <span className="relative inline-flex items-center">
        <UserCog size={14} className="pointer-events-none absolute left-2 text-accent" />
        <select
          value={role}
          onChange={(e) => setRole(e.target.value as Role)}
          aria-label="权限视图角色"
          className="h-[28px] appearance-none rounded-ctl bg-field border border-line text-txt text-xs
            pl-7 pr-7 outline-none focus:border-accent/60 cursor-pointer"
        >
          {ROLES.map((r) => (
            <option key={r} value={r} className="bg-p2">
              {r}
            </option>
          ))}
        </select>
        <ChevronDown size={13} className="pointer-events-none absolute right-2 text-sub" />
      </span>
    </label>
  );
}
