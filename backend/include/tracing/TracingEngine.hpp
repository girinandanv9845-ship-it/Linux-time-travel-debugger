#pragma once

#include <sys/types.h>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <atomic>
#include "common/Types.hpp"

namespace ttd {

enum class StopReason {
    INITIAL_STOP,
    SINGLE_STEP,
    SIGNAL_RECEIVED,
    SYSCALL_ENTRY,
    SYSCALL_EXIT,
    PAUSED_BY_USER,
    PROCESS_EXITED,
    PROCESS_SIGNALED,
    ERROR
};

struct StopInfo {
    StopReason reason{StopReason::ERROR};
    int status_raw{0};
    int signal_number{0};
    std::string signal_name;
    int exit_code{0};
    bool is_terminated{false};
    std::string error_message;
};

class TracingEngine {
public:
    explicit TracingEngine(pid_t pid);
    ~TracingEngine();

    // Configures essential ptrace options on the stopped child
    bool initialize_tracing(std::string& out_error);

    // Reads current architecture-specific CPU registers
    bool get_registers(Registers& out_regs, std::string& out_error);

    // Single-steps the CPU by one machine instruction
    bool single_step(StopInfo& out_stop, int signal_to_pass = 0);

    // Resumes continuous execution of the tracee
    bool continue_execution(int signal_to_pass = 0);

    // Pauses a running tracee
    bool pause(StopInfo& out_stop);

    // Waits for the next trace stop or termination event
    bool wait_for_stop(StopInfo& out_stop, int flags = 0);

    // Reads a bounded slice of tracee virtual memory safely
    MemoryDump read_memory(uint64_t address, size_t length);

    // PID accessor
    pid_t get_pid() const { return pid_; }

    // Detect whether a traced process still exists and is eligible for ptrace work.
    bool is_process_alive() const;

    // Check if target is known to be terminated
    bool is_terminated() const { return is_terminated_.load(std::memory_order_acquire); }
    const std::string& get_last_error() const { return last_error_; }
    void clear_last_error() { last_error_.clear(); }

private:
    pid_t pid_{-1};
    // PTRACE_TRACEME associates the tracee with the exact Linux thread that
    // forked it. Keep that TID in diagnostics to detect ownership handoffs.
    pid_t tracer_tid_{-1};
    bool is_initialized_{false};
    std::atomic<bool> is_terminated_{false};
    bool in_syscall_{false};
    std::string last_error_;

    StopInfo parse_wait_status(int status);
    std::string get_signal_name(int sig) const;
    bool check_owner_thread(std::string& out_error) const;
};

} // namespace ttd
