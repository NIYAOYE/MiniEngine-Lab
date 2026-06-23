# Farm Editor — MiniEngine Tool-API front-end

A runnable, interactive desktop editor front-end for the MiniEngine Tool API.
The engine exposes its capabilities as **13 Tools** (Query / Mutation, each with a
permission level and JSON params/result). Every interaction in this UI goes
through a single `invoke(name, params, role, dryRun)` call.

> **Transport is mocked right now.** `src/lib/toolClient.ts` keeps an in-memory
> engine that honours the *real* contract (permission ladder, `ToolResult`
> shape, crop/entity semantics, audit = client call history). To go live, swap
> the bodies of `invoke()` / `listTools()` for `fetch` against
> `http://127.0.0.1:8080` — **no UI component changes required.**

## Run

```bash
cd tools/editor-frontend
npm install
npm run dev      # → http://localhost:5173
```

Other scripts: `npm run build` (typecheck + production build), `npm run typecheck`.

## Stack

React 18 · TypeScript · Vite · Tailwind CSS · lucide-react. No backend, no large
UI component library.

## Contract fidelity (the important part)

- **Permissions** `Agent < Automation < Editor`. A Tool is greyed out (with a
  tooltip) when the selected role is below its permission. Even if a disabled
  button were forced, `invoke()` returns `PermissionDenied` before mutating —
  the gate is enforced in the client, mirroring the engine whitelist.
- **`ToolResult`** is always `{ ok, code, message, data, invocationId }`.
- **Crops** are *not* entities. The field comes from `crop.get_field`
  (per `tileX/tileY`); empty tiles are simply absent and drawn as "未种植".
  Stages are per-crop (`parsnip`: seed/sprout/growing/mature;
  `cauliflower`: seed/sprout/leafy/heading/mature). There is **no** harvested
  stage — `crop.harvest` empties the tile.
- **Entities** are only `id` + transform + real `parentId`/`children`. Friendly
  names (`Player`, `Barn`) are **front-end-local** annotations (see
  `src/data/labels.ts`), never part of the contract. The Components section and
  Renderable/Collider toggles are disabled `future` placeholders.
- **Audit** uses the front-end call history so every column (client time, role,
  params, result, duration) is honest; the engine's `log.read` is noted as the
  authoritative subset.
- **Undo/Redo** are disabled `future` (no `edit.undo/redo` Tool yet).

## The 13 Tools

| Tool | Category | Permission |
|------|----------|------------|
| scene.list_entities | Query | AgentAllowed |
| scene.get_entity | Query | AgentAllowed |
| log.read | Query | AgentAllowed |
| crop.get_field | Query | AgentAllowed |
| time.get | Query | AgentAllowed |
| scene.create_entity | Mutation | Automation |
| entity.set_transform | Mutation | Automation |
| time.advance | Mutation | Automation |
| crop.plant | Mutation | Automation |
| crop.water | Mutation | Automation |
| crop.advance_days | Mutation | Automation |
| scene.destroy_entity | Mutation | EditorOnly |
| crop.harvest | Mutation | EditorOnly |

## Implemented interactions

- Role view switch → live permission gating across all action buttons.
- Entity tree (real hierarchy via `scene.list_entities` + per-node
  `scene.get_entity`): expand/collapse, select, search, create, destroy.
- Inspector: read-only id/parentId/children + copy id; editable Transform via
  `entity.set_transform`; future component placeholders.
- Time panel: `time.advance` (slider + input), day/season/year rollover.
- Farm grid (`crop.get_field`): select tile, grid toggle, stage/crop colouring,
  legend; SVG crop sprites that change by species and stage; watered/mature cues.
- Crop ops: `crop.plant`, `crop.water`, `crop.advance_days` (whole field),
  `crop.harvest` (mature only) with toasts and live field refresh.
- Audit/history panel with time/tool/status filters; honest per-call rows.
- Tool overview (`listTools`) with search, permission tabs, role-linked dimming.
- Dry-run supported by `invoke()` (executes then rolls back).
