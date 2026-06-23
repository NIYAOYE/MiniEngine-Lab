你是一名资深前端工程师和 UI 设计师。请根据我描述的「Farm Editor 农场编辑器」桌面端管理界面,实现一个可以真实运行、可交互的前端页面。

本界面是一个 C++ 游戏引擎(MiniEngine)的外部编辑器前端。引擎把它的全部能力抽象成 13 个「Tool」,每个 Tool 有:名称、类别(Query 查询 / Mutation 变更)、权限等级、JSON 参数 Schema、JSON 结果。前端通过统一的 `invoke(name, params, role)` 调用这些 Tool。**当前阶段引擎传输层尚未接通,所有数据用本地 Mock,但 Mock 的数据形状必须与真实契约一致**,以后把 mock 换成 `fetch` 即可。

请不要只生成静态图片,而是实现可运行、可交互的前端。

## 〇、最重要:必须严格遵守的「真实契约」(不要臆造)

### 0.1 统一调用约定(API 客户端)
实现一个 `lib/toolClient.ts`,导出:

```ts
type Role = 'Agent' | 'Automation' | 'Editor';
type ToolErrorCode = 'Ok' | 'UnknownTool' | 'PermissionDenied' | 'InvalidParams' | 'PreconditionFailed' | 'ExecutionFailed';

interface ToolResult {
  ok: boolean;
  code: ToolErrorCode;
  message: string;
  data: Record<string, unknown>;   // 成功载荷或失败明细
  invocationId: number;
}

// 现在用 mock 实现;以后改为 POST http://127.0.0.1:8080/invoke
async function invoke(name: string, params?: object, role?: Role, dryRun?: boolean): Promise<ToolResult>;

// 现在用 mock 实现;以后改为 GET http://127.0.0.1:8080/tools
async function listTools(): Promise<ToolDefinition[]>;
```

**所有交互都必须经过 `invoke()`**,不允许组件直接改 mock 状态。mock 的 `invoke` 内部维护一份内存中的引擎状态(实体表、农田、时间、审计日志),按下面的真实语义处理每个 Tool,并返回真实形状的 `ToolResult`。

### 0.2 13 个 Tool 的真实定义(权限不得改动)

| name | category | permission | params | data(成功时) |
|------|----------|------------|--------|----------------|
| `scene.list_entities` | Query | **AgentAllowed** | `{}` | `{count, entities:[{id, position:{x,y}, rotation, scale:{x,y}}]}` |
| `scene.get_entity` | Query | **AgentAllowed** | `{id:int}` | `{id, position:{x,y}, rotation, scale:{x,y}, parentId, children:[id...]}` |
| `log.read` | Query | **AgentAllowed** | `{limit?:int}` | `{count, invocations:[{id, tool, params, dryRun, ok, code, message}]}` |
| `crop.get_field` | Query | **AgentAllowed** | `{}` | `{crops:[{x,y,cropId,stage,stageName,daysInStage,watered,mature}]}` |
| `time.get` | Query | **AgentAllowed** | `{}` | `{year, season, seasonName, dayOfSeason, minuteOfDay, hour, minute}` |
| `scene.create_entity` | Mutation | **Automation** | `{}`(或父子/变换可选) | `{id}` |
| `entity.set_transform` | Mutation | **Automation** | `{id:int, position:{x,y}, rotation, scale:{x,y}}` | 变换后视图 |
| `time.advance` | Mutation | **Automation** | `{minutes:int(≥1)}` | `{step:{...}, time:{...}}` |
| `crop.plant` | Mutation | **Automation** | `{tileX:int, tileY:int, cropId:string}` | 种植后瓦片视图 |
| `crop.water` | Mutation | **Automation** | `{tileX:int, tileY:int}` | 浇水后瓦片视图 |
| `crop.advance_days` | Mutation | **Automation** | `{days:int(≥1)}` | `{advanced, crops:[...]}`(**作用于整片农田**) |
| `scene.destroy_entity` | Mutation | **EditorOnly** | `{id:int}` | `{}` |
| `crop.harvest` | Mutation | **EditorOnly** | `{tileX:int, tileY:int}` | `{itemId, count}` |

权限统计:AgentAllowed 5 个、Automation 6 个、EditorOnly 2 个,共 13。

