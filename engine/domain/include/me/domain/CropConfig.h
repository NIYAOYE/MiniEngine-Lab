#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::domain {

/**
 * @brief 单种作物的数据驱动配置(全部从 JSON 加载,源码零硬编码数值)。
 *
 * stageNames 与 stageDays 一一对应,0 基;最后一个阶段即成熟/可收获阶段。
 * stageDays[i] = 进入下一阶段所需的"已浇水天数";最后阶段的值生长上不使用。
 */
struct CropConfig {
    std::string id;                       ///< 唯一非空标识
    std::string name;                     ///< 展示名
    std::vector<std::string> stageNames;  ///< 阶段名(0 基)
    std::vector<int> stageDays;           ///< 各阶段所需已浇水天数(每项 ≥1)
    std::string harvestItemId;            ///< 收获产出物品 id
    int yield = 0;                        ///< 单株产量(≥1)

    /// @brief 阶段总数。
    int StageCount() const { return static_cast<int>(stageNames.size()); }
    /// @brief 最后(成熟)阶段索引。
    int LastStage() const { return StageCount() - 1; }
    /// @brief 给定阶段是否为成熟阶段。
    bool IsMatureStage(int stage) const { return stage == LastStage(); }
};

/// @brief 作物表:id → CropConfig 的只读集合。
class CropDatabase {
public:
    CropDatabase() = default;

    /// @brief 查作物;未命中返回 nullptr。
    const CropConfig* Find(const std::string& id) const;
    /// @brief 作物种类数。
    std::size_t Size() const { return crops_.size(); }

private:
    friend std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json&);
    std::vector<CropConfig> crops_;
};

/**
 * @brief 从 JSON 数组解析并校验作物表。
 * @return 校验通过返回数据库;顶层非数组/任一作物字段缺失/类型错/语义越界/id 重复
 *         返回 std::nullopt(不抛异常)。
 */
std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json& j);

} // namespace me::domain
