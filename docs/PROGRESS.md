# MiniEngine 开发进度

> 跨会话进度索引。**每次有实质进展时更新本文件**:勾选里程碑、更新"当前状态"和"下一步"、追加决策记录。

- **最后更新**:2026-06-23
- **当前阶段**:M8.3 库存/物品系统完成(`Inventory` 格位网格 + `ItemDatabase` + 3 inventory Tool + `crop.harvest` 原子直写库存;231/231 全绿);下一步 M8.4 NPC 日程调度 / 库存前端消费
- **方向**:2D/2.5D 农场模拟游戏引擎(C++/DX12 + Win32),引擎能力抽象为受控、可验证的 Agent-ready Tool API

## 一句话现状

M1 精灵上屏已完成:Win32 Window/Input + RHI(GpuDevice/SwapChain/CommandContext/Fence/DescriptorHeap/GpuBuffer/GpuTexture/Shader/PSO)+ stb_image 纹理解码 + SpriteRenderer(根签名+PSO+单位四边形,经 MVP 绘制)。跨平台逻辑在 WSL 用 doctest 红绿;DX12/GPU 正确性由 WARP 软件适配器 + 离屏像素回读自动化把关(me_gpu_tests:设备/围栏、纹理上传往返、带纹理精灵渲染),并经 sandbox 真机目视确认(开窗、贴图正立、WASD 平移、ESC 退出)。M2 批渲染 + 正交相机已完成:SpriteBatch 按纹理合批 + OrthographicCamera + 8×5 多精灵 sandbox;WARP 多精灵/色调/srcRect 像素回读 + 相机 doctest + sandbox 目视验证。M3 瓦片地图已完成:TileLayout(gid→UV)/TileGeometry(col/row→worldRect)/Tileset/TileMapRenderer + nlohmann/json Tiled JSON 子集加载器(LoadTiledMap) + sandbox 数据驱动演示(12×8 地图、2 层、相机平移/缩放);WARP 像素回读 + doctest 单测 + sandbox 目视(pending-user)。M4 Scene + 组件已完成:Entity/Handle(index+generation)/Scene(生命周期/层级/脏标记) + Transform2D 父子层级 + IComponentStorage/ComponentStorage(sparse-set) + SpriteComponent/CameraComponent/TileMapComponent + TransformSystem/RenderSystem(RenderView 按层+Y 降序) + CameraView/ResolveActiveCamera;sandbox 用 Scene+System 驱动瓦片地面+精灵 prop+player(WASD)+相机跟随+Q/E 缩放;sandbox 目视 pending-user。M5 Command 中枢已完成:Scene 持久 EntityId 身份锚定(IdOf/Resolve/CreateEntityWithId)+ 类型擦除组件快照(CaptureComponents/RestoreComponents) + ICommand/CommandResult/CommandStack(标准 Undo/Redo 双栈,peek-then-pop,新 execute 清空 redo) + CreateEntityCmd/SetTransformCmd/DestroyEntityCmd(命令存 EntityId 而非裸 handle;Destroy 全子树+组件+active camera 还原);顺带修 Scene::DestroyEntity 悬垂 active camera;全程 CPU-only doctest,WSL 101/101 全绿。M6 ToolAPI 已完成:新增 me_toolapi 静态库(toolapi→command→scene→core + nlohmann/json 边界),含 ToolResult(五码错误模型 + JSON 序列化)+ 三层权限白名单(CallerRole/Permission/IsAllowed)+ 手写 JSON Schema 子集校验器(type/required/properties/minimum/maximum/enum,递归)+ ToolInvocation/ToolInvocationLog(单调 id 审计)+ ITool 接口 + ToolContext 受控门面(Scene&/CommandStack&/Log&,前置声明隔离)+ ToolRegistry 统一流水线(查找→白名单→Schema 校验→dryRun?→run→全路径记录回填 invocationId);首批 6 Tool:查询 scene.list_entities/scene.get_entity/log.read(AgentAllowed),变更经 CommandStack scene.create_entity[Automation]/scene.destroy_entity[EditorOnly]/entity.set_transform[Automation](dry-run 零副作用、PreconditionFailed、Undo 往返均测);全程 CPU-only doctest,WSL 129/129 全绿。M7 Editor as Client(精简切片)CPU 核心已完成:新增 me_editor 静态库(editor→toolapi→command→scene→core,不依赖 ImGui/RHI)含 EditorController——读写都只经 ToolRegistry::Invoke(CallerRole::Editor)反向验证 M6 接口,DTO(EntityRow/EntityDetails/LogRow)解析 Tool JSON 结果;七意图方法 RefreshHierarchy/Select+InspectSelected/CreateEntity/ApplyTransform/DestroySelected/Undo·Redo(直调 CommandStack,M6 无 undo Tool 之缺口)/RefreshLog + reconcileSelection + 全路径 LastError 暴露绝不静默;13 个 EditorController doctest 证明 6 Tool 足以驱动"实体增删+变换编辑+层级浏览+审计查看"完整编辑器,WSL 142/142 全绿。ImGui 目视层(代码就绪,pending Windows 构建+目视):Dear ImGui v1.91.5(URL 钉版,封死 sandbox)+ platform::Window 裸类型 Win32 消息钩子(不依赖 ImGui)+ sandbox 主循环接入四面板(Hierarchy/Inspector/Stats/Log,Space 切换,全变更经 EditorController→ToolAPI,WASD 运行时直调保留)。反向验证三发现转 M8+ 接口需求:edit.undo/redo、entity.set_name、get_entity.components+add_component。M8.1 时间系统已完成:新增 me_domain 静态库(domain→core + nlohmann/json),含 TimeConfig(全参数 JSON 驱动,LoadTimeConfig 返回 std::optional)+ TimeSystem(四级日历 分钟/天/季/年,内部 totalMinutes_ 单一真相源,Now()/Advance()/Update())+ TimeStep 践跳计数(minutesCrossed/daysCrossed/seasonsCrossed/yearsCrossed);time.get(AgentAllowed)/ time.advance(Automation,dry-run 值拷贝副本)经既有 Registry 流水线,ToolContext 加可选 TimeSystem*(前置声明隔离,默认 nullptr),Builtin Tool 总数升至 8;CPU-only,WSL 162/162 全绿(无 Windows/GPU 依赖);assets/config/time.json 示例配置体现数据驱动。M8.1 后(非里程碑,2026-06-22)接入农场美术:**修复 SpriteBatch PSO 未开 alpha 混合的遗留 bug**(此前 M1~M4 仅不透明素材未触发,透明精灵黑底;现开 src-over,不透明内容零回归,WARP GPU 测试验证);新增 tools/pack_atlas.py 美术管线(AI 大图 largeImage/ → 地面 32px 图集 ground_tileset.png + 抠白底物件精灵 prop_*.png/player_sprite.png,LANCZOS 降采样 + 四边洪水填充抠透明);assets/maps/farm_demo.tmj(12×8 农场地图引用新图集);sandbox 改为加载 farm_demo + 8 张独立精灵纹理(SRV 堆 8→16)+ 玩家中心夹紧地图世界边界(按半精灵内缩)不出界;sandbox MSVC 构建通过(目视 pending-user)。largeImage 源图与 imgui.ini 已 gitignore。M8.2 作物生长已完成:新增 `CropConfig`/`CropDatabase`(JSON 数据驱动作物表,`LoadCropDatabase` 返回 `std::optional`,顶层数组/字段/id 唯一性全校验)+ `FarmField`(以 `TileKey` 为键的瓦片网格,浇水驱动生长状态机——`daysInStage` 计已浇水天数,未浇水停滞不死亡,成熟不再前进,`Harvest` 清空瓦片返回产出);5 crop Tool(`crop.get_field`[AgentAllowed]/`plant`[Automation]/`water`[Automation]/`advance_days`[Automation]/`harvest`[EditorOnly])经既有 Registry 流水线,dry-run 用 `FarmField` 值拷贝零副作用,Builtin Tool 总数升至 13;`assets/config/crops.json` 数据驱动,与单测 `ValidCropJson()` 严格一致;CPU-only,WSL 193/193 全绿(无 Windows/GPU 依赖)。M9.1 Tool HTTP 传输层已完成:新增 `me_toolserver` 静态库(`ToolDispatcher` 纯逻辑 string→string 核心 + `HttpToolServer` cpp-httplib 薄壳封死边界)+ `toolserver_app` 无头可执行(无窗口无 DX12);三端点(`POST /invoke` 镜像 ToolRegistry::Invoke、`GET /tools` 暴露 13 Tool 元数据、`GET /*` 可选静态根);串行互斥锁串行化每次 Tool 调用保证共享 Scene/Farm 原子;业务错误走 ToolResult + HTTP 200;仅绑 127.0.0.1 + `kDefaultPort=8080`;time.json/crops.json mandatory 失败退出,空场景启动(无 tmj→Scene 加载器,前端经 scene.create_entity 填入);curl 冒烟(`curl -s -XPOST localhost:8080/invoke -d '{"name":"scene.create_entity"}'`)验证;WSL 208/208 全绿(193 基线 + 15 条 ToolDispatcher 新用例)。网页编辑器前端接线已完成(2026-06-23,非里程碑):`tools/editor-frontend/src/lib/toolClient.ts` 的 `invoke()`/`listTools()` 由内存 mock 切换为真实 `fetch`——`POST /api/invoke`、`GET /api/tools` 经 Vite dev 代理(`vite.config.ts` 加 `server.proxy['/api'] → 127.0.0.1:8080`)打到 `toolserver_app`,绕开 cpp-httplib 无 CORS 的限制;`VITE_USE_MOCK=true` 可回退内存 mock 离线开发(`.env.example` 文档化);客户端审计历史(比 `log.read` 子集更全)保留在 `invoke()` 包装层、两种传输通用;`/tools` 不带 description,前端按 name 回贴本地 UI 文案。后端空场景启动,前端首次连上若实体为空则经 `seedDemoWorld()` 用真实 Tool 调用注入 6 实体 + 5 作物(全 `silent`,不刷审计)。**接线暴露契约缺口**(记录而非绕过):真实 `scene.create_entity` 无参且无 reparent Tool,故种子实体扁平(无 World→Player 层级,labels 仍按 id 1..6 映射);作物 stage 无法直设(`crop.advance_days` 全田),种子用 plant+water+advance_days(1) 体现状态/浇水差异。`npm run typecheck` 通过,UI 组件零改动。M8.3 库存/物品系统已完成:新增 `ItemConfig`/`ItemDatabase`/`InventoryConfig`/`LoadInventoryConfig`(JSON 数据驱动物品表,`LoadInventoryConfig` 返回 `std::optional`,顶层对象/capacity/items 字段/id 唯一全校验)+ `Inventory`(固定格位网格,all-or-nothing Add/Remove,`CanAdd` 非破坏性预判,值语义可拷贝供 dry-run)+ `FarmField::PeekHarvest`(非破坏性收获预判,不清瓦片)+ 3 inventory Tool(`get`[AgentAllowed]/`add`[Automation]/`remove`[EditorOnly])经既有 Registry 流水线,Builtin Tool 总数升至 16;`crop.harvest` 改造为 PeekHarvest+CanAdd 原子直写库存(库满→收获失败且瓦片不清,`ctx.inventory==nullptr` 向后兼容仍产出);`assets/config/items.json` 数据驱动(`capacity:36`,覆盖 crops.json 全 harvestItemId);CPU-only,MSVC 231/231 全绿(无 GPU 依赖)。

