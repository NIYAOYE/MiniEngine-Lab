/**
 * toolClient — the single gateway between the UI and the MiniEngine Tool API.
 *
 * Two interchangeable transports, selected at build time by `VITE_USE_MOCK`:
 *
 *  - HTTP (default): `fetch` against the headless Tool server (me_toolserver,
 *    `POST /invoke` + `GET /tools`), reached through the Vite dev proxy at
 *    `/api`. This is the real engine over the wire.
 *  - Mock (`VITE_USE_MOCK=true`): a self-contained in-memory engine that honours
 *    the *real* contract (permission ranking, ToolResult shape, crop/entity
 *    semantics — brief §0). Kept for offline UI work when no server is running.
 *
 * Both transports return identical `ToolResult` / `ToolDefinition` shapes, so UI
 * components require ZERO changes. The client-side audit history (richer than the
 * engine's `log.read` subset) is recorded by the `invoke()` wrapper regardless of
 * transport.
 *
 * Note on globals: the mock holds mutable state because it *stands in for a
 * remote engine process*. The "no global mutable state" rule in CLAUDE.md
 * governs the C++ engine, not a mock of a separate server.
 */

import type {
  AuditEntry,
  CropTile,
  Permission,
  Role,
  ToolCategory,
  ToolDefinition,
  ToolErrorCode,
  ToolResult,
  Vec2,
} from '@/types';

// ───────────────────────────── Constants (no magic numbers) ────────────────

const MINUTES_PER_HOUR = 60;
const MINUTES_PER_DAY = 24 * MINUTES_PER_HOUR;
const DAYS_PER_SEASON = 28;
const SEASON_NAMES = ['Spring', 'Summer', 'Fall', 'Winter'] as const;
const SEASONS_PER_YEAR = SEASON_NAMES.length;

/** Per-crop ordered stage names. The last index is the mature/harvestable stage. */
export const CROP_DB: Readonly<Record<string, readonly string[]>> = {
  parsnip: ['seed', 'sprout', 'growing', 'mature'],
  cauliflower: ['seed', 'sprout', 'leafy', 'heading', 'mature'],
};

/** Simulated network latency so the UI shows real pending/disabled states. */
const MOCK_LATENCY_MS = 120;

// ───────────────────────────── Transport selection ─────────────────────────

/**
 * Default to the real HTTP transport; set `VITE_USE_MOCK=true` (e.g. in a
 * `.env.local`) to fall back to the in-memory mock for offline UI development.
 */
const USE_MOCK = import.meta.env.VITE_USE_MOCK === 'true';

/** Base path for the Tool server; Vite proxies `/api` → http://127.0.0.1:8080. */
const API_BASE = '/api';

/** Human label for the active transport, shown in the status bar. */
export const TRANSPORT_LABEL = USE_MOCK ? '内存 mock' : 'HTTP → 127.0.0.1:8080';

// ───────────────────────────── Permission model ────────────────────────────

const ROLE_RANK: Record<Role, number> = { Agent: 0, Automation: 1, Editor: 2 };
const PERMISSION_RANK: Record<Permission, number> = {
  AgentAllowed: 0,
  Automation: 1,
  EditorOnly: 2,
};

/** True when `role` is privileged enough to invoke a Tool of `permission`. */
export function canCall(role: Role, permission: Permission): boolean {
  return ROLE_RANK[role] >= PERMISSION_RANK[permission];
}

/** Human-readable requirement for a denied Tool, used in tooltips. */
export function requiredRoleLabel(permission: Permission): string {
  switch (permission) {
    case 'AgentAllowed':
      return '需要 Agent 权限';
    case 'Automation':
      return '需要 Automation 权限';
    case 'EditorOnly':
      return '需要 Editor 权限';
  }
}

// ───────────────────────────── Tool registry (the 13) ──────────────────────
// permission values here are the AUTHORITATIVE contract — do not change.

