#include "tracing/TracingEngine.hpp"

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <iomanip>
#include <sstream>
#include <iostream>
#if defined(__aarch64__)
#include <elf.h>
#endif

namespace ttd {

static pid_t current_tid() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

TracingEngine::TracingEngine(pid_t pid) : pid_(pid), tracer_tid_(current_tid()) {}

TracingEngine::~TracingEngine() = default;

bool TracingEngine::check_owner_thread(std::string& out_error) const {
    const pid_t caller_tid = current_tid();
    if (caller_tid == tracer_tid_) {
        return true;
    }
    out_error = "Tracing operation called from Linux TID " + std::to_string(caller_tid) +
        "; tracee is owned by TID " + std::to_string(tracer_tid_);
    return false;
}

std::string TracingEngine::get_signal_name(int sig) const {
    switch (sig) {
        case SIGTRAP: return "SIGTRAP";
        case SIGSTOP: return "SIGSTOP";
        case SIGSEGV: return "SIGSEGV";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGINT:  return "SIGINT";
        case SIGKILL: return "SIGKILL";
        case SIGPIPE: return "SIGPIPE";
        case SIGALRM: return "SIGALRM";
        case SIGTERM: return "SIGTERM";
        default:      return "SIGNAL_" + std::to_string(sig);
    }
}

static void log_trace_op(const std::string& op, pid_t pid, pid_t owner_tid, const std::string& details) {
    if (std::getenv("TTD_TRACE_DEBUG") == nullptr) {
        return;
    }
    std::cerr << "[ttd-trace][" << op << "] pid=" << pid
              << " callerTid=" << current_tid()
              << " tracerOwnerTid=" << owner_tid
              << " errno=" << errno
              << " details=" << details << std::endl;
}

bool TracingEngine::initialize_tracing(std::string& out_error) {
    clear_last_error();
    if (!check_owner_thread(out_error)) {
        last_error_ = out_error;
        return false;
    }
    if (pid_ <= 0) {
        out_error = "Invalid target PID";
        last_error_ = out_error;
        return false;
    }
    if (!is_process_alive()) {
        out_error = "Target process is no longer running";
        last_error_ = out_error;
        log_trace_op("initialize_tracing", pid_, tracer_tid_, "process no longer alive");
        return false;
    }

    // PTRACE_SETOPTIONS configures modern tracing behavior:
    // - PTRACE_O_EXITKILL: Automatically sends SIGKILL to the tracee if the tracer exits.
    // - PTRACE_O_TRACESYSGOOD: Sets bit 7 in the stop signal (SIGTRAP | 0x80) when stopping on system calls.
    // TRACEEXIT is intentionally omitted: this engine consumes final wait
    // statuses and does not need an intermediate PTRACE_EVENT_EXIT stop.
    int options = PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD;
    if (ptrace(PTRACE_SETOPTIONS, pid_, 0, options) < 0) {
        if (errno == ESRCH) {
            out_error = "Target process disappeared before ptrace options could be set";
            last_error_ = out_error;
            log_trace_op("PTRACE_SETOPTIONS", pid_, tracer_tid_, "ESRCH before options set");
            return false;
        }
        out_error = "ptrace(PTRACE_SETOPTIONS) failed: " + std::string(strerror(errno));
        last_error_ = out_error;
        log_trace_op("PTRACE_SETOPTIONS", pid_, tracer_tid_, out_error);
        return false;
    }

    is_initialized_ = true;
    return true;
}

bool TracingEngine::is_process_alive() const {
    if (pid_ <= 0 || is_terminated_) {
        return false;
    }

    if (kill(pid_, 0) < 0) {
        if (errno == ESRCH || errno == EPERM) {
            return errno != ESRCH;
        }
        return false;
    }

    return true;
}

bool TracingEngine::get_registers(Registers& out_regs, std::string& out_error) {
    clear_last_error();
    if (!check_owner_thread(out_error)) {
        last_error_ = out_error;
        return false;
    }
    if (is_terminated_) {
        out_error = "Process has already terminated; registers unavailable";
        last_error_ = out_error;
        return false;
    }

#if defined(__x86_64__) || defined(_M_X64)
    struct user_regs_struct regs;
    // PTRACE_GETREGS copies the target process general-purpose CPU registers into our buffer.
    if (ptrace(PTRACE_GETREGS, pid_, nullptr, &regs) < 0) {
        out_error = "ptrace(PTRACE_GETREGS) failed: " + std::string(strerror(errno));
        last_error_ = out_error;
        return false;
    }

    out_regs.rip    = regs.rip;
    out_regs.rsp    = regs.rsp;
    out_regs.rbp    = regs.rbp;
    out_regs.rax    = regs.rax;
    out_regs.rbx    = regs.rbx;
    out_regs.rcx    = regs.rcx;
    out_regs.rdx    = regs.rdx;
    out_regs.rsi    = regs.rsi;
    out_regs.rdi    = regs.rdi;
    out_regs.r8     = regs.r8;
    out_regs.r9     = regs.r9;
    out_regs.r10    = regs.r10;
    out_regs.r11    = regs.r11;
    out_regs.r12    = regs.r12;
    out_regs.r13    = regs.r13;
    out_regs.r14    = regs.r14;
    out_regs.r15    = regs.r15;
    out_regs.eflags = regs.eflags;
    out_regs.cs     = regs.cs;
    out_regs.ss     = regs.ss;
    out_regs.ds     = regs.ds;
    out_regs.es     = regs.es;
    out_regs.fs     = regs.fs;
    out_regs.gs     = regs.gs;
    return true;
#elif defined(__aarch64__)
    // ARM64 has no PTRACE_GETREGS; read NT_PRSTATUS and map onto the x86-named fields.
    struct user_regs_struct regs;
    struct iovec iov{&regs, sizeof(regs)};
    if (ptrace(PTRACE_GETREGSET, pid_, reinterpret_cast<void*>(NT_PRSTATUS), &iov) < 0) {
        out_error = "ptrace(PTRACE_GETREGSET) failed: " + std::string(strerror(errno));
        last_error_ = out_error;
        return false;
    }
    out_regs.rip    = regs.pc;
    out_regs.rsp    = regs.sp;
    out_regs.rbp    = regs.regs[29];
    out_regs.rax    = regs.regs[0];
    out_regs.rbx    = regs.regs[1];
    out_regs.rcx    = regs.regs[2];
    out_regs.rdx    = regs.regs[3];
    out_regs.rsi    = regs.regs[4];
    out_regs.rdi    = regs.regs[5];
    out_regs.r8     = regs.regs[8];
    out_regs.r9     = regs.regs[9];
    out_regs.r10    = regs.regs[10];
    out_regs.r11    = regs.regs[11];
    out_regs.r12    = regs.regs[12];
    out_regs.r13    = regs.regs[13];
    out_regs.r14    = regs.regs[14];
    out_regs.r15    = regs.regs[15];
    out_regs.eflags = regs.pstate;
    return true;
#else
    out_error = "Unsupported CPU architecture for register inspection";
    return false;
#endif
}

StopInfo TracingEngine::parse_wait_status(int status) {
    StopInfo info;
    info.status_raw = status;

    if (WIFEXITED(status)) {
        info.reason = StopReason::PROCESS_EXITED;
        info.exit_code = WEXITSTATUS(status);
        info.is_terminated = true;
        is_terminated_ = true;
    } else if (WIFSIGNALED(status)) {
        info.reason = StopReason::PROCESS_SIGNALED;
        info.signal_number = WTERMSIG(status);
        info.signal_name = get_signal_name(info.signal_number);
        info.is_terminated = true;
        is_terminated_ = true;
    } else if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        info.signal_number = sig;
        info.signal_name = get_signal_name(sig);

        // Check if stopped due to system call with PTRACE_O_TRACESYSGOOD (bit 7 set)
        if (sig == (SIGTRAP | 0x80)) {
            if (!in_syscall_) {
                info.reason = StopReason::SYSCALL_ENTRY;
                in_syscall_ = true;
            } else {
                info.reason = StopReason::SYSCALL_EXIT;
                in_syscall_ = false;
            }
        } else if (sig == SIGTRAP) {
            info.reason = StopReason::SINGLE_STEP;
        } else if (sig == SIGSTOP) {
            info.reason = StopReason::PAUSED_BY_USER;
        } else {
            info.reason = StopReason::SIGNAL_RECEIVED;
        }
    } else {
        info.reason = StopReason::ERROR;
        info.error_message = "Unknown process wait status: " + std::to_string(status);
    }

