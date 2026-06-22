# assets —— 美术/数据资产

AI 生成的大图源放在仓库根 `largeImage/`(1254×1254 RGB),经 `tools/pack_atlas.py` 处理为下列引擎可用资产。
重跑:`python tools/pack_atlas.py`(需 pillow)。

## textures/

### ground_tileset.png(地面图集,128×64,32px 瓦片,4 列)
供 Tiled 地图引用。`gid = firstgid(1) + localId`:

| gid | localId | 瓦片 |
|-----|---------|------|
| 1 | 0 | 草地 grass |
| 2 | 1 | 泥路 dirt path |
| 3 | 2 | 耕地 tilled soil |
| 4 | 3 | 浅水 water |
| 5 | 4 | 石板 stone floor |
| 6 | 5 | 沙地 sand |
| 7 | 6 | 木地板 plank |

### 物件精灵(各 64×64,RGBA,白底已抠透明)
`prop_small_leafy_farm_tree.png` / `prop_gray_boulder_rock.png` / `prop_wooden_fence_segment.png` /
`prop_watering_can.png` / `prop_wooden_crate.png` / `prop_sign_post.png` / `prop_bush_with_berries.png`
作为独立精灵纹理用(`LoadImageRGBA8` + `GpuTexture::Create`,各自一个 textureId)。

### player_sprite.png(64×64,RGBA)
角色单帧正面立绘(逐帧动画系统未落地,先单帧)。

### 既有 demo 资产(保留不动)
`tileset.png`(16px 占位图集)/ `test_sprite.png` —— M1~M4 demo 用,勿覆盖。

## maps/

- `farm_demo.tmj` —— 12×8 正交地图,引用 `ground_tileset.png`(草地基底 + 泥路 + 耕地 + 水塘 + 沙角 + 石板底排)。
- `demo.tmj` —— 旧 16px 占位地图(沿用)。

## 处理管线说明(tools/pack_atlas.py)
- **地面瓦片**:满帧无缝纹理 → LANCZOS 降采样到 32×32(源是平滑插画非真像素画,故用 LANCZOS 而非 NEAREST)。
- **物件/角色**:模型把背景烤成近白底而非透明 → 从四边洪水填充连通近白像素抠为透明(保留物体内部白色)→ 按内容包围盒裁剪 → 等比缩放居中装入 64×64 透明方框。

> `largeImage/Entire_ground_atlas.png` 是「整图集直生」尝试,因网格对不齐未采用;地面瓦片用逐张 B1 源更干净。
