#include "me/domain/FarmField.h"

namespace me::domain {

FarmField::FarmField(CropDatabase db) : db_(std::move(db)) {}

PlantStatus FarmField::Plant(int x, int y, const std::string& cropId) {
    const TileKey key{x, y};
    if (crops_.find(key) != crops_.end()) return PlantStatus::TileOccupied;
    if (db_.Find(cropId) == nullptr) return PlantStatus::UnknownCrop;
    crops_[key] = CropInstance{cropId, /*stage=*/0, /*daysInStage=*/0, /*watered=*/false};
    return PlantStatus::Ok;
}

bool FarmField::Water(int x, int y) {
    auto it = crops_.find(TileKey{x, y});
    if (it == crops_.end()) return false;
    it->second.watered = true; // 幂等
    return true;
}

const CropInstance* FarmField::At(int x, int y) const {
    auto it = crops_.find(TileKey{x, y});
    return it == crops_.end() ? nullptr : &it->second;
}

} // namespace me::domain
