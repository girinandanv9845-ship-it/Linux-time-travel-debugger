#include "session/Session.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace ttd {

Session::Session(const std::string& session_id,
                 const std::string& source_code,
                 const CompilationOptions& options)
    : session_id_(session_id),
      source_code_(source_code),
      options_(options),
      recorder_(10000),
      checkpoints_(200),
      replay_(recorder_, checkpoints_) {
    tracer_thread_ = std::thread(&Session::tracer_executor_loop, this);
}

Session::~Session() {
    std::string err;
    stop(err);
    shutdown_tracer_executor();
    process_mgr_.cleanup_session_dir(session_id_);
}

SessionState Session::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

pid_t Session::get_pid() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return pid_;
}

void Session::set_event_callback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    event_callback_ = std::move(cb);
}

void Session::transition_to(SessionState new_state) {
    state_ = new_state;
    notify_event("session_state_changed", {
        {"sessionId", session_id_},
        {"state", to_string(new_state)},
        {"pid", pid_}
    });
}

static void log_debug_trace(const std::string& session_id, const std::string& stage, pid_t pid, const std::string& detail) {
    if (std::getenv("TTD_TRACE_DEBUG") == nullptr) {
        return;
    }
    std::cerr << "[ttd-trace][" << stage << "] sessionId=" << session_id
              << " pid=" << pid
              << " detail=" << detail << std::endl;
}

void Session::notify_event(const std::string& type, const nlohmann::json& payload) {
    if (event_callback_) {
        event_callback_(type, payload);
    }
}

void Session::stop_run_loop_and_join() {
    run_loop_active_.store(false, std::memory_order_release);
    if (run_thread_ && run_thread_->joinable()) {
        run_thread_->join();
        run_thread_.reset();
    }
}

void Session::tracer_executor_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(tracer_queue_mutex_);
            tracer_queue_cv_.wait(lock, [this] {
                return tracer_queue_stopping_ || !tracer_tasks_.empty();
            });
            if (tracer_queue_stopping_ && tracer_tasks_.empty()) {
                return;
            }
            task = std::move(tracer_tasks_.front());
            tracer_tasks_.pop_front();
        }
        task();
    }
}

void Session::shutdown_tracer_executor() {
    {
        std::lock_guard<std::mutex> lock(tracer_queue_mutex_);
        tracer_queue_stopping_ = true;
    }
    tracer_queue_cv_.notify_all();
    if (tracer_thread_.joinable()) {
        tracer_thread_.join();
    }
}

void Session::drain_output_locked() {
    if (child_process_.pid <= 0) return;
    std::string new_out, new_err;
    process_mgr_.drain_output(child_process_, new_out, new_err);

    bool changed = false;
    if (!new_out.empty()) {
        accumulated_stdout_.append(new_out);
        changed = true;
    }
    if (!new_err.empty()) {
        accumulated_stderr_.append(new_err);
        changed = true;
    }

    if (changed) {
        notify_event("program_output", {
            {"sessionId", session_id_},
            {"stdout", accumulated_stdout_},
            {"stderr", accumulated_stderr_},
            {"newStdout", new_out},
            {"newStderr", new_err}
        });
    }
}

