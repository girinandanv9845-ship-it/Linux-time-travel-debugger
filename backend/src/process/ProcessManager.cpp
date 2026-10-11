#include "process/ProcessManager.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cerrno>

namespace fs = std::filesystem;

namespace ttd {

ProcessManager::ProcessManager() = default;

ProcessManager::~ProcessManager() = default;

std::string ProcessManager::get_session_dir(const std::string& session_id) const {
    return "/tmp/ttd_sessions/" + session_id;
}

void ProcessManager::make_non_blocking(int fd) {
    if (fd < 0) return;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

CompilationResult ProcessManager::compile(const std::string& session_id,
                                          const std::string& source_code,
                                          const CompilationOptions& options) {
    CompilationResult result;
    if (source_code.empty()) {
        result.success = false;
        result.error_message = "Source code is empty";
        return result;
    }

    // Enforce reasonable source code size limit (1 MB max)
    if (source_code.size() > 1024 * 1024) {
        result.success = false;
        result.error_message = "Source code exceeds maximum allowed size (1 MB)";
        return result;
    }

    std::string work_dir = get_session_dir(session_id);
    result.work_dir = work_dir;

    std::error_code ec;
    fs::create_directories(work_dir, ec);
    if (ec) {
        result.success = false;
        result.error_message = "Failed to create session directory: " + ec.message();
        return result;
    }

    std::string ext = (options.language == "cpp" || options.language == "c++") ? ".cpp" : ".c";
    std::string src_file = work_dir + "/target" + ext;
    std::string bin_file = work_dir + "/target.bin";
    result.source_path = src_file;
    result.binary_path = bin_file;

    // Write source code to file
    {
        std::ofstream out(src_file, std::ios::trunc);
        if (!out.is_open()) {
            result.success = false;
            result.error_message = "Failed to write source file to disk";
            return result;
        }
        out << source_code;
    }

    // Determine compiler executable
    std::string compiler = (options.language == "cpp" || options.language == "c++") ? "/usr/bin/g++" : "/usr/bin/gcc";

    // Setup pipes for capturing compiler output
    int out_pipe[2]{-1, -1};
    int err_pipe[2]{-1, -1};
    if (pipe2(out_pipe, O_CLOEXEC) != 0) {
        result.success = false;
        result.error_message = "Failed to create compiler stdout pipe: " + std::string(strerror(errno));
        return result;
    }
    if (pipe2(err_pipe, O_CLOEXEC) != 0) {
        int pipe_error = errno;
        close(out_pipe[0]); close(out_pipe[1]);
        result.success = false;
        result.error_message = "Failed to create compiler stderr pipe: " + std::string(strerror(pipe_error));
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        result.success = false;
        result.error_message = "Failed to fork compiler process: " + std::string(strerror(errno));
        return result;
    }

    if (pid == 0) {
        // Child process: redirect standard output and error to pipes
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        // Prepare compiler argument list safely without shell interpolation
        std::vector<std::string> args = {
            compiler,
            "-g",
            "-O0",
            "-Wall",
            "-Wextra",
            "-no-pie", // Produce fixed-base binary for consistent instruction addresses
            "-o", bin_file,
            src_file
        };

        if (options.optimization == "O1") args[2] = "-O1";
        else if (options.optimization == "O2") args[2] = "-O2";

        std::vector<char*> c_args;
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execv(compiler.c_str(), c_args.data());
        // If execv fails:
        std::cerr << "Failed to exec compiler: " << strerror(errno) << "\n";
        _exit(127);
    }

    // Parent process: close write ends of pipes
    close(out_pipe[1]);
    close(err_pipe[1]);

    // Read compiler output
    auto read_all = [](int fd) -> std::string {
        std::string res;
        char buf[1024];
        ssize_t n = 0;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            res.append(buf, n);
        }
        close(fd);
        return res;
    };

    result.stdout_output = read_all(out_pipe[0]);
    result.stderr_output = read_all(err_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.success = (result.exit_code == 0 && fs::exists(bin_file));
    } else {
        result.exit_code = -1;
        result.success = false;
        result.error_message = "Compiler process terminated abnormally";
    }

    return result;
}

ChildProcess ProcessManager::spawn_traced_child(const std::string& binary_path) {
    ChildProcess child;
    child.binary_path = binary_path;

    if (!fs::exists(binary_path)) {
        child.error_message = "Binary path does not exist: " + binary_path;
        return child;
    }

    int stdout_pipe[2]{-1, -1};
    int stderr_pipe[2]{-1, -1};

    if (pipe2(stdout_pipe, O_CLOEXEC) != 0) {
        child.error_message = "Failed to allocate child stdout pipe: " + std::string(strerror(errno));
        return child;
    }
    if (pipe2(stderr_pipe, O_CLOEXEC) != 0) {
        int pipe_error = errno;
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        child.error_message = "Failed to allocate child stderr pipe: " + std::string(strerror(pipe_error));
        return child;
    }

    pid_t pid = fork();
    if (pid < 0) {
        int fork_error = errno;
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        child.error_message = "Failed to fork tracee child: " + std::string(strerror(fork_error));
        return child;
    }

    if (pid == 0) {
        // In Child Process:
        // 1. Establish tracing relationship with parent
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            perror("ptrace(PTRACE_TRACEME) failed");
            _exit(1);
        }

        // 2. Redirect standard output and error to our pipes
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        // 3. Close inherited pipe descriptors
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);

