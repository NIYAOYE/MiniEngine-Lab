#pragma once

#include <memory>

namespace me::platform {

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

private:
    Window();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace me::platform
