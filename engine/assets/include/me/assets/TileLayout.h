#pragma once

#include "me/core/Rect.h"
#include "me/core/Assert.h"
#include "me/assets/TileMapData.h"

namespace me::assets {

/**
 * @brief 由 tileset 描述与局部瓦片 id(已减 firstGid)算归一化采样 UV。
 *
 * 返回 me::Rect:x=uMin,y=vMin(V 向下),width/height=UV 跨度,与 SpriteDesc::srcRect 同约定。
 * localId 按行主序展开:col = localId % columns,row = localId / columns。
 */
inline me::Rect SrcRectForLocalId(const TilesetDesc& ts, int localId) {
    ME_ASSERT_MSG(ts.columns > 0 && ts.imageWidth > 0 && ts.imageHeight > 0,
                  "TilesetDesc 尺寸/列数必须为正");
    ME_ASSERT_MSG(localId >= 0, "localId 不可为负");
    const int col = localId % ts.columns;
    const int row = localId / ts.columns;
    const float px = float(ts.margin + col * (ts.tileWidth + ts.spacing));
    const float py = float(ts.margin + row * (ts.tileHeight + ts.spacing));
    return me::Rect{
        px / float(ts.imageWidth),
        py / float(ts.imageHeight),
        float(ts.tileWidth) / float(ts.imageWidth),
        float(ts.tileHeight) / float(ts.imageHeight),
    };
}

} // namespace me::assets
