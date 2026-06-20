# me_scene

混合实体模型场景层(纯 CPU,可在 WSL doctest 单测)。namespace `me::scene`。

## 内容(M4)
- **Entity / Scene**:`Entity = Handle<EntityTag>`(index+generation,销毁后旧句柄失效);
  Scene 管理实体生命周期(槽位回收)、父子层级、缓存世界矩阵 + 脏标记、组件存储。
- **Transform 层级**:每实体局部 `Transform2D` + 父/子邻接;`world = local * parentWorld`(行向量)。
- **Component(数据型)**:`SpriteComponent` / `CameraComponent` / `TileMapComponent`;
  存储隐藏在 `IComponentStorage` / `ComponentStorage<T>`(sparse-set,swap-pop)之后。
- **System**:`TransformSystem`(批解析世界矩阵)、`RenderSystem`(收集精灵 → 算 dstRect →
  按层升序、世界 Y 降序稳定排序 → `RenderView`;解析活动相机 → `CameraView`)。

## 关键约定
- **不依赖 RHI/Renderer**:`RenderView` 用 RHI 无关的 `uint32_t textureId` 引用纹理;
  解析为 `GpuTexture*` 的桥接在渲染边界(当前是 sandbox,未来 Engine 层)完成。
- 2.5D 叠压:同层按世界 Y 降序提交(低 Y 后画 = 压在上);demo 用单图集使 SpriteBatch
  稳定纹理排序保留该顺序。
- 禁全局状态:System 为无状态静态方法,显式接收 `Scene&`。

## 依赖
- `me_core`(数学/句柄/断言)、`me_assets`(`TileMapData`)。**不得**依赖 `me_rhi`/`me_renderer`。