    return info;
}

bool TracingEngine::single_step(StopInfo& out_stop, int signal_to_pass) {
    clear_last_error();
    if (!check_owner_thread(out_stop.error_message)) {
        out_stop.reason = StopReason::ERROR;
        last_error_ = out_stop.error_message;
        return false;
    }
    if (is_terminated_) {
        out_stop.reason = StopReason::PROCESS_EXITED;
        out_stop.is_terminated = true;
        out_stop.error_message = "Target process termination was already confirmed";
        last_error_ = out_stop.error_message;
        return false;
    }
    if (!is_process_alive()) {
        // kill(pid, 0) only establishes that the task is absent. Consume its
        // actual wait status on the tracer thread before declaring termination.
        return wait_for_stop(out_stop, 0);
    }

    // PTRACE_SINGLESTEP configures the CPU hardware trap flag (EFLAGS TF on x86)
    // to execute exactly one instruction, after which the CPU automatically raises SIGTRAP.
    if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(signal_to_pass))) < 0) {
        if (errno == ESRCH) {
            StopInfo observed;
            if (wait_for_stop(observed, 0)) {
                out_stop = std::move(observed);
                return true;
            }
            out_stop.reason = StopReason::ERROR;
            out_stop.is_terminated = false;
            out_stop.error_message = "ptrace(PTRACE_SINGLESTEP) returned ESRCH; caller TID=" +
                std::to_string(current_tid()) + ", PTRACE_TRACEME owner TID=" +
                std::to_string(tracer_tid_);
            if (!observed.error_message.empty()) {
                out_stop.error_message += "; waitpid: " + observed.error_message;
            }
            last_error_ = out_stop.error_message;
            log_trace_op("PTRACE_SINGLESTEP", pid_, tracer_tid_, out_stop.error_message);
            return false;
        }
        out_stop.reason = StopReason::ERROR;
        out_stop.error_message = "ptrace(PTRACE_SINGLESTEP) failed: " + std::string(strerror(errno));
        last_error_ = out_stop.error_message;
        log_trace_op("PTRACE_SINGLESTEP", pid_, tracer_tid_, out_stop.error_message);
        return false;
    }

    return wait_for_stop(out_stop, 0);
}

