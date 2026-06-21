#include <doctest/doctest.h>

#include "me/toolapi/SchemaValidator.h"

using me::toolapi::ValidateAgainstSchema;
using nlohmann::json;

static const json kSchema = {
    {"type", "object"},
    {"required", {"id"}},
    {"properties",
     {{"id", {{"type", "integer"}, {"minimum", 1}}},
      {"kind", {{"type", "string"}, {"enum", {"sprite", "camera"}}}},
      {"position", {{"type", "object"},
                    {"properties", {{"x", {{"type", "number"}}},
                                    {"y", {{"type", "number"}}}}}}}}}};

TEST_CASE("Schema:合法参数通过") {
    json p = {{"id", 5}, {"kind", "sprite"}, {"position", {{"x", 1.5}, {"y", 2}}}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK(r.ok);
    CHECK(r.errors.empty());
}

TEST_CASE("Schema:缺失 required 报错") {
    json p = {{"kind", "sprite"}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK_FALSE(r.ok);
    CHECK(r.errors.size() == 1);
}

TEST_CASE("Schema:类型不匹配报错") {
    json p = {{"id", "not-a-number"}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK_FALSE(r.ok);
}

TEST_CASE("Schema:minimum 越界报错") {
    json p = {{"id", 0}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK_FALSE(r.ok);
}

TEST_CASE("Schema:enum 不命中报错") {
    json p = {{"id", 1}, {"kind", "tree"}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK_FALSE(r.ok);
}

TEST_CASE("Schema:嵌套对象字段类型错报错") {
    json p = {{"id", 1}, {"position", {{"x", "bad"}, {"y", 2}}}};
    auto r = ValidateAgainstSchema(kSchema, p);
    CHECK_FALSE(r.ok);
}

TEST_CASE("Schema:integer 不接受小数,number 接受整数") {
    CHECK_FALSE(ValidateAgainstSchema(kSchema, json{{"id", 1.5}}).ok);
    json ok = {{"id", 2}, {"position", {{"x", 3}, {"y", 4}}}};
    CHECK(ValidateAgainstSchema(kSchema, ok).ok);
}

TEST_CASE("Schema:params 非对象报错") {
    auto r = ValidateAgainstSchema(kSchema, json::array({1, 2}));
    CHECK_FALSE(r.ok);
}
