# engine/assets(me_assets)

资源解码层。M1 含图片解码(stb_image)→ 紧凑 RGBA8 `ImageData`。M3 新增 Tiled JSON 地图加载器。
跨平台,可在 WSL 单测。依赖:Core, Platform(单向)。
后续里程碑扩展 AssetManager/句柄/图集/内容 JSON。

## 公开类型

### `me::assets::ImageData`

stb_image 解码结果:RGBA8 像素字节数组 + `width`/`height`。

```cpp
std::optional<ImageData> LoadImageRGBA8(const std::string& path);
```

### `me::assets::TileMapData` / `TilesetDesc` / `TileLayer`

```cpp
struct TilesetDesc {
    std::string imagePath;  // 已解析为可加载的绝对/相对路径
    int tileWidth, tileHeight, columns, margin, spacing;
    int imageWidth, imageHeight, firstGid;
};

struct TileLayer {
    std::string name;
    std::vector<int> gids;  // 行主序,row0 在顶部;0=空格(kEmptyTileGid)
};

struct TileMapData {
    int mapCols, mapRows, tileWidth, tileHeight;
    std::vector<TileLayer> layers;
    TilesetDesc tileset;
};
```

## Tiled 地图加载

```cpp
std::optional<TileMapData> LoadTiledMap(const std::string& path);
```

**支持的 Tiled JSON 子集:**
- `orientation` 必须为 `"orthogonal"`。
- `layers` 中仅处理 `type == "tilelayer"` 的层(忽略 objectgroup 等)。
- 只读第一个嵌入式 `tilesets` 条目(单 tileset 约定)。
- `data` 为整数数组(gid=0 表示空格)。

**`TilesetDesc.imagePath` 解析规则:**
加载器将 `tilesets[0].image`(相对路径)拼接到 `.tmj` 文件所在目录,得到可直接传给 `LoadImageRGBA8` 的路径。
例:`assets/maps/demo.tmj` + `"../textures/tileset.png"` → `assets/maps/../textures/tileset.png`。

**错误返回约定:**
所有错误(文件不可读、JSON 非法、字段缺失、非 orthogonal、无瓦片层、无 tileset)均返回 `std::nullopt` 并通过 `ME_LOG_ERROR` 记录原因;不抛异常。

## 纯函数工具

### `me::assets::TileLayout`(`TileLayout.h`)

```cpp
/// gid 本地索引(local id = gid - firstGid) → 归一化 UV srcRect。
me::Rect SrcRectForLocalId(const TilesetDesc& desc, int localId);
```

### `me::assets::TileGeometry`(`TileGeometry.h`)

```cpp
/// 瓦片行列坐标 → 世界 dstRect(像素,原点左上,Y 向下)。
me::Rect TileWorldRect(int col, int row, int tileW, int tileH);
```

两个函数均为无副作用纯函数,可在 WSL 脱离 GPU 环境单独单测。
