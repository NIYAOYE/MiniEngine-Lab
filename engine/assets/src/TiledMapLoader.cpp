#include "me/assets/TiledMapLoader.h"

#include <nlohmann/json.hpp>

#include "me/platform/FileSystem.h"
#include "me/core/Log.h"

namespace me::assets {

namespace {
using json = nlohmann::json;

// 取出地图文件所在目录(用于解析 tileset 图片相对路径);无分隔符则为空。
std::string DirOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
}
} // namespace

std::optional<TileMapData> LoadTiledMap(const std::string& path) {
    const auto text = me::platform::ReadTextFile(path);
    if (!text.has_value()) {
        ME_LOG_ERROR("LoadTiledMap: 无法读取地图文件");
        return std::nullopt;
    }

    // 非抛出解析:失败返回 discarded 值(不使用 C++ 异常)。
    const json root = json::parse(*text, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        ME_LOG_ERROR("LoadTiledMap: JSON 解析失败");
        return std::nullopt;
    }

    if (root.value("orientation", std::string()) != "orthogonal") {
        ME_LOG_ERROR("LoadTiledMap: 仅支持 orthogonal 地图");
        return std::nullopt;
    }

    TileMapData map;
    map.mapCols   = root.value("width", 0);
    map.mapRows   = root.value("height", 0);
    map.tileWidth = root.value("tilewidth", 0);
    map.tileHeight = root.value("tileheight", 0);
    if (map.mapCols <= 0 || map.mapRows <= 0 ||
        map.tileWidth <= 0 || map.tileHeight <= 0) {
        ME_LOG_ERROR("LoadTiledMap: 地图尺寸字段缺失或非法");
        return std::nullopt;
    }

    // 瓦片层:仅取 type=="tilelayer" 且 data 为整数数组,且数量匹配。
    const auto layersIt = root.find("layers");
    if (layersIt == root.end() || !layersIt->is_array()) {
        ME_LOG_ERROR("LoadTiledMap: 缺少 layers");
        return std::nullopt;
    }
    const size_t expected = size_t(map.mapCols) * size_t(map.mapRows);
    for (const auto& layer : *layersIt) {
        if (layer.value("type", std::string()) != "tilelayer") continue;
        const auto dataIt = layer.find("data");
        if (dataIt == layer.end() || !dataIt->is_array() ||
            dataIt->size() != expected) {
            ME_LOG_ERROR("LoadTiledMap: tilelayer data 缺失或长度不符");
            return std::nullopt;
        }
        TileLayer tl;
        tl.name = layer.value("name", std::string());
        tl.gids.reserve(expected);
        for (const auto& g : *dataIt) tl.gids.push_back(g.get<int>());
        map.layers.push_back(std::move(tl));
    }
    if (map.layers.empty()) {
        ME_LOG_ERROR("LoadTiledMap: 无可用 tilelayer");
        return std::nullopt;
    }

    // 单个嵌入式 tileset(取第一个含 image 的)。
    const auto tsIt = root.find("tilesets");
    if (tsIt == root.end() || !tsIt->is_array() || tsIt->empty()) {
        ME_LOG_ERROR("LoadTiledMap: 缺少 tilesets");
        return std::nullopt;
    }
    const json& ts = tsIt->front();
    if (!ts.contains("image")) {
        ME_LOG_ERROR("LoadTiledMap: 仅支持嵌入式 tileset(需含 image)");
        return std::nullopt;
    }
    TilesetDesc& d = map.tileset;
    d.firstGid     = ts.value("firstgid", 1);
    d.imagePath    = DirOf(path) + ts.value("image", std::string());
    d.imageWidth   = ts.value("imagewidth", 0);
    d.imageHeight  = ts.value("imageheight", 0);
    d.tileWidth    = ts.value("tilewidth", map.tileWidth);
    d.tileHeight   = ts.value("tileheight", map.tileHeight);
    d.columns      = ts.value("columns", 0);
    d.margin       = ts.value("margin", 0);
    d.spacing      = ts.value("spacing", 0);
    if (d.columns <= 0 || d.imageWidth <= 0 || d.imageHeight <= 0) {
        ME_LOG_ERROR("LoadTiledMap: tileset 尺寸/列数字段缺失或非法");
        return std::nullopt;
    }

    return map;
}

} // namespace me::assets
