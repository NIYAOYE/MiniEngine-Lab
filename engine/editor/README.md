# me_editor — Editor as Client(M7)

CPU-only 编辑器决策核心。`EditorController` 把 UI 意图翻译为 M6 Tool 调用,
**只经 `ToolRegistry::Invoke` 读写引擎**(读走查询 Tool、写走变更 Tool),
以此反向验证 Tool API 完备性。不依赖 ImGui/RHI,可在 WSL doctest 红绿;
ImGui 面板是 sandbox 的目视层,不在本库内。

依赖:editor → toolapi → command → scene → core + nlohmann/json。
