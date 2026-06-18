#include "me/core/Version.h"

namespace me {

namespace {
// 引擎名称常量(零魔法数字/字符串:具名常量集中存放)。
constexpr const char* kEngineName = "MiniEngine";
} // namespace

const char* EngineName() {
    return kEngineName;
}

} // namespace me
