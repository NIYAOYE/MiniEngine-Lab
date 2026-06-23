# ADR 0008:M9.1 无头 Tool 服务器 HTTP 传输层

- 日期:2026-06-23
- 状态:已采纳
- 相关:[ADR 0004 ToolAPI](0004-m6-toolapi.md)、[ADR 0007 作物生长](0007-m8-2-crop-growth.md)、[M9.1 设计](../superpowers/specs/2026-06-22-toolserver-http-transport-design.md)

## 背景

M6~M8.2 的 Tool API 以 C++ 进程内调用为唯一入口:ImGui 面板和单测能驱动 13 个 Tool,但任何外部进程(网页编辑器、脚本、未来 LLM agent)均无法触达。M9.1 在不改动现有 DX12 sandbox 的前提下,将 Tool API 暴露为本地 HTTP+JSON 服务——最小化地打通"外部进程 → 受控引擎能力"通路。

## 决策

1. **Tool API 暴露为本地 HTTP+JSON = Agent-ready 传输层首切**。
   人类网页 UI 与未来 LLM agent 走同一受控边界(权限/Schema/审计全复用 M6 ToolRegistry 流水线),无需两套接口。

2. **无头 Tool 服务器(非嵌入 DX12)**。
   浏览器从 JSON 数据自画场景视图;省去渲染主循环的跨线程投递,最快落地;现有 DX12 sandbox 原样保留为试玩,两者独立运行互不干扰。

3. **`cpp-httplib` 单头 + FetchContent 钉版 v0.18.3**。
   契合最小依赖原则;单头无子库,构建无侵入;其内部使用线程/C++ 异常属第三方实现细节,不违反「我方代码不用异常」约定。

4. **ToolDispatcher(纯逻辑核心)与 HttpToolServer(socket 薄壳)剖分**。
   `ToolDispatcher` 只做 `std::string → std::string`,持有 `ToolContext` 与 `ToolRegistry` 引用,完全无 socket 代码,可 doctest 红绿覆盖所有 Tool 逻辑分支。`HttpToolServer` 只做 HTTP 路由转发,薄到无需自动化测试,手动 curl 冒烟即可。延续 M7「EditorController(可测)vs ImGui(封死边界)」的剖分惯例。

5. **串行锁:一把 `std::mutex` 串行化每次 Tool 调用**。
   cpp-httplib 每请求一线程,多请求并发共享同一 Scene/FarmField。以最小机制(单锁全串行)保证调用原子——编辑器调用频率低,锁竞争可忽略;任何 create/destroy 操作不被穿插是正确性底线。

6. **业务错误走 ToolResult 模型 + HTTP 200;HTTP 状态码只表达协议级问题**。
   请求体非法 JSON、缺 `name`、未知 role、权限不足等统一返回 `{ok:false, code, message}` 且 HTTP 200;仅错误 HTTP 方法或服务器内部崩溃才使用 4xx/5xx。错误码复用既有 `ToolErrorCode` 稳定字符串(如 `UnknownTool`/`PermissionDenied`/`InvalidParams`)。

7. **仅绑 `127.0.0.1`、默认 Editor 全权;鉴权留后续**。
   本地开发工具定位;`kBindHost`/`kDefaultPort=8080` 为具名 `constexpr`,端口可由 argv[1] 覆盖;明确文档注明非生产服务、无鉴权/无 HTTPS。

8. **`GET /tools` 暴露 Tool 元数据(name/category/permission/paramsSchema)**。
   前端从接口数据驱动生成表单、按权限灰按钮,不在 UI 侧硬编码 Tool 列表;同时为未来 LLM agent 提供可机读的 Tool 清单(与 OpenAI function-calling 格式类似)。

## 实现取舍(本切片范围)

- **空场景启动**:当前无 tmj→Scene 实体加载器(`LoadTiledMap` 加载瓦片层,不重建 Scene 实体),`toolserver_app` 以空场景启动。场景种子(从 tmj 预填实体)留后续切片;前端可立即通过 `scene.create_entity` 填入内容。
- **time.json / crops.json 必选**:两配置缺失或非法则打错误日志并退出(返回非零码)——无配置 time/crop Tool 无法工作;场景文件缺失则打警告后以空场景继续。
- **ME_LOG 单字符串适配**:项目 `ME_LOG` 接受单字符串参数,内联手写 `"key=" + value` 拼接;无流式操作符。
- **`scene.destroy_entity` 参数确认为 `id`**:经 `ITool.params_schema` 字段核查,destroy 参数键名为 `"id"`(非 `"entity_id"`);测试断言与实现一致。

## 后果

- 208/208 WSL doctest 全绿(193 基线 + 15 条 ToolDispatcher 新用例);`HttpToolServer` 薄壳边界手动 curl 冒烟验证。
- 前端 `tools/editor-frontend` 只需把 `src/lib/toolClient.ts` 中的 mock `invoke()`/`listTools()` 替换为 `fetch http://127.0.0.1:8080`——UI 组件零改动。
- 鉴权、SSE/WS 推送、实时 DX12、前端本体均不在本切片范围内(YAGNI)。
