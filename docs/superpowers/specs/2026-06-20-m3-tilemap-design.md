# M3 瓦片地图 — 设计文档

- **日期**:2026-06-20
- **里程碑**:M3(Tileset + TileMap 渲染 + 从 JSON 加载地图)
- **状态**:已确认,待生成实现计划
- **上游**:[架构设计](2026-06-17-miniengine-design.md)、`docs/PROGRESS.md`

## 目标

数据驱动的正交瓦片地图:从外部 **Tiled JSON(`.tmj`)** 加载多层瓦片地图,经合批渲染上屏,支持相机平移/缩放。所有可纯 CPU 验证的逻辑(解析、UV 计算、坐标翻转、视野裁剪)在 WSL 用 doctest 红绿;GPU 部分用 WARP 像素回读 + sandbox 目视,沿用 M1/M2 既有手段。

## 已确认决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 地图格式 | 兼容 **Tiled JSON** 子集 | 可用成熟编辑器画地图;只取够用子集避免一口气吃完整规范 |
| Tiled 子集 | 正交(orthogonal)+ 多瓦片层(tilelayer)+ **单个嵌入式 tileset** | 足以画出多层农场地图(地面/装饰);解析复杂度适中 |
| 渲染路径 | **TileMapRenderer 薄层 + 复用 SpriteBatch** | 零重复渲染逻辑;同 tileset 自然合成 1 drawcall/层;契合 M2 ADR(srcRect 为瓦片预留) |
| 空格表示 | `gid == 0` | Tiled 约定 |
| 多 tileset / 外部 tileset / 翻转标志位 | **本期不做** | YAGNI;留待后续需要时扩展 |
| 2.5D 动态精灵 Y-sort | **本期不做**(留 M4 RenderSystem) | M3 只做静态层叠;Y-sort 依赖 Scene/Component(M4) |

## 模块划分(保持单向依赖)

下层不得反向依赖上层,由 CMake 构建图强制。

### `me_assets`(跨平台,WSL 可单测)— 新增数据 + 加载

- **`TileMapData`**(纯数据 POD,header `me/assets/TileMapData.h`)
  - `int mapCols, mapRows`:地图尺寸(瓦片数)
  - `int tileWidth, tileHeight`:瓦片像素尺寸
  - `struct TileLayer { std::string name; std::vector<int> gids; }`,`gids.size() == mapCols * mapRows`,行主序(row0 在顶部,Tiled 约定)
  - `std::vector<TileLayer> layers`(数组顺序 = 绘制顺序,首层在底)
  - `struct TilesetDesc { std::string imagePath; int tileWidth, tileHeight, columns, margin, spacing, imageWidth, imageHeight, firstGid; }`
- **`LoadTiledMap(const std::string& path) -> std::optional<TileMapData>`**(`me/assets/TiledMapLoader.h`)
  - 用 `me::platform::ReadTextFile` 读文本;失败 → `nullopt` + `ME_LOG_ERROR`
  - 用 nlohmann/json **非抛出** `json::parse(text, nullptr, /*allow_exceptions=*/false)`;`is_discarded()` 即解析失败 → `nullopt` + 日志(**不使用 C++ 异常**,符合 CLAUDE.md)
  - 字段校验:`orientation == "orthogonal"`、存在至少一个 `tilelayer`、存在一个嵌入 tileset(含 `image`)。任一不满足 → `nullopt` + 具名错误日志
  - tileset 图片路径相对地图文件目录解析为可用路径
- **`TileLayout`**(纯函数 helper,header-only `me/assets/TileLayout.h`)
  - 输入 `TilesetDesc` + `localId`(已减去 firstGid),输出归一化 `me::Rect` srcRect(`{uMin, vMin, uSpan, vSpan}`,V 向下,与 `SpriteDesc::srcRect` 同约定)
  - 处理列回绕:`col = localId % columns; row = localId / columns`,并计入 `margin`/`spacing`

### `me_renderer`(WIN32-only,GPU 测)— 新增运行时绑定 + 渲染

- **`Tileset`**(`me/render/Tileset.h`)
  - 持有非拥有 `const me::rhi::GpuTexture*` + `TilesetDesc`(用于 TileLayout)
  - 提供 `SrcRectForGid(int gid) -> me::Rect`(内部 `localId = gid - firstGid`,委托 `TileLayout`)
- **`TileMapRenderer`**(`me/render/TileMapRenderer.h` / `.cpp`)
  - `Render(SpriteBatch& batch, const OrthographicCamera& cam, const TileMapData& map, const Tileset& tileset)`
  - 调用方负责 `batch.Begin(cam.ViewProj())` 之外的 RT/视口绑定;`TileMapRenderer` 只产出 `Submit`,**不**自己 Begin/End(由调用方控制合批边界,可与精灵共批)。实际边界在实现计划中定;默认约定:调用方 `Begin` → `renderer.Render(...)`(逐层 Submit)→ `batch.End(cmd)`
  - 逐层、逐**可见**瓦片:跳过 `gid == kEmptyTileGid`,算 dstRect(世界像素,Y 翻转)+ srcRect,`batch.Submit(...)`

