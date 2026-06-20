#include <doctest/doctest.h>

#include "me/assets/TiledMapLoader.h"

using me::assets::LoadTiledMap;

namespace {
// 由 CMake 注入的 fixture 目录宏(见 tests/CMakeLists.txt)。
std::string Fixture(const char* name) {
    return std::string(ME_TEST_FIXTURE_DIR) + "/" + name;
}
} // namespace

TEST_CASE("LoadTiledMap:解析合法地图——尺寸/层/gid/tileset") {
    auto map = LoadTiledMap(Fixture("demo.tmj"));
    REQUIRE(map.has_value());
    CHECK(map->mapCols == 4);
    CHECK(map->mapRows == 3);
    CHECK(map->tileWidth == 16);
    CHECK(map->tileHeight == 16);
    REQUIRE(map->layers.size() == 2);
    CHECK(map->layers[0].name == "ground");
    REQUIRE(map->layers[0].gids.size() == 12);
    CHECK(map->layers[0].gids[0] == 1);
    CHECK(map->layers[0].gids[10] == 4);
    CHECK(map->layers[1].gids[5] == 5); // decor 层中部一个瓦片
    CHECK(map->tileset.columns == 4);
    CHECK(map->tileset.firstGid == 1);
    CHECK(map->tileset.imageWidth == 64);
}

TEST_CASE("LoadTiledMap:文件不存在 → nullopt") {
    CHECK_FALSE(LoadTiledMap(Fixture("does_not_exist.tmj")).has_value());
}

TEST_CASE("LoadTiledMap:非法 JSON → nullopt(不抛异常)") {
    CHECK_FALSE(LoadTiledMap(Fixture("bad.tmj")).has_value());
}