bool TracingEngine::continue_execution(int signal_to_pass) {
    clear_last_error();
    if (!check_owner_thread(last_error_)) {
        return false;
    }
    if (is_terminated_) {
        last_error_ = "Target process is no longer running; cannot continue execution";
        return false;
    }
    if (!is_process_alive()) {
        last_error_ = "Target process is not currently observable; collect its wait status before continuing";
        return false;
    }

    // PTRACE_CONT instructs the kernel to resume the stopped tracee until the next signal or exit.
    if (ptrace(PTRACE_CONT, pid_, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(signal_to_pass))) < 0) {
        if (errno == ESRCH) {
            last_error_ = "ptrace(PTRACE_CONT) failed with ESRCH (tracee unavailable to this caller; caller TID=" +
                std::to_string(current_tid()) + ", PTRACE_TRACEME owner TID=" +
                std::to_string(tracer_tid_) + ")";
            log_trace_op("PTRACE_CONT", pid_, tracer_tid_, last_error_);
            return false;
        }
        last_error_ = "ptrace(PTRACE_CONT) failed: " + std::string(strerror(errno));
        log_trace_op("PTRACE_CONT", pid_, tracer_tid_, last_error_);
        return false;
    }
    log_trace_op("PTRACE_CONT", pid_, tracer_tid_, "resume issued");
    return true;
}

bool TracingEngine::pause(StopInfo& out_stop) {
    clear_last_error();
    if (!check_owner_thread(out_stop.error_message)) {
        out_stop.reason = StopReason::ERROR;
        last_error_ = out_stop.error_message;
        return false;
    }
    if (is_terminated_ || !is_process_alive()) {
        out_stop.reason = StopReason::PROCESS_EXITED;
        out_stop.is_terminated = true;
        is_terminated_ = true;
        out_stop.error_message = "Target process is already gone";
        last_error_ = out_stop.error_message;
        return false;
    }

    // Send SIGSTOP to suspend tracee execution
    if (kill(pid_, SIGSTOP) < 0) {
        if (errno == ESRCH) {
            out_stop.reason = StopReason::PROCESS_EXITED;
            out_stop.is_terminated = true;
            is_terminated_ = true;
            out_stop.error_message = "Target process disappeared before pause could take effect";
            last_error_ = out_stop.error_message;
            return false;
        }
        out_stop.reason = StopReason::ERROR;
        out_stop.error_message = "kill(SIGSTOP) failed: " + std::string(strerror(errno));
        last_error_ = out_stop.error_message;
        return false;
    }

    return wait_for_stop(out_stop, 0);
}

