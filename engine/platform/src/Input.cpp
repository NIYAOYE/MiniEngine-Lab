#include "me/platform/Input.h"

namespace me::platform {

void InputState::NewFrame() {
    m_pressed.fill(false);
    m_released.fill(false);
}

void InputState::OnKeyDown(KeyCode k) {
    const std::size_t i = Index(k);
    if (!m_down[i]) {        // 仅在状态翻转时记录边沿,过滤系统的自动重复
        m_pressed[i] = true;
    }
    m_down[i] = true;
}

void InputState::OnKeyUp(KeyCode k) {
    const std::size_t i = Index(k);
    if (m_down[i]) {
        m_released[i] = true;
    }
    m_down[i] = false;
}

bool InputState::IsDown(KeyCode k) const { return m_down[Index(k)]; }
bool InputState::WasPressed(KeyCode k) const { return m_pressed[Index(k)]; }
bool InputState::WasReleased(KeyCode k) const { return m_released[Index(k)]; }

} // namespace me::platform