权限规则(用于「权限视图」下拉置灰):特权 `Agent < Automation < Editor`。当前所选 role **低于** 某 Tool 的 permission 时,该 Tool 对应的按钮**置灰禁用**并给 tooltip(如「需要 Editor 权限」)。例如选 `Agent` 时,除查询类外全部禁用;选 `Automation` 时,`scene.destroy_entity` 与 `crop.harvest` 禁用。

### 0.3 作物领域模型(严格按此,不要臆造阶段)
- 作物种类来自配置,真实有两种:`parsnip`(防风草,阶段:`seed, sprout, growing, mature`)、`cauliflower`(花椰菜,阶段:`seed, sprout, leafy, heading, mature`)。**阶段名是每作物自带的英文小写数组,不是全局固定枚举。**
- 瓦片状态:`{x, y, cropId, stage(整数下标), stageName(字符串), daysInStage, watered(bool), mature(bool)}`。`mature` 为真表示处于最后阶段、可收获。
- **没有「已收获/枯萎」阶段。** `crop.harvest` 成功后该瓦片**变为空**(从农田数据移除)。
- 空瓦片**不出现在 `crop.get_field` 返回里**;UI 自行把没有数据的格子画成「未种植 Empty」。
- 生长规则:`crop.advance_days {days}` 推进**整片农田** N 天;每株只有当天「已浇水」才前进一格生长(未浇水停滞),到 mature 后不再前进。`crop.water {tileX,tileY}` 把该瓦片标记为已浇水(幂等)。`crop.plant {tileX,tileY,cropId}` 在空瓦片种下(stage=0、未浇水)。
- 图例改为:未种植 / 各生长阶段(用 stage 下标着色)/ 可收获(mature=true 高亮)。**不要 Harvested 项。**

### 0.4 实体模型
- 实体**只有 `id` 和 transform(position/rotation/scale)**,外加 `parentId` 与 `children`(父子层级是真实的)。
- **实体当前没有名字、没有类型、没有组件列表**(`entity.set_name`、组件读写都是未来接口)。
- 因此:实体树用 id 作为标签(如 `ent_0012`);若想显示「Player / Barn」这类友好名,只能作为**前端本地标注**,并注明是 future(mock 里可带一个 `label` 字段做展示,但要清楚这不是引擎契约的一部分)。
- Inspector 真实可编辑的只有 Transform(经 `entity.set_transform`)+ 只读 ID/parentId/children。组件区(TileGrid/CropField/Renderable/Collider/土壤/湿度)在当前契约里**不存在**——保留为明确标注「future(当前 Tool 契约未暴露组件)」的禁用占位即可,用于匹配布局,但不要假装能编辑。
- **作物不是实体**:农田面板的数据来自 `crop.get_field`(按 tileX/tileY),与「选中某实体」是两条独立的数据线,不要把选中实体和农田格子绑在一起。

### 0.5 审计 / 历史
- 引擎 `log.read` 每条只有:`{id, tool, params, dryRun, ok, code, message}`——**没有时间戳、没有角色、没有结果 JSON、没有耗时**。
- 为了让审计表更有用,**审计数据用前端自己的「调用历史」**:每次 `invoke()` 时前端记录 `{时刻(客户端), tool, role(本次所发), params, result(本次 response), ok, code, 耗时(客户端测得 ms)}`。这样所有列都诚实。表里注明「引擎 log.read 为权威子集」。

### 0.6 Undo / Redo
- 当前 Tool 契约**没有** undo/redo 端点;且 crop/time 是运行时态、**不可撤销**,只有 scene 增删/变换理论上可撤销。
- 顶部 Undo/Redo 按钮**默认禁用**,tooltip 注明「future:edit.undo/redo 待引擎暴露」。

---

## 一、技术栈
- React 18 + TypeScript + Vite + Tailwind CSS + Lucide React 图标
- 无后端;数据走 §0.1 的 `toolClient`(mock 实现)
- 不用大型 UI 组件库;通用控件拆成独立组件
- `npm install` + `npm run dev` 后可直接运行;无 TypeScript 报错
- 输出完整项目代码,不要省略关键文件,不要伪代码

