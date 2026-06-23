#pragma once

#include <memory>
#include <string>

namespace httplib { class Server; } // 前置声明:封死 httplib.h 在 .cpp 内

namespace me::toolserver {

class ToolDispatcher;

/// @brief 默认绑定主机(仅本地;本地开发工具,非生产服务)。
inline constexpr const char* kBindHost = "127.0.0.1";
/// @brief 默认端口(可由 toolserver_app 命令行覆盖)。
inline constexpr int kDefaultPort = 8080;

/**
 * @brief cpp-httplib 薄壳:HTTP 路由 → ToolDispatcher,逻辑全部委托 dispatcher。
 *
 * 路由:POST /invoke、GET /tools、可选 GET /*(静态根目录)。
 * 薄到无需自动化 socket 测试,手动 curl 冒烟(见 README)。
 */
class HttpToolServer {
public:
    /// @param dispatcher  逻辑核心(本壳不拥有,生命周期由调用方保证)。
    /// @param host        绑定地址(如 kBindHost)。
    /// @param port        监听端口。
    /// @param staticRoot  可选静态文件根目录;空则禁用静态服务。
    HttpToolServer(ToolDispatcher& dispatcher, std::string host, int port,
                   std::string staticRoot = {});
    ~HttpToolServer(); // 定义在 .cpp:unique_ptr<httplib::Server> 需完整类型

    HttpToolServer(const HttpToolServer&) = delete;
    HttpToolServer& operator=(const HttpToolServer&) = delete;

    /// @brief 注册三路由并阻塞 listen();绑定失败返回 false。
    bool Run();
    /// @brief 线程安全地停止 listen()(供信号处理调用)。
    void Stop();

private:
    ToolDispatcher& dispatcher_;
    std::string host_;
    int port_;
    std::string staticRoot_;
    std::unique_ptr<httplib::Server> server_;
};

} // namespace me::toolserver
