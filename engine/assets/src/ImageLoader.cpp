#include "me/assets/ImageData.h"

#include "stb_image.h"
#include "me/platform/FileSystem.h"

namespace me::assets {

namespace {
constexpr int kRequiredChannels = 4; // 强制 RGBA
} // namespace

std::optional<ImageData> DecodeImageRGBA8(const std::uint8_t* bytes,
                                          std::size_t size) {
    int w = 0, h = 0, channelsInFile = 0;
    stbi_uc* decoded = stbi_load_from_memory(
        bytes, static_cast<int>(size), &w, &h, &channelsInFile, kRequiredChannels);
    if (decoded == nullptr) {
        return std::nullopt; // 解码失败:非图片/损坏
    }
    ImageData img;
    img.width = w;
    img.height = h;
    img.pixels.assign(decoded,
                      decoded + static_cast<std::size_t>(w) * h * kRequiredChannels);
    stbi_image_free(decoded);
    return img;
}

std::optional<ImageData> LoadImageRGBA8(const std::string& path) {
    auto bytes = me::platform::ReadBinaryFile(path);
    if (!bytes.has_value()) {
        return std::nullopt; // 文件不存在/不可读
    }
    return DecodeImageRGBA8(bytes->data(), bytes->size());
}

} // namespace me::assets
