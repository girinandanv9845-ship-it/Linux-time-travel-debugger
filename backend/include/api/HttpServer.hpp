#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <set>
#include <map>
#include "session/Session.hpp"

// Forward declare Crow app
namespace crow {
    template <typename ... Middlewares>
    class Crow;
    namespace websocket {
        struct connection;
    }
}

namespace ttd {

class HttpServer {
public:
    HttpServer(int port, std::shared_ptr<SessionManager> session_mgr);
    ~HttpServer();

    // Configures routes and starts server (blocking or non-blocking)
    void run();
    void stop();

    // Broadcasts an event to all connected WebSocket clients for a session
    void broadcast_ws_event(const std::string& session_id,
                            const std::string& type,
                            const nlohmann::json& payload);

private:
    int port_;
    std::shared_ptr<SessionManager> session_mgr_;
    std::unique_ptr<crow::Crow<>> app_;

    mutable std::mutex ws_mutex_;
    // session_id -> set of active websocket connections
    std::map<std::string, std::set<crow::websocket::connection*>> ws_connections_;

    void setup_routes();
    void setup_websocket();
};

} // namespace ttd
