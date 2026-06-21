#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/command/ICommand.h"
#include "me/scene/Scene.h"

using me::command::CommandResult;
using me::command::CommandStack;
using me::command::ICommand;
using me::scene::Scene;

namespace {
/// 假命令:execute +1,undo -1,写入共享计数器,验证栈调度。
struct CounterCmd final : ICommand {
    int* counter;
    bool failExecute;
    bool failUndo;
    explicit CounterCmd(int* c, bool fail = false, bool failU = false)
        : counter(c), failExecute(fail), failUndo(failU) {}
    CommandResult execute(Scene&) override {
        if (failExecute) return CommandResult::Fail("故意失败");
        ++(*counter);
        return CommandResult::Ok();
    }
    CommandResult undo(Scene&) override {
        if (failUndo) return CommandResult::Fail("undo 故意失败");
        --(*counter);
        return CommandResult::Ok();
    }
    std::string describe() const override { return "counter"; }
};
} // namespace

TEST_CASE("CommandStack:execute 改状态并入 undo 栈") {
    Scene scene;
    CommandStack stack;
    int n = 0;
    const CommandResult r = stack.execute(std::make_unique<CounterCmd>(&n), scene);
    CHECK(r.ok);
    CHECK(n == 1);
    CHECK(stack.canUndo());
    CHECK_FALSE(stack.canRedo());
    CHECK(stack.undoDepth() == 1);
}

TEST_CASE("CommandStack:undo/redo 往返") {
    Scene scene;
    CommandStack stack;
    int n = 0;
    stack.execute(std::make_unique<CounterCmd>(&n), scene);
    CHECK(stack.undo(scene).ok);
    CHECK(n == 0);
    CHECK(stack.canRedo());
    CHECK(stack.redo(scene).ok);
    CHECK(n == 1);
    CHECK_FALSE(stack.canRedo());
}

TEST_CASE("CommandStack:新 execute 清空 redo 栈") {
    Scene scene;
    CommandStack stack;
    int n = 0;
    stack.execute(std::make_unique<CounterCmd>(&n), scene);
    stack.undo(scene);
    CHECK(stack.redoDepth() == 1);
    stack.execute(std::make_unique<CounterCmd>(&n), scene); // 应清空 redo
    CHECK(stack.redoDepth() == 0);
    CHECK_FALSE(stack.canRedo());
}

TEST_CASE("CommandStack:空栈 undo/redo 返回失败") {
    Scene scene;
    CommandStack stack;
    CHECK_FALSE(stack.undo(scene).ok);
    CHECK_FALSE(stack.redo(scene).ok);
}

TEST_CASE("CommandStack:execute 失败的命令不入栈、不改状态") {
    Scene scene;
    CommandStack stack;
    int n = 0;
    const CommandResult r =
        stack.execute(std::make_unique<CounterCmd>(&n, /*fail=*/true), scene);
    CHECK_FALSE(r.ok);
    CHECK(n == 0);
    CHECK_FALSE(stack.canUndo());
    CHECK(stack.undoDepth() == 0);
}

TEST_CASE("CommandStack:undo 失败时命令留在 undo 栈不移动") {
    Scene scene;
    CommandStack stack;
    int n = 0;
    // 先成功执行,undo 栈深度变为 1
    stack.execute(std::make_unique<CounterCmd>(&n, /*failExecute=*/false, /*failUndo=*/true), scene);
    CHECK(stack.undoDepth() == 1);
    // undo 时命令内部返回失败
    const CommandResult r = stack.undo(scene);
    CHECK_FALSE(r.ok);
    // 命令不应移出 undo 栈,也不应进入 redo 栈
    CHECK(stack.undoDepth() == 1);
    CHECK_FALSE(stack.canRedo());
    CHECK(stack.redoDepth() == 0);
    // 计数器不应被修改
    CHECK(n == 1);
}
