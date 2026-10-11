#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "common/Types.hpp"
#include "process/ProcessManager.hpp"
#include "tracing/TracingEngine.hpp"
#include "recording/EventRecorder.hpp"
#include "checkpoint/CheckpointManager.hpp"
#include "replay/ReplayEngine.hpp"

namespace ttd {

using EventCallback = std::function<void(const std::string& event_type, const nlohmann::json& payload)>;

class Session {
public:
    Session(const std::string& session_id,
            const std::string& source_code,
            const CompilationOptions& options);
    ~Session();

    // Lifecycle actions
    bool start(std::string& out_error);
    bool step(TraceEvent& out_event, std::string& out_error);
    bool resume(std::string& out_error);
    bool pause(std::string& out_error);
    bool stop(std::string& out_error);

    // Queries
    std::string get_id() const { return session_id_; }
    SessionState get_state() const;
    pid_t get_pid() const;
    std::string get_source_code() const { return source_code_; }
    const CompilationResult& get_compilation_result() const { return compile_result_; }

    // Component accessors
    const EventRecorder& get_recorder() const { return recorder_; }
    const CheckpointManager& get_checkpoints() const { return checkpoints_; }
    const ReplayEngine& get_replay() const { return replay_; }

    // State inspections
    nlohmann::json get_current_state() const;
    std::optional<Registers> get_live_registers() const;
    MemoryDump read_memory(uint64_t address, size_t length) const;
    ProgramOutput get_output() const;

    // Time-travel seek
    SeekResult seek(uint64_t sequence);

    // Register event listeners (e.g. for WebSockets)
    void set_event_callback(EventCallback cb);

private:
    std::string session_id_;
    std::string source_code_;
    CompilationOptions options_;
    CompilationResult compile_result_;

    mutable std::mutex state_mutex_;
    mutable std::mutex control_mutex_;
    SessionState state_{SessionState::CREATED};
    pid_t pid_{-1};

    ProcessManager process_mgr_;
    std::unique_ptr<TracingEngine> tracer_;
    EventRecorder recorder_;
    CheckpointManager checkpoints_;
    ReplayEngine replay_;

    ChildProcess child_process_;
    std::string accumulated_stdout_;
    std::string accumulated_stderr_;

    std::atomic<bool> run_loop_active_{false};
    std::unique_ptr<std::thread> run_thread_;

    // All ptrace and tracee wait operations run on this persistent Linux
    // thread. The tracee is forked there so PTRACE_TRACEME ownership never
    // changes as HTTP requests and Resume workers come and go.
    mutable std::mutex tracer_queue_mutex_;
    mutable std::condition_variable tracer_queue_cv_;
    mutable std::deque<std::function<void()>> tracer_tasks_;
    mutable bool tracer_queue_stopping_{false};
    std::thread tracer_thread_;

    EventCallback event_callback_;

    uint64_t step_count_{0};
    uint64_t selected_sequence_{0};

    void transition_to(SessionState new_state);
    void notify_event(const std::string& type, const nlohmann::json& payload);
    void stop_run_loop_and_join();
    void tracer_executor_loop();
    void shutdown_tracer_executor();
    void drain_output_locked();
    void background_run_loop();
    void capture_step_state(const StopInfo& stop, EventType event_type, const std::string& desc);

    template <typename Callable>
    decltype(auto) run_trace_task(Callable&& callable) const {
        using Result = std::invoke_result_t<Callable>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Callable>(callable));
        auto result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(tracer_queue_mutex_);
            if (tracer_queue_stopping_) {
                throw std::runtime_error("Tracing executor is shutting down");
            }
            tracer_tasks_.emplace_back([task]() { (*task)(); });
        }
        tracer_queue_cv_.notify_one();
        return result.get();
    }
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    std::shared_ptr<Session> create_session(const std::string& source_code,
                                            const CompilationOptions& options);
    std::shared_ptr<Session> get_session(const std::string& session_id) const;
    bool remove_session(const std::string& session_id);
    std::vector<std::string> list_session_ids() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    uint64_t session_id_counter_{1};

    std::string generate_session_id();
};

} // namespace ttd
