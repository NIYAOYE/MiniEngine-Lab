#include "me/platform/Window.h"
#include "me/platform/Input.h"

#include <windows.h>
#include <string>

namespace me::platform {

namespace {
/// 窗口类名(具名常量,避免裸字符串散落)。
constexpr wchar_t kWindowClassName[] = L"MiniEngineWindowClass";

std::wstring Widen(const char* utf8) {
    if (utf8 == nullptr) return std::wstring();
    int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring out(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), len);
    return out;
}
} // namespace

struct Window::Impl {
    HWND hwnd = nullptr;
    int width = 0;
    int height = 0;
    bool shouldClose = false;
    InputState* input = nullptr; // 可选:注册后接收键消息
};

// 把 HWND 的 userdata 指向 Impl,从而在静态 WndProc 中取回实例状态。
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    auto* impl = reinterpret_cast<Window::Impl*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (impl != nullptr) {
        switch (msg) {
        case WM_CLOSE:
            impl->shouldClose = true;
            return 0;
        case WM_SIZE:
            impl->width = LOWORD(lparam);
            impl->height = HIWORD(lparam);
            return 0;
        case WM_KEYDOWN:
            if (impl->input && (lparam & (1 << 30)) == 0) { // 过滤自动重复
                if (auto k = detail::MapPlatformKey(static_cast<unsigned>(wparam)))
                    impl->input->OnKeyDown(*k);
            }
            return 0;
        case WM_KEYUP:
            if (impl->input) {
                if (auto k = detail::MapPlatformKey(static_cast<unsigned>(wparam)))
                    impl->input->OnKeyUp(*k);
            }
            return 0;
        }
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

Window::Window() : m_impl(std::make_unique<Impl>()) {}
Window::~Window() {
    if (m_impl && m_impl->hwnd) ::DestroyWindow(m_impl->hwnd);
}

std::unique_ptr<Window> Window::Create(const WindowDesc& desc) {
    HINSTANCE hinst = ::GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &WndProc;
    wc.hInstance = hinst;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    ::RegisterClassExW(&wc); // 重复注册返回 0 可忽略(同名类已存在)

    // 根据期望客户区尺寸反推窗口外框尺寸。
    RECT rc = {0, 0, desc.width, desc.height};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    ::AdjustWindowRect(&rc, style, FALSE);

    std::unique_ptr<Window> self(new Window());
    self->m_impl->width = desc.width;
    self->m_impl->height = desc.height;

    HWND hwnd = ::CreateWindowExW(
        0, kWindowClassName, Widen(desc.title).c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hinst, self->m_impl.get());
    if (hwnd == nullptr) {
        return nullptr; // 创建失败:不抛异常,返回 nullptr
    }
    self->m_impl->hwnd = hwnd;
    ::ShowWindow(hwnd, SW_SHOW);
    return self;
}

void Window::PumpMessages() {
    MSG msg = {};
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

bool Window::ShouldClose() const { return m_impl->shouldClose; }
int Window::Width() const { return m_impl->width; }
int Window::Height() const { return m_impl->height; }
void* Window::NativeHandle() const { return static_cast<void*>(m_impl->hwnd); }
void Window::SetInput(InputState* input) { m_impl->input = input; }

} // namespace me::platform