## 坐标与裁剪(纯数学,可单测)

- **Y 翻转**:Tiled row0 在顶、Y 向下;引擎世界 Y 向上、原点左下。
  瓦片 `(col, row)` 的世界左下角:`worldX = col * tileWidth`,`worldY = (mapRows - 1 - row) * tileHeight`,dstRect = `{worldX, worldY, tileWidth, tileHeight}`。
- **视野裁剪**:由相机可见世界矩形 `[camPos ± halfExtent/zoom]` 反算可见瓦片范围
  `colMin = clamp(floor(visLeft / tileWidth), 0, mapCols-1)` 等,只 Submit `[colMin..colMax] × [rowMin..rowMax]`。瓦片地图可能很大且相机平移,裁剪避免提交不可见瓦片。
- 提供独立纯函数 `VisibleTileRange(...)` 便于单测。

## 零魔法数字

- `constexpr int kEmptyTileGid = 0;`
- `localId = gid - tileset.firstGid;`(firstGid 来自数据,非裸数字)
- tileset 行列、margin、spacing 全部来自 `TilesetDesc`,不在源码硬编码地图数值。

## 依赖新增

`third_party/CMakeLists.txt` 增加 FetchContent 拉取 `nlohmann/json`(单头,钉到具体 release tag 保证可复现),暴露 target 供 `me_assets` 链接。`me_assets/CMakeLists.txt` 链接该 target。同步更新模块 README。

## 错误处理(不使用异常)

- 文件缺失 / 读失败 → `LoadTiledMap` 返回 `nullopt` + `ME_LOG_ERROR`。
- JSON 解析失败(`is_discarded()`)→ `nullopt` + 日志。
- 必需字段缺失 / 非 orthogonal / 无瓦片层 / 无 tileset → `nullopt` + 具名错误日志。
- 不变量违反(如 `gids.size() != cols*rows`)→ 加载阶段视为数据错误返回 `nullopt`;运行时索引越界用 `ME_ASSERT`。

## 测试策略

### WSL doctest(`me_tests`,跨平台)
- `TileLayout`:`localId → srcRect` 四角值、列回绕(`localId == columns` 进下一行)、含 margin/spacing 的偏移。
- 坐标转换:Tiled `(col,row) → 世界 dstRect` 的 Y 翻转(顶行映射到最大 Y)。
- `VisibleTileRange`:相机居中/平移/缩放下的可见范围,边界 clamp。
- `LoadTiledMap`:解析提交的样例 `tests/assets/*.tmj`(断言 mapCols/Rows、层数、若干 gid、tileset 列数);坏 JSON → `nullopt`;缺文件 → `nullopt`;`orientation != orthogonal` → `nullopt`。

### WARP 像素回读(`me_gpu_tests`)
- 渲染一张 2 层小地图到离屏 RT,回读断言:已知 gid 的瓦片像素颜色出现在预期屏幕位置;空 gid 处为清屏色;`SpriteBatch::DrawCallCount()` == 非空层数(合批生效)。

### sandbox 目视
- 加载真实 `assets/maps/demo.tmj` + `assets/textures/tileset.png`,相机 WASD 平移、Q/E 缩放,目视确认地图正立、层叠正确、裁剪无缝。

## 新增资源

- `assets/textures/tileset.png`:小图集(如 4×4 个 16px 瓦片,颜色可辨便于像素断言)。
- `assets/maps/demo.tmj`:手写的精简 Tiled 导出(正交、2 层、嵌入上述 tileset)。
- `tests/assets/` 下:解析单测用的 `.tmj` fixture(可与 demo 共用或单独精简版)+ 坏 JSON 样例。

## 验收标准

- [ ] `LoadTiledMap` 正确解析样例地图,异常输入全部安全返回 `nullopt`(WSL 红绿)。
- [ ] `TileLayout` / Y 翻转 / 可见范围三组纯函数单测通过。
- [ ] WARP:多层地图像素回读断言通过,drawcall 数 == 非空层数。
- [ ] sandbox:真机加载地图,相机平移/缩放目视正确。
- [ ] `third_party`、各模块 `CMakeLists.txt` 与 README、`docs/PROGRESS.md` 同步更新。

## 不在本期范围(YAGNI / 留后续里程碑)

- 多 tileset、外部 `.tsj` tileset 引用、gid 翻转标志位。
- 对象层(objectgroup)、图块动画、无限地图(chunks)。
- AssetManager / TilesetHandle 句柄化与缓存(spec 提及,留后续)。
- TileMapComponent 与 Scene 集成、2.5D Y-sort(M4)。
- 瓦片地图编辑(M7,经 Tool API)。