## 文档索引

| 文档 | 用途 |
|------|------|
| [架构设计](superpowers/specs/2026-06-17-miniengine-design.md) | 权威设计文档:分层架构、2D 渲染、Tool API、Command、农场领域层、路线图 |
| [M5 Command 设计](superpowers/specs/2026-06-20-m5-command-design.md) | M5 详设:EntityId 身份锚定、组件快照、ICommand/CommandStack、三命令 |
| [ADR 0003](architecture/0003-m5-command.md) | M5 架构决策记录 |
| [M6 ToolAPI 计划](superpowers/plans/2026-06-21-m6-toolapi.md) | M6 实现计划:9 任务 TDD(ToolResult/权限/Schema/审计/Registry/六 Tool) |
| [ADR 0004](architecture/0004-m6-toolapi.md) | M6 架构决策记录:Schema 手写、Tool 范围、ToolContext 边界、变更经 Command、权限+审计 |
| [M7 Editor 设计](superpowers/specs/2026-06-21-m7-editor-design.md) | M7 详设:EditorController 读写都走 Tool、ImGui 封死 sandbox、精简切片、反向验证发现 |
| [M7 Editor 计划](superpowers/plans/2026-06-21-m7-editor.md) | M7 实现计划:11 任务(me_editor 7 步 CPU-only TDD + ImGui 3 步目视 + 文档) |
| [ADR 0005](architecture/0005-m7-editor.md) | M7 架构决策记录:读写都走 Tool、剖分可测 controller、宿 sandbox、精简切片、缺口记录 |
| [M8.1 时间系统设计](superpowers/specs/2026-06-21-m8-1-time-system-design.md) | M8.1 详设:me_domain 职责、四级日历、TimeStep 计数、time.get/advance Tool |
| [M8.1 实现计划](superpowers/plans/2026-06-21-m8-1-time-system.md) | M8.1 实现计划:6 任务 TDD(TimeConfig/TimeSystem/ToolAPI/EditorController 扩展/资产/文档) |
| [ADR 0006](architecture/0006-m8-1-time-system.md) | M8.1 架构决策记录:me_domain 模块、四级日历、totalMinutes_ 单一真相源、TimeStep、两 Tool、time.advance 不经 CommandStack |
| [M8.2 作物生长设计](superpowers/specs/2026-06-22-m8-2-crop-growth-design.md) | M8.2 详设:CropConfig/CropDatabase、FarmField 浇水驱动状态机、5 crop Tool、权限梯度、dry-run、错误处理 |
| [M8.2 实现计划](superpowers/plans/2026-06-22-m8-2-crop-growth.md) | M8.2 实现计划:7 任务 TDD(CropConfig/FarmField/生长机/收获/ToolContext+5Tool/dry-run+权限/资产+文档) |
| [ADR 0007](architecture/0007-m8-2-crop-growth.md) | M8.2 架构决策记录:FarmField 非 Scene 组件、浇水驱动状态机、5 crop Tool、不经 CommandStack、harvest=EditorOnly、产出不入库 |
| [M9.1 Tool HTTP 传输层设计](superpowers/specs/2026-06-22-toolserver-http-transport-design.md) | M9.1 详设:无头 Tool 服务器架构、ToolDispatcher/HttpToolServer 剖分、三端点协议、并发串行锁、资产安全边界、8 条关键决策 |
| [M9.1 实现计划](superpowers/plans/2026-06-23-m9-1-toolserver-http-transport.md) | M9.1 实现计划:7 任务 TDD(ToolDispatcher/HandleInvoke/错误路径/role/HandleListTools/域贯通/toolserver_app + 文档) |
| [ADR 0008](architecture/0008-m9-1-toolserver-http-transport.md) | M9.1 架构决策记录:HTTP+JSON 传输层首切、无头服务器、cpp-httplib 钉版、ToolDispatcher/HttpToolServer 剖分、串行锁、业务错误 HTTP 200、安全边界 |
| [M8.3 库存/物品设计](superpowers/specs/2026-06-23-m8-3-inventory-design.md) | M8.3 详设:格位网格 Inventory + ItemDatabase JSON 驱动 + 3 inventory Tool + crop.harvest 原子直写库存 + 权限梯度 + 7 关键决策 |
| [M8.3 实现计划](superpowers/plans/2026-06-23-m8-3-inventory.md) | M8.3 实现计划:7 任务 TDD(ItemConfig/Inventory/FarmField PeekHarvest/InventoryTools/crop.harvest 改造/资产装配/文档回归) |
| [ADR 0009](architecture/0009-m8-3-inventory.md) | M8.3 架构决策记录:格位网格、all-or-nothing、harvest 原子直写、PeekHarvest、sellPrice 无 icon、不经 CommandStack、权限梯度、独立加载软告警 |
| 本文件 `docs/PROGRESS.md` | 跨会话进度追踪 |
| `../CLAUDE.md` | 项目定位与代码生成规则(本地文件,被 .gitignore 忽略) |

