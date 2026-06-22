# Tool API HTTP 传输层(无头 Tool 服务器)— 设计文档

- **日期**:2026-06-22
- **里程碑**:M9 切片 1 —— Agent-ready 传输层(为外部语言编辑器 UI + 未来 LLM agent 提供统一入口)
- **状态**:已确认,待写实现计划
- **前置**:M6 ToolAPI(ToolRegistry/ToolContext/ToolResult/权限/审计)、M8.1 时间、M8.2 作物(共 13 Tool)

## 0. 背景与动机

当前编辑能力只有 C++/ImGui 的 4 个浮窗(Hierarchy/Inspector/Stats/Log),默认皮肤、控件少且无说明,既不好看也不好懂。用户希望用**更方便的语言(网页)做一个好看、能真正驱动引擎的编辑器 UI**。

本项目的 Tool API 本就是**语言无关的 JSON 契约**(每个 Tool:name + JSON 参数 Schema + JSON 结果),天然适合跨语言/跨进程调用。缺的只是一层**传输**:把进程内 C++ 的 `ToolRegistry::Invoke` 暴露为本地 HTTP+JSON 服务。这层传输同时是未来 LLM agent 的入口 —— 人类网页 UI 与 agent 走同一个受控边界。

整体目标拆为两个独立子项目,各自 spec→plan→实现:
1. **本文档:Tool API HTTP 传输层(无头 Tool 服务器)** —— 地基。
2. **(后续)网页编辑器前端** —— 用 AI 参考图 + frontend-design skill 构建,连接本传输层。

## 1. 范围

### 做
- 新依赖 `cpp-httplib`(单头,FetchContent 钉版)。
- 新模块 `me_toolserver`:`ToolDispatcher`(纯逻辑 string→string 核心)+ `HttpToolServer`(cpp-httplib 薄壳)。
- 新可执行 `toolserver_app`:无头 main,构造引擎状态、加载现有资产、跑 HTTP 服务。
- 三端点:`POST /invoke`、`GET /tools`、`GET /*`(静态文件)。
- `ToolDispatcher` 全 CPU-only doctest。

### 不做(YAGNI / 后续切片)
- 网页前端本体(子项目 2)。
- SSE/WebSocket 实时推送(前端先用轮询)。
- 实时驱动 DX12 游戏窗口(本切片引擎无头;DX12 sandbox 原样保留为「试玩」)。
- 鉴权 / HTTPS / 多用户。
- 把 `time.advance`/`crop.advance_days` 之外的自动 tick(运行时态仍只显式驱动)。

### 不动
- 现有 DX12 sandbox + ImGui 面板:本切片不删不改。

## 2. 架构

```
浏览器 / curl ──HTTP+JSON──> me_toolserver ──> ToolRegistry::Invoke ──> Scene / CommandStack / TimeSystem / FarmField
                              │
                              ├─ ToolDispatcher  (纯逻辑:JSON string → JSON string,持有 ToolContext + ToolRegistry)
                              └─ HttpToolServer   (cpp-httplib 薄壳:HTTP 路由 → ToolDispatcher)
```

### 模块依赖
```
me_toolserver → me_toolapi + me_domain + me_scene + me_command + me_core + nlohmann/json + cpp-httplib
```
单向;不被任何引擎模块反向依赖(它是应用层装配)。

### 单元划分(剖分:可测核心 + 封死边界)
- **`ToolDispatcher`**(`ToolDispatcher.h/.cpp`)—— 拥有 `ToolContext`(引用 Scene/CommandStack/ToolInvocationLog/TimeSystem/FarmField)与 `ToolRegistry`;关键方法见 §4。**不含任何 socket / cpp-httplib 代码**,纯 `std::string → std::string`,可 doctest 红绿。线程安全(内部互斥锁,见 §5)。
- **`HttpToolServer`**(`HttpToolServer.h/.cpp`)—— 构造时接受 `ToolDispatcher&` 与绑定地址/端口/静态根目录;`Run()` 注册三路由并 `listen()` 阻塞。只做 HTTP 收发,逻辑全部委托 dispatcher。薄到无需自动化测试,手动 curl 冒烟。
- **`toolserver_app`**(`main.cpp`)—— 构造 Scene/CommandStack/Log/TimeSystem/FarmField,加载资产(§6),组 `ToolContext` → `ToolDispatcher` → `HttpToolServer.Run()`。无窗口、不链接 RHI/DX12。

