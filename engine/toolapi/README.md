# me_toolapi — 受控 Tool API 层(M6)

编辑期/自动化/未来 Agent 对场景的唯一受控变更入口。每个 Tool = name + category +
permission + JSON Schema + dryRun/run。`ToolRegistry` 跑统一流水线
(find → permission → validate → dry-run? → run → record)。Tool 只经注入的
`ToolContext`(Scene + CommandStack + 调用日志)访问引擎,变更型 Tool 构造 M5 的
`ICommand` 经 `CommandStack` 执行,天然获得 Undo/Redo。

依赖:toolapi → command → scene → core(+ nlohmann/json 边界)。不依赖 rhi/renderer。
