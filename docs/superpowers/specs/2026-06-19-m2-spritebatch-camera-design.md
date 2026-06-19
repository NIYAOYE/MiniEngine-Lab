# M2 设计文档 —— 批渲染 + 正交相机

- **日期**:2026-06-19
- **里程碑**:M2(承接已完成的 M1 精灵上屏)
- **状态**:已与用户确认
- **上游设计**:[2026-06-17-miniengine-design.md](2026-06-17-miniengine-design.md) §5 Renderer、§13 路线图

## 1. 目标与范围

在 M1「单精灵经 MVP 上屏」的基础上,建设 2D 渲染管线的合批能力与可控相机:

- **SpriteBatch**:把多个精灵累积后按纹理合批,产出最少 drawcall。学习重点是「为什么合批能减少 drawcall」与动态顶点缓冲的填充。
- **OrthographicCamera**:持 position + zoom + viewport,产出 `viewProj` 矩阵;世界空间 Y 轴向上、像素为单位(延续 M1 约定)。
- **多精灵 + 图集采样 + 每精灵色调**:顶点格式一次到位,使 M3 瓦片地图可直接复用。

### 明确不做(YAGNI / 推迟)

- **帧并行(FrameRing / per-frame fence、CPU/GPU 重叠)**:延续 M1 每帧 `fence->Flush` 全同步。`rhi/include` 已存在 `FrameRing.h`,但其接入留作后续性能里程碑(M2.5 或并入更晚的性能专项),不在 M2 范围。
- **默认堆 + 一次性拷贝**:动态顶点缓冲天然每帧 CPU 写入,延续 **上传堆**。默认堆迁移留给性能里程碑。
- **AssetManager / 句柄化**:M2 仍以裸 `GpuTexture*`(由 RHI 创建、生命周期归 sandbox/测试)作为提交单元里的纹理引用,不引入 Assets 层。句柄化在 M4 Scene 起。
- **排序(图层 + Y 排序的 2.5D 遮挡)**:M2 仅做「按纹理分组以合批」,不做语义排序。语义排序留给 M4 RenderSystem。

## 2. 架构与新增单元

三个新单元全部落在 `engine/renderer`(Scene 层尚未存在,相机暂居 renderer):

| 单元 | 职责 | 依赖 |
|------|------|------|
| `OrthographicCamera` | 持 position + zoom + viewport,产出 `viewProj` 矩阵 | Core |
| `SpriteDesc`(POD 提交单元) | `{ const GpuTexture* texture, Rect srcRect, Rect dstRect, Vector4 color, float rotation }` | Core, RHI(仅前置声明 GpuTexture) |
| `SpriteBatch` | 累积 `SpriteDesc` → 填动态顶点缓冲 → 按纹理合批 → 录制最少 drawcall;拥有根签名/PSO/着色器/共享四边形 IB | RHI, Core |

### SpriteRenderer 的处置

M1 的 `SpriteRenderer`(单精灵、单元四边形 VB/IB、经根常量传 MVP)**退役**。其职责被 `SpriteBatch` 吸收:

- PSO / 根签名 / 着色器加载的拥有权移入 `SpriteBatch`。
- 单元四边形被「每精灵在 CPU 端展开 4 顶点」的动态 VB 取代。
- M1 的 GPU 测试 `test_sprite_render` 改打 `SpriteBatch`。
- 删除 `engine/renderer/include/me/render/SpriteRenderer.h` 与对应 `.cpp`,并同步更新 `engine/renderer/CMakeLists.txt`、`engine/renderer/README.md`、sandbox 引用。

### SpriteDesc 字段语义

- `srcRect`:在纹理中的采样子区域。约定用**归一化 UV**(0..1),由调用方或后续 Atlas 辅助函数从像素换算;整贴图传 `{0,0,1,1}`。
- `dstRect`:目标世界空间矩形(像素单位,原点左下,Y 向上)。位置与尺寸合一,取代 M1 的 `MakeSpriteModelMatrix`。
- `color`:RGBA 线性色调,逐顶点写入,着色器与采样结果相乘。整色传 `{1,1,1,1}`。
- `rotation`:绕 `dstRect` 中心的弧度旋转,CPU 端烘进顶点位置。

## 3. 数据流与帧循环

```
sandbox 每帧:
  camera.SetPosition(...) / SetZoom(...)            // 输入驱动
  Matrix4x4 vp = camera.ViewProj();
  batch.Begin(vp);                                  // 清 CPU 端累积、记录 viewProj
  for each sprite:
      batch.Submit(SpriteDesc{tex, src, dst, color, rot});  // 仅写 CPU 端数组,不碰 GPU
  batch.End(cmd);   // 排序(按纹理分组)→ 填动态 VB → 逐组 SetGraphicsRootDescriptorTable + DrawIndexedInstanced
```

