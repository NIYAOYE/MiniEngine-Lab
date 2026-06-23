# me_toolapi — 受控 Tool API 层(M6)

编辑期/自动化/未来 Agent 对场景的唯一受控变更入口。每个 Tool = name + category +
permission + JSON Schema + dryRun/run。`ToolRegistry` 跑统一流水线
(find → permission → validate → dry-run? → run → record)。Tool 只经注入的
`ToolContext`(Scene + CommandStack + 调用日志 + 可选 TimeSystem*/FarmField*/Inventory*)
访问引擎,变更型 Tool 构造 M5 的 `ICommand` 经 `CommandStack` 执行,天然获得 Undo/Redo。

依赖:toolapi → command → scene → core(+ nlohmann/json 边界)。不依赖 rhi/renderer。

## Builtin Tool 清单(共 16 个)

| Tool | 权限 | 里程碑 |
|------|------|--------|
| `scene.list_entities` | AgentAllowed | M6 |
| `scene.get_entity` | AgentAllowed | M6 |
| `log.read` | AgentAllowed | M6 |
| `scene.create_entity` | Automation | M6 |
| `entity.set_transform` | Automation | M6 |
| `scene.destroy_entity` | EditorOnly | M6 |
| `time.get` | AgentAllowed | M8.1 |
| `time.advance` | Automation | M8.1 |
| `crop.get_field` | AgentAllowed | M8.2 |
| `crop.plant` | Automation | M8.2 |
| `crop.water` | Automation | M8.2 |
| `crop.advance_days` | Automation | M8.2 |
| `crop.harvest` | EditorOnly | M8.2 |
| `inventory.get` | AgentAllowed | M8.3 |
| `inventory.add` | Automation | M8.3 |
| `inventory.remove` | EditorOnly | M8.3 |
