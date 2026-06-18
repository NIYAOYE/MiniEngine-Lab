#include <doctest/doctest.h>

#include "me/core/Handle.h"

struct Texture; // 仅用作类型标签

using me::Handle;

TEST_CASE("default handle is invalid") {
    Handle<Texture> h;
    CHECK_FALSE(h.IsValid());
    CHECK(h == Handle<Texture>::Invalid());
}

TEST_CASE("constructed handle is valid and comparable") {
    Handle<Texture> a{3u, 1u};
    Handle<Texture> b{3u, 1u};
    Handle<Texture> c{3u, 2u}; // 同 index 不同 generation
    CHECK(a.IsValid());
    CHECK(a == b);
    CHECK(a != c);
}
