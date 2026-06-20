#pragma once

#include "me/assets/TileMapData.h"
#include "me/render/OrthographicCamera.h"
#include "me/render/Tileset.h"

namespace me::render {

class SpriteBatch;

/**
 * @brief 瓦片地图渲染器(薄层):逐层、逐可见瓦片算 dst/src 矩形并 Submit 给 SpriteBatch。
 *
 * 不自行 Begin/End,合批边界由调用方控制:
 *   batch.Begin(camera.ViewProj()); renderer.Render(batch, camera, map, tileset); batch.End(cmd);
 * 按相机可见矩形裁剪,只提交可见瓦片;跳过 gid==kEmptyTileGid 的空格。
 * 层按数组顺序绘制(首层在底)。
 */
class TileMapRenderer {
public:
    void Render(SpriteBatch& batch, const OrthographicCamera& camera,
                const me::assets::TileMapData& map, const Tileset& tileset) const;
};

} // namespace me::render
