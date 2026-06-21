#include "me/toolapi/SchemaValidator.h"

namespace me::toolapi {
namespace {

using nlohmann::json;

// 类型关键字 → JSON 值类型匹配。number 接受整数与浮点;integer 仅整数。
bool TypeMatches(const std::string& type, const json& v) {
    if (type == "object")  return v.is_object();
    if (type == "array")   return v.is_array();
    if (type == "string")  return v.is_string();
    if (type == "boolean") return v.is_boolean();
    if (type == "integer") return v.is_number_integer();
    if (type == "number")  return v.is_number();
    return false; // 未知 type 关键字:视为不匹配
}

void ValidateNode(const json& schema, const json& value, const std::string& path,
                  std::vector<std::string>& errors);

// 校验对象:required 存在性 + 各 property 递归。
void ValidateObject(const json& schema, const json& obj, const std::string& path,
                    std::vector<std::string>& errors) {
    if (schema.contains("required")) {
        for (const auto& r : schema["required"]) {
            const std::string key = r.get<std::string>();
            if (!obj.contains(key))
                errors.push_back("missing required field: " + path + key);
        }
    }
    if (schema.contains("properties")) {
        const json& props = schema["properties"];
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (props.contains(it.key()))
                ValidateNode(props[it.key()], it.value(), path + it.key() + ".", errors);
            // 未声明字段:宽松放行(M6 不做 additionalProperties 严格模式)
        }
    }
}

void ValidateNode(const json& schema, const json& value, const std::string& path,
                  std::vector<std::string>& errors) {
    // path 末尾带 '.';去掉它得到字段名,根节点显示 <root>。
    const std::string field = path.empty() ? "<root>" : path.substr(0, path.size() - 1);

    if (schema.contains("type")) {
        const std::string type = schema["type"].get<std::string>();
        if (!TypeMatches(type, value)) {
            errors.push_back("field '" + field + "' expected " + type);
            return; // 类型不符:不再深入检查本节点
        }
        if (type == "object") {
            ValidateObject(schema, value, path, errors);
            return;
        }
        if (type == "number" || type == "integer") {
            const double n = value.get<double>();
            if (schema.contains("minimum") && n < schema["minimum"].get<double>())
                errors.push_back("field '" + field + "' below minimum");
            if (schema.contains("maximum") && n > schema["maximum"].get<double>())
                errors.push_back("field '" + field + "' above maximum");
        }
        if (type == "string" && schema.contains("enum")) {
            bool found = false;
            for (const auto& e : schema["enum"]) {
                if (e.get<std::string>() == value.get<std::string>()) {
                    found = true;
                    break;
                }
            }
            if (!found) errors.push_back("field '" + field + "' not in enum");
        }
    } else if (value.is_object()) {
        // 无显式 type 但带 properties/required → 按对象处理
        ValidateObject(schema, value, path, errors);
    }
}

} // namespace

ValidationResult ValidateAgainstSchema(const json& schema, const json& params) {
    ValidationResult result;
    if (!params.is_object()) {
        result.ok = false;
        result.errors.push_back("params must be a JSON object");
        return result;
    }
    ValidateNode(schema, params, "", result.errors);
    result.ok = result.errors.empty();
    return result;
}

} // namespace me::toolapi
