#include <doctest/doctest.h>

#include "me/toolapi/ToolResult.h"

using me::toolapi::ToolErrorCode;
using me::toolapi::ToolResult;

TEST_CASE("ToolResult:Success 默认 ok=true、code=Ok") {
    ToolResult r = ToolResult::Success({{"id", 7}}, "done");
    CHECK(r.ok);
    CHECK(r.code == ToolErrorCode::Ok);
    CHECK(r.message == "done");
    CHECK(r.data["id"] == 7);
}

TEST_CASE("ToolResult:Error 携带错误码与消息") {
    ToolResult r = ToolResult::Error(ToolErrorCode::InvalidParams, "bad",
                                     {{"errors", {"x missing"}}});
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
    CHECK(r.data["errors"][0] == "x missing");
}

TEST_CASE("ToolResult:toJson 序列化 code 为字符串") {
    ToolResult r = ToolResult::Error(ToolErrorCode::UnknownTool, "no");
    r.invocationId = 3;
    nlohmann::json j = r.toJson();
    CHECK(j["ok"] == false);
    CHECK(j["code"] == "UnknownTool");
    CHECK(j["message"] == "no");
    CHECK(j["invocationId"] == 3);
    CHECK(j["data"] == nlohmann::json::object());
}