export const TOOLS: readonly ToolDefinition[] = [
  {
    name: 'scene.list_entities',
    category: 'Query',
    permission: 'AgentAllowed',
    paramsSchema: {},
    description: '列出场景内全部实体的扁平变换视图。',
  },
  {
    name: 'scene.get_entity',
    category: 'Query',
    permission: 'AgentAllowed',
    paramsSchema: { id: 'int' },
    description: '读取单个实体(含真实 parentId / children 层级)。',
  },
  {
    name: 'log.read',
    category: 'Query',
    permission: 'AgentAllowed',
    paramsSchema: { limit: 'int?' },
    description: '读取引擎调用日志(权威子集:无时间/角色/结果/耗时)。',
  },
  {
    name: 'crop.get_field',
    category: 'Query',
    permission: 'AgentAllowed',
    paramsSchema: {},
    description: '读取农田已种植瓦片(空瓦片不返回)。',
  },
  {
    name: 'time.get',
    category: 'Query',
    permission: 'AgentAllowed',
    paramsSchema: {},
    description: '读取游戏内年/季/日/时刻。',
  },
  {
    name: 'scene.create_entity',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { parentId: 'int?', position: 'Vec2?', rotation: 'number?', scale: 'Vec2?' },
    description: '创建新实体并返回其 id。',
  },
  {
    name: 'entity.set_transform',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { id: 'int', position: 'Vec2', rotation: 'number', scale: 'Vec2' },
    description: '设置实体的位置 / 旋转 / 缩放。',
  },
  {
    name: 'time.advance',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { minutes: 'int>=1' },
    description: '推进游戏内时间(分钟)。',
  },
  {
    name: 'crop.plant',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { tileX: 'int', tileY: 'int', cropId: 'string' },
    description: '在空瓦片种下作物(stage=0,未浇水)。',
  },
  {
    name: 'crop.water',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { tileX: 'int', tileY: 'int' },
    description: '将瓦片标记为已浇水(幂等)。',
  },
  {
    name: 'crop.advance_days',
    category: 'Mutation',
    permission: 'Automation',
    paramsSchema: { days: 'int>=1' },
    description: '推进整片农田 N 天(仅当天已浇水的作物前进一格)。',
  },
  {
    name: 'scene.destroy_entity',
    category: 'Mutation',
    permission: 'EditorOnly',
    paramsSchema: { id: 'int' },
    description: '销毁实体及其所有子孙。',
  },
  {
    name: 'crop.harvest',
    category: 'Mutation',
    permission: 'EditorOnly',
    paramsSchema: { tileX: 'int', tileY: 'int' },
    description: '收获成熟作物;成功后该瓦片变空。',
  },
];

const TOOL_BY_NAME = new Map(TOOLS.map((t) => [t.name, t]));

// ───────────────────────────── Mutable engine state ────────────────────────

interface EntityRecord {
  id: number;
  position: Vec2;
  rotation: number;
  scale: Vec2;
  parentId: number | null;
  children: number[];
}

interface EngineState {
  entities: Record<number, EntityRecord>;
  field: Record<string, CropTile>;
  time: { year: number; season: number; dayOfSeason: number; minuteOfDay: number };
  nextEntityId: number;
}

const tileKey = (x: number, y: number) => `${x},${y}`;