        // 4. Execute the target program
        char* const argv[] = {
            const_cast<char*>(binary_path.c_str()),
            nullptr
        };
        execv(binary_path.c_str(), argv);

        // If execv fails
        perror("execv of target failed");
        _exit(127);
    }

    // In Parent Process:
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    make_non_blocking(stdout_pipe[0]);
    make_non_blocking(stderr_pipe[0]);

    int pid_fd = static_cast<int>(syscall(SYS_pidfd_open, pid, 0));
    if (pid_fd < 0) {
        int pidfd_error = errno;
        kill(pid, SIGKILL);
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        child.error_message = "pidfd_open failed for traced child: " + std::string(strerror(pidfd_error));
        return child;
    }
    fcntl(pid_fd, F_SETFD, FD_CLOEXEC);

    child.pid = pid;
    child.pid_fd = pid_fd;
    child.stdout_fd = stdout_pipe[0];
    child.stderr_fd = stderr_pipe[0];
    child.is_running = true;

    return child;
}

void ProcessManager::drain_output(ChildProcess& child, std::string& out_stdout, std::string& out_stderr) {
    if (child.stdout_fd >= 0) {
        char buf[2048];
        ssize_t n = 0;
        while ((n = read(child.stdout_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            out_stdout.append(buf, n);
        }
    }

    if (child.stderr_fd >= 0) {
        char buf[2048];
        ssize_t n = 0;
        while ((n = read(child.stderr_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            out_stderr.append(buf, n);
        }
    }
}

bool ProcessManager::signal_child(ChildProcess& child, int signal_number, std::string& out_error) {
    if (child.pid <= 0 || !child.is_running) {
        errno = ESRCH;
        out_error = "Child process is not running";
        return false;
    }
    int result;
    if (child.pid_fd < 0) {
        errno = EBADF;
        out_error = "Child process has no pidfd; refusing an unsafe PID-based signal";
        return false;
    }
    do {
        result = static_cast<int>(syscall(SYS_pidfd_send_signal, child.pid_fd, signal_number, nullptr, 0));
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        out_error = "Failed to signal child " + std::to_string(child.pid) + ": " + strerror(errno);
        return false;
    }
    return true;
}

void ProcessManager::cleanup_child(ChildProcess& child) {
    if (child.pid > 0 && child.is_running) {
        // Kill and synchronously reap. Call this on the tracer-owner thread for
        // traced children so no other waiter can consume their final status.
        std::string signal_error;
        if (!signal_child(child, SIGKILL, signal_error) && errno != ESRCH) {
            std::cerr << signal_error << "\n";
        }
        int status = 0;
        pid_t waited;
        do {
            waited = waitpid(child.pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited < 0 && errno != ECHILD) {
            std::cerr << "Failed to reap child " << child.pid << ": " << strerror(errno) << "\n";
        }
        child.is_running = false;
    }

    if (child.stdout_fd >= 0) {
        close(child.stdout_fd);
        child.stdout_fd = -1;
    }
    if (child.stderr_fd >= 0) {
        close(child.stderr_fd);
        child.stderr_fd = -1;
    }
    if (child.pid_fd >= 0) {
        close(child.pid_fd);
        child.pid_fd = -1;
    }
    child.pid = -1;
}

void ProcessManager::cleanup_session_dir(const std::string& session_id) {
    std::string dir = get_session_dir(session_id);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

} // namespace ttd
