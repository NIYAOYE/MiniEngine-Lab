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

void FarmField::AdvanceOneDay() {
    for (auto& [key, crop] : crops_) {
        (void)key;
        if (!crop.watered) continue; // 未浇水:停滞
        crop.watered = false;
        crop.daysInStage++;
        const CropConfig* cfg = db_.Find(crop.cropId);
        if (cfg == nullptr) continue; // 防御:配置缺失则不推进(理论不该发生)
        if (cfg->IsMatureStage(crop.stage)) continue; // 已成熟:不再前进
        if (crop.daysInStage >= cfg->stageDays[crop.stage]) {
            crop.stage++;
            crop.daysInStage = 0;
        }
    }
}

void FarmField::AdvanceDays(int n) {
    for (int i = 0; i < n; ++i) AdvanceOneDay();
}

const CropInstance* FarmField::At(int x, int y) const {
    auto it = crops_.find(TileKey{x, y});
    return it == crops_.end() ? nullptr : &it->second;
}

} // namespace me::domain
