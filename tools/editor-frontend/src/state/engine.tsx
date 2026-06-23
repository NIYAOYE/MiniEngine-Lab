/**
 * Engine state hook + React context.
 *
 * This is the ONLY place that calls toolClient. Every mutation goes through
 * `run()` → `invoke()` with the current role, so permission decisions and the
 * audit log are uniform and components never touch engine state directly
 * (brief §11). Views are refreshed by re-reading Query Tools, exactly as a real
 * networked client would.
 */
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';

import {
  getHistory,
  invoke,
  listTools,
  seedDemoWorld,
  subscribeHistory,
} from '@/lib/toolClient';
import { LOCAL_LABELS } from '@/data/labels';
import type {
  AuditEntry,
  CropTile,
  EntityDetail,
  EntityNode,
  Role,
  TimeView,
  ToastMessage,
  ToolDefinition,
  ToolResult,
} from '@/types';

export interface TilePos {
  x: number;
  y: number;
}

interface EngineApi {
  role: Role;
  setRole: (r: Role) => void;

  tree: EntityNode[];
  detailMap: Record<number, EntityDetail>;
  selectedEntityId: number | null;
  selectedDetail: EntityDetail | null;
  selectEntity: (id: number) => void;

  time: TimeView | null;
  field: CropTile[];
  fieldByKey: Record<string, CropTile>;
  selectedTile: TilePos;
  selectTile: (pos: TilePos) => void;

  tools: ToolDefinition[];
  history: AuditEntry[];

  toasts: ToastMessage[];
  dismissToast: (id: number) => void;

  /** Run a Tool through invoke() with the current role; toasts + refreshes. */
  run: (
    name: string,
    params?: Record<string, unknown>,
    opts?: { dryRun?: boolean },
  ) => Promise<ToolResult>;

  busy: boolean;
}

const EngineContext = createContext<EngineApi | null>(null);

/** Default farm dimensions for the demo (brief §六: 3×3). */
export const FIELD_COLS = 3;
export const FIELD_ROWS = 3;

const tileKey = (x: number, y: number) => `${x},${y}`;

function buildTree(details: Record<number, EntityDetail>): EntityNode[] {
  const toNode = (id: number): EntityNode => {
    const d = details[id];
    return {
      ...d,
      label: LOCAL_LABELS[id],
      childNodes: d.children.filter((c) => details[c]).map(toNode),
    };
  };
  return Object.values(details)
    .filter((d) => d.parentId === null || !details[d.parentId])
    .sort((a, b) => a.id - b.id)
    .map((d) => toNode(d.id));
}