## 里程碑进度

图例:☐ 未开始 ◐ 进行中 ☑ 完成。当前阶段交付止于 M7(接口层 + 编辑器,不接大模型);农场领域层 M8 起。

| 里程碑 | 状态 | 说明 |
|--------|------|------|
| 架构设计 | ☑ | 已确认并提交(融合:2D/2.5D 农场 + Agent-ready Tool API) |
| **M0 地基** | ☑ | CMake 骨架 + Core(2D Math/Log/Handle/Assert)+ Platform(计时/文件系统)+ doctest 单测;Win32 窗口/输入推迟到 M1 |
| **M1 精灵上屏** | ☑ | Win32 Window/Input + RHI(Device/SwapChain/CmdList/Fence/PSO)+ stb_image 纹理 + SpriteRenderer;WARP 像素回读 + sandbox 目视验证 |
| **M2 批渲染 + 正交相机** | ☑ | SpriteBatch 按纹理合批 + OrthographicCamera + 多精灵;WARP 多精灵/色调/srcRect 像素回读 + 相机 doctest + sandbox 目视 |
| **M3 瓦片地图** | ☑ | Tileset + TileMap 渲染 + 从 JSON 加载地图;sandbox 数据驱动演示;sandbox 目视 pending-user |
| **M4 Scene + 组件** | ☑ | Entity/Transform2D + Component + System;sandbox Scene 驱动演示;sandbox 目视 pending-user |
| **M5 Command 中枢** | ☑ | ICommand + CommandStack(标准 Undo/Redo)+ CreateEntity/Destroy/SetTransform;EntityId 身份锚定 + 类型擦除组件快照;CPU-only doctest 101/101 全绿 |
| **M6 ToolAPI** | ☑ | me_toolapi:ToolRegistry 统一流水线 + ITool + ToolContext 受控门面 + 手写 JSON Schema 子集校验 + dry-run + 三层权限白名单 + ToolInvocation 审计;首批 6 Tool(3 query + 3 mutation 经 CommandStack 可 Undo);CPU-only doctest 129/129 全绿 |
| **M7 Editor as Client** | ◐ | me_editor + EditorController(读写都经 6 Tool)CPU 核心完成,13 doctest 反向验证 WSL 142/142;ImGui 四面板目视层代码就绪 **pending-user Windows 构建+目视**。瓦片地图编辑/存档留后续切片 |
| M8 农场领域层 | ◐ | M8.1 时间系统 ☑;M8.2 作物生长 ☑(FarmField + CropDatabase + 5 Tool + doctest 全绿);M8.3 库存 ☑(Inventory 格位网格 + ItemDatabase + 3 inventory Tool + harvest 原子直写;231/231 全绿);M8.4 NPC 日程调度 ☐ |
| **M9.1 Tool HTTP 传输层** | ☑ | me_toolserver(ToolDispatcher + HttpToolServer)+ toolserver_app;三端点;串行锁;208/208 全绿 |
| **网页编辑器前端接线** | ☑ | toolClient mock→真实 fetch(Vite `/api` 代理→:8080);`VITE_USE_MOCK` 回退;`seedDemoWorld` 空场景引导;typecheck 通过、UI 零改动 |
| M9+ 未来 | ☐ | M8.3 库存;对话/配方数据驱动、2D 物理/碰撞、存档;reparent/set_name Tool(接线缺口);未来 Agent/LLM 接入白名单 Tool |