bool TracingEngine::wait_for_stop(StopInfo& out_stop, int flags) {
    clear_last_error();
    if (!check_owner_thread(out_stop.error_message)) {
        out_stop.reason = StopReason::ERROR;
        last_error_ = out_stop.error_message;
        return false;
    }
    // Reject calls for an invalid PID or a tracee already confirmed terminated.
    if (pid_ <= 0 || is_terminated_) {
        out_stop.reason = StopReason::PROCESS_EXITED;
        out_stop.is_terminated = true;
        out_stop.error_message = "Target process is already terminated";
        last_error_ = out_stop.error_message;
        return true;
    }

    // Wait for the tracee's actual status instead of inferring its state from kill(pid, 0).
    int status = 0;
    pid_t res;

    // Retry waitpid if an unrelated signal interrupts the wait.
    do {
        res = waitpid(pid_, &status, flags);
    } while (res < 0 && errno == EINTR);

    // Log the returned PID and status while tracing diagnostics are enabled.
    log_trace_op(
        "waitpid",
        pid_,
        tracer_tid_,
        "res=" + std::to_string(res) +
            " status=" + std::to_string(status) +
            (res < 0 ? " errno=" + std::to_string(errno) +
                " (" + std::string(strerror(errno)) + ")" : "")
    );

    // A negative result means waitpid could not provide a status.
    if (res < 0) {
        // ECHILD means this process is not waitable by this caller.
        // Do not claim a normal exit code because no exit status was collected.
        if (errno == ECHILD) {
            out_stop.reason = StopReason::ERROR;
            out_stop.is_terminated = false;
            out_stop.error_message =
                "waitpid returned ECHILD: tracee status is unavailable; "
                "check for another waitpid caller";
            last_error_ = out_stop.error_message;
            return false;
        }

        // Report other wait errors without silently treating them as normal exits.
        out_stop.reason = StopReason::ERROR;
        out_stop.error_message =
            "waitpid failed: " + std::string(strerror(errno));
        last_error_ = out_stop.error_message;
        return false;
    }

    // With WNOHANG, zero means no status is ready yet.
    if (res == 0) {
        return false;
    }

    // Only a status actually returned by waitpid is parsed into a stop or exit event.
    out_stop = parse_wait_status(status);
    return true;
}

MemoryDump TracingEngine::read_memory(uint64_t address, size_t length) {
    MemoryDump dump;
    dump.address = address;
    dump.is_historical = false;
    if (!check_owner_thread(dump.error_message)) {
        dump.is_accessible = false;
        return dump;
    }

    // Strict bounds enforcement: max 1024 bytes per read request
    if (length > 1024) {
        length = 1024;
    }
    dump.length = length;

    if (address == 0) {
        dump.is_accessible = false;
        dump.error_message = "Null memory address cannot be inspected";
        return dump;
    }

    if (is_terminated_) {
        dump.is_accessible = false;
        dump.error_message = "Process has terminated; memory is unmapped";
        return dump;
    }

    dump.bytes.resize(length, 0);

    // Prefer process_vm_readv for atomic, fast memory reads without context switch overhead
    struct iovec local_iov;
    local_iov.iov_base = dump.bytes.data();
    local_iov.iov_len = length;

    struct iovec remote_iov;
    remote_iov.iov_base = reinterpret_cast<void*>(address);
    remote_iov.iov_len = length;

    ssize_t nread = process_vm_readv(pid_, &local_iov, 1, &remote_iov, 1, 0);
    if (nread > 0) {
        dump.bytes.resize(nread);
        dump.length = nread;
        dump.is_accessible = true;
    } else {
        // Fallback: PTRACE_PEEKDATA word-by-word
        size_t words = (length + sizeof(long) - 1) / sizeof(long);
        bool any_success = false;
        for (size_t i = 0; i < words; ++i) {
            uint64_t target_addr = address + (i * sizeof(long));
            errno = 0;
            long val = ptrace(PTRACE_PEEKDATA, pid_, reinterpret_cast<void*>(target_addr), nullptr);
            if (errno != 0) {
                if (!any_success) {
                    dump.is_accessible = false;
                    dump.error_message = "Inaccessible memory region (EFAULT/unmapped address): " + std::string(strerror(errno));
                    dump.bytes.clear();
                    return dump;
                }
                break;
            }
            any_success = true;
            size_t copy_len = std::min(sizeof(long), length - (i * sizeof(long)));
            std::memcpy(dump.bytes.data() + (i * sizeof(long)), &val, copy_len);
        }
        dump.is_accessible = any_success;
    }

    // Format hexadecimal and ASCII representations
    std::ostringstream hex_ss;
    std::string ascii_str;
    for (size_t i = 0; i < dump.bytes.size(); ++i) {
        uint8_t b = dump.bytes[i];
        hex_ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        if (i + 1 < dump.bytes.size()) hex_ss << " ";

        if (b >= 32 && b <= 126) {
            ascii_str.push_back(static_cast<char>(b));
        } else {
            ascii_str.push_back('.');
        }
    }
    dump.hex_string = hex_ss.str();
    dump.ascii_string = ascii_str;

    return dump;
}

} // namespace ttd
