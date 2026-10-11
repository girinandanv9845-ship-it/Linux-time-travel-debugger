#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>
#include "common/Types.hpp"

namespace ttd {

struct CompilationOptions {
    std::string language{"c"};          // "c" or "cpp"
    std::string optimization{"O0"};      // "O0", "O1", "O2"
    bool debug_symbols{true};            // -g
    std::vector<std::string> extra_flags;
};

struct CompilationResult {
    bool success{false};
    int exit_code{-1};
    std::string stdout_output;
    std::string stderr_output;
    std::string binary_path;
    std::string source_path;
    std::string work_dir;
    std::string error_message;
};

struct ChildProcess {
    pid_t pid{-1};
    int pid_fd{-1};
    int stdout_fd{-1};
    int stderr_fd{-1};
    std::string binary_path;
    std::string error_message;
    bool is_running{false};
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    // Compiles source code in an isolated session directory via fork/execv
    CompilationResult compile(const std::string& session_id,
                              const std::string& source_code,
                              const CompilationOptions& options);

    // Spawns the compiled binary for tracing with ptrace (child calls PTRACE_TRACEME)
    ChildProcess spawn_traced_child(const std::string& binary_path);

    // Reads available data non-blockingly from child pipes
    void drain_output(ChildProcess& child, std::string& out_stdout, std::string& out_stderr);

    // Signal a specific child instance without risking PID reuse.
    bool signal_child(ChildProcess& child, int signal_number, std::string& out_error);

    // Safely terminates and reaps child process if still active
    void cleanup_child(ChildProcess& child);

    // Cleans up session disk directory
    void cleanup_session_dir(const std::string& session_id);

private:
    std::string get_session_dir(const std::string& session_id) const;
    void make_non_blocking(int fd);
};

} // namespace ttd
