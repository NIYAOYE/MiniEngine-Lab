#include <doctest/doctest.h>
#include <cstdint>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"

using namespace me::rhi;

namespace {
constexpr size_t kCapBytes = 256; // 动态缓冲容量(字节)
} // namespace

TEST_CASE("动态上传缓冲:Write 后经 Mapped 可读回相同字节") {
    auto device = GpuDevice::Create(/*useWarp=*/true);
    REQUIRE(device != nullptr);

    auto buf = GpuBuffer::CreateDynamic(device->Device(), kCapBytes);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->Mapped() != nullptr);

    const uint32_t pattern[4] = {0xAABBCCDDu, 1u, 2u, 3u};
    buf->Write(pattern, sizeof(pattern), 0);

    // 上传堆 CPU 可见:持久映射指针应反映刚写入的数据。
    const auto* got = reinterpret_cast<const uint32_t*>(buf->Mapped());
    CHECK(got[0] == 0xAABBCCDDu);
    CHECK(got[3] == 3u);
}
