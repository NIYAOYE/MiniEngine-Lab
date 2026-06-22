#include "me/domain/CropConfig.h"

namespace me::domain {

const CropConfig* CropDatabase::Find(const std::string& id) const {
    for (const auto& c : crops_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

namespace {

// 解析并校验单条作物;任一不满足返回 false。
bool ReadCrop(const nlohmann::json& j, CropConfig& out) {
    if (!j.is_object()) return false;

    if (!j.contains("id") || !j["id"].is_string()) return false;
    out.id = j["id"].get<std::string>();
    if (out.id.empty()) return false;

    if (!j.contains("name") || !j["name"].is_string()) return false;
    out.name = j["name"].get<std::string>();
    if (out.name.empty()) return false;

    if (!j.contains("harvestItemId") || !j["harvestItemId"].is_string()) return false;
    out.harvestItemId = j["harvestItemId"].get<std::string>();
    if (out.harvestItemId.empty()) return false;

    if (!j.contains("yield") || !j["yield"].is_number_integer()) return false;
    out.yield = j["yield"].get<int>();
    if (out.yield < 1) return false;

    if (!j.contains("stageNames") || !j["stageNames"].is_array()) return false;
    for (const auto& n : j["stageNames"]) {
        if (!n.is_string()) return false;
        out.stageNames.push_back(n.get<std::string>());
    }

    if (!j.contains("stageDays") || !j["stageDays"].is_array()) return false;
    for (const auto& d : j["stageDays"]) {
        if (!d.is_number_integer()) return false;
        const int days = d.get<int>();
        if (days < 1) return false;
        out.stageDays.push_back(days);
    }

    // 语义不变量:阶段名与天数一一对应,且至少一个阶段。
    if (out.stageNames.empty()) return false;
    if (out.stageNames.size() != out.stageDays.size()) return false;

    return true;
}

} // namespace

std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json& j) {
    if (!j.is_array()) return std::nullopt;

    CropDatabase db;
    for (const auto& item : j) {
        CropConfig c;
        if (!ReadCrop(item, c)) return std::nullopt;
        // id 唯一性
        for (const auto& existing : db.crops_) {
            if (existing.id == c.id) return std::nullopt;
        }
        db.crops_.push_back(std::move(c));
    }
    return db;
}

} // namespace me::domain
