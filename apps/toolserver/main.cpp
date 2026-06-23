// MiniEngine 无头 Tool 服务器:加载 time/crops 配置,组装受控上下文,跑本地 HTTP 服务。
// 不链接 RHI/DX12;场景以空场景启动,经 scene.create_entity 编辑(见计划范围说明)。
#include <csignal>
#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "me/command/CommandStack.h"
#include "me/core/Log.h"
#include "me/domain/CropConfig.h"
#include "me/domain/FarmField.h"
#include "me/domain/Inventory.h"
#include "me/domain/ItemConfig.h"
#include "me/domain/TimeConfig.h"
#include "me/domain/TimeSystem.h"
#include "me/platform/FileSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"
#include "me/toolserver/HttpToolServer.h"
#include "me/toolserver/ToolDispatcher.h"

namespace {

me::toolserver::HttpToolServer* g_server = nullptr; ///< 供信号处理停止(进程级唯一,生命周期覆盖 listen)

void HandleSigint(int) {
    if (g_server) g_server->Stop();
}

/// @brief 读文件并解析为 JSON;失败打错误日志返回 nullopt。
// NOTE: ME_LOG_* 接受单个 std::string(非 printf 风格),故用字符串拼接构建消息。
std::optional<nlohmann::json> LoadJsonFile(const std::string& path) {
    const auto text = me::platform::ReadTextFile(path);
    if (!text) {
        ME_LOG_ERROR("无法读取配置文件: " + path);
        return std::nullopt;
    }
    auto j = nlohmann::json::parse(*text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        ME_LOG_ERROR("配置文件非法 JSON: " + path);
        return std::nullopt;
    }
    return j;
}

} // namespace

int main(int argc, char** argv) {
    namespace dom = me::domain;
    namespace api = me::toolapi;
    namespace srv = me::toolserver;

    const std::string assetDir = ME_ASSET_DIR; // 编译期注入(见 apps/toolserver/CMakeLists.txt)
    int port = srv::kDefaultPort;
    if (argc >= 2) {
        port = std::atoi(argv[1]); // 可选命令行覆盖端口
        if (port <= 0) {
            ME_LOG_ERROR(std::string("非法端口: ") + argv[1]);
            return 1;
        }
    }
    const std::string staticRoot = (argc >= 3) ? argv[2] : std::string{}; // 可选前端静态根

    // —— 加载 mandatory 配置:缺失/非法即退出(无它们 time/crop Tool 无法工作)——
    const auto timeJson = LoadJsonFile(assetDir + "/config/time.json");
    if (!timeJson) return 1;
    const auto timeCfg = dom::LoadTimeConfig(*timeJson);
    if (!timeCfg) { ME_LOG_ERROR("time.json 语义校验失败"); return 1; }

    const auto cropJson = LoadJsonFile(assetDir + "/config/crops.json");
    if (!cropJson) return 1;
    auto cropDb = dom::LoadCropDatabase(*cropJson);
    if (!cropDb) { ME_LOG_ERROR("crops.json 语义校验失败"); return 1; }

    const auto itemJson = LoadJsonFile(assetDir + "/config/items.json");
    if (!itemJson) return 1;
    auto invCfg = dom::LoadInventoryConfig(*itemJson);
    if (!invCfg) { ME_LOG_ERROR("items.json 语义校验失败"); return 1; }

    // 交叉软告警(不硬失败):每个作物的 harvestItemId 应能在物品表找到。
    for (const auto& c : *cropJson) {
        if (c.contains("harvestItemId") && c["harvestItemId"].is_string()) {
            const std::string hid = c["harvestItemId"].get<std::string>();
            if (invCfg->items.Find(hid) == nullptr)
                ME_LOG_WARN("crops.json 的 harvestItemId 不在 items.json: " + hid);
        }
    }

    // —— 组装受控状态(空场景:前端经 scene.create_entity 编辑)——
    me::scene::Scene scene;
    me::command::CommandStack stack;
    api::ToolInvocationLog log;
    dom::TimeSystem time(*timeCfg);
    dom::FarmField farm(*cropDb);
    dom::Inventory inventory(invCfg->items, invCfg->capacity);
    api::ToolContext ctx{scene, stack, log, &time, &farm, &inventory};

    api::ToolRegistry registry;
    api::RegisterBuiltinTools(registry);

    srv::ToolDispatcher dispatcher(ctx, registry);
    srv::HttpToolServer server(dispatcher, srv::kBindHost, port, staticRoot);
    g_server = &server;
    std::signal(SIGINT, HandleSigint);

    ME_LOG_INFO("Tool 服务器监听 http://" + std::string(srv::kBindHost) + ":" + std::to_string(port) + " (Ctrl+C 退出)");
    const bool ok = server.Run(); // 阻塞
    g_server = nullptr;           // 防第二次 SIGINT 触发悬空指针
    if (!ok) {
        ME_LOG_ERROR("监听失败(端口 " + std::to_string(port) + " 可能被占用)");
        return 1;
    }
    ME_LOG_INFO("Tool 服务器已停止");
    return 0;
}
