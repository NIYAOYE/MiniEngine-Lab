#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace me::platform {

/** @brief M1 关心的最小键集合(零魔法数字:用枚举,不用裸 VK 码)。 */
enum class KeyCode { Escape, Space, W, A, S, D, Q, E, Count };

/**
 * @brief 跨平台键盘状态机:持续态 + 本帧边沿(刚按下/刚抬起)。
 *
 * 平台层每帧先调用 NewFrame(),再把消息翻译成 OnKeyDown/OnKeyUp 喂入。
 * 纯逻辑、无 OS 依赖,可独立单测。
 */
class InputState {
public:
    /** @brief 每帧开始:清除上一帧的边沿标记,保留持续按住态。 */
    void NewFrame();

    void OnKeyDown(KeyCode k);
    void OnKeyUp(KeyCode k);

    bool IsDown(KeyCode k) const;       ///< 当前是否按住
    bool WasPressed(KeyCode k) const;   ///< 本帧是否刚按下(边沿)
    bool WasReleased(KeyCode k) const;  ///< 本帧是否刚抬起(边沿)

private:
    static constexpr std::size_t kCount = static_cast<std::size_t>(KeyCode::Count);
    std::array<bool, kCount> m_down{};       // 持续态
    std::array<bool, kCount> m_pressed{};    // 本帧边沿:按下
    std::array<bool, kCount> m_released{};   // 本帧边沿:抬起

    static std::size_t Index(KeyCode k) { return static_cast<std::size_t>(k); }
};

} // namespace me::platform

namespace me::platform::detail {
/** @brief 平台原生按键码(Win32 VK)→ KeyCode;不关心的键返回 nullopt。 */
std::optional<KeyCode> MapPlatformKey(unsigned int nativeKey);
} // namespace me::platform::detail