bool Session::start(std::string& out_error) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ != SessionState::CREATED) {
        out_error = "Session can only be started from CREATED state; current state is " + to_string(state_);
        return false;
    }

    transition_to(SessionState::COMPILING);

    // 1. Compile the target program
    compile_result_ = process_mgr_.compile(session_id_, source_code_, options_);
    if (!compile_result_.success) {
        transition_to(SessionState::FAILED);
        out_error = "Compilation failed:\n" + compile_result_.stderr_output;
        notify_event("compilation_failed", {
            {"sessionId", session_id_},
            {"exitCode", compile_result_.exit_code},
            {"stderr", compile_result_.stderr_output},
            {"stdout", compile_result_.stdout_output}
        });
        shutdown_tracer_executor();
        return false;
    }

    transition_to(SessionState::STARTING);

    // Creation, initial wait, ptrace setup, and first inspection all happen on
    // the persistent executor thread. This is the thread bound by TRACEME.
    struct TraceStartResult {
        ChildProcess child;
        std::unique_ptr<TracingEngine> tracer;
        StopInfo initial_stop;
        Registers registers;
        MemoryDump stack_sample;
        bool has_registers{false};
        std::string error;
    };

    log_debug_trace(session_id_, "start.launch", -1, "queueing tracee creation on owner thread");
    TraceStartResult started = run_trace_task([this]() {
        TraceStartResult result;
        result.child = process_mgr_.spawn_traced_child(compile_result_.binary_path);
        if (result.child.pid <= 0) {
            result.error = result.child.error_message.empty()
                ? "Failed to spawn traced child process"
                : result.child.error_message;
            return result;
        }

        result.tracer = std::make_unique<TracingEngine>(result.child.pid);
        if (!result.tracer->wait_for_stop(result.initial_stop) || result.initial_stop.is_terminated) {
            result.error = "Failed to intercept initial tracee stop: " + result.initial_stop.error_message;
            if (result.initial_stop.is_terminated) {
                result.child.is_running = false;
            }
            process_mgr_.cleanup_child(result.child);
            return result;
        }

        std::string init_error;
        if (!result.tracer->initialize_tracing(init_error)) {
            result.error = "Failed to initialize ptrace options: " + init_error;
            process_mgr_.cleanup_child(result.child);
            return result;
        }

        std::string register_error;
        result.has_registers = result.tracer->get_registers(result.registers, register_error);
        if (!result.has_registers) {
            result.error = "Failed to capture initial registers: " + register_error;
            process_mgr_.cleanup_child(result.child);
            return result;
        }
        result.stack_sample = result.tracer->read_memory(result.registers.rsp - 16, 64);
        return result;
    });

    if (!started.error.empty()) {
        transition_to(SessionState::FAILED);
        out_error = started.error;
        log_debug_trace(session_id_, "start.trace_init", started.child.pid, started.error);
        shutdown_tracer_executor();
        return false;
    }

    child_process_ = std::move(started.child);
    tracer_ = std::move(started.tracer);
    pid_ = child_process_.pid;
    const Registers& init_regs = started.registers;
    log_debug_trace(session_id_, "start.launch", pid_, "tracee initialized on persistent owner thread");

    // 6. Record process start events
    auto ev_start = recorder_.record(EventType::PROCESS_STARTED, pid_, init_regs.rip,
                                     "Process launched under ptrace supervision (PID: " + std::to_string(pid_) + ")");
    notify_event("trace_event", ev_start.to_json());

    auto ev_stop = recorder_.record(EventType::INITIAL_TRACING_STOP, pid_, init_regs.rip,
                                    "Initial tracing stop intercepted at entry point",
                                    std::optional<Registers>(init_regs));
    notify_event("trace_event", ev_stop.to_json());

    // 7. Capture initial checkpoint
    auto ckpt = checkpoints_.create_checkpoint(ev_stop.sequence, pid_, init_regs,
                                               started.stack_sample, "Process entry point");
    notify_event("checkpoint_created", ckpt.to_json());

    // 8. Ready in paused state
    transition_to(SessionState::PAUSED);
    drain_output_locked();
    return true;
}

void Session::capture_step_state(const StopInfo& stop, EventType event_type, const std::string& desc) {
    struct CapturedState {
        Registers registers;
        MemoryDump stack_sample;
        bool has_registers{false};
    } captured = run_trace_task([this, event_type]() {
        CapturedState state;
        std::string register_error;
        state.has_registers = tracer_ && tracer_->get_registers(state.registers, register_error);
        if (state.has_registers && ((step_count_ + 1) % 20 == 0 || event_type == EventType::SIGNAL_OBSERVED)) {
            state.stack_sample = tracer_->read_memory(state.registers.rsp - 16, 64);
        }
        return state;
    });
    const Registers& regs = captured.registers;

    step_count_++;
    uint64_t rip = captured.has_registers ? regs.rip : 0;

    std::optional<int32_t> sig_num = (stop.signal_number > 0) ? std::optional<int32_t>(stop.signal_number) : std::nullopt;

    auto ev = recorder_.record(event_type, pid_, rip, desc,
                               captured.has_registers ? std::optional<Registers>(regs) : std::nullopt,
                               sig_num, stop.signal_name);

    notify_event("trace_event", ev.to_json());

    // Checkpoint strategy: capture checkpoint every 20 steps, or on signals/syscalls
    if (captured.has_registers && (step_count_ % 20 == 0 || event_type == EventType::SIGNAL_OBSERVED)) {
        auto ckpt = checkpoints_.create_checkpoint(ev.sequence, pid_, regs, captured.stack_sample,
                                                   "Step #" + std::to_string(step_count_));
        notify_event("checkpoint_created", ckpt.to_json());
    }
}

