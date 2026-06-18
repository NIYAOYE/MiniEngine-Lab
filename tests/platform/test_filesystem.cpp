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

TEST_CASE("write then read binary roundtrips with embedded nulls") {
    const fs::path tmp = fs::temp_directory_path() / "me_fs_test.bin";
    // 故意包含 0x00 与高位字节:这是二进制路径区别于文本的关键
    const std::vector<std::uint8_t> data{0x00, 0xFF, 0x42, 0x00, 0x7E};

    CHECK(pf::WriteBinaryFile(tmp.string(), data));
    CHECK(pf::Exists(tmp.string()));

    auto back = pf::ReadBinaryFile(tmp.string());
    REQUIRE(back.has_value());
    CHECK(back->size() == data.size());
    CHECK(*back == data); // 逐字节完全一致,空字节未被截断

    fs::remove(tmp);
}

TEST_CASE("write empty binary file produces a zero-length file") {
    const fs::path tmp = fs::temp_directory_path() / "me_fs_empty.bin";
    const std::vector<std::uint8_t> empty;

    CHECK(pf::WriteBinaryFile(tmp.string(), empty));
    CHECK(pf::Exists(tmp.string()));

    auto back = pf::ReadBinaryFile(tmp.string());
    REQUIRE(back.has_value());
    CHECK(back->empty());

    fs::remove(tmp);
}

TEST_CASE("missing file reports absent and returns nullopt") {
    const std::string missing = (fs::temp_directory_path() / "me_fs_does_not_exist_42.txt").string();
    CHECK_FALSE(pf::Exists(missing));
    CHECK_FALSE(pf::ReadTextFile(missing).has_value());
    CHECK_FALSE(pf::ReadBinaryFile(missing).has_value());
}
