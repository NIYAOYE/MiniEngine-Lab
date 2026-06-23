#include "me/domain/ItemConfig.h"

namespace me::domain {

const ItemConfig* ItemDatabase::Find(const std::string& id) const {
    for (const auto& it : items_) {
        if (it.id == id) return &it;
    }
    return nullptr;
}

namespace {

// 解析并校验单条物品;任一不满足返回 false。
bool ReadItem(const nlohmann::json& j, ItemConfig& out) {
    if (!j.is_object()) return false;

    if (!j.contains("id") || !j["id"].is_string()) return false;
    out.id = j["id"].get<std::string>();
    if (out.id.empty()) return false;

    if (!j.contains("name") || !j["name"].is_string()) return false;
    out.name = j["name"].get<std::string>();
    if (out.name.empty()) return false;

    if (!j.contains("category") || !j["category"].is_string()) return false;
    out.category = j["category"].get<std::string>();
    if (out.category.empty()) return false;

    if (!j.contains("maxStack") || !j["maxStack"].is_number_integer()) return false;
    out.maxStack = j["maxStack"].get<int>();
    if (out.maxStack < 1) return false;

    if (!j.contains("sellPrice") || !j["sellPrice"].is_number_integer()) return false;
    out.sellPrice = j["sellPrice"].get<int>();
    if (out.sellPrice < 0) return false;

    return true;
}

} // namespace

std::optional<InventoryConfig> LoadInventoryConfig(const nlohmann::json& j) {
    if (!j.is_object()) return std::nullopt;

    if (!j.contains("capacity") || !j["capacity"].is_number_integer()) return std::nullopt;
    const int capacity = j["capacity"].get<int>();
    if (capacity < 1) return std::nullopt;

    if (!j.contains("items") || !j["items"].is_array()) return std::nullopt;

    InventoryConfig cfg;
    cfg.capacity = capacity;
    for (const auto& item : j["items"]) {
        ItemConfig c;
        if (!ReadItem(item, c)) return std::nullopt;
        // id 唯一性
        for (const auto& existing : cfg.items.items_) {
            if (existing.id == c.id) return std::nullopt;
        }
        cfg.items.items_.push_back(std::move(c));
    }
    return cfg;
}

} // namespace me::domain
