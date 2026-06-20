# ADR 0001 — M3 瓦片地图架构决策

- **状态**: 已接受
- **日期**: 2026-06-20
- **里程碑**: M3 瓦片地图

---

## 背景

M3 目标是实现数据驱动的正交瓦片地图渲染(Tileset + TileMapRenderer + Tiled JSON 加载),供 sandbox 演示并为后续农场领域层(M8)提供基础。需要决策三件事:JSON 格式与解析库、TileMapRenderer 与 SpriteBatch 的关系、TilesetDesc.imagePath 的解析约定。

---

## 决策一:Tiled JSON 子集

**决策**:LoadTiledMap 仅支持 `orientation=orthogonal`、`type=tilelayer`(整数 data 数组)、单个嵌入式 tileset。不支持 objectgroup、多 tileset、外部 tileset 引用、压缩编码 data。

**理由**:
- 覆盖农场模拟(星露谷式)所需的全部核心场景;
- 保持解析器简洁易读(< 100 行);
- 复杂子集(对象层、多 tileset)延迟到有实际需求时扩展,符合"代码清晰可读优先"原则。

**替代方案**:完整 Tiled API 实现 — 过重,当前里程碑不需要。

---

## 决策二:TileMapRenderer 复用 SpriteBatch(不自行 Begin/End)

**决策**:`TileMapRenderer::Render` 只调用 `SpriteBatch::Submit`,不调用 `Begin`/`End`。调用方包裹:
```cpp
batch.Begin(camera.ViewProj());
tileRenderer.Render(batch, camera, map, tileset);
batch.End(cmd);
```

**理由**:
- 合批边界由调用方控制 — 允许同一帧内瓦片与精灵混合提交;
- 单 tileset 全合批 → **1 次 drawcall**,无额外开销;
- 薄层设计消除重复根签名/PSO 逻辑;SpriteBatch 已有完整的排序/VB/IB 管理。

**替代方案**:TileMapRenderer 独立持有 SpriteBatch — 导致多 PSO 创建、多 `SetDescriptorHeaps` 绑定,性能与维护成本更高。

---

## 决策三:nlohmann/json v3.11.3(FetchContent)

**决策**:选用 nlohmann/json v3.11.3 作为 JSON 解析库,通过 CMake FetchContent 引入。

**理由**:
- 单头文件,无额外运行时依赖;
- `get_to` 零异常模式 + `contains()` 检查符合项目"不使用 C++ 异常"规范;
- API 简洁,易于单测与审阅;
- 已被 Tool API 边界(M6)列为规划依赖,提前引入避免后续重复决策。

**替代方案**:
- rapidjson — API 冗长,需手动管理 DOM 生命周期;
- simdjson — SIMD 加速不必要(地图文件 < 10KB);
- 手写解析 — 维护成本高,可读性低。

---

## 后果

- `me_assets` 模块对 nlohmann/json 产生 PRIVATE 依赖(不泄漏到调用方头文件)。
- `LoadTiledMap` 解析失败返回 `std::nullopt` + `ME_LOG_ERROR`,不抛异常。
- 若后续需要 objectgroup 或多 tileset,在 `TiledMapLoader.cpp` 中扩展即可,公开接口(`TileMapData`)无需破坏性修改。