/** Build the initial demo world (matches the brief's example data exactly). */
function seedState(): EngineState {
  const entities: Record<number, EntityRecord> = {
    1: { id: 1, position: { x: 0, y: 0 }, rotation: 0, scale: { x: 1, y: 1 }, parentId: null, children: [2, 3, 6] },
    2: { id: 2, position: { x: 5, y: 3 }, rotation: 0, scale: { x: 1, y: 1 }, parentId: 1, children: [] },
    3: { id: 3, position: { x: 12, y: 4 }, rotation: 0, scale: { x: 1, y: 1 }, parentId: 1, children: [4, 5] },
    4: { id: 4, position: { x: 12, y: 6 }, rotation: 90, scale: { x: 1, y: 1 }, parentId: 3, children: [] },
    5: { id: 5, position: { x: 13, y: 4 }, rotation: 0, scale: { x: 1, y: 1 }, parentId: 3, children: [] },
    6: { id: 6, position: { x: 2, y: 8 }, rotation: 0, scale: { x: 3, y: 3 }, parentId: 1, children: [] },
  };

  const field: Record<string, CropTile> = {};
  const plant = (x: number, y: number, cropId: string, stage: number, watered: boolean) => {
    const stages = CROP_DB[cropId];
    field[tileKey(x, y)] = {
      x,
      y,
      cropId,
      stage,
      stageName: stages[stage],
      daysInStage: 1,
      watered,
      mature: stage === stages.length - 1,
    };
  };
  // Exact seed field from brief §六.
  plant(0, 0, 'parsnip', 1, true); // sprout
  plant(1, 0, 'parsnip', 2, true); // growing
  plant(2, 0, 'parsnip', 3, true); // mature
  plant(0, 1, 'cauliflower', 2, true); // leafy
  plant(2, 1, 'cauliflower', 1, false); // sprout, unwatered
  plant(1, 2, 'cauliflower', 4, true); // mature

  return {
    entities,
    field,
    time: { year: 1, season: 0, dayOfSeason: 5, minuteOfDay: 8 * MINUTES_PER_HOUR + 20 },
    nextEntityId: 7,
  };
}

let state: EngineState = seedState();
let invocationCounter = 0;

// ───────────────────────────── View builders ───────────────────────────────

function timeView() {
  const { year, season, dayOfSeason, minuteOfDay } = state.time;
  return {
    year,
    season,
    seasonName: SEASON_NAMES[season],
    dayOfSeason,
    minuteOfDay,
    hour: Math.floor(minuteOfDay / MINUTES_PER_HOUR),
    minute: minuteOfDay % MINUTES_PER_HOUR,
  };
}

function transformView(e: EntityRecord) {
  return {
    id: e.id,
    position: { ...e.position },
    rotation: e.rotation,
    scale: { ...e.scale },
  };
}

function cropView(t: CropTile): CropTile {
  return { ...t };
}

// ───────────────────────────── Param validation helpers ────────────────────

/** Thrown internally by handlers; caught by invoke() and turned into ToolResult. */
class ToolError extends Error {
  constructor(public code: ToolErrorCode, message: string) {
    super(message);
  }
}

const isInt = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v);
const isNum = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);

function requireInt(params: Record<string, unknown>, key: string): number {
  const v = params[key];
  if (!isInt(v)) throw new ToolError('InvalidParams', `参数 "${key}" 必须为整数。`);
  return v;
}

function requireVec2(params: Record<string, unknown>, key: string): Vec2 {
  const v = params[key] as Vec2 | undefined;
  if (!v || !isNum(v.x) || !isNum(v.y)) {
    throw new ToolError('InvalidParams', `参数 "${key}" 必须为 {x:number, y:number}。`);
  }
  return { x: v.x, y: v.y };
}

function requireNum(params: Record<string, unknown>, key: string): number {
  const v = params[key];
  if (!isNum(v)) throw new ToolError('InvalidParams', `参数 "${key}" 必须为数字。`);
  return v;
}

function getEntityOrFail(id: number): EntityRecord {
  const e = state.entities[id];
  if (!e) throw new ToolError('PreconditionFailed', `实体 ${id} 不存在。`);
  return e;
}

// ───────────────────────────── Tool handlers ───────────────────────────────
// Each returns the success `data` payload, or throws ToolError.

type Handler = (params: Record<string, unknown>) => Record<string, unknown>;

