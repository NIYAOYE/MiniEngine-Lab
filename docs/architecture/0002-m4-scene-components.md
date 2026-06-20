# ADR 0002 — M4 Scene + 组件架构决策

- **状态**: 已接受
- **日期**: 2026-06-20
- **里程碑**: M4 Scene + 组件

---

## 背景

M4 目标是实现混合实体模型场景层:Entity/Scene 生命周期管理、Transform2D 父子层级与世界矩阵缓存、数据型组件(SpriteComponent/CameraComponent/TileMapComponent)、TransformSystem/RenderSystem 产出纯数据 RenderView。需要决策四件事:Scene 与 Renderer 的解耦边界、组件存储实现、2.5D 叠压排序策略、RenderView→SpriteBatch 桥接放置位置。

---

## 决策一:`me_scene` CPU-only;`RenderView` 用 RHI 无关 `textureId`

**决策**:`me_scene` 模块不依赖 `me_rhi` 或 `me_renderer`。`RenderView` 中每条 `RenderItem` 用 `uint32_t textureId` 引用纹理,不持有 `GpuTexture*`。`GpuTexture*` 的解析发生在渲染边界(当前为 sandbox,未来为 Engine 层)。

**理由**:
- Scene 可在 WSL(无 DX12/GPU 环境)用 doctest 独立单测,覆盖 Transform 层级、组件增删、System 产出 RenderView 全路径;
- 不受 RHI 头文件污染,编译速度更快;
- 解耦保证 Scene 可在未来替换渲染后端(Vulkan/Metal)时零修改。

**替代方案**:RenderItem 直接携带 `GpuTexture*` — 使 `me_scene` 反向依赖 `me_rhi`,破坏单向依赖约束。

---

## 决策二:组件存储用 sparse-set 隐藏在 `ComponentStorage<T>` / `IComponentStorage` 接口后

**决策**:每种组件类型对应一个 `ComponentStorage<T>`(sparse-set:dense 数组 + index 映射,swap-pop 删除 O(1));类型擦除基类 `IComponentStorage` 供 `Scene` 以 `unordered_map<type_index, unique_ptr<IComponentStorage>>` 统一管理。外部 API(`AddComponent`/`GetComponent`/`HasComponent`/`RemoveComponent`)不暴露存储实现。

**理由**:
- 存储封装保留向纯 ECS(连续 dense 数组、archetype 分组)演进路径;
- swap-pop 删除保持 dense 数组紧密,System 顺序遍历缓存友好;
- 接口隐藏使测试只关心语义,不绑定实现细节。

**替代方案**:
- `std::unordered_map<Entity, T>` per-component — 随机访问、内存碎片、不缓存友好;
- EnTT — 引入重量级外部依赖,CLAUDE.md 明确禁止。

---

## 决策三:2.5D 叠压 = 层升序 + 同层世界 Y 降序稳定排序

**决策**:`RenderSystem::BuildRenderView` 对 RenderItem 先按 `sortLayer` 升序、同层内按世界 Y 降序稳定排序后产出。SpriteBatch 按纹理指针稳定排序合批,单图集时保留 Y 序。

**理由**:
- 农场模拟 2.5D 透视感:Y 值大(画面偏上/远)的物体被 Y 值小(偏下/近)的物体遮挡;
- 稳定排序保证同纹理、同层、同 Y 值时提交顺序不变,避免闪烁;
- demo 单图集时 SpriteBatch 合批不切换纹理,SpriteBatch 的稳定纹理排序不会打乱 Y 序。

**替代方案**:深度缓冲 + Z 值 — 增加 3D 管线复杂度;2D/2.5D 农场模拟通常用画家算法即可。

---

## 决策四:`RenderView→SpriteBatch` 桥接暂放 sandbox(Engine 层未建)

**决策**:将 `textureId` 解析为 `GpuTexture*` 并调用 `SpriteBatch::Submit` 的桥接逻辑暂时写在 `sandbox/main.cpp` 中,以 `std::vector<const rhi::GpuTexture*> textureTable` 实现 id→指针映射。M5+ 建立 Engine 层后迁移。

**理由**:
- M4 无独立 Engine 层;提前抽象会建立空壳类,增加无谓维护成本;
- sandbox 作为当前唯一消费方,桥接代码量少(< 10 行),可读性高;
- 明确记录为临时位置,后续迁移边界清晰。

**替代方案**:在 `me_renderer` 中新建 `SceneBridge` 类 — 使 `me_renderer` 反向知晓 `me_scene` 类型,或引入循环依赖,均违反模块单向约束。

---

## 后果

- `me_scene` 在全平台(WSL + Windows)构建,跨平台单测覆盖 Transform/组件/System。
- `sandbox` 同时链接 `me_scene` + `me_renderer`,桥接 textureId→GpuTexture*,是当前唯一运行时层。
- M4 不新增 WARP GPU 测试(复用 M2/M3 已验证 SpriteBatch/TileMapRenderer);渲染正确性由 sandbox 目视把关(pending-user)。
- 若后续需要多图集/多纹理,`textureTable` 按 index 扩展即可;`RenderItem.textureId` 语义不变。
