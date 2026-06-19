#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace me::assets {

/** @brief 解码后的位图:紧凑 RGBA8(每像素 4 字节,无行填充)。 */
struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // 大小 = width*height*4

    bool IsValid() const {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }
};

/** @brief 从磁盘加载图片并转 RGBA8;失败返回 std::nullopt(不抛异常)。 */
std::optional<ImageData> LoadImageRGBA8(const std::string& path);

/** @brief 从内存字节解码图片为 RGBA8;失败返回 std::nullopt。 */
std::optional<ImageData> DecodeImageRGBA8(const std::uint8_t* bytes,
                                          std::size_t size);

} // namespace me::assets