const HANDLERS: Record<string, Handler> = {
  'scene.list_entities': () => {
    const entities = Object.values(state.entities)
      .sort((a, b) => a.id - b.id)
      .map(transformView);
    return { count: entities.length, entities };
  },

  'scene.get_entity': (p) => {
    const id = requireInt(p, 'id');
    const e = getEntityOrFail(id);
    return {
      ...transformView(e),
      parentId: e.parentId,
      children: [...e.children],
    };
  },

  'log.read': (p) => {
    const limit = p.limit === undefined ? history.length : requireInt(p, 'limit');
    // The engine's authoritative subset: id/tool/params/dryRun/ok/code/message only.
    const invocations = history
      .slice(-Math.max(0, limit))
      .map((h) => ({
        id: h.id,
        tool: h.tool,
        params: h.params,
        dryRun: false,
        ok: h.ok,
        code: h.code,
        message: h.result.message,
      }));
    return { count: invocations.length, invocations };
  },

  'crop.get_field': () => {
    const crops = Object.values(state.field)
      .sort((a, b) => a.y - b.y || a.x - b.x)
      .map(cropView);
    return { crops };
  },

  'time.get': () => ({ ...timeView() }),

  'scene.create_entity': (p) => {
    const id = state.nextEntityId++;
    const parentId = p.parentId === undefined ? null : requireInt(p, 'parentId');
    const position = p.position === undefined ? { x: 0, y: 0 } : requireVec2(p, 'position');
    const rotation = p.rotation === undefined ? 0 : requireNum(p, 'rotation');
    const scale = p.scale === undefined ? { x: 1, y: 1 } : requireVec2(p, 'scale');
    if (parentId !== null && !state.entities[parentId]) {
      throw new ToolError('PreconditionFailed', `父实体 ${parentId} 不存在。`);
    }
    state.entities[id] = { id, position, rotation, scale, parentId, children: [] };
    if (parentId !== null) state.entities[parentId].children.push(id);
    return { id };
  },

  'entity.set_transform': (p) => {
    const id = requireInt(p, 'id');
    const e = getEntityOrFail(id);
    e.position = requireVec2(p, 'position');
    e.rotation = requireNum(p, 'rotation');
    e.scale = requireVec2(p, 'scale');
    return { ...transformView(e) };
  },

  'scene.destroy_entity': (p) => {
    const id = requireInt(p, 'id');
    const e = getEntityOrFail(id);
    // Detach from parent.
    if (e.parentId !== null && state.entities[e.parentId]) {
      const siblings = state.entities[e.parentId].children;
      const idx = siblings.indexOf(id);
      if (idx >= 0) siblings.splice(idx, 1);
    }
    // Destroy the whole subtree (depth-first).
    const stack = [id];
    while (stack.length) {
      const cur = stack.pop()!;
      const rec = state.entities[cur];
      if (!rec) continue;
      stack.push(...rec.children);
      delete state.entities[cur];
    }
    return {};
  },

  'time.advance': (p) => {
    const minutes = requireInt(p, 'minutes');
    if (minutes < 1) throw new ToolError('InvalidParams', '参数 "minutes" 必须 ≥ 1。');

    let total = state.time.minuteOfDay + minutes;
    let daysPassed = 0;
    let { dayOfSeason, season, year } = state.time;
    while (total >= MINUTES_PER_DAY) {
      total -= MINUTES_PER_DAY;
      daysPassed += 1;
      dayOfSeason += 1;
      if (dayOfSeason > DAYS_PER_SEASON) {
        dayOfSeason = 1;
        season += 1;
        if (season >= SEASONS_PER_YEAR) {
          season = 0;
          year += 1;
        }
      }
    }
    state.time = { year, season, dayOfSeason, minuteOfDay: total };
    return {
      step: { minutes, daysPassed },
      time: { ...timeView() },
    };
  },

  'crop.plant': (p) => {
    const tileX = requireInt(p, 'tileX');
    const tileY = requireInt(p, 'tileY');
    const cropId = p.cropId;
    if (typeof cropId !== 'string' || !(cropId in CROP_DB)) {
      throw new ToolError('InvalidParams', `未知作物 "${String(cropId)}"。`);
    }
    const key = tileKey(tileX, tileY);
    if (state.field[key]) {
      throw new ToolError('PreconditionFailed', `瓦片 (${tileX},${tileY}) 已被占用。`);
    }
    const tile: CropTile = {
      x: tileX,
      y: tileY,
      cropId,
      stage: 0,
      stageName: CROP_DB[cropId][0],
      daysInStage: 0,
      watered: false,
      mature: false,
    };
    state.field[key] = tile;
    return { ...cropView(tile) };
  },

  'crop.water': (p) => {
    const tileX = requireInt(p, 'tileX');
    const tileY = requireInt(p, 'tileY');
    const tile = state.field[tileKey(tileX, tileY)];
    if (!tile) throw new ToolError('PreconditionFailed', `瓦片 (${tileX},${tileY}) 没有作物。`);
    tile.watered = true; // idempotent
    return { ...cropView(tile) };
  },

  'crop.advance_days': (p) => {
    const days = requireInt(p, 'days');
    if (days < 1) throw new ToolError('InvalidParams', '参数 "days" 必须 ≥ 1。');
    for (let d = 0; d < days; d++) {
      for (const tile of Object.values(state.field)) {
        if (tile.mature) continue;
        const stages = CROP_DB[tile.cropId];
        if (tile.watered) {
          tile.stage += 1;
          tile.stageName = stages[tile.stage];
          tile.daysInStage = 0;
          tile.mature = tile.stage === stages.length - 1;
          tile.watered = false; // water consumed for the day's growth
        } else {
          tile.daysInStage += 1; // stalls (not watered)
        }
      }
    }
    const crops = Object.values(state.field)
      .sort((a, b) => a.y - b.y || a.x - b.x)
      .map(cropView);
    return { advanced: days, crops };
  },

  'crop.harvest': (p) => {
    const tileX = requireInt(p, 'tileX');
    const tileY = requireInt(p, 'tileY');
    const key = tileKey(tileX, tileY);
    const tile = state.field[key];
    if (!tile) throw new ToolError('PreconditionFailed', `瓦片 (${tileX},${tileY}) 没有作物。`);
    if (!tile.mature) throw new ToolError('PreconditionFailed', '作物尚未成熟,无法收获。');
    delete state.field[key];
    return { itemId: tile.cropId, count: 1 };
  },
};

