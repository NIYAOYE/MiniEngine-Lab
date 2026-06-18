#pragma once

namespace me {

/// 数学常量(零魔法数字:全部具名 constexpr)。
constexpr float kPi       = 3.14159265358979323846f;
constexpr float kTwoPi    = 2.0f * kPi;
constexpr float kEpsilon  = 1e-6f;              ///< 浮点近零阈值
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

} // namespace me
