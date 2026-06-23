#pragma once

#include <string>
#include <vector>

#include "me/domain/ItemConfig.h"

namespace me::domain {

/// @brief 一个库存格位。空格:count==0 且 itemId 为空。
struct ItemStack {
    std::string itemId;  ///< 空格为空串
    int count = 0;       ///< 空格 0;非空 1..maxStack
    bool Empty() const { return count == 0; }
};

/// @brief Add 结果状态。
enum class AddStatus {
    Ok,           ///< 全量入库成功
    UnknownItem,  ///< itemId 不在物品表
    Full,         ///< 容纳量不足(零状态变更)
};
struct AddResult {
    AddStatus status = AddStatus::Full;
    int added = 0;  ///< status==Ok 时等于请求 count
};

/// @brief Remove 结果状态。
enum class RemoveStatus {
    Ok,         ///< 全量扣减成功
    NotEnough,  ///< 持有量不足(零状态变更)
};
struct RemoveResult {
    RemoveStatus status = RemoveStatus::NotEnough;
    int removed = 0;  ///< status==Ok 时等于请求 count
};

/**
 * @brief 库存:固定格位网格 + 全量或不加/不减语义。
 *
 * 值语义可拷贝(Tool dry-run 在副本上预演 → 零副作用)。不进 Command/Undo
 * (运行时态,见 ADR 0006/0007)。
 */
class Inventory {
public:
    /// @brief 注入物品表(值持有)+ 固定格位数(capacity≥1)。
    Inventory(ItemDatabase db, int capacity);

    /// @brief 加入 count 个 itemId:先补同物品未满堆,再占空格。放不下则零变更返回 Full。
    AddResult Add(const std::string& itemId, int count);

    /// @brief 移除 count 个 itemId:不足则零变更返回 NotEnough。
    RemoveResult Remove(const std::string& itemId, int count);

    /// @brief 预判 count 个 itemId 能否全量加入(不改状态)。
    bool CanAdd(const std::string& itemId, int count) const;

    /// @brief 跨格位统计某物品总量。
    int CountOf(const std::string& itemId) const;

    /// @brief 只读遍历全部格位(含空格,下标稳定)。
    const std::vector<ItemStack>& Slots() const { return slots_; }
    /// @brief 格位数。
    int Capacity() const { return static_cast<int>(slots_.size()); }
    /// @brief 只读访问物品表。
    const ItemDatabase& Database() const { return db_; }

private:
    /// @brief 给定 itemId 当前可再容纳的数量(未满堆剩余 + 空格 × maxStack)。
    int FreeSpaceFor(const std::string& itemId, int maxStack) const;

    ItemDatabase db_;
    std::vector<ItemStack> slots_;
};

} // namespace me::domain