// ───────────────────────────── Client-side audit history ───────────────────

const history: AuditEntry[] = [];
const listeners = new Set<() => void>();

function notify() {
  listeners.forEach((fn) => fn());
}

/** Subscribe to audit-history changes. Returns an unsubscribe fn. */
export function subscribeHistory(fn: () => void): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

/** Snapshot of the recorded call history (most-recent last). */
export function getHistory(): AuditEntry[] {
  return [...history];
}

function pushHistory(entry: AuditEntry) {
  history.push(entry);
  notify();
}

// Seed a few realistic past calls so the audit table is meaningful on load
// (brief §八). These use real permissions and ToolResult shapes.
(function seedHistory() {
  const base = Date.now() - 1000 * 60 * 6;
  const seed = (
    offset: number,
    tool: string,
    role: Role,
    params: unknown,
    result: ToolResult,
    durationMs: number,
  ) => {
    pushHistory({
      id: ++invocationCounter,
      timestamp: base + offset,
      tool,
      role,
      params,
      result,
      ok: result.ok,
      code: result.code,
      durationMs,
    });
  };

  seed(
    0,
    'crop.water',
    'Editor',
    { tileX: 0, tileY: 0 },
    {
      ok: true,
      code: 'Ok',
      message: 'watered (0,0)',
      data: { x: 0, y: 0, cropId: 'parsnip', stage: 1, stageName: 'sprout', watered: true, mature: false },
      invocationId: invocationCounter + 1,
    },
    2.4,
  );
  seed(
    1000 * 42,
    'time.advance',
    'Automation',
    { minutes: 60 },
    {
      ok: true,
      code: 'Ok',
      message: 'advanced 60m',
      data: { step: { minutes: 60, daysPassed: 0 }, time: timeView() },
      invocationId: invocationCounter + 1,
    },
    1.1,
  );
  seed(
    1000 * 95,
    'crop.harvest',
    'Editor',
    { tileX: 2, tileY: 0 },
    {
      ok: true,
      code: 'Ok',
      message: 'harvested parsnip',
      data: { itemId: 'parsnip', count: 1 },
      invocationId: invocationCounter + 1,
    },
    3.7,
  );
  seed(
    1000 * 150,
    'crop.harvest',
    'Agent',
    { tileX: 2, tileY: 0 },
    {
      ok: false,
      code: 'PermissionDenied',
      message: 'crop.harvest 需要 Editor 权限(当前 Agent)。',
      data: { required: 'EditorOnly', role: 'Agent' },
      invocationId: invocationCounter + 1,
    },
    0.6,
  );
})();

