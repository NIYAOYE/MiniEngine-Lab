#pragma once

#include <algorithm>
#include <cmath>

#include "me/core/Rect.h"
#include "me/core/Assert.h"
#include "me/assets/TileMapData.h"

namespace me::assets {

/// 可见瓦片闭区间 [colMin,colMax] x [rowMin,rowMax];empty 时区间无意义。
struct TileRange {
    int colMin = 0;
    int colMax = 0;
    int rowMin = 0;
    int rowMax = 0;
    bool empty = true;
};

/**
 * @brief 瓦片 (col,row) 的世界目标矩形(像素,左下原点,Y 向上)。
 *
 * Tiled 数据 row0 在顶部、Y 向下;此处翻转为引擎世界:worldY = (mapRows-1-row)*tileH。
 */
inline me::Rect TileWorldRect(const TileMapData& map, int col, int row) {
    ME_ASSERT_MSG(map.tileWidth > 0 && map.tileHeight > 0, "tile 尺寸必须为正");
    const float x = float(col * map.tileWidth);
    const float y = float((map.mapRows - 1 - row) * map.tileHeight);
    return me::Rect{x, y, float(map.tileWidth), float(map.tileHeight)};
}

/**
 * @brief 与可见世界矩形相交的瓦片范围(闭区间),用于只提交可见瓦片。
 *
 * 入参为相机可见世界矩形边界(像素)。返回的行号已按 Tiled row0 在顶约定换算:
 * 世界 Y 越大行号越小。完全不可见时 empty=true。
 */
inline TileRange VisibleTileRange(const TileMapData& map,
                                  float viewLeft, float viewRight,
                                  float viewBottom, float viewTop) {
    ME_ASSERT_MSG(map.tileWidth > 0 && map.tileHeight > 0, "tile 尺寸必须为正");
    TileRange r;
    const float mapRight = float(map.mapCols * map.tileWidth);
    const float mapTop = float(map.mapRows * map.tileHeight);
    // 与地图范围相交判定(半开区间)。
    if (viewRight <= 0.0f || viewLeft >= mapRight ||
        viewTop <= 0.0f || viewBottom >= mapTop) {
        return r; // empty
    }
    const int colMin = std::max(0, int(std::floor(viewLeft / map.tileWidth)));
    const int colMax = std::min(map.mapCols - 1,
                                int(std::floor((viewRight - 1e-4f) / map.tileWidth)));
    // 世界 Y → Tiled 行:worldY 越大行号越小;故 viewTop 给出最小行、viewBottom 给出最大行。
    const int rowMin = std::max(0,
        (map.mapRows - 1) - int(std::floor((viewTop - 1e-4f) / map.tileHeight)));
    const int rowMax = std::min(map.mapRows - 1,
        (map.mapRows - 1) - int(std::floor(viewBottom / map.tileHeight)));
    r.colMin = colMin; r.colMax = colMax;
    r.rowMin = rowMin; r.rowMax = rowMax;
    r.empty = (colMin > colMax || rowMin > rowMax);
    return r;
}

} // namespace me::assets
