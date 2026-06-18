#pragma once

#include <cstdint>

namespace me {

/// 句柄无效索引哨兵(零魔法数字:具名常量)。
constexpr uint32_t kInvalidHandleIndex = 0xFFFFFFFFu;

/**
 * @brief 类型化资源句柄(index + generation)。
 *
 * 模板参数 T 仅作类型标签,使不同资源句柄不可互相赋值,获得编译期类型安全。
 * generation 用于检测“悬垂句柄”(槽位被复用后旧句柄失效)。
 */
template <typename T>
struct Handle {
    uint32_t index = kInvalidHandleIndex;
    uint32_t generation = 0u;

    /** @brief 是否为有效句柄。 */
    bool IsValid() const { return index != kInvalidHandleIndex; }

    /** @brief 无效句柄常量。 */
    static Handle Invalid() { return Handle{}; }

    bool operator==(const Handle& r) const {
        return index == r.index && generation == r.generation;
    }
    bool operator!=(const Handle& r) const { return !(*this == r); }
};

} // namespace me