bool Session::step(TraceEvent& out_event, std::string& out_error) {
    std::lock_guard<std::mutex> control_lock(control_mutex_);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != SessionState::PAUSED) {
            out_error = "Target can only be stepped while paused; current state: " + to_string(state_);
            return false;
        }
    }
    stop_run_loop_and_join();
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ != SessionState::PAUSED) {
        out_error = "Target can only be stepped while paused; current state: " + to_string(state_);
        return false;
    }
    if (!tracer_) {
        transition_to(SessionState::EXITED);
        out_error = "Tracing engine is unavailable; cannot single-step";
        return false;
    }

    StopInfo stop;
    bool stepped = run_trace_task([this, &stop] { return tracer_->single_step(stop); });
    drain_output_locked();

    if (!stepped && stop.reason == StopReason::ERROR) {
        out_error = "Single-step failed: " + stop.error_message;
        return false;
    }

    if (stop.is_terminated) {
        child_process_.is_running = false;
        if (stop.reason == StopReason::PROCESS_EXITED) {
            auto ev = recorder_.record(EventType::PROCESS_EXITED, pid_, 0,
                                       "Process exited normally with code " + std::to_string(stop.exit_code),
                                       std::nullopt, std::nullopt, "", std::nullopt, "", "",
                                       {{"exitCode", stop.exit_code}});
            out_event = ev;
            notify_event("trace_event", ev.to_json());
        } else {
            auto ev = recorder_.record(EventType::PROCESS_TERMINATED_BY_SIGNAL, pid_, 0,
                                       "Process terminated by signal " + stop.signal_name + " (" + std::to_string(stop.signal_number) + ")",
                                       std::nullopt, stop.signal_number, stop.signal_name);
            out_event = ev;
            notify_event("trace_event", ev.to_json());
        }
        transition_to(SessionState::EXITED);
        run_trace_task([this] { process_mgr_.cleanup_child(child_process_); });
        shutdown_tracer_executor();
        return true;
    }

    std::string desc = "Executed instruction at single-step #" + std::to_string(step_count_ + 1);
    EventType ev_type = EventType::SINGLE_STEP_COMPLETED;
    if (stop.reason == StopReason::SIGNAL_RECEIVED) {
        ev_type = EventType::SIGNAL_OBSERVED;
        desc = "Observed signal " + stop.signal_name;
    }

    capture_step_state(stop, ev_type, desc);
    auto latest_seq = recorder_.get_latest_sequence();
    auto ev_opt = recorder_.get_by_sequence(latest_seq);
    if (ev_opt.has_value()) {
        out_event = ev_opt.value();
    }
    return true;
}

