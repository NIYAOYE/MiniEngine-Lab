#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::domain {

/// @brief 单个物品的数据驱动配置(全部从 JSON 加载,源码零硬编码)。
struct ItemConfig {
    std::string id;        ///< 唯一非空标识
    std::string name;      ///< 展示名(非空)
    std::string category;  ///< 分类(如 "crop"/"tool"/"resource";非空)
    int maxStack = 0;      ///< 单格最大堆叠(≥1)
    int sellPrice = 0;     ///< 卖出单价(≥0;未来商店用,本里程碑无消费者)
};

struct InventoryConfig;  // 前置声明:供 ItemDatabase 友元
/// @brief 从 JSON 对象解析并校验 { capacity, items:[...] }。
std::optional<InventoryConfig> LoadInventoryConfig(const nlohmann::json& j);

/// @brief 物品表:id → ItemConfig 的只读集合。
class ItemDatabase {
public:
    ItemDatabase() = default;

    /// @brief 查物品;未命中返回 nullptr。
    const ItemConfig* Find(const std::string& id) const;
    /// @brief 物品种类数。
    std::size_t Size() const { return items_.size(); }

private:
    friend std::optional<InventoryConfig> LoadInventoryConfig(const nlohmann::json&);
    std::vector<ItemConfig> items_;
};

/// @brief 库存配置:格位数 + 物品表。
struct InventoryConfig {
    int capacity = 0;    ///< 库存格位数(≥1)
    ItemDatabase items;  ///< 物品表
};

} // namespace me::domain