- **顶点格式**:`pos(float2) + uv(float2) + color(float4)`。模型变换(dstRect 平移缩放 + rotation)在 **CPU 端烘进顶点位置**;合批后无法每精灵传根常量矩阵,故每精灵展开 4 个已变换顶点。
- **索引**:共享一个静态四边形 IB(每精灵 6 索引,`0 1 2 / 0 2 3` 偏移),避免重复上传索引。
- **合批策略**:`End` 时把累积的精灵**按纹理分组**(稳定分组,保持提交顺序内的相对次序);每组一次 `SetGraphicsRootDescriptorTable(SRV)` + 一次 draw。纹理切换或达到容量上限触发 flush。
- **根签名**:b0 用 **32-bit 根常量**(16 个 float = 一个 `float4x4` viewProj,延续 M1 根常量风格)+ SRV 描述符表(t0)+ 静态采样器(s0)。viewProj 在 `End` 内每组 draw 前设置(同一帧各组相同)。

## 4. 着色器变更(`assets/shaders/sprite.hlsl`)

- `cbuffer SpriteConstants`:`uMVP` 改名/语义改为 `uViewProj`(模型变换已烘进顶点,着色器只做世界→裁剪)。
- `VSInput` 增加 `float4 color : COLOR`。
- `PSInput` 透传 `color`。
- `VSMain`:`o.position = mul(float4(input.position, 0, 1), uViewProj)`;`o.color = input.color`。
- `PSMain`:`return gTexture.Sample(gSampler, input.uv) * input.color`。

## 5. 错误处理与约束(对齐 CLAUDE.md)

- **零魔法数字**:容量上限具名 `constexpr kMaxSpritesPerBatch`;每精灵顶点数/索引数、四边形局部坐标等均具名常量。
- **容量溢出**:`Submit` 超过 `kMaxSpritesPerBatch` 时**自动 flush 续批**(`End` 内分多次 draw),不静默丢弃。
- **不变量**:`Submit` 必须在 `Begin` 与 `End` 之间 → `ME_ASSERT`;`Begin/End` 配对 → `ME_ASSERT`。
- **资源失败**:PSO/根签名/着色器创建失败 → `Create` 返回空 `unique_ptr`,sandbox/测试检查并 `ME_LOG_ERROR`;`SpriteDesc.texture == nullptr` → 该精灵跳过并记日志(可恢复)。
- **DX12 HRESULT** 一律 `ME_HR_CHECK`。不使用 C++ 异常。

## 6. 测试策略

| 层 | 方式 | 断言 |
|----|------|------|
| `OrthographicCamera` | 纯 CPU,doctest | 已知 position/zoom/viewport → `ViewProj()` 把指定世界点映射到期望 NDC(含原点、缩放、平移三组用例) |
| `SpriteBatch` 合批 | WARP 离屏像素回读(扩展现有 `tests/gpu/test_sprite_render`) | 渲染 2~3 个不同位置/不同色调/(含一个用 srcRect 子区域)的精灵,断言各自像素中心颜色正确;验证合批正确性 + 色调相乘 + 相机变换 |
| sandbox 目视 | 真机运行 | 多个静态精灵铺一片;WASD 平移相机、Q/E 缩放、ESC 退出;贴图正立、合批后画面正确 |

- 跨平台逻辑(相机数学)在 WSL 用 doctest 红绿。
- GPU 正确性由 WARP + 像素回读把关,辅以 sandbox 目视(延续 M1 验证范式)。

## 7. sandbox 演示变更

从 M1 的「移动单个精灵」改为:

- 以多个静态精灵(同纹理多数、可含 srcRect 子区域示例)铺一片场景,验证合批。
- **WASD 平移相机**、**Q/E 缩放相机**,ESC 退出。
- 移除 M1 的单精灵 `MakeSpriteModelMatrix` 直调路径,改走 `batch.Submit`。

## 8. 受影响文件清单(预期)

- 新增:`engine/renderer/include/me/render/{OrthographicCamera.h, SpriteBatch.h, SpriteDesc.h}` 与对应 `src/*.cpp`。
- 修改:`assets/shaders/sprite.hlsl`、`engine/renderer/CMakeLists.txt`、`engine/renderer/README.md`、`sandbox/main.cpp`、`tests/gpu/test_sprite_render.cpp`、`tests/CMakeLists.txt`(新增相机单测)。
- 删除:`engine/renderer/include/me/render/SpriteRenderer.h`、`engine/renderer/src/SpriteRenderer.cpp`。
- 可能复用:`engine/rhi/include/me/rhi/QuadGeometry.h`、`SpriteTransform.h`(评估是否并入或保留)。

## 9. 完成定义(DoD)

- [ ] 相机单测在 WSL doctest 绿。
- [ ] `test_sprite_render` 扩展为多精灵 + 色调 + srcRect,WARP 像素回读绿。
- [ ] sandbox 真机:多精灵合批显示、相机平移/缩放/退出可用、贴图正立。
- [ ] M1 `SpriteRenderer` 已删除且无悬挂引用;`CMakeLists.txt` / README 同步。
- [ ] `docs/PROGRESS.md` 回写 M2 ☑ 及 ADR(合批策略/上传堆延续/帧并行推迟)。