void Session::background_run_loop() {
    int signal_to_pass = 0;
    while (run_loop_active_.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ != SessionState::RUNNING) {
                run_loop_active_.store(false, std::memory_order_release);
                break;
            }
            if (!tracer_) {
                transition_to(SessionState::FAILED);
                run_loop_active_.store(false, std::memory_order_release);
                break;
            }
        }

        if (recorder_.get_total_events() >= recorder_.get_max_events()) {
            run_loop_active_.store(false, std::memory_order_release);
            notify_event("tracing_limit_reached", {
                {"sessionId", session_id_},
                {"limit", recorder_.get_max_events()},
                {"message", "Trace event limit reached. Execution paused to preserve event history and UI responsiveness."}
            });
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == SessionState::RUNNING) {
                transition_to(SessionState::PAUSED);
            }
            break;
        }

        StopInfo stop;
        bool step_ok = false;
        try {
            step_ok = run_trace_task([this, &stop, &signal_to_pass] {
                return tracer_->single_step(stop, signal_to_pass);
            });
        } catch (const std::exception& ex) {
            stop.reason = StopReason::ERROR;
            stop.error_message = std::string("Tracing executor failed: ") + ex.what();
        }
        signal_to_pass = 0;

        drain_output_locked();

        if (!step_ok && stop.reason == StopReason::ERROR) {
            notify_event("tracing_error", {
                {"sessionId", session_id_},
                {"operation", "single_step_during_resume"},
                {"message", stop.error_message}
            });
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == SessionState::RUNNING) {
                transition_to(SessionState::FAILED);
            }
            run_loop_active_.store(false, std::memory_order_release);
            break;
        }

        if (stop.is_terminated) {
            if (stop.reason == StopReason::PROCESS_EXITED) {
                auto ev = recorder_.record(EventType::PROCESS_EXITED, pid_, 0,
                                           "Process exited with status " + std::to_string(stop.exit_code),
                                           std::nullopt, std::nullopt, "", std::nullopt, "", "",
                                           {{"exitCode", stop.exit_code}});
                notify_event("trace_event", ev.to_json());
            } else {
                auto ev = recorder_.record(EventType::PROCESS_TERMINATED_BY_SIGNAL, pid_, 0,
                                           "Process terminated by signal " + stop.signal_name,
                                           std::nullopt, stop.signal_number, stop.signal_name);
                notify_event("trace_event", ev.to_json());
            }
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                child_process_.is_running = false;
                transition_to(SessionState::EXITED);
                run_loop_active_.store(false, std::memory_order_release);
                run_trace_task([this] { process_mgr_.cleanup_child(child_process_); });
            }
            shutdown_tracer_executor();
            break;
        }

        if (stop.reason == StopReason::PAUSED_BY_USER ||
            (stop.signal_number == SIGSTOP && stop.reason == StopReason::SIGNAL_RECEIVED)) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == SessionState::RUNNING) {
                transition_to(SessionState::PAUSED);
            }
            run_loop_active_.store(false, std::memory_order_release);
            break;
        }

        if (stop.reason == StopReason::SIGNAL_RECEIVED) {
            capture_step_state(stop, EventType::SIGNAL_OBSERVED,
                               "Observed signal " + stop.signal_name + " during resumed execution");
            signal_to_pass = stop.signal_number;
        } else {
            capture_step_state(stop, EventType::SINGLE_STEP_COMPLETED,
                               "Executed one instruction during resumed execution");
        }

        // Each successful iteration leaves the tracee stopped after exactly
        // one instruction. This is both the recording boundary and a safe
        // point for Pause, Stop, or queued inspection requests.
        if (!run_loop_active_.load(std::memory_order_acquire) || recorder_.is_limit_reached()) {
            run_loop_active_.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == SessionState::RUNNING) {
                transition_to(SessionState::PAUSED);
            }
            if (recorder_.is_limit_reached()) {
                notify_event("tracing_limit_reached", {
                    {"sessionId", session_id_},
                    {"limit", recorder_.get_max_events()},
                    {"message", "Trace event limit reached. Execution paused to preserve event history and UI responsiveness."}
                });
            }
            break;
        }
    }
}

bool Session::resume(std::string& out_error) {
    std::lock_guard<std::mutex> control_lock(control_mutex_);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != SessionState::PAUSED) {
            out_error = "Target can only be resumed when paused; current state: " + to_string(state_);
            return false;
        }
    }

    // Join a completed prior Resume worker before creating another one.
    stop_run_loop_and_join();
    bool alive = tracer_ && run_trace_task([this] {
        return !tracer_->is_terminated() && tracer_->is_process_alive();
    });
    if (!alive) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        transition_to(SessionState::EXITED);
        out_error = "Target process is no longer available to the tracing owner thread";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != SessionState::PAUSED) {
            out_error = "Target can only be resumed when paused; current state: " + to_string(state_);
            return false;
        }
        transition_to(SessionState::RUNNING);
    }

    log_debug_trace(session_id_, "resume", pid_, "starting run loop");
    auto ev = recorder_.record(EventType::EXECUTION_RESUMED, pid_, 0, "Execution resumed by user");
    notify_event("trace_event", ev.to_json());

    run_loop_active_.store(true, std::memory_order_release);
    run_thread_ = std::make_unique<std::thread>(&Session::background_run_loop, this);
    return true;
}

