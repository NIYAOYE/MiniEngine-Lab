#pragma once

#include <memory>

namespace me::platform {

class InputState; // 前置声明:窗口可把键消息馈入键盘状态机

/** @brief 窗口创建参数(零魔法数字:默认值具名于此)。 */
struct WindowDesc {
    int width = 1280;
    int height = 720;
    const char* title = "MiniEngine";
};

/**
 * @brief 平台窗口(Win32 实现细节封死在 .cpp)。
 *
 * 不可拷贝:持有不可复制的 OS 窗口句柄。失败时工厂返回 nullptr(不抛异常)。
 */
class Window {
public:
    /** @brief 创建并显示窗口;失败返回 nullptr。 */
    static std::unique_ptr<Window> Create(const WindowDesc& desc);

    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /** @brief 处理本帧累积的所有窗口消息(非阻塞)。 */
    void PumpMessages();
    /** @brief 用户是否请求关闭窗口。 */
    bool ShouldClose() const;

    int Width() const;
    int Height() const;

    /** @brief 原生窗口句柄(HWND,转为 void*);供 RHI 创建交换链使用。 */
    void* NativeHandle() const;

    /** @brief 注册键盘状态机;注册后窗口把 WM_KEYDOWN/UP 翻译并喂入。 */
    void SetInput(InputState* input);

    /// @brief Win32 消息钩子签名(裸类型,使 platform 不依赖 ImGui)。
    /// @return true 表示消息已被钩子消费(WndProc 不再继续默认处理)。
    using WndProcHook = bool (*)(void* hwnd, unsigned int msg,
                                 unsigned long long wparam, long long lparam);

    /// @brief 注册消息钩子(供 ImGui 等外部后端拦截窗口消息);传 nullptr 取消。
    void SetWndProcHook(WndProcHook hook);

    // 前置声明公开,定义仍封死在 .cpp(供 .cpp 内的自由 WndProc 访问其成员)。
    struct Impl;

private:
    Window();
    std::unique_ptr<Impl> m_impl;
};

} // namespace me::platform
