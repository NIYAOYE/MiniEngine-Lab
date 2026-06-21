# me_command — Command 中枢(M5)

可回滚变更基础设施:`ICommand`(execute/undo/describe)+ `CommandStack`(标准 Undo/Redo 双栈)+ 具体命令(CreateEntity / DestroyEntity / SetTransform)。

- **CPU-only**:仅依赖 `me_scene` → `me_core`,可脱离 DX12/窗口单测。
- **身份锚定**:命令持有 `me::scene::EntityId` 而非裸 handle,在 execute/undo 时 `Scene::Resolve` 为存活句柄,跨销毁/重建稳定。
- **错误模型**:不抛异常,以 `CommandResult { ok, message }` 返回;execute 失败不入栈。
- **后续(M6)**:Tool API 将变更型 Tool 经 `CommandStack` 执行,并把 `CommandResult` 包装为 JSON `ToolResult`。