bool Session::pause(std::string& out_error) {
    std::lock_guard<std::mutex> control_lock(control_mutex_);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != SessionState::RUNNING) {
            out_error = "Target is not running; cannot pause";
            return false;
        }
        if (!tracer_) {
            transition_to(SessionState::EXITED);
            out_error = "Target process has already exited; cannot pause";
            return false;
        }
    }

    bool alive = run_trace_task([this] {
        return !tracer_->is_terminated() && tracer_->is_process_alive();
    });
    if (!alive) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        transition_to(SessionState::EXITED);
        out_error = "Target process has already exited; cannot pause";
        return false;
    }

    // The worker may be blocked in waitpid() until the tracee stops. Request the
    // stop before joining it, otherwise Pause deadlocks on a running program.
    std::string signal_error;
    bool signalled = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        signalled = process_mgr_.signal_child(child_process_, SIGSTOP, signal_error);
    }
    if (!signalled) {
        out_error = "Failed to signal tracee for pause: " + signal_error;
        if (errno == ESRCH) {
            stop_run_loop_and_join();
        }
        return false;
    }

    stop_run_loop_and_join();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == SessionState::RUNNING) {
            transition_to(SessionState::PAUSED);
        }
        if (state_ != SessionState::PAUSED) {
            out_error = "Target exited before pause completed; current state: " + to_string(state_);
            drain_output_locked();
            return false;
        }
        drain_output_locked();
    }

    Registers regs;
    std::string err;
    bool have_regs = tracer_ && run_trace_task([this, &regs, &err] {
        return tracer_->get_registers(regs, err);
    });
    if (have_regs) {
        auto ev = recorder_.record(EventType::REGISTER_SNAPSHOT_CAPTURED, pid_, regs.rip,
                                   "Execution paused by user", regs);
        notify_event("trace_event", ev.to_json());
    }
    return true;
}

bool Session::stop(std::string& out_error) {
    std::lock_guard<std::mutex> control_lock(control_mutex_);
    // Wake a worker blocked in waitpid() before joining it, and request tracee
    // termination while it is still known to belong to this session.
    std::string signal_error;
    bool signal_failed = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (child_process_.is_running) {
            signal_failed = !process_mgr_.signal_child(child_process_, SIGKILL, signal_error);
        }
    }
    // Do not join a worker blocked in waitpid unless the tracee has been
    // successfully interrupted. ESRCH is safe only when the owner can collect
    // its terminal status, so route that case through the owner below.
    if (signal_failed && errno != ESRCH) {
        out_error = "Failed to signal tracee for stop: " + signal_error;
        return false;
    }
    stop_run_loop_and_join();

    std::optional<StopInfo> final_stop;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        drain_output_locked();
        bool executor_stopping = false;
        {
            std::lock_guard<std::mutex> queue_lock(tracer_queue_mutex_);
            executor_stopping = tracer_queue_stopping_;
        }
        if (child_process_.pid > 0 && !executor_stopping) {
            try {
                run_trace_task([this, &out_error, &final_stop] {
                    if (child_process_.is_running && tracer_ && !tracer_->is_terminated()) {
                        while (child_process_.is_running && !tracer_->is_terminated()) {
                            StopInfo observed_stop;
                            bool waited = tracer_->wait_for_stop(observed_stop);
                            if (!waited) {
                                if (!observed_stop.error_message.empty()) {
                                    out_error = "Failed to collect tracee termination status: " + observed_stop.error_message;
                                }
                                break;
                            }
                            if (observed_stop.is_terminated) {
                                final_stop = observed_stop;
                                child_process_.is_running = false;
                                break;
                            }
                            // A ptrace stop can arrive before the pending kill
                            // is reported as a final wait status. Reassert it,
                            // then keep collecting statuses on this owner.
                            std::string retry_signal_error;
                            if (!process_mgr_.signal_child(child_process_, SIGKILL, retry_signal_error) && errno != ESRCH) {
                                out_error = "Failed to re-signal tracee during stop: " + retry_signal_error;
                                break;
                            }
                        }
                    }
                    process_mgr_.cleanup_child(child_process_);
                });
            } catch (const std::exception& ex) {
                out_error = std::string("Failed to clean up tracing owner: ") + ex.what();
            }
        } else if (!executor_stopping) {
            run_trace_task([this] { process_mgr_.cleanup_child(child_process_); });
        }
        if (final_stop && final_stop->is_terminated) {
            if (final_stop->reason == StopReason::PROCESS_EXITED) {
                auto ev = recorder_.record(EventType::PROCESS_EXITED, pid_, 0,
                    "Process exited normally with code " + std::to_string(final_stop->exit_code),
                    std::nullopt, std::nullopt, "", std::nullopt, "", "",
                    {{"exitCode", final_stop->exit_code}});
                notify_event("trace_event", ev.to_json());
            } else {
                auto ev = recorder_.record(EventType::PROCESS_TERMINATED_BY_SIGNAL, pid_, 0,
                    "Process terminated by signal " + final_stop->signal_name +
                    " (" + std::to_string(final_stop->signal_number) + ")",
                    std::nullopt, final_stop->signal_number, final_stop->signal_name);
                notify_event("trace_event", ev.to_json());
            }
        }
        if (state_ != SessionState::STOPPED && state_ != SessionState::EXITED) {
            transition_to(SessionState::STOPPED);
        }
    }
    shutdown_tracer_executor();
    return true;
}

