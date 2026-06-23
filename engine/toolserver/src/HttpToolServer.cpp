#include "me/toolserver/HttpToolServer.h"

#include <httplib.h>

#include "me/core/Log.h"
#include "me/toolserver/ToolDispatcher.h"

namespace me::toolserver {

namespace {
constexpr const char* kJsonMime = "application/json";
} // namespace

HttpToolServer::HttpToolServer(ToolDispatcher& dispatcher, std::string host, int port,
                               std::string staticRoot)
    : dispatcher_(dispatcher),
      host_(std::move(host)),
      port_(port),
      staticRoot_(std::move(staticRoot)) {}

HttpToolServer::~HttpToolServer() = default;

bool HttpToolServer::Run() {
    server_ = std::make_unique<httplib::Server>();

    server_->Post("/invoke", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_content(dispatcher_.HandleInvoke(req.body), kJsonMime);
    });
    server_->Get("/tools", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(dispatcher_.HandleListTools(), kJsonMime);
    });
    if (!staticRoot_.empty()) {
        // 同源服务前端静态资源(免 CORS);失败不致命(端点仍可用)。
        if (!server_->set_mount_point("/", staticRoot_)) {
            ME_LOG_WARN("静态根目录不存在或无法挂载: " + staticRoot_);
        }
    }

    return server_->listen(host_, port_); // 阻塞直到 Stop()
}

void HttpToolServer::Stop() {
    if (server_) server_->stop(); // httplib::Server::stop 线程安全
}

} // namespace me::toolserver