// ───────────────────────────── Public API ──────────────────────────────────

const delay = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

/** Deep snapshot used to roll back state after a dry-run. */
function snapshot() {
  return JSON.stringify({
    entities: state.entities,
    field: state.field,
    time: state.time,
    nextEntityId: state.nextEntityId,
  });
}
function restore(snap: string) {
  const s = JSON.parse(snap) as Omit<EngineState, never>;
  state.entities = s.entities;
  state.field = s.field;
  state.time = s.time;
  state.nextEntityId = s.nextEntityId;
}

export interface InvokeOptions {
  /**
   * Client-side only: when true the call is NOT recorded in the audit history.
   * Used for housekeeping view refreshes so the log isn't flooded. Has no
   * effect on the wire request when swapped to fetch.
   */
  silent?: boolean;
}

/**
 * MOCK transport — validates permission + params, mutates in-memory engine
 * state, and returns a real-shaped ToolResult. dry-run executes the handler then
 * rolls back, so callers can preview effects without committing.
 */
async function mockInvoke(
  name: string,
  params: Record<string, unknown>,
  role: Role,
  dryRun: boolean,
): Promise<ToolResult> {
  await delay(MOCK_LATENCY_MS);

  const invocationId = ++invocationCounter;
  const tool = TOOL_BY_NAME.get(name);
  if (!tool) {
    return { ok: false, code: 'UnknownTool', message: `未知 Tool "${name}"。`, data: {}, invocationId };
  }
  if (!canCall(role, tool.permission)) {
    // Permission decided BEFORE any state change (matches engine whitelist).
    return {
      ok: false,
      code: 'PermissionDenied',
      message: `${name} ${requiredRoleLabel(tool.permission)}(当前 ${role})。`,
      data: { required: tool.permission, role },
      invocationId,
    };
  }
  const snap = dryRun ? snapshot() : null;
  try {
    const data = HANDLERS[name](params);
    return {
      ok: true,
      code: 'Ok',
      message: dryRun ? `${name} dry-run 预览成功(未提交)。` : `${name} 执行成功。`,
      data,
      invocationId,
    };
  } catch (err) {
    const te = err instanceof ToolError ? err : new ToolError('ExecutionFailed', String(err));
    return { ok: false, code: te.code, message: te.message, data: {}, invocationId };
  } finally {
    if (snap !== null) restore(snap); // dry-run never commits
  }
}

/**
 * HTTP transport — POST the request to me_toolserver via the Vite dev proxy.
 * The server returns a `ToolResult` JSON with HTTP 200 even for business errors,
 * so the only failure to synthesize here is a transport (network) error.
 */
async function httpInvoke(
  name: string,
  params: Record<string, unknown>,
  role: Role,
  dryRun: boolean,
): Promise<ToolResult> {
  try {
    const res = await fetch(`${API_BASE}/invoke`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, params, role, dryRun }),
    });
    return (await res.json()) as ToolResult;
  } catch (err) {
    return {
      ok: false,
      code: 'ExecutionFailed',
      message: `网络错误:无法连接 Tool 服务器(${API_BASE})。请确认 toolserver_app 正在运行。${String(err)}`,
      data: {},
      invocationId: -1,
    };
  }
}

/**
 * Invoke a Tool by name through the selected transport, then record the call in
 * the client-side audit history (unless `silent`). The wire request shape
 * `{ name, params, role, dryRun }` is identical for both transports.
 */
export async function invoke(
  name: string,
  params: Record<string, unknown> = {},
  role: Role = 'Editor',
  dryRun = false,
  opts: InvokeOptions = {},
): Promise<ToolResult> {
  const started = performance.now();
  const result = USE_MOCK
    ? await mockInvoke(name, params, role, dryRun)
    : await httpInvoke(name, params, role, dryRun);
  const durationMs = Math.round((performance.now() - started) * 10) / 10;

  if (!opts.silent) {
    pushHistory({
      id: ++invocationCounter,
      timestamp: Date.now(),
      tool: name,
      role,
      params,
      result,
      ok: result.ok,
      code: result.code,
      durationMs,
    });
  }

  return result;
}

