#include <doctest/doctest.h>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"

using namespace me::rhi;

TEST_CASE("WARP 设备可创建,围栏 signal/wait 推进完成值") {
    auto device = GpuDevice::Create(/*useWarp=*/true);
    REQUIRE(device != nullptr);
    REQUIRE(device->Device() != nullptr);
    REQUIRE(device->Queue() != nullptr);

    auto fence = Fence::Create(device->Device());
    REQUIRE(fence != nullptr);

    const uint64_t before = fence->CompletedValue();
    const uint64_t target = fence->Signal(device->Queue());
    fence->Wait(target);
    CHECK(fence->CompletedValue() >= target);
    CHECK(fence->CompletedValue() > before);

    // Flush 再来一次,确保可重复同步。
    fence->Flush(device->Queue());
}