## 下一步行动

1. **M7 收尾(pending-user)**:在 Windows / vcvars64 下构建 sandbox(`feature/M7` 分支),目视确认四面板交互——Create/Destroy/Undo/Redo 改变场景、Inspector 拖动 transform 移动实体、Log Refresh 显示工具调用链、Space 切换编辑器、WASD 仍移动 player、Escape 干净退出无 D3D 校验错误。目视通过后 M7 行勾 ☑。
2. ~~**M8.1 时间系统**~~ ☑:me_domain 模块(TimeConfig JSON 驱动 + TimeSystem 四级日历 + TimeStep 践跳计数)+ time.get/time.advance ToolAPI + 162/162 WSL 全绿 + ADR 0006 + assets/config/time.json。
3. ~~**M8.2 作物生长**~~ ☑:FarmField(浇水驱动状态机)+ CropDatabase(JSON 数据驱动)+ 5 crop Tool(get_field/plant/water/advance_days/harvest)+ 193/193 WSL 全绿 + ADR 0007 + assets/config/crops.json。
4. ~~**M9.1 Tool HTTP 传输层**~~ ☑:me_toolserver(ToolDispatcher/HttpToolServer)+ toolserver_app + 三端点 + 串行锁 + 208/208 WSL 全绿 + ADR 0008。
5. ~~**网页编辑器前端接线**~~ ☑:`toolClient.ts` 的 `invoke()`/`listTools()` 改真实 `fetch`(Vite `/api` 代理→`toolserver_app:8080`),`VITE_USE_MOCK` 回退 mock,`seedDemoWorld` 空场景引导;typecheck 通过、UI 零改动。暴露 reparent/set_name 契约缺口。
6. ~~**M8.3 库存/物品**~~ ☑:定义 `Inventory`/`ItemConfig`/`ItemDatabase` 领域模型(JSON 驱动 `items.json`);`FarmField::PeekHarvest` + `Inventory::CanAdd` 原子直写库存;inventory.get/add/remove 3 Tool API(Builtin 总数 16);231/231 MSVC 全绿;ADR 0009 + domain README。
7. **M8.4 NPC 日程调度**:NPC 行为状态机 + 日程数据驱动;`time.advance` 驱动 NPC 决策;库存/经济后续可接入。
8. **库存前端消费(可选切片)**:前端读 `inventory.get` 显示格位;`add`/`remove` UI 操作;与作物收获结合展示。
9. **接线缺口补 Tool(可选)**:`scene.create_entity` 加 parent/transform 参数 或 `scene.set_parent` Tool(让前端种子可重建层级);`entity.set_name`(替代前端本地 labels)。
10. 按里程碑实现并回写本文件。ToolAPI 主线(M6 接口 + M7 首个客户端反向验证 + M8.x 领域层消费 + M9.1 HTTP 传输层)是本项目区别于普通学习引擎的核心特色。

