# me_toolserver — HTTP 传输层(M9.1)

将进程内 `ToolRegistry` 暴露为本地 HTTP+JSON 服务,供网页编辑器 UI 和未来 LLM agent 调用。
网页 UI 与 agent 走同一受控边界:权限校验、JSON Schema 验证、审计日志全复用 M6 流水线。

依赖:`me_toolserver → me_toolapi + me_domain + me_scene + me_command + me_core + nlohmann/json + cpp_httplib`。
单向;不被任何引擎模块反向依赖(应用层装配)。

## 模块责任

### ToolDispatcher

纯逻辑调度核心(`ToolDispatcher.h / .cpp`)。

- 持有 `ToolContext&`(Scene/CommandStack/Log/TimeSystem/FarmField)与 `ToolRegistry&`。
- 两个公开方法:`HandleInvoke(jsonBody)` 和 `HandleListTools()`——均为 `std::string → std::string`,无 socket 代码。
- 内部持有一把 `std::mutex`,串行化每次调用(共享 Scene/Farm 原子性)。
- **完全无 socket / cpp-httplib 代码**,可 doctest 红绿。15 个专项 doctest 覆盖合法 invoke、dryRun 零副作用、6 种错误路径、role 解析、HandleListTools 元数据、域能力贯通。

### HttpToolServer

cpp-httplib 薄壳(`HttpToolServer.h / .cpp`)。

- 构造时接受 `ToolDispatcher&`、绑定地址/端口、静态根目录(可选)。
- `Run()` 注册三路由并 `listen()` 阻塞主线程;`Stop()` 干净退出(SIGINT)。
- `httplib::Server` 以前置声明 + pImpl 封死在 `.cpp` 内(`~HttpToolServer` 在 `.cpp` out-of-line),不外泄 httplib.h。
- 逻辑全部委托给 ToolDispatcher;薄到无需自动化测试,手动 curl 冒烟即可。

### toolserver_app

无头可执行(`apps/toolserver/main.cpp`)。

构造 Scene / CommandStack / ToolInvocationLog / TimeSystem / FarmField,加载资产,
组装 ToolContext → ToolDispatcher → HttpToolServer,跑 HTTP 服务直到 SIGINT。
无窗口,不链接 RHI/DX12。

## HTTP 协议

全部 JSON。服务器同源服务静态文件(为前端免 CORS,可选)。

| 方法 + 路径 | 请求体 | 响应体 | 用途 |
|---|---|---|---|
| `POST /invoke` | `{"name": string, "params": object?, "role": string?, "dryRun": bool?}` | `{"ok": bool, "code": string, "message": string, "data": object, "invocationId": number}` | 通用 Tool 调用,镜像 `ToolRegistry::Invoke` |
| `GET /tools` | — | `[{"name": string, "category": string, "permission": string, "paramsSchema": object}]` | 列出全部 Tool 元数据;前端据此自动生成表单、按权限灰按钮 |
| `GET /*`(静态) | — | 文件内容 | 服务前端静态资源;`staticRoot` 为空时禁用 |

### 约定

- `role` 缺省 `"Editor"`(本地编辑器全权)。合法值:`"Agent"` / `"Automation"` / `"Editor"`。非法字符串 → `{ok:false, code:"InvalidParams", message:"unknown role"}`。
- `params` 缺省空对象;`dryRun` 缺省 `false`。
- **业务错误 HTTP 200**:body 非法 JSON / 缺 `name` / 未知 role 等均返回 `{ok:false, code, message}` 且 HTTP 200。HTTP 4xx/5xx 仅用于协议级问题(错误 HTTP 方法、服务器内部异常)。
- `code` 为 `ToolErrorCode` 稳定字符串:`Ok` / `UnknownTool` / `PermissionDenied` / `InvalidParams` / `PreconditionFailed` / `ExecutionFailed`。

## 运行

```bash
./build-wsl/bin/toolserver_app [port] [staticRoot]
# 例:
./build-wsl/bin/toolserver_app           # 监听 127.0.0.1:8080,无静态根
./build-wsl/bin/toolserver_app 9090      # 端口 9090
./build-wsl/bin/toolserver_app 8080 tools/editor-frontend/dist  # + 静态前端
```

- `time.json` / `crops.json` 缺失或非法 → 打错误日志并退出(非零码)。
- 场景文件缺失 → 打警告后以**空场景**继续(可立即用 `scene.create_entity` 填入内容)。
- SIGINT(Ctrl+C)→ 干净退出。

## curl 冒烟示例

```bash
# 创建实体
curl -s -XPOST localhost:8080/invoke -d '{"name":"scene.create_entity"}'

# 列出实体
curl -s -XPOST localhost:8080/invoke -d '{"name":"scene.list_entities"}'

# 列出所有 Tool 元数据
curl -s localhost:8080/tools
```

期望响应示例:

```json
// scene.create_entity
{"ok":true,"code":"Ok","message":"","data":{"entityId":1},"invocationId":1}

// scene.list_entities(创建后)
{"ok":true,"code":"Ok","message":"","data":{"entities":[{"entityId":1}]},"invocationId":2}
```

## 与前端衔接

现有 `tools/editor-frontend`(React18 + TypeScript + Vite + Tailwind)使用 mock 传输层。
对接 HTTP 服务只需修改 `src/lib/toolClient.ts`:

```typescript
// 当前:mock 内存传输
// 替换为:
export async function invoke(name: string, params?: object, options?: InvokeOptions) {
  const res = await fetch('http://127.0.0.1:8080/invoke', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name, params, ...options }),
  });
  return res.json();
}

export async function listTools() {
  const res = await fetch('http://127.0.0.1:8080/tools');
  return res.json();
}
```

UI 组件(作物面板、实体面板、时间面板、审计日志)零改动——它们只调 `invoke()`/`listTools()`。

## 安全边界

- 仅绑定 `127.0.0.1`(回环),不对外网暴露。
- 无鉴权、无 HTTPS——本地开发工具定位,非生产服务。
- 默认 `role` 为 `"Editor"`(全权);生产化前须在此层补鉴权与 role 限制。
- 端口以具名 `constexpr kDefaultPort = 8080` 表示,可由 argv[1] 覆盖。

详见 [ADR 0008](../../docs/architecture/0008-m9-1-toolserver-http-transport.md)。
