#pragma once

#include <wrl/client.h> // Microsoft::WRL::ComPtr

namespace me::rhi {

/// 智能 COM 指针别名:RAII 释放 ID3D12* / IDXGI* 等。
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace detail {
/** @brief HRESULT 失败处理:打印诊断后中止(不变量违反路径)。 */
[[noreturn]] void HrFail(long hr, const char* expr, const char* file, int line);
} // namespace detail

} // namespace me::rhi

/** @brief 检查 DX12 调用的 HRESULT;FAILED 则中止。表达式仅求值一次。 */
#define ME_HR_CHECK(expr)                                                      \
    do {                                                                       \
        const HRESULT _meHr = (expr);                                          \
        if (FAILED(_meHr)) {                                                   \
            ::me::rhi::detail::HrFail(static_cast<long>(_meHr), #expr,         \
                                      __FILE__, __LINE__);                     \
        }                                                                      \
    } while (false)