延续 M7「EditorController(可测)vs ImGui(封死边界)」的剖分一贯做法。

## 3. HTTP 协议

全部 JSON;服务器同源服务静态文件(为子项目 2 前端免 CORS)。

| 方法 + 路径 | 请求体 | 响应体 | 用途 |
|------|------|------|------|
| `POST /invoke` | `{ "name": string, "params": object?, "role": string?, "dryRun": bool? }` | `{ "ok": bool, "code": string, "message": string, "data": object, "invocationId": number }` | 通用 Tool 调用,镜像 `ToolRegistry::Invoke` |
| `GET /tools` | — | `[ { "name": string, "category": string, "permission": string, "paramsSchema": object } ]` | 列出全部 Tool + 参数 Schema;前端据此自动生成表单、按权限灰按钮 |
| `GET /*`(静态) | — | 文件内容 | 服务子项目 2 前端静态资源(可选根目录;缺省禁用) |

### 约定
- `role` 缺省 `"Editor"`(本地编辑器全权)。识别 `"Agent"`/`"Automation"`/`"Editor"`(对应 `CallerRole`);非法字符串 → `{ok:false, code:"InvalidParams", message:"unknown role"}`。
- `params` 缺省空对象;`dryRun` 缺省 `false`。
- **业务错误走 ToolResult 模型 + HTTP 200**:请求体非法 JSON / 缺 `name` / 未知 role 等都返回 `{ok:false, code, message}` 且 HTTP 200。**HTTP 4xx/5xx 仅用于协议级问题**(如错误的 HTTP 方法、服务器内部异常兜底)。
- 响应 `code` 为 `ToolErrorCode` 的稳定字符串(`Ok`/`UnknownTool`/`PermissionDenied`/`InvalidParams`/`PreconditionFailed`/`ExecutionFailed`),复用既有 `ToString`。

## 4. ToolDispatcher 接口

```cpp
namespace me::toolserver {

/// @brief 纯逻辑 Tool 调度核心:JSON 字符串入,JSON 字符串出,不含 socket。
///        线程安全:内部互斥锁串行化每次调用(见 §5)。
class ToolDispatcher {
public:
    /// @brief 注入受控上下文与已注册 Builtin Tool 的 registry。
    ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry);

    /// @brief 处理一次 /invoke:解析 body → Invoke → 序列化 ToolResult。
    ///        body 非法/缺 name/非法 role 均返回结构化 {ok:false,code} 的 JSON,绝不抛。
    std::string HandleInvoke(const std::string& jsonBody);

    /// @brief 处理 /tools:返回全部 Tool 的 [{name,category,permission,paramsSchema}] JSON。
    std::string HandleListTools();

private:
    me::toolapi::ToolContext& ctx_;
    me::toolapi::ToolRegistry& registry_;
    std::mutex mutex_; ///< 串行化 Invoke(共享 Scene/Farm 状态,见 §5)
};

} // namespace me::toolserver
```

注:`ToolRegistry` 需能枚举已注册 Tool 的元数据(name/category/permission/paramsSchema)以实现 `HandleListTools`。若现有 `ToolRegistry` 未暴露枚举接口,本切片补一个只读枚举方法(最小新增,不改既有调用)。

## 5. 并发与生命周期

- cpp-httplib 默认**每请求一线程**;多请求可能并发 `Invoke`,共享同一 Scene/Farm → 竞争。
- **`ToolDispatcher` 内一把 `std::mutex`,在 `HandleInvoke`/`HandleListTools` 全程加锁**,把 Tool 调用串行化。编辑器调用频率低,锁竞争可忽略;且单次 Tool 调用本应原子(create/destroy 不能被穿插)。
- `toolserver_app` 主线程 `HttpToolServer.Run()` 阻塞 `listen()`;SIGINT/Ctrl+C 触发 `Stop()`(cpp-httplib `stop()`)干净退出。
- 无后台 tick:时间/作物推进只由显式 `time.advance` / `crop.advance_days` 驱动。
- 相比「嵌入 DX12」方案,省掉跨线程投递到渲染主循环;此处只有 HTTP 线程间一把锁。

