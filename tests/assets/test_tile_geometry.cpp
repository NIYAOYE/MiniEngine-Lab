#include <doctest/doctest.h>

#include "me/assets/TileGeometry.h"

using namespace me::assets;

namespace {
constexpr float kEps = 1e-4f;

// 4 列 x 3 行,32px 瓦片(地图 128x96 像素)。
TileMapData MakeMap() {
    TileMapData m;
    m.mapCols = 4; m.mapRows = 3;
    m.tileWidth = 32; m.tileHeight = 32;
    return m;
}
} // namespace

TEST_CASE("TileWorldRect:Tiled 顶行 row0 映射到最大世界 Y") {
    const TileMapData m = MakeMap();
    // row0 在顶部 → 世界 Y = (mapRows-1-0)*32 = 64。
    const me::Rect top = TileWorldRect(m, 0, 0);
    CHECK(top.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(top.y == doctest::Approx(64.0f).epsilon(kEps));
    CHECK(top.width == doctest::Approx(32.0f).epsilon(kEps));
    // 底行 row2 → 世界 Y = 0。
    const me::Rect bottom = TileWorldRect(m, 0, 2);
    CHECK(bottom.y == doctest::Approx(0.0f).epsilon(kEps));
    // 第 3 列 col2 → 世界 X = 64。
    CHECK(TileWorldRect(m, 2, 0).x == doctest::Approx(64.0f).epsilon(kEps));
}

TEST_CASE("VisibleTileRange:覆盖整图返回全范围") {
    const TileRange r = VisibleTileRange(MakeMap(), 0.0f, 128.0f, 0.0f, 96.0f);
    CHECK_FALSE(r.empty);
    CHECK(r.colMin == 0); CHECK(r.colMax == 3);
    CHECK(r.rowMin == 0); CHECK(r.rowMax == 2);
}

TEST_CASE("VisibleTileRange:只看左下一格(世界 [0,32)x[0,32))→ 底行第一列") {
    // 世界 y∈[0,32) 是底行 row2;x∈[0,32) 是 col0。
    const TileRange r = VisibleTileRange(MakeMap(), 0.0f, 31.0f, 0.0f, 31.0f);
    CHECK_FALSE(r.empty);
    CHECK(r.colMin == 0); CHECK(r.colMax == 0);
    CHECK(r.rowMin == 2); CHECK(r.rowMax == 2);
}

TEST_CASE("VisibleTileRange:完全在图外返回 empty") {
    const TileRange r = VisibleTileRange(MakeMap(), 1000.0f, 1100.0f, 0.0f, 96.0f);
    CHECK(r.empty);
}
