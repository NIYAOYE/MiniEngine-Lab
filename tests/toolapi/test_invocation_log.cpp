#include <doctest/doctest.h>

#include "me/toolapi/ToolInvocation.h"

using me::toolapi::ToolErrorCode;
using me::toolapi::ToolInvocation;
using me::toolapi::ToolInvocationLog;

TEST_CASE("InvocationLog:Append 分配单调递增 id 从 1 起") {
    ToolInvocationLog log;
    ToolInvocation a;
    a.tool = "scene.create_entity";
    ToolInvocation b;
    b.tool = "scene.list_entities";

    const auto id1 = log.Append(a);
    const auto id2 = log.Append(b);
    CHECK(id1 == 1);
    CHECK(id2 == 2);
    CHECK(log.Size() == 2);
    CHECK(log.Entries()[0].id == 1);
    CHECK(log.Entries()[1].tool == "scene.list_entities");
}

TEST_CASE("InvocationLog:ToolInvocation::toJson 携带核心字段") {
    ToolInvocation inv;
    inv.id = 4;
    inv.tool = "scene.destroy_entity";
    inv.params = {{"id", 9}};
    inv.dryRun = true;
    inv.ok = false;
    inv.code = ToolErrorCode::PreconditionFailed;
    inv.message = "no such entity";

    nlohmann::json j = inv.toJson();
    CHECK(j["id"] == 4);
    CHECK(j["tool"] == "scene.destroy_entity");
    CHECK(j["params"]["id"] == 9);
    CHECK(j["dryRun"] == true);
    CHECK(j["ok"] == false);
    CHECK(j["code"] == "PreconditionFailed");
    CHECK(j["message"] == "no such entity");
}