## 6. 资产、安全与边界

- **启动加载**:`toolserver_app` 复用 `assets/config/time.json`(TimeSystem)+ `assets/config/crops.json`(FarmField 的 CropDatabase)+ `assets/maps/farm_demo`(场景实体),使前端一连上即有真实内容。**失败策略(定稿)**:配置文件(time.json/crops.json)缺失或非法 → 打错误日志并**退出**(返回非零码),因为没有它们 time/crop Tool 无法工作;场景文件缺失 → 打警告并以**空场景继续**(仍可经 create_entity 编辑)。
- **安全**:仅绑 `127.0.0.1`;端口为具名 `constexpr`(如 `kDefaultPort = 8080`,可由命令行/环境覆盖)。文档注明「本地开发工具,非生产服务;默认 Editor 全权」。
- **边界**:不做鉴权/HTTPS、不做 SSE/WS 推送(前端轮询)、不做实时 DX12、不做前端本体。

## 7. 测试策略(CPU-only doctest,WSL 红绿)

`ToolDispatcher` 为纯 string→string,可完全脱离 socket 测试(新 `tests/toolserver/test_tool_dispatcher.cpp`):
1. **合法 invoke**:`{"name":"scene.create_entity"}` → `ok:true` + `invocationId>0`;随后 `scene.list_entities` 见到新实体。
2. **dryRun 零副作用**:`{"name":"scene.create_entity","dryRun":true}` 后 `list_entities` 数量不变。
3. **错误路径**(各返回 `ok:false` + 对应 code,均 HTTP 200 语义):
   - body 非法 JSON → `InvalidParams`。
   - 缺 `name` → `InvalidParams`。
   - 未知 Tool 名 → `UnknownTool`。
   - 非法 role 字符串 → `InvalidParams`。
   - 权限不足:`role:"Agent"` 调 `scene.destroy_entity`(EditorOnly)→ `PermissionDenied`。
   - Tool 前置失败:destroy 不存在实体 → `PreconditionFailed`。
4. **role 解析**:`"Agent"/"Automation"/"Editor"` 分别裁决正确(同一 Tool 不同 role 不同结果)。
5. **HandleListTools**:返回 13 条;每条含 name/category/permission/paramsSchema;含某具体 Tool 的 schema 校验(如 `entity.set_transform` 的 required 字段)。
6. **域能力贯通**(证明 dispatcher 能驱动各子系统):经 invoke 调 `time.advance`、`crop.plant`+`crop.get_field` 往返。

`HttpToolServer` 薄壳不写自动化 socket 测试(边界封死),文档给 `curl` 冒烟示例:
```
curl -s -XPOST localhost:8080/invoke -d '{"name":"scene.create_entity"}'
curl -s localhost:8080/tools
```
现有 193 doctest 全程不回归。

## 8. 关键决策(转 ADR)

1. **Tool API 暴露为本地 HTTP+JSON = Agent-ready 传输层首切**:人类网页 UI 与未来 LLM agent 走同一受控边界(权限/Schema/审计全复用)。
2. **无头 Tool 服务器**(非嵌入 DX12):浏览器从 JSON 数据自画场景视图;省去渲染主循环的跨线程投递,最快落地,DX12 sandbox 原样保留为试玩。
3. **`cpp-httplib` 单头 + FetchContent 钉版**:契合最小依赖;其内部用线程/异常属第三方,不违反「我方代码不用异常」。
4. **ToolDispatcher(纯逻辑核心)与 HttpToolServer(socket 薄壳)剖分**:核心 string→string 可 doctest,壳封死边界手动 curl(延续 M7 EditorController/ImGui 剖分)。
5. **串行锁**:每请求一线程下用一把互斥锁串行化 Tool 调用,保证共享 Scene/Farm 状态原子,最小正确。
6. **业务错误走 ToolResult + HTTP 200**;HTTP 状态码只表达协议级问题。
7. **仅绑 127.0.0.1、默认 Editor 全权**:本地开发工具定位,鉴权留后续。
8. **`GET /tools` 暴露 Tool 元数据**:前端数据驱动生成表单/按权限灰按钮,不写死接口。
