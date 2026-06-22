#pragma once

#include <map>
#include <string>

#include "me/domain/CropConfig.h"

namespace me::domain {

/// @brief 农田瓦片坐标键(整数格点)。字典序排序使遍历确定性。
struct TileKey {
    int x = 0;
    int y = 0;
};

inline bool operator<(const TileKey& a, const TileKey& b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

/// @brief 一株作物的运行时状态。
struct CropInstance {
    std::string cropId;   ///< 指向 CropDatabase 的作物 id
    int stage = 0;        ///< 0 基,当前阶段索引
    int daysInStage = 0;  ///< 本阶段已累计的"已浇水天数"
    bool watered = false; ///< 今日是否已浇水
};

/// @brief Plant 结果。
enum class PlantStatus {
    Ok,            ///< 种植成功
    TileOccupied,  ///< 该瓦片已有作物
    UnknownCrop,   ///< cropId 不在数据库
};

/**
 * @brief 农田:以瓦片坐标为键的作物实例网格 + 浇水驱动生长状态机。
 *
 * 纯 CPU 运行时态,值语义可拷贝(Tool dry-run 在副本上预演 → 零副作用)。
 * 不进 Command/Undo(运行时态,见 ADR 0006/0007)。
 */
class FarmField {
public:
    /// @brief 注入作物表(值持有)。
    explicit FarmField(CropDatabase db);

    /// @brief 在空瓦片种植已知作物。
    PlantStatus Plant(int x, int y, const std::string& cropId);

    /// @brief 给瓦片上的作物浇水(幂等)。无作物返回 false。
    bool Water(int x, int y);

    /// @brief 查瓦片作物;空返回 nullptr。
    const CropInstance* At(int x, int y) const;

    /// @brief 只读遍历全部作物(键有序)。
    const std::map<TileKey, CropInstance>& Crops() const { return crops_; }

    /// @brief 只读访问作物表。
    const CropDatabase& Database() const { return db_; }

private:
    CropDatabase db_;
    std::map<TileKey, CropInstance> crops_;
};

} // namespace me::domain
