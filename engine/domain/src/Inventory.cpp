#include "me/domain/Inventory.h"

#include <algorithm>

namespace me::domain {

Inventory::Inventory(ItemDatabase db, int capacity)
    : db_(std::move(db)), slots_(static_cast<std::size_t>(capacity < 0 ? 0 : capacity)) {}

int Inventory::FreeSpaceFor(const std::string& itemId, int maxStack) const {
    int free = 0;
    for (const auto& s : slots_) {
        if (s.Empty())
            free += maxStack;
        else if (s.itemId == itemId && s.count < maxStack)
            free += maxStack - s.count;
    }
    return free;
}

int Inventory::CountOf(const std::string& itemId) const {
    int total = 0;
    for (const auto& s : slots_)
        if (!s.Empty() && s.itemId == itemId) total += s.count;
    return total;
}

bool Inventory::CanAdd(const std::string& itemId, int count) const {
    const ItemConfig* cfg = db_.Find(itemId);
    if (cfg == nullptr) return false;
    return FreeSpaceFor(itemId, cfg->maxStack) >= count;
}

AddResult Inventory::Add(const std::string& itemId, int count) {
    const ItemConfig* cfg = db_.Find(itemId);
    if (cfg == nullptr) return AddResult{AddStatus::UnknownItem, 0};
    const int maxStack = cfg->maxStack;
    if (FreeSpaceFor(itemId, maxStack) < count) return AddResult{AddStatus::Full, 0};

    int remaining = count;
    // 第一遍:补同物品未满堆。
    for (auto& s : slots_) {
        if (remaining == 0) break;
        if (!s.Empty() && s.itemId == itemId && s.count < maxStack) {
            const int add = std::min(remaining, maxStack - s.count);
            s.count += add;
            remaining -= add;
        }
    }
    // 第二遍:占空格。
    for (auto& s : slots_) {
        if (remaining == 0) break;
        if (s.Empty()) {
            const int add = std::min(remaining, maxStack);
            s.itemId = itemId;
            s.count = add;
            remaining -= add;
        }
    }
    return AddResult{AddStatus::Ok, count};
}

RemoveResult Inventory::Remove(const std::string& itemId, int count) {
    if (CountOf(itemId) < count) return RemoveResult{RemoveStatus::NotEnough, 0};

    int remaining = count;
    for (auto& s : slots_) {
        if (remaining == 0) break;
        if (!s.Empty() && s.itemId == itemId) {
            const int take = std::min(remaining, s.count);
            s.count -= take;
            remaining -= take;
            if (s.count == 0) s.itemId.clear(); // 掏空后归还空格
        }
    }
    return RemoveResult{RemoveStatus::Ok, count};
}

} // namespace me::domain