> M6 收尾结转(非阻塞,见 ADR 0004):`Make*Tool` 工厂定义行可统一升级为 `@brief`;schema `minimum:1` 可抽 EntityId 域常量;校验器嵌套 `required` 错误消息 framing 统一;`ToolInvocationLog::Entries/Size` 可加 noexcept。M5/M4 更早结转:`Handle::IsValid` 不校验 generation 的统一;RenderItem 哨兵统一 / RenderView→SpriteBatch 桥接迁出 sandbox(见各 ADR)。

## 关键决策记录(ADR 摘要)

| 日期 | 决策 | 理由 |
|------|------|------|
| 2026-06-17 | 实体模型用混合方案(OOP 层级 + 数据组件 + System),不用 EnTT | 直观好学,预留 ECS 演进路径 |
| 2026-06-17 | 底座 C++ + DirectX 12 + Win32 | 学习现代图形 API |
| 2026-06-17 | 转向 Agent-ready Tool API | 编辑期能力统一为受控、可验证接口 |
| 2026-06-17 | 双轨:运行时直调 C++,编辑期走 Tool API | 兼顾性能与受控 |
| 2026-06-17 | Tool 参数/结果用 JSON + JSON Schema 校验 | 适配未来 LLM 工具调用 |
| 2026-06-17 | 变更型 Tool 必经 Command(可 Undo/dry-run) | 可审计、可预览、可回滚 |
| 2026-06-17 | **融合定调:维度改为 2D/2.5D 农场向** | 目标品类是星露谷式农场模拟;简化渲染/数学/物理 |
| 2026-06-17 | **去掉 SDL2**,渲染/输入用 DX12 + Win32 | 保留 Agent-ready/DX12 底座 |
| 2026-06-17 | **农场领域模块(时间/瓦片玩法/NPC 日程)纳入后期里程碑(M8)** | 引擎核心保持通用,领域玩法独立成层 |
| 2026-06-17 | M0 Core/Platform 跨平台,WSL 可单测;Win32 Window/Input 推迟到 M1 | Core/计时/文件系统不依赖窗口,可即时 TDD;窗口需真实 Windows 验证,与 DX12 上屏合并 |
| 2026-06-17 | 数学库定为行主序 + 行向量(p' = p*M) | 与 DX12/DirectXMath 同源,降低 M1 接图形 API 的心智负担 |
| 2026-06-17 | 单测框架选 doctest(FetchContent) | 单头、编译快、API 简洁,契合最小依赖 |
| 2026-06-19 | M1 着色器用 FXC(D3DCompileFromFile,SM5.1)而非 DXC | 零额外依赖、最快上屏;DXC 留到需 SM6 时 |
| 2026-06-19 | M1 用系统 DX12(d3d12/dxgi/d3dcompiler),不引入 Agility SDK | 最小依赖,够用即可 |
| 2026-06-19 | GPU 代码用 WARP 软件适配器 + 离屏像素回读做自动化测试,辅以 sandbox 目视 | 无独显/无窗口环境也能红绿,补足 RHI 不可纯 CPU 单测的空缺 |
| 2026-06-19 | Windows 侧定义 UNICODE/_UNICODE | 全程用 -W API + 宽字符串,使资源宏(IDC_ARROW)解析为宽字符版本 |
| 2026-06-19 | 合批策略 = 按纹理指针稳定排序后逐 run 合 drawcall;模型变换 CPU 端烘入顶点 | 合批无法每精灵传根常量,CPU 侧烘入顶点无额外 GPU 开销;稳定排序保持同纹理提交顺序 |
| 2026-06-19 | 顶点格式 pos+uv+color 一次到位,srcRect 支持图集采样 | color 色调支持 M2 测试用例;srcRect UV 子区域供 M3 瓦片图集复用 |
| 2026-06-19 | M1 `SpriteRenderer` 退役并入 `SpriteBatch` | 消除重复根签名/PSO/VB 逻辑;单一渲染路径降低维护成本 |
| 2026-06-19 | 延续上传堆 + 每帧全同步;帧并行(FrameRing)/默认堆迁移推迟到性能里程碑 | M2 目标是合批正确性与相机;提前引入 per-frame fence 会增加调试复杂度,与本里程碑目标无关 |
| 2026-06-19 | 容量溢出策略 = VB/IB 按高水位自动增长(不静默丢弃) | 静默丢弃会导致渲染缺失难以排查;自动增长保证正确性,性能影响在可接受范围内 |

| 2026-06-20 | Tiled JSON 子集:只支持 orthogonal / tilelayer / 单嵌入 tileset | 覆盖农场模拟所需场景;复杂子集(objectgroup、多 tileset)推迟到需求明确后 |
| 2026-06-20 | TileMapRenderer 复用 SpriteBatch(不自行 Begin/End) | 瓦片与精灵同帧内混合提交;单 tileset 全合批 → 1 drawcall |
| 2026-06-20 | JSON 解析库选 nlohmann/json v3.11.3(FetchContent) | 头文件单一、API 简洁、零异常模式(`get_to`)符合项目规范 |

| 2026-06-20 | `me_scene` CPU-only 始终构建;`RenderView` 用 RHI 无关 `textureId`(Scene/Renderer 解耦) | Scene 可在 WSL 独立单测,不依赖 DX12/GPU;RHI 指针解析推迟到渲染边界 |
| 2026-06-20 | 组件存储用 sparse-set 隐藏在 `ComponentStorage<T>` / `IComponentStorage` 接口后 | 外部 API 不暴露存储实现,保留向纯 ECS 演进路径;swap-pop 删除 O(1) |
| 2026-06-20 | 2.5D 叠压:层升序 + 同层世界 Y 降序稳定排序;demo 单图集使 SpriteBatch 稳定排序保留 Y 序 | 农场模拟 2.5D 透视感;单图集合批避免排序被纹理切换打断 |
| 2026-06-20 | `RenderView→SpriteBatch` 桥接暂放 sandbox(Engine 层未建,运行时层代行) | M4 无独立 Engine 层;当桥接逻辑增长到值得提取时迁移,不提前建 |

| 2026-06-21 | M5 命令以持久 `EntityId` 锚定身份(非裸 handle),execute/undo 时 `Resolve` | 销毁重建后 generation 变化会令裸 handle 悬垂;单调 EntityId 跨重建稳定,并为序列化铺路(见 ADR 0003) |
| 2026-06-21 | M5 命令范围 = Create/Destroy/SetTransform;PaintTile/AddComponent/SaveScene 推迟 | create↔destroy↔transform 往返是最强 undo 证明;其余依赖未落地能力(YAGNI) |
| 2026-06-21 | CommandStack 标准双栈,新 execute 清空 redo,peek-then-pop;不合并/不设上限 | 最小正确模型;合并/容量待 M7 交互编辑器有需求再加 |
| 2026-06-21 | 命令结果用轻量 `CommandResult{ok,message}` + 字符串 `describe()`,不提前引入 JSON | 遵守不抛异常;JSON/ToolResult 留 M6,CommandResult 可平滑包装 |
| 2026-06-21 | 组件快照 `RestoreTo(Entity)` 经存储指针回写(非回调 Scene) | 避免 `Scene.h↔ComponentStorage.h` 头文件循环;Capture/CaptureComponents 非 const |
| 2026-06-21 | `Scene::DestroyEntity` 清除被销毁(子树)实体上的悬垂 `m_activeCamera` | 任意销毁路径都不应留下悬垂活动相机句柄,不变量归位 Scene 层而非命令代偿 |

| 2026-06-21 | M6 JSON Schema 校验**手写最小子集**(type/required/properties/range/enum,递归),不引第三方库 | 契合最小依赖 + 不抛异常;首批 Tool schema 简单够用;schema 是 json 可序列化,适配未来 LLM |
| 2026-06-21 | M6 首批 Tool 聚焦已落地能力(3 query + 3 mutation 复用 M5 三命令);add_component/tilemap.paint/save/load 推迟 | create→set_transform→destroy→undo 往返是接口完备性最强证明;其余依赖未落地子系统(YAGNI),同 ADR 0003 |
| 2026-06-21 | Tool 只经注入的 `ToolContext{Scene&,CommandStack&,Log&}` 访问引擎;`ITool.h` 前置声明隔离 Scene/Command | 禁 Singleton/禁全局可变状态/依赖注入;受控边界可审 + 为 Agent 白名单收紧铺路;降低编译耦合 |
| 2026-06-21 | 变更型 Tool 只经 `ctx.commands.execute` 落地;dry-run 零副作用(destroy/set_transform 先校验存活) | 变更经 CommandStack 自动获得 Undo,是 ToolAPI 受控性核心;dry-run+回滚+审计=Agent-ready 安全基础 |
| 2026-06-21 | 三层权限白名单(query=AgentAllowed / create+set_transform=Automation / destroy=EditorOnly);每次 Invoke(含失败)都写审计日志并回填 invocationId | 以最小机制演示完整白名单裁决,为 Agent 只调 agent-allowed 铺路;全路径记录保证审计无盲区、绝不静默 |

| 2026-06-21 | M7 EditorController **读写都只经 ToolRegistry::Invoke**(读走查询 Tool、写走变更 Tool),不直接读 Scene | 反向验证是 M7 存在理由;直接读 Scene 会绕过被验证对象,逼出查询接口缺口才是价值(见 ADR 0005) |
| 2026-06-21 | 剖分 CPU-only `me_editor`(不依赖 ImGui/RHI),ImGui+DX12 后端封死在 sandbox 目视层 | 编辑决策逻辑可 WSL doctest 红绿(13 用例),ImGui 同 RHI 封死应用边界;延续 M4「桥接暂放 sandbox」 |
| 2026-06-21 | M7 精简切片只用现有 6 Tool、零序列化;宿在 sandbox 主循环不造 Engine/Layer | create→set_transform→destroy→undo 往返是接口完备性最强证明;Engine 栈与验证正交,YAGNI(同 M4「无 Engine 层」) |
| 2026-06-21 | Undo/Redo 编辑器直调 CommandStack(M6 无 undo Tool);三处接口缺口显式记录为反向验证发现不补 Tool | 缺口(edit.undo/redo、entity label、get_entity 组件)是 M7 兑现的产出而非瑕疵,转 M8+ 接口需求;补 Tool 需先落地底层能力 |

| 2026-06-21 | 新增 CPU-only `me_domain` 模块,四级日历全参数 JSON 驱动,`totalMinutes_` 单一真相源 | 时间系统是作物/NPC 共同地基;纯逻辑可 WSL doctest,与 RHI/窗口完全解耦(见 ADR 0006) |
| 2026-06-21 | `TimeStep` 践跳计数(非布尔/非回调),消费者轮询 | 契合"显式传参、零全局状态";daysCrossed/minuteOfDay 分别供 M8.2 作物/M8.4 NPC 消费 |
| 2026-06-21 | `time.get`(AgentAllowed)/ `time.advance`(Automation)经既有 Registry 流水线;`ToolContext` 加可选 `TimeSystem*` | Builtin Tool 总数升至 8;前置声明隔离,保持 M6/M7 构造默认有效(nullptr) |
| 2026-06-21 | `time.advance` 不经 CommandStack;dry-run 用值拷贝副本推进 | 时间是运行时状态,Undo 无意义;此为"变更经 Command"约定的文档化例外(场景编辑约定,不适用运行时状态) |
| 2026-06-21 | `assets/config/time.json` 示例配置与测试 `ValidJson()` 取值严格一致 | 数据驱动原则:游戏参数从外部文件加载,不硬编码到源码 |
| 2026-06-21 | M8.1 WSL 全量 doctest 162/162 全绿(CPU-only,无 Windows/GPU 依赖) | 纯逻辑里程碑;时间系统可独立红绿,作物/NPC 切片有稳定地基 |
| 2026-06-22 | SpriteBatch PSO 启用标准 alpha 混合(src-over) | 2D 农场需透明精灵;此前遗留未开混合致透明区黑底;不透明内容 alpha=1 等同覆盖零回归 |
| 2026-06-22 | AI 美术经 tools/pack_atlas.py 处理入库(地面图集 + 抠底物件精灵);largeImage 源图不入库 | 数据驱动美术管线;生成纹理小体积入库供 clone 直接运行,大图源外置 |
| 2026-06-22 | 作物存 me_domain 独立 FarmField(非 Scene 组件)+ 浇水驱动状态机(daysInStage 计已浇水天数) | 沿用 TimeSystem 先例纯 CPU 可测;未浇水停滞符合星露谷核心循环(见 ADR 0007) |
| 2026-06-22 | 5 crop Tool;crop 变更不经 CommandStack(运行时态例外),harvest=EditorOnly | 沿用 ADR 0006 例外;harvest 销毁性产出收紧权限;产出不入库留 M8.3 |
| 2026-06-22 | 新增 `tools/editor-frontend`(React18+TS+Vite+Tailwind,非 CMake)Farm Editor 前端原型:全交互经统一 `invoke()` 调 13 Tool,**mock 传输**(内存引擎守真实权限/ToolResult/作物·实体语义/审计=客户端调用历史) | 从客户端视角验证 Tool 契约对外可用(Agent/编辑器);形状严格对齐契约,日后换 `fetch`(HTTP 传输层)UI 零改动;实体名/组件/undo-redo 标注为 future 占位不臆造 |
| 2026-06-23 | Tool API 暴露为本地 HTTP+JSON 无头 Tool 服务器(me_toolserver);仅绑 127.0.0.1;cpp-httplib v0.18.3 FetchContent 钉版 | Agent-ready 传输层首切:人类 UI 与未来 LLM agent 走同一受控边界;无头省去渲染主循环跨线程投递 |
| 2026-06-23 | ToolDispatcher(纯逻辑 string→string 可 doctest)与 HttpToolServer(socket 薄壳封死边界)剖分 | 延续 M7「EditorController/ImGui」剖分惯例;核心 15 用例 CPU-only 红绿,薄壳 curl 冒烟 |
| 2026-06-23 | 串行互斥锁串行化每次 Tool 调用(cpp-httplib 每请求一线程) | 最小正确:共享 Scene/Farm 原子不被穿插;编辑器低频调用锁竞争可忽略 |
| 2026-06-23 | 业务错误走 ToolResult + HTTP 200;HTTP 4xx/5xx 仅协议级 | 复用既有 ToolErrorCode 稳定字符串;调用方只需检查 `ok` 字段,无需解析 HTTP 状态 |
| 2026-06-23 | 前端接线用 Vite dev 代理(`/api`→:8080)而非后端加 CORS / 静态托管 | 零改 C++、保留 npm 热更新;cpp-httplib 无 CORS 头,代理同源最省事;生产再议 |
| 2026-06-23 | 保留 mock 传输,`VITE_USE_MOCK` 切换(默认真实 fetch) | 后端没起也能离线开发 UI;两传输同形,UI 零改动;审计历史在 `invoke()` 包装层两传输通用 |
| 2026-06-23 | 空场景由前端 `seedDemoWorld` 经真实 Tool 引导(非后端 tmj→Scene 加载器) | 沿用 M9.1 范围:连上即有内容由前端注入兑现;暴露 reparent/set_name 契约缺口(记录,转后续 Tool) |
| 2026-06-23 | M8.3:库存格位网格 + all-or-nothing + harvest 原子直写(PeekHarvest+CanAdd)+ 不经 CommandStack;权限梯度 get/add/remove;items 独立加载软告警 | 格位贴近农场模拟 UI;all-or-nothing 让 harvest 原子性成为一行 CanAdd 预判;不可 Undo 沿 ADR 0006/0007 例外;remove=EditorOnly 演示三层白名单梯度 |

## 待解决 / 开放问题

- 瓦片地图是否兼容 Tiled JSON 格式,M3 时确定。
- 上传堆顶点/纹理为 M1 简化(可读性优先);仍延续,推迟到后续性能里程碑(M2 已确认不做默认堆迁移)。
- M1 每帧 `fence->Flush` 全同步(无帧并行);仍延续,推迟到后续性能里程碑(M2 已确认不做帧并行)。
