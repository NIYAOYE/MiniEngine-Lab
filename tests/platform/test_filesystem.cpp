#include <doctest/doctest.h>

#include "me/platform/FileSystem.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
namespace pf = me::platform;

TEST_CASE("write then read text roundtrips") {
    const fs::path tmp = fs::temp_directory_path() / "me_fs_test.txt";
    const std::string content = "line1\nline2\n";

    CHECK(pf::WriteTextFile(tmp.string(), content));
    CHECK(pf::Exists(tmp.string()));

    auto read = pf::ReadTextFile(tmp.string());
    REQUIRE(read.has_value());
    CHECK(*read == content);

    auto bin = pf::ReadBinaryFile(tmp.string());
    REQUIRE(bin.has_value());
    CHECK(bin->size() == content.size());

    fs::remove(tmp);
}

TEST_CASE("missing file reports absent and returns nullopt") {
    const std::string missing = (fs::temp_directory_path() / "me_fs_does_not_exist_42.txt").string();
    CHECK_FALSE(pf::Exists(missing));
    CHECK_FALSE(pf::ReadTextFile(missing).has_value());
    CHECK_FALSE(pf::ReadBinaryFile(missing).has_value());
}