std::optional<Registers> Session::get_live_registers() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!tracer_ || state_ == SessionState::EXITED || state_ == SessionState::STOPPED) {
        return std::nullopt;
    }
    Registers r;
    std::string err;
    bool have_regs = run_trace_task([this, &r, &err] {
        return tracer_->get_registers(r, err);
    });
    if (have_regs) {
        return r;
    }
    return std::nullopt;
}

MemoryDump Session::read_memory(uint64_t address, size_t length) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!tracer_ || state_ == SessionState::EXITED || state_ == SessionState::STOPPED) {
        MemoryDump dump;
        dump.address = address;
        dump.length = length;
        dump.is_accessible = false;
        dump.error_message = !tracer_ ? "No active tracing engine in session"
                                      : "Target process has exited; live memory is unavailable";
        return dump;
    }
    return run_trace_task([this, address, length] {
        return tracer_->read_memory(address, length);
    });
}

ProgramOutput Session::get_output() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ProgramOutput out;
    out.stdout_data = accumulated_stdout_;
    out.stderr_data = accumulated_stderr_;
    out.sequence = recorder_.get_latest_sequence();
    out.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return out;
}

SeekResult Session::seek(uint64_t sequence) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    selected_sequence_ = sequence;
    return replay_.seek(sequence);
}

nlohmann::json Session::get_current_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    nlohmann::json j;
    j["sessionId"] = session_id_;
    j["state"] = to_string(state_);
    j["pid"] = pid_;
    j["totalEvents"] = recorder_.get_total_events();
    j["latestSequence"] = recorder_.get_latest_sequence();
    j["selectedSequence"] = selected_sequence_;
    j["checkpointsCount"] = checkpoints_.count();
    j["isLive"] = (state_ == SessionState::RUNNING || state_ == SessionState::PAUSED);

    if (tracer_ && (state_ == SessionState::PAUSED || state_ == SessionState::RUNNING)) {
        Registers r;
        std::string err;
        bool have_regs = run_trace_task([this, &r, &err] {
            return tracer_->get_registers(r, err);
        });
        if (have_regs) {
            j["liveRegisters"] = r.to_json();
        } else {
            j["liveRegisters"] = nullptr;
        }
    } else {
        j["liveRegisters"] = nullptr;
    }

    j["output"] = {
        {"stdout", accumulated_stdout_},
        {"stderr", accumulated_stderr_}
    };
    return j;
}

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager() = default;

SessionManager::~SessionManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}

std::string SessionManager::generate_session_id() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "sess_" + std::to_string(now) + "_" + std::to_string(session_id_counter_++);
}

std::shared_ptr<Session> SessionManager::create_session(const std::string& source_code,
                                                        const CompilationOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = generate_session_id();
    auto session = std::make_shared<Session>(id, source_code, options);
    sessions_[id] = session;
    return session;
}

std::shared_ptr<Session> SessionManager::get_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

bool SessionManager::remove_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        std::string err;
        it->second->stop(err);
        sessions_.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> SessionManager::list_session_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    for (const auto& [id, _] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

} // namespace ttd
