/**
 * Shared TypeScript contract types.
 *
 * These mirror the MiniEngine Tool API wire contract exactly (see brief §0).
 * When the mock transport is swapped for `fetch` against the headless Tool
 * server, none of these shapes change.
 */

/** Caller privilege level. Ordered Agent < Automation < Editor. */
export type Role = 'Agent' | 'Automation' | 'Editor';

/** Structured error codes returned by every Tool invocation. */
export type ToolErrorCode =
  | 'Ok'
  | 'UnknownTool'
  | 'PermissionDenied'
  | 'InvalidParams'
  | 'PreconditionFailed'
  | 'ExecutionFailed';

/** A Tool either reads state (Query) or changes it (Mutation). */
export type ToolCategory = 'Query' | 'Mutation';

/** Minimum role required to invoke a Tool. */
export type Permission = 'AgentAllowed' | 'Automation' | 'EditorOnly';

/** Uniform envelope returned by `invoke()` — success payload or failure detail. */
export interface ToolResult {
  ok: boolean;
  code: ToolErrorCode;
  message: string;
  data: Record<string, unknown>;
  invocationId: number;
}

/** Self-describing Tool metadata returned by `listTools()`. */
export interface ToolDefinition {
  name: string;
  category: ToolCategory;
  permission: Permission;
  /** JSON-schema-ish description of accepted params (display only). */
  paramsSchema: Record<string, unknown>;
  description: string;
}

export interface Vec2 {
  x: number;
  y: number;
}

/** Flat transform view returned by scene.list_entities / entity.set_transform. */
export interface EntityTransform {
  id: number;
  position: Vec2;
  rotation: number;
  scale: Vec2;
}

/** Full entity view returned by scene.get_entity (adds real hierarchy). */
export interface EntityDetail extends EntityTransform {
  parentId: number | null;
  children: number[];
}

/**
 * Tree node assembled client-side from list_entities + per-entity get_entity.
 * `label` is a FRONT-END LOCAL annotation only — the engine contract has no
 * entity names (see brief §0.4). It is never part of any ToolResult.
 */
export interface EntityNode extends EntityDetail {
  childNodes: EntityNode[];
  label?: string;
}

/** A single planted tile (crop.get_field). Empty tiles are simply absent. */
export interface CropTile {
  x: number;
  y: number;
  cropId: string;
  stage: number;
  stageName: string;
  daysInStage: number;
  watered: boolean;
  mature: boolean;
}

/** Calendar/clock view (time.get). seasonName is engine English. */
export interface TimeView {
  year: number;
  season: number;
  seasonName: string;
  dayOfSeason: number;
  minuteOfDay: number;
  hour: number;
  minute: number;
}

/**
 * Front-end call history — the honest audit source (see brief §0.5).
 * The engine's own log.read exposes only a subset (no time/role/result/duration),
 * so the audit panel records every invoke client-side instead.
 */
export interface AuditEntry {
  id: number;
  timestamp: number;
  tool: string;
  role: Role;
  params: unknown;
  result: ToolResult;
  ok: boolean;
  code: ToolErrorCode;
  durationMs: number;
}

/** Lightweight toast notification (front-end only). */
export interface ToastMessage {
  id: number;
  ok: boolean;
  title: string;
  detail?: string;
  code?: ToolErrorCode;
}
