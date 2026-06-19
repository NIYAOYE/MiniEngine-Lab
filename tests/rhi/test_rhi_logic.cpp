#include <doctest/doctest.h>

#include "me/rhi/FenceTracker.h"
#include "me/rhi/FrameRing.h"
#include "me/rhi/DescriptorAllocatorLogic.h"
#include "me/rhi/QuadGeometry.h"
#include "me/rhi/SpriteTransform.h"

using namespace me::rhi;

TEST_CASE("FenceTracker 递增信号值且单调完成判定") {
    FenceTracker t;
    const uint64_t a = t.NextSignalValue();
    const uint64_t b = t.NextSignalValue();
    CHECK(b == a + 1);
    CHECK_FALSE(t.IsComplete(a - 1, a)); // GPU 还没到 a
    CHECK(t.IsComplete(a, a));           // 刚好到 a
    CHECK(t.IsComplete(b, a));           // 超过 a
}

TEST_CASE("FrameRing 在 kFrameCount 内循环") {
    FrameRing ring;
    CHECK(ring.Current() == 0u);
    CHECK(ring.Advance() == 1u % kFrameCount);
    // 推进 kFrameCount 次回到起点
    FrameRing r2;
    for (uint32_t i = 0; i < kFrameCount; ++i) r2.Advance();
    CHECK(r2.Current() == 0u);
}

TEST_CASE("DescriptorAllocatorLogic 线性分配 + 容量上限 + 字节偏移") {
    constexpr uint32_t kIncrement = 32;
    DescriptorAllocatorLogic alloc(2, kIncrement);
    auto i0 = alloc.Allocate();
    auto i1 = alloc.Allocate();
    auto i2 = alloc.Allocate();
    REQUIRE(i0.has_value());
    REQUIRE(i1.has_value());
    CHECK(*i0 == 0u);
    CHECK(*i1 == 1u);
    CHECK_FALSE(i2.has_value());                 // 满了返回 nullopt
    CHECK(alloc.CpuOffsetBytes(*i1) == kIncrement);
    CHECK(alloc.Count() == 2u);
}

TEST_CASE("UnitQuad 顶点/索引") {
    auto v = UnitQuadVertices();
    auto idx = UnitQuadIndices();
    CHECK(v.size() == kSpriteVertexCount);
    CHECK(idx.size() == kSpriteIndexCount);
    // 单位四边形居中:四角在 ±0.5
    CHECK(v[0].x == doctest::Approx(-0.5f));
    CHECK(v[0].y == doctest::Approx(-0.5f));
    CHECK(v[2].x == doctest::Approx(0.5f));
    CHECK(v[2].y == doctest::Approx(0.5f));
    // 左下角 UV 的 v 分量在底部(=1,纹理行向下)
    CHECK(v[0].u == doctest::Approx(0.0f));
    CHECK(v[0].v == doctest::Approx(1.0f));
}

TEST_CASE("MakeSpriteModelMatrix 把单位四边形角点映射到世界矩形") {
    using me::Vector2;
    // 位置 (10,20),尺寸 (4,6),无旋转
    auto model = MakeSpriteModelMatrix(Vector2{10.0f, 20.0f}, Vector2{4.0f, 6.0f}, 0.0f);
    // 右上角局部 (0.5,0.5) → 世界 (10+2, 20+3) = (12,23)
    Vector2 topRight = model.TransformPoint(Vector2{0.5f, 0.5f});
    CHECK(topRight.x == doctest::Approx(12.0f));
    CHECK(topRight.y == doctest::Approx(23.0f));
    // 中心 (0,0) → 世界 (10,20)
    Vector2 center = model.TransformPoint(Vector2{0.0f, 0.0f});
    CHECK(center.x == doctest::Approx(10.0f));
    CHECK(center.y == doctest::Approx(20.0f));
}