## 二、整体视觉风格
深色、专业、偏游戏引擎编辑器风格(参考 Unity / Unreal / VS Code 深色主题 / 农场模拟数据编辑器)。
- 画布以 1536×1024 为基准,铺满视口
- 背景深蓝黑;面板用不同层级深灰蓝;细微渐变、1px 边框、4–7px 圆角、柔和阴影
- 主文字灰白、次文字蓝灰、强调蓝;成功绿、错误/删除红;作物操作用绿/蓝/紫/橙区分
- 控件密度较高但清晰有序;所有按钮/输入/下拉/列表项有 hover/active/focus 态
- 字体:`Inter, "PingFang SC", "Microsoft YaHei", sans-serif`

建议色值:背景 `#0b121b`;顶栏 `#0e1722`;一级面板 `#121c28`;二级面板 `#182431`;输入框 `#202c39`;边框 `rgba(148,163,184,0.14)`;主文字 `#e5edf5`;次文字 `#8f9cab`;蓝 `#3b82f6`;绿 `#69b84f`;红 `#d9544d`;紫 `#7c4cc4`;橙 `#c9892f`。

## 三、整体布局
顶部导航栏 + 三列主体 + 底部状态栏。

### 顶部导航(高约 44px)
- 左:绿色叶子图标、标题「Farm Editor」、绿色版本标签「M8.2」、导航标签「场景 / 时间 / 作物 / 审计·历史 / 工具(13)」(当前激活「场景」)
- 右:绿色状态点 +「权限视图:」+ **角色下拉(Agent / Automation / Editor,默认 Editor)** + 分隔线 + Undo(禁用) + Redo(禁用) + 设置齿轮
- 角色下拉切换时,全局按可用权限置灰/恢复对应操作按钮

### 主体三列(外边距约 12px,列间距约 10px)
- 左:实体管理栏 ≈294px
- 中:主工作区(自适应)
- 右:属性检查器 ≈330px
- 高度自适应视口,底部留状态栏;内部用滚动,不让整页无限增高

## 四、左侧实体面板
### 卡片 A:场景 / 实体
- 标题「A. 场景 / 实体」,副标题「实体层级 (scene.list_entities)」
- 操作区:蓝色「创建实体」(`scene.create_entity`,Automation)、红色「删除」(`scene.destroy_entity`,EditorOnly)、整行搜索框(放大镜图标,placeholder「搜索实体...」)
- 树形结构:用 `scene.list_entities` + 各实体 `parentId/children` 渲染**真实父子层级**;节点标签用实体 id(可选叠加前端本地 `label`,标注 future)。每个父节点有展开箭头,选中项蓝灰高亮,右侧三点菜单。
- mock 初始可放一棵示例树(根 + 若干子节点),但标签遵循「id 为主、友好名为前端标注」。

### 卡片 B:实体快捷操作
- 标题「实体快捷操作」,显示「选中:<id>」
- 三个大按钮(主文字 + 副文字为 Tool 名,按权限置灰):
  1. 蓝色 扳手「查看详情 / scene.get_entity」(AgentAllowed)
  2. 青绿 准星「设置变换 / entity.set_transform」(Automation)
  3. 红色 垃圾桶「删除实体 / scene.destroy_entity」(EditorOnly)

## 五、中间顶部:时间 / 季节
- 标题「B. 时间 / 季节 (time.get)」
- 主卡三列:左=季节(植物图标 + 季节名 + 第 N 天)、中=年 + `HH:MM`(太阳/月亮图标)、右=`dayOfSeason` + `seasonName(英文)`。**注意 seasonName 是英文(如 Spring),中文为 UI 本地化映射。**
- 推进栏:标签「推进时间 (time.advance)」+ 横向滑块 + 数字输入(默认 60)+「分钟」+ 蓝色「推进」按钮(播放三角图标)。点击 → `invoke('time.advance', {minutes}, role)`,更新时间并向审计历史加一条记录。`time.advance` 是 Automation,Agent 角色时禁用。

