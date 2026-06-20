# engine/renderer(me_renderer,仅 Windows)

2D 渲染。M2 提供 SpriteBatch(按纹理合批)、OrthographicCamera(header-only 正交相机)、SpriteDesc(提交单元)。M3 新增 Tileset + TileMapRenderer。
依赖:Core, RHI, Assets(单向)。

## 公开类型

### `me::render::SpriteDesc`(POD 提交单元)

```cpp
struct SpriteDesc {
    const me::rhi::GpuTexture* texture; // 非拥有指针,生命周期归调用方
    me::Rect srcRect;   // 归一化 UV 子区域(x=uMin,y=vMin(上),w/h=跨度);整贴图传 {0,0,1,1}
    me::Rect dstRect;   // 目标世界矩形,像素单位,原点左下、Y 向上
    me::Vector4 color;  // RGBA 线性色调,与采样结果相乘;不调色传 {1,1,1,1}
    float rotation;     // 绕 dstRect 中心的弧度旋转
};
```

### `me::render::SpriteBatch`

按纹理指针稳定排序后逐 run 合批,每 run 发一次 `DrawIndexedInstanced`。
模型变换(dstRect + rotation)在 CPU 端烘进顶点(pos+uv+color)。
动态持久映射上传 VB + 静态四边形 IB(R32),单帧提交量超出容量时自动增长到高水位(`kInitialSpriteCapacity = 1024`)。

```cpp
static std::unique_ptr<SpriteBatch> Create(me::rhi::GpuDevice& device);

void Begin(const me::Matrix4x4& viewProj);  // 开始帧,传入 viewProj
void Submit(const SpriteDesc& sprite);      // 累积一个精灵
void End(ID3D12GraphicsCommandList* cmd);   // 排序、写 VB、发 drawcall

size_t DrawCallCount() const;               // 上次 End 发出的 drawcall 数(run 数)
```

调用方须已绑定 RT/视口/裁剪、已清屏、已 `SetDescriptorHeaps`。

### `me::render::OrthographicCamera`(header-only)

```cpp
void SetViewportSize(float width, float height); // 通常等于渲染目标尺寸
void SetPosition(const Vector2& position);       // 相机中心(世界像素坐标)
void SetZoom(float zoom);                        // >0;>1 放大(可见范围缩小)

Matrix4x4 ViewProj() const; // 世界→NDC 正交矩阵,供 SpriteBatch::Begin 使用
```

`ViewProj()` 复用 `Matrix4x4::Orthographic`,将可见矩形 `[pos ± halfExtent/zoom]` 映射到 NDC。
行向量约定(v' = v * viewProj),与 DX12/DirectXMath 同源。

### `me::render::Tileset`

运行时 tileset:将 `TilesetDesc` 绑定到已上传的 `GpuTexture`。非拥有纹理指针,生命周期归调用方。

```cpp
Tileset(const me::rhi::GpuTexture* texture, me::assets::TilesetDesc desc);

const me::rhi::GpuTexture* Texture() const;

/// 全局 gid → 归一化采样 UV(调用方须保证 gid != kEmptyTileGid)。
me::Rect SrcRectForGid(int gid) const;
```

### `me::render::TileMapRenderer`

瓦片地图渲染器薄层:逐层、逐可见瓦片算 dst/src 矩形并 Submit 给 SpriteBatch。

```cpp
void Render(SpriteBatch& batch, const OrthographicCamera& camera,
            const me::assets::TileMapData& map, const Tileset& tileset) const;
```

**复用 SpriteBatch:**`TileMapRenderer` 不自行 Begin/End,由调用方包裹:

```cpp
batch.Begin(camera.ViewProj());
tileRenderer.Render(batch, camera, map, tileset);
batch.End(cmd);
```

此设计使瓦片与精灵可在同一 Begin/End 内混合提交,合批边界由调用方控制。
整张单 tileset 地图所有可见瓦片共享同一纹理,SpriteBatch 合批后产生 **1 次 drawcall**。

**视野裁剪:**根据相机可见矩形只提交可见瓦片列/行范围;跳过 `gid == kEmptyTileGid` 的空格(减少无效 Submit)。

**层序:**`TileMapData.layers` 数组顺序绘制,首层在底(ground 层先于 decor 层)。

## 着色器

`assets/shaders/sprite.hlsl`:顶点接收 pos+uv+color,uniform `uViewProj`,PS 采样纹理后乘以 color 色调。

## RenderView → SpriteBatch 桥接约定(M4)

`me::scene::RenderSystem::BuildRenderView` 产出的 `RenderView`(即 `std::vector<RenderItem>`)
使用 RHI 无关的 `uint32_t textureId` 引用纹理,Scene 层不持有 `GpuTexture*`。
渲染边界(当前为 sandbox,未来为 Engine 层)负责将 `textureId` 解析为 `GpuTexture*`,
再填入 `SpriteDesc` 提交给 `SpriteBatch`：

```cpp
// textureId → GpuTexture* 解析表(调用方维护,生命周期归调用方)。
std::vector<const rhi::GpuTexture*> textureTable;
textureTable.push_back(tileTex.get()); // index 0 = tileset 纹理

batch->Begin(camera.ViewProj());
// 地面瓦片:TileMapComponent → TileMapRenderer
tileRenderer.Render(*batch, camera, *tm->map, tileset);
// Scene 精灵:RenderItem → SpriteDesc 桥接
for (const scene::RenderItem& it : renderView) {
    render::SpriteDesc d;
    d.texture = textureTable[it.textureId]; // textureId 解析点
    d.srcRect  = it.srcRect;
    d.dstRect  = it.dstRect;
    d.color    = it.color;
    d.rotation = it.rotation;
    batch->Submit(d);
}
batch->End(cmd);
```

瓦片与精灵在同一 `Begin/End` 内提交;单图集时 SpriteBatch 合批 → **1 次 drawcall**。
稳定纹理排序保留 `RenderSystem` 按层 + 世界 Y 降序产出的 2.5D 叠压顺序。

## 模块说明

- M1 `SpriteRenderer`(单精灵,根签名+PSO+单位四边形)已于 M2 退役并入 `SpriteBatch`。
- M2 暂不引入 AssetManager 句柄化(texture 指针生命周期由调用方管理)。
- 帧并行(FrameRing/per-frame fence)与默认堆迁移推迟到后续性能里程碑。
