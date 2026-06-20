#pragma once

#include <optional>
#include <string>

#include "me/assets/TileMapData.h"

namespace me::assets {

/**
 * @brief 从 Tiled JSON(.tmj)加载正交瓦片地图。
 *
 * 仅支持 orientation=orthogonal、tilelayer(整数 data 数组)、单个嵌入式 tileset。
 * tileset.imagePath 解析为相对地图文件所在目录的路径。
 * 失败(文件不可读 / JSON 非法 / 字段缺失 / 非 orthogonal / 无瓦片层 / 无 tileset)
 * 返回 std::nullopt 并记录 ME_LOG_ERROR;不抛异常。
 */
std::optional<TileMapData> LoadTiledMap(const std::string& path);

} // namespace me::assets
