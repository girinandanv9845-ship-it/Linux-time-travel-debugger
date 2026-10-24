#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace ttd {

// ============================================================================
// Architecture Detection & Verification
// ============================================================================
#if defined(__x86_64__) || defined(_M_X64)
constexpr const char* SUPPORTED_ARCHITECTURE = "x86_64";
constexpr bool IS_SUPPORTED_ARCH = true;
#elif defined(__aarch64__)
constexpr const char* SUPPORTED_ARCHITECTURE = "aarch64";
constexpr bool IS_SUPPORTED_ARCH = true;
#else
constexpr const char* SUPPORTED_ARCHITECTURE = "unsupported";
constexpr bool IS_SUPPORTED_ARCH = false;
#endif

// ============================================================================
// Session Lifecycle States
// ============================================================================
enum class SessionState {
    CREATED,
    COMPILING,
    STARTING,
    RUNNING,
    PAUSED,
    EXITED,
    STOPPED,
    FAILED
};

inline std::string to_string(SessionState state) {
    switch (state) {
        case SessionState::CREATED:    return "created";
        case SessionState::COMPILING:  return "compiling";
        case SessionState::STARTING:   return "starting";
        case SessionState::RUNNING:    return "running";
        case SessionState::PAUSED:     return "paused";
        case SessionState::EXITED:     return "exited";
        case SessionState::STOPPED:    return "stopped";
        case SessionState::FAILED:     return "failed";
    }
    return "unknown";
}

// ============================================================================
// Register State (Architecture: x86_64)
// ============================================================================
struct Registers {
    uint64_t rip{0};
    uint64_t rsp{0};
    uint64_t rbp{0};
    uint64_t rax{0};
    uint64_t rbx{0};
    uint64_t rcx{0};
    uint64_t rdx{0};
    uint64_t rsi{0};
    uint64_t rdi{0};
    uint64_t r8{0};
    uint64_t r9{0};
    uint64_t r10{0};
    uint64_t r11{0};
    uint64_t r12{0};
    uint64_t r13{0};
    uint64_t r14{0};
    uint64_t r15{0};
    uint64_t eflags{0};
    uint64_t cs{0};
    uint64_t ss{0};
    uint64_t ds{0};
    uint64_t es{0};
    uint64_t fs{0};
    uint64_t gs{0};

    // Serialize registers to JSON dictionary
    nlohmann::json to_json() const;
    static Registers from_json(const nlohmann::json& j);
};

// ============================================================================
// Memory Region & Inspection Dumps
// ============================================================================
struct MemoryDump {
    uint64_t address{0};
    size_t length{0};
    std::vector<uint8_t> bytes;
    std::string hex_string;
    std::string ascii_string;
    bool is_accessible{false};
    std::string error_message;
    bool is_historical{false};

    nlohmann::json to_json() const;
};

// ============================================================================
// Execution Event Definitions
// ============================================================================
enum class EventType {
    PROCESS_STARTED,
    INITIAL_TRACING_STOP,
    EXECUTION_RESUMED,
    SINGLE_STEP_COMPLETED,
    REGISTER_SNAPSHOT_CAPTURED,
    SIGNAL_OBSERVED,
    SYSCALL_ENTRY,
    SYSCALL_EXIT,
    CHECKPOINT_CAPTURED,
    PROCESS_EXITED,
    PROCESS_TERMINATED_BY_SIGNAL,
    TRACING_ERROR
};

inline std::string to_string(EventType type) {
    switch (type) {
        case EventType::PROCESS_STARTED:              return "process_started";
        case EventType::INITIAL_TRACING_STOP:         return "initial_tracing_stop";
        case EventType::EXECUTION_RESUMED:            return "execution_resumed";
        case EventType::SINGLE_STEP_COMPLETED:        return "single_step_completed";
        case EventType::REGISTER_SNAPSHOT_CAPTURED:   return "register_snapshot_captured";
        case EventType::SIGNAL_OBSERVED:              return "signal_observed";
        case EventType::SYSCALL_ENTRY:                return "syscall_entry";
        case EventType::SYSCALL_EXIT:                 return "syscall_exit";
        case EventType::CHECKPOINT_CAPTURED:          return "checkpoint_captured";
        case EventType::PROCESS_EXITED:               return "process_exited";
        case EventType::PROCESS_TERMINATED_BY_SIGNAL: return "process_terminated_by_signal";
        case EventType::TRACING_ERROR:                return "tracing_error";
    }
    return "unknown";
}

struct TraceEvent {
    uint64_t sequence{0};
    EventType type{EventType::PROCESS_STARTED};
    int64_t timestamp_ms{0};
    int32_t pid{0};
    int32_t thread_id{0};
    uint64_t rip{0};
    std::optional<uint32_t> source_line;
    std::string source_file;
    std::string description;
    std::optional<Registers> registers;
    std::optional<int32_t> signal_number;
    std::string signal_name;
    std::optional<int64_t> syscall_number;
    std::string syscall_name;
    std::string checkpoint_id;
    nlohmann::json metadata;

    nlohmann::json to_json() const;
    static TraceEvent from_json(const nlohmann::json& j);
};

// ============================================================================
// Checkpoint Representation
// ============================================================================
struct Checkpoint {
    std::string id;
    uint64_t sequence{0};
    int64_t timestamp_ms{0};
    int32_t pid{0};
    Registers registers;
    MemoryDump stack_sample;
    std::string coverage_description;
    nlohmann::json metadata;

    nlohmann::json to_json() const;
};

// ============================================================================
// Program Output Structure
// ============================================================================
struct ProgramOutput {
    std::string stdout_data;
    std::string stderr_data;
    int64_t timestamp_ms{0};
    uint64_t sequence{0};

    nlohmann::json to_json() const;
};

} // namespace ttd