export function EngineProvider({ children }: { children: ReactNode }) {
  const [role, setRole] = useState<Role>('Editor');
  const [detailMap, setDetailMap] = useState<Record<number, EntityDetail>>({});
  const [tree, setTree] = useState<EntityNode[]>([]);
  const [selectedEntityId, setSelectedEntityId] = useState<number | null>(null);
  const [time, setTime] = useState<TimeView | null>(null);
  const [fieldByKey, setFieldByKey] = useState<Record<string, CropTile>>({});
  const [selectedTile, setSelectedTile] = useState<TilePos>({ x: 0, y: 0 });
  const [tools, setTools] = useState<ToolDefinition[]>([]);
  const [history, setHistory] = useState<AuditEntry[]>(getHistory());
  const [toasts, setToasts] = useState<ToastMessage[]>([]);
  const [busy, setBusy] = useState(false);

  const toastSeq = useRef(0);
  const roleRef = useRef(role);
  roleRef.current = role;

  // ── Toasts ───────────────────────────────────────────────────────────────
  const dismissToast = useCallback((id: number) => {
    setToasts((t) => t.filter((x) => x.id !== id));
  }, []);

  const pushToast = useCallback(
    (res: ToolResult, toolName: string) => {
      const id = ++toastSeq.current;
      setToasts((t) => [
        ...t,
        {
          id,
          ok: res.ok,
          title: res.ok ? `${toolName} 成功` : `${toolName} 失败`,
          detail: res.message,
          code: res.code,
        },
      ]);
      window.setTimeout(() => dismissToast(id), res.ok ? 2600 : 4200);
    },
    [dismissToast],
  );

  // ── Silent view refreshers (housekeeping reads, not logged) ───────────────
  const refreshEntities = useCallback(async () => {
    const list = await invoke('scene.list_entities', {}, roleRef.current, false, {
      silent: true,
    });
    const entities = (list.data.entities as { id: number }[]) ?? [];
    const details: Record<number, EntityDetail> = {};
    await Promise.all(
      entities.map(async (e) => {
        const r = await invoke('scene.get_entity', { id: e.id }, roleRef.current, false, {
          silent: true,
        });
        if (r.ok) details[e.id] = r.data as unknown as EntityDetail;
      }),
    );
    setDetailMap(details);
    setTree(buildTree(details));
    return details;
  }, []);

  const refreshField = useCallback(async () => {
    const r = await invoke('crop.get_field', {}, roleRef.current, false, { silent: true });
    const crops = (r.data.crops as CropTile[]) ?? [];
    const map: Record<string, CropTile> = {};
    crops.forEach((c) => (map[tileKey(c.x, c.y)] = c));
    setFieldByKey(map);
  }, []);

  const refreshTime = useCallback(async () => {
    const r = await invoke('time.get', {}, roleRef.current, false, { silent: true });
    if (r.ok) setTime(r.data as unknown as TimeView);
  }, []);

  // ── Entity selection (logged scene.get_entity — a real user query) ────────
  const selectEntity = useCallback((id: number) => {
    setSelectedEntityId(id);
    void invoke('scene.get_entity', { id }, roleRef.current).then((r) => {
      if (r.ok) {
        const d = r.data as unknown as EntityDetail;
        setDetailMap((m) => ({ ...m, [id]: d }));
      }
    });
  }, []);

  const selectTile = useCallback((pos: TilePos) => setSelectedTile(pos), []);

  // ── The one mutation entry point ──────────────────────────────────────────
  const run = useCallback<EngineApi['run']>(
    async (name, params = {}, opts = {}) => {
      setBusy(true);
      try {
        const res = await invoke(name, params, roleRef.current, opts.dryRun ?? false);
        pushToast(res, name);
        if (res.ok && !opts.dryRun) {
          if (name.startsWith('scene.') || name.startsWith('entity.')) {
            const details = await refreshEntities();
            // Auto-select a freshly created entity; drop selection if destroyed.
            if (name === 'scene.create_entity' && typeof res.data.id === 'number') {
              selectEntity(res.data.id as number);
            } else if (selectedEntityId !== null && !details[selectedEntityId]) {
              setSelectedEntityId(null);
            }
          }
          if (name.startsWith('crop.')) await refreshField();
          if (name === 'time.advance') await refreshTime();
        }
        return res;
      } finally {
        setBusy(false);
      }
    },
    [pushToast, refreshEntities, refreshField, refreshTime, selectEntity, selectedEntityId],
  );

  // ── Initial load ──────────────────────────────────────────────────────────
  useEffect(() => {
    void (async () => {
      setTools(await listTools());
      let details = await refreshEntities();
      await refreshField();
      // The headless engine starts empty (no tmj→Scene loader yet). Seed a demo
      // world via real Tool calls so a freshly connected client sees content.
      // No-op against the pre-seeded mock (it's never empty here).
      if (Object.keys(details).length === 0) {
        await seedDemoWorld();
        details = await refreshEntities();
        await refreshField();
      }
      await refreshTime();
      // Populate the inspector with a sensible default (silent — load shouldn't
      // spam the audit log). Player entity if present, else first root.
      const initial = details[2] ?? Object.values(details)[0];
      if (initial) {
        setSelectedEntityId(initial.id);
        setDetailMap((m) => ({ ...m, [initial.id]: initial }));
      }
    })();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── Audit history subscription ────────────────────────────────────────────
  useEffect(() => {
    setHistory(getHistory());
    return subscribeHistory(() => setHistory(getHistory()));
  }, []);

  const field = useMemo(
    () =>
      Object.values(fieldByKey).sort((a, b) => a.y - b.y || a.x - b.x),
    [fieldByKey],
  );

  const selectedDetail =
    selectedEntityId !== null ? detailMap[selectedEntityId] ?? null : null;

  const api: EngineApi = {
    role,
    setRole,
    tree,
    detailMap,
    selectedEntityId,
    selectedDetail,
    selectEntity,
    time,
    field,
    fieldByKey,
    selectedTile,
    selectTile,
    tools,
    history,
    toasts,
    dismissToast,
    run,
    busy,
  };

  return <EngineContext.Provider value={api}>{children}</EngineContext.Provider>;
}

/** Access the shared engine state. Must be used inside <EngineProvider>. */
export function useEngine(): EngineApi {
  const ctx = useContext(EngineContext);
  if (!ctx) throw new Error('useEngine must be used within <EngineProvider>');
  return ctx;
}
