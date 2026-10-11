#include "api/HttpServer.hpp"
#include "crow_all.h"
#include <iostream>

namespace ttd {

static crow::response json_response(int code, const nlohmann::json& body) {
    crow::response res(code, body.dump());
    res.add_header("Content-Type", "application/json");
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
    res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
    return res;
}

static crow::response error_response(int code, const std::string& message) {
    return json_response(code, {
        {"success", false},
        {"error", message}
    });
}

HttpServer::HttpServer(int port, std::shared_ptr<SessionManager> session_mgr)
    : port_(port),
      session_mgr_(std::move(session_mgr)),
      app_(std::make_unique<crow::SimpleApp>()) {
    setup_routes();
    setup_websocket();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::broadcast_ws_event(const std::string& session_id,
                                    const std::string& type,
                                    const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    auto it = ws_connections_.find(session_id);
    if (it == ws_connections_.end()) return;

    nlohmann::json msg = {
        {"type", type},
        {"sessionId", session_id},
        {"payload", payload}
    };
    std::string text = msg.dump();

    for (auto* conn : it->second) {
        if (conn) {
            conn->send_text(text);
        }
    }
}

void HttpServer::setup_websocket() {
    CROW_WEBSOCKET_ROUTE((*app_), "/ws/debug/<string>")
        .onopen([this](crow::websocket::connection& conn) {
            // URL format: /ws/debug/<sessionId>
            // We can match session ID from connection or request
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            if (is_binary) return;
            try {
                auto j = nlohmann::json::parse(data);
                if (j.contains("action") && j["action"] == "subscribe" && j.contains("sessionId")) {
                    std::string sess_id = j["sessionId"];
                    std::lock_guard<std::mutex> lock(ws_mutex_);
                    ws_connections_[sess_id].insert(&conn);

                    // Send confirmation
                    nlohmann::json ack = {
                        {"type", "subscribed"},
                        {"sessionId", sess_id},
                        {"status", "connected"}
                    };
                    conn.send_text(ack.dump());
                } else if (j.contains("action") && j["action"] == "ping") {
                    conn.send_text("{\"type\":\"pong\"}");
                }
            } catch (...) {}
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& /*reason*/) {
            std::lock_guard<std::mutex> lock(ws_mutex_);
            for (auto& [sess_id, conns] : ws_connections_) {
                conns.erase(&conn);
            }
        });
}

void HttpServer::setup_routes() {
    // ------------------------------------------------------------------------
    // CORS Preflight Options handler
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/<path>")
        .methods(crow::HTTPMethod::OPTIONS)([](const crow::request&, const std::string&) {
            crow::response res(204);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
            res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
            return res;
        });

    CROW_ROUTE((*app_), "/api/<path>")
        .methods(crow::HTTPMethod::OPTIONS)([](const crow::request&, const std::string&) {
            crow::response res(204);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
            res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
            return res;
        });

    // ------------------------------------------------------------------------
    // GET /api/health
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/health")
        .methods(crow::HTTPMethod::GET)([]() {
            return json_response(200, {
                {"status", "healthy"},
                {"service", "linux-time-travel-debugger"},
                {"version", "1.0.0"},
                {"architecture", SUPPORTED_ARCHITECTURE},
                {"isSupportedArch", IS_SUPPORTED_ARCH}
            });
        });

    // ------------------------------------------------------------------------
    // GET /api/capabilities
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/capabilities")
        .methods(crow::HTTPMethod::GET)([]() {
            return json_response(200, {
                {"architecture", SUPPORTED_ARCHITECTURE},
                {"isSupportedArch", IS_SUPPORTED_ARCH},
                {"tracingEngine", "Linux ptrace (PTRACE_SINGLESTEP, PTRACE_GETREGS, PTRACE_PEEKDATA, process_vm_readv)"},
                {"supportedLanguages", {"c", "cpp"}},
                {"maxMemoryReadBytes", 1024},
                {"maxEventsPerSession", 10000},
                {"maxCheckpointsPerSession", 200},
                {"eventTypes", {
                    "process_started", "initial_tracing_stop", "execution_resumed",
                    "single_step_completed", "register_snapshot_captured", "signal_observed",
                    "syscall_entry", "syscall_exit", "checkpoint_captured",
                    "process_exited", "process_terminated_by_signal", "tracing_error"
                }},
                {"replayGuarantees", {
                    {"mode", "Captured State & Checkpoint Navigation"},
                    {"description", "Allows historical navigation to any recorded sequence number. Register snapshots and bounded stack dumps are displayed honestly from recorded checkpoints without falsifying uncaptured state."},
                    {"limitations", "Full arbitrary deterministic process rewind is bounded by external I/O and unrecorded kernel states."}
                }}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/start
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/start")
        .methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
            nlohmann::json body;
            try {
                body = nlohmann::json::parse(req.body);
            } catch (...) {
                return error_response(400, "Invalid JSON in request body");
            }

            if (!body.contains("source") || !body["source"].is_string()) {
                return error_response(400, "Field 'source' is required and must be a string");
            }

            std::string source = body["source"].get<std::string>();
            if (source.empty()) {
                return error_response(400, "Source code cannot be empty");
            }

            CompilationOptions opts;
            if (body.contains("language") && body["language"].is_string()) {
                opts.language = body["language"].get<std::string>();
            }
            if (body.contains("options") && body["options"].is_object()) {
                auto o = body["options"];
                if (o.contains("optimization") && o["optimization"].is_string()) {
                    opts.optimization = o["optimization"].get<std::string>();
                }
                if (o.contains("debugSymbols") && o["debugSymbols"].is_boolean()) {
                    opts.debug_symbols = o["debugSymbols"].get<bool>();
                }
            }

            auto session = session_mgr_->create_session(source, opts);
            std::string sess_id = session->get_id();

            // Wire WebSocket event callbacks from session to connected clients
            session->set_event_callback([this, sess_id](const std::string& type, const nlohmann::json& payload) {
                this->broadcast_ws_event(sess_id, type, payload);
            });

            std::string err;
            bool ok = session->start(err);
            if (!ok) {
                return json_response(422, {
                    {"success", false},
                    {"sessionId", sess_id},
                    {"error", err},
                    {"compilation", {
                        {"success", session->get_compilation_result().success},
                        {"exitCode", session->get_compilation_result().exit_code},
                        {"stdout", session->get_compilation_result().stdout_output},
                        {"stderr", session->get_compilation_result().stderr_output}
                    }}
                });
            }

            return json_response(201, {
                {"success", true},
                {"sessionId", sess_id},
                {"state", to_string(session->get_state())},
                {"pid", session->get_pid()},
                {"initialState", session->get_current_state()}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/<sessionId>/pause
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/pause")
        .methods(crow::HTTPMethod::POST)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            std::string err;
            if (!session->pause(err)) {
                return error_response(400, err);
            }
            return json_response(200, {
                {"success", true},
                {"state", to_string(session->get_state())}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/<sessionId>/resume
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/resume")
        .methods(crow::HTTPMethod::POST)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            std::string err;
            if (!session->resume(err)) {
                return error_response(400, err);
            }
            return json_response(200, {
                {"success", true},
                {"state", to_string(session->get_state())}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/<sessionId>/step
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/step")
        .methods(crow::HTTPMethod::POST)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            TraceEvent event;
            std::string err;
            if (!session->step(event, err)) {
                return error_response(400, err);
            }

            return json_response(200, {
                {"success", true},
                {"state", to_string(session->get_state())},
                {"event", event.to_json()},
                {"currentState", session->get_current_state()}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/<sessionId>/stop
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/stop")
        .methods(crow::HTTPMethod::POST)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            std::string err;
            session->stop(err);
            return json_response(200, {
                {"success", true},
                {"state", to_string(session->get_state())}
            });
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/state
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/state")
        .methods(crow::HTTPMethod::GET)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            return json_response(200, session->get_current_state());
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/timeline
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/timeline")
        .methods(crow::HTTPMethod::GET)([this](const crow::request& req, const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            uint64_t from = 1;
            uint64_t to = UINT64_MAX;
            size_t limit = 1000;
            size_t offset = 0;

            if (req.url_params.get("from")) from = std::stoull(req.url_params.get("from"));
            if (req.url_params.get("to"))   to = std::stoull(req.url_params.get("to"));
            if (req.url_params.get("limit")) limit = std::stoul(req.url_params.get("limit"));
            if (req.url_params.get("offset")) offset = std::stoul(req.url_params.get("offset"));

            auto events = session->get_recorder().query_events(from, to, "", limit, offset);
            auto ckpts = session->get_checkpoints().get_all();

            nlohmann::json events_json = nlohmann::json::array();
            for (const auto& ev : events) events_json.push_back(ev.to_json());

            nlohmann::json ckpt_json = nlohmann::json::array();
            for (const auto& c : ckpts) ckpt_json.push_back(c.to_json());

            return json_response(200, {
                {"sessionId", session_id},
                {"totalEvents", session->get_recorder().get_total_events()},
                {"latestSequence", session->get_recorder().get_latest_sequence()},
                {"events", events_json},
                {"checkpoints", ckpt_json}
            });
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/events
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/events")
        .methods(crow::HTTPMethod::GET)([this](const crow::request& req, const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            std::string type_filter;
            if (req.url_params.get("type")) type_filter = req.url_params.get("type");

            size_t limit = 100;
            size_t offset = 0;
            if (req.url_params.get("limit")) limit = std::stoul(req.url_params.get("limit"));
            if (req.url_params.get("offset")) offset = std::stoul(req.url_params.get("offset"));

            auto events = session->get_recorder().query_events(1, UINT64_MAX, type_filter, limit, offset);
            nlohmann::json events_json = nlohmann::json::array();
            for (const auto& ev : events) events_json.push_back(ev.to_json());

            return json_response(200, {
                {"sessionId", session_id},
                {"events", events_json}
            });
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/checkpoints
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/checkpoints")
        .methods(crow::HTTPMethod::GET)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            auto ckpts = session->get_checkpoints().get_all();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& c : ckpts) arr.push_back(c.to_json());

            return json_response(200, {
                {"sessionId", session_id},
                {"count", ckpts.size()},
                {"checkpoints", arr}
            });
        });

    // ------------------------------------------------------------------------
    // POST /api/debug/<sessionId>/seek
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/seek")
        .methods(crow::HTTPMethod::POST)([this](const crow::request& req, const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            nlohmann::json body;
            try {
                body = nlohmann::json::parse(req.body);
            } catch (...) {
                return error_response(400, "Invalid JSON");
            }

            if (!body.contains("targetSequence") || !body["targetSequence"].is_number()) {
                return error_response(400, "Field 'targetSequence' is required and must be an integer");
            }

            uint64_t target_seq = body["targetSequence"].get<uint64_t>();
            SeekResult res = session->seek(target_seq);
            if (!res.success) {
                return error_response(400, res.error_message);
            }

            return json_response(200, res.to_json());
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/memory
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/memory")
        .methods(crow::HTTPMethod::GET)([this](const crow::request& req, const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            if (!req.url_params.get("address")) {
                return error_response(400, "Query parameter 'address' is required");
            }

            std::string addr_str = req.url_params.get("address");
            uint64_t address = 0;
            try {
                address = std::stoull(addr_str, nullptr, 0);
            } catch (...) {
                return error_response(400, "Invalid hex or decimal address format");
            }

            size_t length = 64;
            if (req.url_params.get("length")) {
                try {
                    length = std::stoul(req.url_params.get("length"));
                } catch (...) {}
            }

            MemoryDump dump = session->read_memory(address, length);
            return json_response(200, dump.to_json());
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/registers
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/registers")
        .methods(crow::HTTPMethod::GET)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            auto live_regs = session->get_live_registers();
            if (live_regs.has_value()) {
                return json_response(200, {
                    {"sessionId", session_id},
                    {"isLive", true},
                    {"registers", live_regs.value().to_json()}
                });
            }

            // If not live, check latest checkpoint
            auto ckpts = session->get_checkpoints().get_all();
            if (!ckpts.empty()) {
                const auto& last = ckpts.back();
                return json_response(200, {
                    {"sessionId", session_id},
                    {"isLive", false},
                    {"isHistorical", true},
                    {"checkpointId", last.id},
                    {"sequence", last.sequence},
                    {"registers", last.registers.to_json()}
                });
            }

            return error_response(404, "No live or captured registers available");
        });

    // ------------------------------------------------------------------------
    // GET /api/debug/<sessionId>/output
    // ------------------------------------------------------------------------
    CROW_ROUTE((*app_), "/api/debug/<string>/output")
        .methods(crow::HTTPMethod::GET)([this](const std::string& session_id) {
            auto session = session_mgr_->get_session(session_id);
            if (!session) return error_response(404, "Session not found: " + session_id);

            return json_response(200, session->get_output().to_json());
        });
}

void HttpServer::run() {
    app_->port(port_).multithreaded().run();
}

void HttpServer::stop() {
    if (app_) {
        app_->stop();
    }
}

} // namespace ttd