## 六、中间主体:作物 / 农田
- 标题「C. 作物 / 农田 (crop.get_field)」
- 工具栏:标签「作物视图」、下拉(可按 阶段/作物 着色)、复选框「显示网格(默认勾)」、图例(未种植 / 各阶段 / 可收获,**无 Harvested**)
- 农田网格:3×3,顶部列号 0/1/2、左侧行号 0/1/2,每格高约 105px,CSS/SVG 模拟土壤背景。
  - 格子内容来自 `crop.get_field`;空瓦片(数据缺失)画成「未种植 Empty」。
  - 有作物的格子:简化植物图形(CSS/SVG/emoji 组合,尽量像真实作物,别只用文字方块)+ 底部深色渐变 + 左下角中文名(本地映射)+ 英文 `cropId` + 下一行 `stageName`;`watered` 为真时右下角蓝色水滴;`mature` 为真时高亮边框;选中格亮蓝 2px 边框。
  - **示例 mock 农田**(用真实作物与阶段):
    - (0,0) parsnip / stage 1 `sprout` / watered:true / mature:false 〔默认选中〕
    - (1,0) parsnip / stage 2 `growing` / watered:true
    - (2,0) parsnip / stage 3 `mature` / watered:true / mature:true
    - (0,1) cauliflower / stage 2 `leafy` / watered:true
    - (1,1) 空
    - (2,1) cauliflower / stage 1 `sprout` / watered:false
    - (0,2) 空
    - (1,2) cauliflower / stage 4 `mature` / watered:true / mature:true
    - (2,2) 空
  - 点击格子更新右侧「作物操作」信息。
- 作物操作面板:
  - 标题「作物操作」,显示:选中瓦片 (x,y)、作物 `cropId`、阶段 `stageName`、已浇水 是/否、是否成熟 是/否(取 `mature`)
  - 四个操作(按权限置灰):
    1. 绿色 叶子「种植 (crop.plant)」(Automation)→ `invoke('crop.plant',{tileX,tileY,cropId})`(需选作物;空瓦片才可种)
    2. 蓝色 水滴「浇水 (crop.water)」(Automation)→ `invoke('crop.water',{tileX,tileY})`
    3. 紫色区 日历「推进天数 / crop.advance_days」(Automation)+ 数字输入(默认 1)+「天」→ `invoke('crop.advance_days',{days})`。**注明:作用于整片农田,非单格。**
    4. 橙色 麦穗「收获 (crop.harvest)」(EditorOnly)→ `invoke('crop.harvest',{tileX,tileY})`,仅 `mature` 时可点;成功后该格变空。
  - 每次操作后:更新农田、追加审计记录、弹轻量 toast(成功/失败按 `ToolResult.ok`)。

## 七、右侧属性检查器(Inspector)
- 标题「Inspector (scene.get_entity)」
- 真实字段:实体 ID(只读 + 复制按钮)、parentId(只读)、children(只读列表/计数)
- **Transform 折叠区(可编辑,经 entity.set_transform)**:position X/Y、rotation(°)、scale X/Y;编辑后调 `invoke('entity.set_transform',{id,position,rotation,scale})`
- **组件折叠区**:标题「组件 (Components)」+「+ 添加组件」按钮。**当前 Tool 契约未暴露组件读写**,本区做成**禁用占位**并标注「future」(可灰显示 TileGrid/CropField 等示意条,但不可交互);不要假装能改土壤/湿度。
- 底部「Renderable / Collider」开关同样标注 future、禁用(契约未暴露)。

## 八、底部区域
### 审计 / 历史(数据 = 前端调用历史,见 §0.5)
- 标题「D. 审计 / 历史 (log.read)」+ 注脚「引擎 log.read 为权威子集(无时间/角色/结果/耗时)」
- 筛选:时间范围、工具筛选、状态筛选、蓝色刷新
- 表头:状态图标 / 时间(客户端) / 工具 / 角色(本次所发) / 参数(JSON) / 结果(JSON) / 状态(ok+code) / 耗时(客户端 ms)
- 成功绿勾、失败红叉(显示 code);行 hover 变色;JSON 等宽字体、过长省略;面板内滚动
- 示例数据须用**真实权限/形状**,例如:
  - `crop.water` role=Editor params `{"tileX":0,"tileY":0}` result `{"ok":true,"code":"Ok","data":{...},"invocationId":7}`
  - `time.advance` role=Automation params `{"minutes":60}` result `{"ok":true,...}`
  - `crop.harvest` role=Editor params `{"tileX":2,"tileY":0}` result `{"ok":true,"data":{"itemId":"parsnip","count":1}}`
  - `crop.harvest` role=Agent → result `{"ok":false,"code":"PermissionDenied",...}`(演示白名单拒绝)