/**
 * List the available Tools and their contract metadata.
 *
 * The HTTP `/tools` endpoint returns `{ name, category, permission, paramsSchema }`
 * (no `description` — those are front-end-local UI strings), so we re-attach the
 * local description by name. Falls back to the bundled 13-Tool contract if the
 * server is unreachable, so the overview panel still renders.
 */
export async function listTools(): Promise<ToolDefinition[]> {
  if (USE_MOCK) {
    await delay(MOCK_LATENCY_MS / 2);
    return TOOLS.map((t) => ({ ...t }));
  }
  try {
    const res = await fetch(`${API_BASE}/tools`);
    const raw = (await res.json()) as Array<{
      name: string;
      category: ToolCategory;
      permission: Permission;
      paramsSchema?: Record<string, unknown>;
    }>;
    return raw.map((t) => ({
      name: t.name,
      category: t.category,
      permission: t.permission,
      paramsSchema: t.paramsSchema ?? {},
      description: TOOL_BY_NAME.get(t.name)?.description ?? '',
    }));
  } catch (err) {
    console.error('listTools 失败,回退本地契约元数据:', err);
    return TOOLS.map((t) => ({ ...t }));
  }
}

/**
 * Seed an equivalent-to-mock demo world on a FRESH (empty) engine.
 *
 * The headless server starts with an empty scene + field (no tmj→Scene loader
 * yet), so a freshly connected client would see nothing. This populates a small
 * demo via real Tool calls so "connect = content". No-op in mock mode (the mock
 * starts pre-seeded), because callers only invoke it when the world is empty.
 *
 * Contract gaps surfaced by going live (recorded, not worked around):
 *  - `scene.create_entity` takes no params and there is no reparent Tool, so the
 *    seeded entities are FLAT (no World→Player hierarchy). Friendly labels still
 *    map by id (1..6) because a fresh scene assigns ids in creation order.
 *  - Crop stages can't be set directly (`crop.advance_days` is field-wide), so we
 *    plant + water + advance one day for visible stage/watered variety.
 */
export async function seedDemoWorld(): Promise<void> {
  const role: Role = 'Editor';
  const silent: InvokeOptions = { silent: true };

  // 6 flat entities; ids 1..6 (creation order) map to LOCAL_LABELS.
  const transforms: Array<{ position: Vec2; rotation: number; scale: Vec2 }> = [
    { position: { x: 0, y: 0 }, rotation: 0, scale: { x: 1, y: 1 } },
    { position: { x: 5, y: 3 }, rotation: 0, scale: { x: 1, y: 1 } },
    { position: { x: 12, y: 4 }, rotation: 0, scale: { x: 1, y: 1 } },
    { position: { x: 12, y: 6 }, rotation: 90, scale: { x: 1, y: 1 } },
    { position: { x: 13, y: 4 }, rotation: 0, scale: { x: 1, y: 1 } },
    { position: { x: 2, y: 8 }, rotation: 0, scale: { x: 3, y: 3 } },
  ];
  for (const t of transforms) {
    const created = await invoke('scene.create_entity', {}, role, false, silent);
    const id = created.data?.id;
    if (typeof id === 'number') {
      await invoke('entity.set_transform', { id, ...t }, role, false, silent);
    }
  }

  // A handful of crops; water three and advance one day for visible variety.
  const plant = (tileX: number, tileY: number, cropId: string) =>
    invoke('crop.plant', { tileX, tileY, cropId }, role, false, silent);
  const water = (tileX: number, tileY: number) =>
    invoke('crop.water', { tileX, tileY }, role, false, silent);
  await plant(0, 0, 'parsnip');
  await plant(1, 0, 'parsnip');
  await plant(2, 0, 'parsnip');
  await plant(0, 1, 'cauliflower');
  await plant(1, 1, 'cauliflower');
  await water(0, 0);
  await water(1, 0);
  await water(2, 0);
  await invoke('crop.advance_days', { days: 1 }, role, false, silent);
}
