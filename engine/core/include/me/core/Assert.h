#pragma once

namespace me::detail {

/** @brief 断言失败处理:写诊断到 stderr 后 std::abort()。 */
[[noreturn]] void AssertFail(const char* expr, const char* file, int line, const char* msg);

} // namespace me::detail

#if defined(NDEBUG)
    // Release:断言编译为空操作。
    #define ME_ASSERT(expr)          ((void)0)
    #define ME_ASSERT_MSG(expr, msg) ((void)0)
#else
    /** @brief 调试断言:expr 为假则打印并中止。 */
    #define ME_ASSERT(expr) \
        ((expr) ? (void)0 : ::me::detail::AssertFail(#expr, __FILE__, __LINE__, nullptr))
    /** @brief 带消息的调试断言。 */
    #define ME_ASSERT_MSG(expr, msg) \
        ((expr) ? (void)0 : ::me::detail::AssertFail(#expr, __FILE__, __LINE__, (msg)))
#endif
