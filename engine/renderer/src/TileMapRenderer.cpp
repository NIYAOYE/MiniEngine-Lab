#include "me/render/TileMapRenderer.h"

#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"
#include "me/assets/TileGeometry.h"
#include "me/core/Vector2.h"

namespace me::render {

void TileMapRenderer::Render(SpriteBatch& batch, const OrthographicCamera& camera,
                             const me::assets::TileMapData& map,
                             const Tileset& tileset) const {
    // 由相机导出可见世界矩形(与 OrthographicCamera::ViewProj 同口径)。
    const me::Vector2 pos = camera.Position();
    const float halfW = (camera.ViewportWidth() * 0.5f) / camera.Zoom();
    const float halfH = (camera.ViewportHeight() * 0.5f) / camera.Zoom();
    const me::assets::TileRange range = me::assets::VisibleTileRange(
        map, pos.x - halfW, pos.x + halfW, pos.y - halfH, pos.y + halfH);
    if (range.empty) return;

    for (const auto& layer : map.layers) {
        for (int row = range.rowMin; row <= range.rowMax; ++row) {
            for (int col = range.colMin; col <= range.colMax; ++col) {
                const int gid = layer.gids[size_t(row) * map.mapCols + col];
                if (gid == me::assets::kEmptyTileGid) continue;
                SpriteDesc d;
                d.texture = tileset.Texture();
                d.srcRect = tileset.SrcRectForGid(gid);
                d.dstRect = me::assets::TileWorldRect(map, col, row);
                batch.Submit(d);
            }
        }
    }
}

} // namespace me::render
