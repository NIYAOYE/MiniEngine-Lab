#pragma once

#include <string>
#include <vector>

namespace me::assets {

/// 空瓦片 gid(Tiled 约定:0 表示该格无瓦片)。
constexpr int kEmptyTileGid = 0;

/**
 * @brief 嵌入式 tileset 描述(单 tileset)。所有像素尺寸来自数据,源码不硬编码。
 *
 * imagePath 为已解析为可加载的图片路径;columns 为图集每行瓦片数;
 * margin 为图集外边距、spacing 为瓦片间距(像素);firstGid 为该 tileset 起始全局 id。
 */
struct TilesetDesc {
    std::string imagePath;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int margin = 0;
    int spacing = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    int firstGid = 1;
};

/// 单个瓦片层:gids 行主序(row0 在顶部),大小 = mapCols*mapRows。
struct TileLayer {
    std::string name;
    std::vector<int> gids;
};

/// 一张正交瓦片地图(单 tileset、多层)。
struct TileMapData {
    int mapCols = 0;
    int mapRows = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    std::vector<TileLayer> layers;
    TilesetDesc tileset;
};

} // namespace me::assets
