#include <doctest/doctest.h>
#include "me/platform/Input.h"

using me::platform::InputState;
using me::platform::KeyCode;

TEST_CASE("InputState 边沿与持续态") {
    InputState in;

    SUBCASE("按下当帧:IsDown 与 WasPressed 均真") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Space);
        CHECK(in.IsDown(KeyCode::Space));
        CHECK(in.WasPressed(KeyCode::Space));
        CHECK_FALSE(in.WasReleased(KeyCode::Space));
    }

    SUBCASE("下一帧持续按住:IsDown 真但 WasPressed 假") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Space);
        in.NewFrame(); // 进入下一帧,边沿清零
        CHECK(in.IsDown(KeyCode::Space));
        CHECK_FALSE(in.WasPressed(KeyCode::Space));
    }

    SUBCASE("抬起当帧:WasReleased 真,IsDown 假") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Escape);
        in.NewFrame();
        in.OnKeyUp(KeyCode::Escape);
        CHECK_FALSE(in.IsDown(KeyCode::Escape));
        CHECK(in.WasReleased(KeyCode::Escape));
    }
}
