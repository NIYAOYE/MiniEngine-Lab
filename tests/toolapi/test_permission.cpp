#include <doctest/doctest.h>

#include "me/toolapi/Permission.h"

using me::toolapi::CallerRole;
using me::toolapi::IsAllowed;
using me::toolapi::Permission;

TEST_CASE("Permission:Editor 可调用任意层级") {
    CHECK(IsAllowed(CallerRole::Editor, Permission::AgentAllowed));
    CHECK(IsAllowed(CallerRole::Editor, Permission::Automation));
    CHECK(IsAllowed(CallerRole::Editor, Permission::EditorOnly));
}

TEST_CASE("Permission:Automation 不可调用 EditorOnly") {
    CHECK(IsAllowed(CallerRole::Automation, Permission::AgentAllowed));
    CHECK(IsAllowed(CallerRole::Automation, Permission::Automation));
    CHECK_FALSE(IsAllowed(CallerRole::Automation, Permission::EditorOnly));
}

TEST_CASE("Permission:Agent 仅可调用 AgentAllowed") {
    CHECK(IsAllowed(CallerRole::Agent, Permission::AgentAllowed));
    CHECK_FALSE(IsAllowed(CallerRole::Agent, Permission::Automation));
    CHECK_FALSE(IsAllowed(CallerRole::Agent, Permission::EditorOnly));
}
