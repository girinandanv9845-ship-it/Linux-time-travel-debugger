#include <iostream>
#include <memory>
#include <cstdlib>
#include <csignal>
#include "common/Types.hpp"
#include "session/Session.hpp"
#include "api/HttpServer.hpp"

static std::unique_ptr<ttd::HttpServer> g_server;

void signal_handler(int sig) {
    std::cout << "\nReceived shutdown signal (" << sig << "), stopping server...\n";
    if (g_server) {
        g_server->stop();
    }
    std::exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================================\n";
    std::cout << " Web-Based Linux Process Time-Travel Debugger (Backend)   \n";
    std::cout << "==========================================================\n";

    // Architecture validation check
    if (!ttd::IS_SUPPORTED_ARCH) {
        std::cerr << "[CRITICAL ERROR] Target host is running on an unsupported architecture: "
                  << ttd::SUPPORTED_ARCHITECTURE << "\n";
        std::cerr << "This tracer requires Linux x86_64 architecture for ptrace register layouts.\n";
        return 1;
    }

    std::cout << "[INFO] Verified host architecture: " << ttd::SUPPORTED_ARCHITECTURE << " (Native Linux ptrace)\n";

    int port = 8080;
    if (const char* env_port = std::getenv("PORT")) {
        try {
            port = std::stoi(env_port);
        } catch (...) {
            std::cerr << "[WARN] Invalid PORT environment variable, defaulting to 8080\n";
        }
    }
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {}
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto session_mgr = std::make_shared<ttd::SessionManager>();
    g_server = std::make_unique<ttd::HttpServer>(port, session_mgr);

    std::cout << "[INFO] REST API listening on: http://0.0.0.0:" << port << "/api/health\n";
    std::cout << "[INFO] WebSocket streaming on: ws://0.0.0.0:" << port << "/ws/debug/{sessionId}\n";
    std::cout << "[INFO] Server started successfully. Ready for debugging sessions.\n";

    g_server->run();
    return 0;
}
