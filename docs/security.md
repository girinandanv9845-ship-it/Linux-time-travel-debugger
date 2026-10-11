# Security & Process Isolation — Web-Based Linux Process Time-Travel Debugger

## 1. Threat Model & Scope

This application compiles and executes native C/C++ code under `ptrace` supervision. In an academic or demonstration setting, running native binaries carries inherent risks. The architecture implements defense-in-depth measures while documenting necessary security boundaries.

---

## 2. Implemented Safeguards

### A. Subprocess Invocation Without Shell Interpolation
- **Vulnerability Avoided:** Command injection via malicious filenames, source payloads, or compiler flags.
- **Implementation:** The compilation and execution logic exclusively uses POSIX `fork()` and `execv()` with direct, sanitized argument arrays (`std::vector<std::string>`). It **never** invokes `system()`, `popen()`, or `sh -c`.

### B. Isolated Session Workspaces
- Each debugging session operates in its own unique temporary filesystem path:
  `/tmp/ttd_sessions/sess_<timestamp>_<id>/`
- Build artifacts (`target.c`, `target.bin`) are strictly scoped to this path and deleted upon session termination or server shutdown.

### C. Resource & Payload Bounding
- **Source Code Size:** Restricted to a maximum of 1 MB.
- **Memory Read Limits:** Strict bounding of max 1024 bytes per read query via `process_vm_readv` / `PTRACE_PEEKDATA`.
- **Event History Limit:** Bounded ring buffer of 10,000 events to prevent host RAM exhaustion.
- **Checkpoint Count:** Restricted to a maximum of 200 checkpoints per session.

### D. Process Orphan Prevention (`PTRACE_O_EXITKILL`)
- The tracer sets the `PTRACE_O_EXITKILL` kernel flag on every child. If the backend server crashes or is terminated, the Linux kernel automatically delivers `SIGKILL` to all traced children.
- On session stop, `ProcessManager::cleanup_child()` explicitly issues `kill(pid, SIGKILL)` and reaps with `waitpid(pid, &status, WNOHANG)`.

### E. Linux Yama `ptrace_scope` Policy
- On modern Linux distributions (Ubuntu), Yama ptrace LSM controls process tracing (`/proc/sys/kernel/yama/ptrace_scope`).
- **Policy 1 (Restricted):** Allows a parent process to trace its child created via `fork()` and `PTRACE_TRACEME`. This is the exact pattern implemented in this project.
- **Security Guidance:** We **do not** require or recommend setting `ptrace_scope` to `0` globally.

---

## 3. Safe Demonstration Practices & Remaining Risks

> [!WARNING]
> Because user code runs as a native ELF binary under the account executing the backend, untrusted code containing malicious syscalls (e.g. `unlink("/")` or unauthorized network egress) could execute if run outside an unprivileged container.

**Demonstration Rules:**
1. Only execute trusted demonstration programs (such as those in `examples/`).
2. Never run the debugger backend as the `root` superuser.
3. For multi-tenant or untrusted deployments, encapsulate the target process in Linux namespaces (cgroups, pivot_root, seccomp-bpf) or run within an isolated container.

