#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::toolapi {

/// @brief 校验结果:ok=true 表示无错误,否则 errors 列出全部人读错误。
struct ValidationResult {
    bool ok = true;
    std::vector<std::string> errors;
};

/**
 * @brief 按 JSON Schema 子集校验参数(手写,零额外依赖,不抛异常)。
 *
 * 支持子集:type(object/array/string/number/integer/boolean)、required、
 * properties(递归)、minimum/maximum(number/integer)、enum(string)。
 * params 顶层须为对象;未在 schema 声明的多余字段宽松放行。
 *
 * @param schema  JSON Schema 子集描述。
 * @param params  待校验的 Tool 参数。
 * @return 收集到的全部错误;errors 为空即通过。
 */
ValidationResult ValidateAgainstSchema(const nlohmann::json& schema,
                                       const nlohmann::json& params);

} // namespace me::toolapi