### 工具总览
- 标题「工具 (13) 总览」+ 搜索框 + 分类标签(全部 / AgentAllowed / Automation / EditorOnly)
- 列表(左图标、右权限,**用 §0.2 的真实权限**):
  - scene.list_entities — AgentAllowed
  - scene.get_entity — AgentAllowed
  - log.read — AgentAllowed
  - crop.get_field — AgentAllowed
  - time.get — AgentAllowed
  - scene.create_entity — Automation
  - entity.set_transform — Automation
  - time.advance — Automation
  - crop.plant — Automation
  - crop.water — Automation
  - crop.advance_days — Automation
  - scene.destroy_entity — EditorOnly
  - crop.harvest — EditorOnly
- 数据由 `listTools()` 提供(name/category/permission/paramsSchema);列表与「权限视图」联动:当前角色不可调用的 Tool 用更暗的样式标注。

## 九、底部状态栏(高约 36px,固定底部)
- 左:绿色连接点 +「连接状态:已连接(mock)」+「项目:demo_farm」+「版本:0.1.0」
- 右:「Powered by Tool Contract (13 Tools)」

## 十、组件拆分建议
AppShell / TopNavigation / RolePicker / EntityTreePanel / EntityTreeItem / EntityQuickActions / TimeSeasonPanel / FarmGrid / FarmTile / CropActionPanel / InspectorPanel / TransformSection / ComponentSection(future 占位) / AuditLogPanel / ToolOverviewPanel / StatusBar / Button / Input / Select / Slider / Switch / Panel / Toast。
TS 类型:`Role`、`ToolErrorCode`、`ToolResult`、`ToolDefinition`、`EntityNode`、`CropTile`、`AuditEntry`。

## 十一、交互与状态
React hooks 管理即可(不用 Redux)。需实现:
- **所有变更都经 `toolClient.invoke()`**,按当前 role 做权限裁决(不可调用的返回/禁用)
- 角色下拉切换 → 全局按钮可用性更新
- 实体树展开/收起、选中、搜索;选中实体 → 调 `scene.get_entity` 填 Inspector
- Transform 编辑 → `entity.set_transform`
- 农田格子选中/高亮;种植/浇水/推进天数(整片)/收获(仅 mature),按真实语义更新 mock 农田
- 推进时间;Inspector Transform 编辑;组件区/开关为 future 禁用
- 审计历史:每次 invoke 追加一条(客户端时间/角色/参数/结果/耗时);筛选
- 工具总览:搜索 + 分类筛选 + 与角色联动置灰
- Toast 提示;复制实体 ID;Undo/Redo 禁用(future)

## 十二、响应式
桌面优先:≥1440 三栏完整;1100–1439 压缩左右栏;<1100 允许横向滚动(不堆叠);最小宽约 1050;主区内部滚动,整页不无限增高。

## 十三、细节
- 高度还原布局比例/间距/层级/控件密度;不要做成普通 SaaS 仪表盘;不要纯黑大背景、不要霓虹光效、不要过大圆角(4–7px)
- 输入框高 26–32px;顶栏 44px;主按钮 28–34px;图标 14–18px;自定义深色细滚动条;文字紧凑
- 语义化 HTML;足够对比度;按钮加 aria-label;列表用稳定 key

## 十四、输出要求
1. 目录结构 → 2. 完整 package.json → 3. Tailwind/Vite 配置 → 4. 全部 React/TS/CSS 文件(含 `lib/toolClient.ts` mock 实现)→ 5. 运行命令 → 6. 检查 import 路径 → 7. 无 TS 报错 → 8. 启动即见完整界面 → 9/10. 不省略、不「其余类似」→ 11. 不让我补 mock → 12. 最后列出已实现的交互。
**特别强调**:`toolClient` 的 mock 必须按 §0 的真实语义和形状工作(权限裁决、ToolResult 形状、作物/实体模型、审计=调用历史),以便日后把 mock 换成对 `127.0.0.1:8080` 的 `fetch` 时,UI 代码零改动。
