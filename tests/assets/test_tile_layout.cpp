#include <doctest/doctest.h>

#include "me/assets/TileLayout.h"

using me::assets::TilesetDesc;
using me::assets::SrcRectForLocalId;

namespace {
constexpr float kEps = 1e-5f;

// 64x64 图集,4 列 4 行,16px 瓦片,无 margin/spacing。
TilesetDesc MakeDesc() {
    TilesetDesc ts;
    ts.tileWidth = 16; ts.tileHeight = 16;
    ts.columns = 4; ts.margin = 0; ts.spacing = 0;
    ts.imageWidth = 64; ts.imageHeight = 64;
    ts.firstGid = 1;
    return ts;
}
} // namespace

TEST_CASE("TileLayout:localId 0 → 左上 texel 块,归一化 UV") {
    const me::Rect uv = SrcRectForLocalId(MakeDesc(), 0);
    CHECK(uv.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(uv.y == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(uv.width == doctest::Approx(0.25f).epsilon(kEps));  // 16/64
    CHECK(uv.height == doctest::Approx(0.25f).epsilon(kEps));
}

TEST_CASE("TileLayout:列回绕——localId == columns 进入第二行第一列") {
    const me::Rect uv = SrcRectForLocalId(MakeDesc(), 4); // col=0,row=1
    CHECK(uv.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(uv.y == doctest::Approx(0.25f).epsilon(kEps));
}

TEST_CASE("TileLayout:含 margin/spacing 的偏移") {
    TilesetDesc ts = MakeDesc();
    ts.margin = 1; ts.spacing = 2; ts.imageWidth = 71; ts.imageHeight = 71;
    // localId 1 = col=1,row=0:像素 x = margin + col*(tileW+spacing) = 1 + 1*18 = 19
    const me::Rect uv = SrcRectForLocalId(ts, 1);
    CHECK(uv.x == doctest::Approx(19.0f / 71.0f).epsilon(kEps));
    CHECK(uv.y == doctest::Approx(1.0f / 71.0f).epsilon(kEps));
    CHECK(uv.width == doctest::Approx(16.0f / 71.0f).epsilon(kEps));
}
