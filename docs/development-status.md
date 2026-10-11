# Development Status — Web-Based Linux Process Time-Travel Debugger

**Last Updated:** Tracing-owner refactor implemented; Linux verification pending (2026-10-10)  
**Target Architecture:** Linux x86_64 (the earlier revision was tested on Ubuntu 24.04/Linux 6.18; the current tracing-owner changes have not yet been built on Linux)

---

## 1. Milestone Tracking

| Milestone | Description | Status | Verification |
|---|---|---|---|
| **M1: Environment & Foundation** | Repo structure, C++20 CMake build, React+Vite frontend skeleton, Health/Capabilities API, Docs | Completed | CMake 4.2.3, GCC 15.2.0, Crow v1.2, GTest |
| **M2: Real Process Execution** | Subprocess compilation (`-g -O0`), pipe stdout/stderr, process isolation, lifecycle management | Completed | `ProcessManagerTest` (3 tests passed) |
| **M3: Real Linux Tracing** | `ptrace` setup, `PTRACE_GETREGS`, memory peek, single-stepping, signal handling, persistent owner thread | Implemented; re-verification pending | Current revision has not yet been built or run on Linux |
| **M4: Recording & Checkpoints** | Monotonic sequence events, instruction-step register snapshots, periodic stack checkpoints, bounds enforcement | Implemented; re-verification pending | Current revision has not yet been built or run on Linux |
| **M5: Historical Navigation** | Historical state seek, nearest checkpoint lookup, honest state reconstruction & limitations | Completed | `ReplayEngineTest` (3 tests passed) |
| **M6: API & WebSocket Layer** | Crow REST API, WebSocket event stream `/ws/debug/{sessionId}`, session state machine | Completed | `ttd_backend` binary built with full routes & WS |
| **M7: Frontend IDE** | Monaco/syntax editor, execution toolbar, timeline, registers, memory hex viewer, output log | In Progress | React + Vite + TypeScript implementation |
| **M8: Verification & Delivery** | Full end-to-end integration, documentation, test suite, demo guide | Pending | Ready for end-to-end verification |

---

## 2. Implemented Features

- [x] Host environment audit & toolchain configuration (GCC 15.2, CMake 4.2.3, Node.js v22.22.1, Ninja).
- [x] Safe subprocess compilation without shell interpolation (`fork()` + `execv()` with `-g -O0 -no-pie`).
- [x] Native Linux `ptrace` tracing engine source (`PTRACE_TRACEME`, `PTRACE_SINGLESTEP`, `PTRACE_GETREGS`, `PTRACE_PEEKDATA`, `process_vm_readv`) with a persistent owner thread; runtime verification of this revision is pending.
- [x] Memory bounds safety (strict max 1024 bytes per read) and NULL/unmapped page safety.
- [x] Monotonically increasing sequence numbers for execution telemetry; Resume source records one register-bearing event per completed instruction.
- [x] Immutable checkpoint manager with register snapshots and periodic RSP stack samples; arbitrary memory is not snapshotted at every instruction.
- [x] Replay & historical navigation engine (`seek()`) with register change diffing.
- [x] Thread-safe session lifecycle manager (`CREATED -> COMPILING -> STARTING -> RUNNING <-> PAUSED -> EXITED / STOPPED`).
- [x] Crow REST API (`/api/health`, `/api/capabilities`, `/api/debug/start`, `/api/debug/{id}/step`, `/api/debug/{id}/pause`, `/api/debug/{id}/resume`, `/api/debug/{id}/stop`, `/api/debug/{id}/timeline`, `/api/debug/{id}/seek`, `/api/debug/{id}/memory`, `/api/debug/{id}/registers`, `/api/debug/{id}/output`).
- [x] Crow WebSocket streaming endpoint (`/ws/debug/{sessionId}`).
- [x] Comprehensive GoogleTest test suite with 16 automated tests.

---

## 3. Last Recorded Test Results (Before the Tracing-Owner Refactor)

These results describe an earlier build and do **not** verify the current source after the persistent owner-thread and per-instruction Resume changes. The current Windows environment denied WSL startup with `Wsl/Service/CreateInstance/E_ACCESSDENIED`, so the backend has not been rebuilt or tested since those changes.

```text
[==========] Running 16 tests from 6 test suites.
[----------] Global test environment set-up.
[----------] 4 tests from TypesTest
[ RUN      ] TypesTest.RegistersSerializationAndDeserialization (4 ms) [OK]
[ RUN      ] TypesTest.MemoryDumpFormatting (1 ms) [OK]
[ RUN      ] TypesTest.TraceEventSerialization (0 ms) [OK]
[ RUN      ] TypesTest.ArchitectureCheck (0 ms) [OK]
[----------] 3 tests from ProcessManagerTest
[ RUN      ] ProcessManagerTest.CompileValidCSource (417 ms) [OK]
[ RUN      ] ProcessManagerTest.RejectInvalidSyntaxWithStderrDiagnostics (12 ms) [OK]
[ RUN      ] ProcessManagerTest.RejectEmptySource (0 ms) [OK]
[----------] 3 tests from EventRecorderTest
[ RUN      ] EventRecorderTest.MonotonicSequenceOrdering (0 ms) [OK]
[ RUN      ] EventRecorderTest.QueryAndFilterEvents (0 ms) [OK]
[ RUN      ] EventRecorderTest.MaxCapacityEnforcement (0 ms) [OK]
[----------] 2 tests from CheckpointManagerTest
[ RUN      ] CheckpointManagerTest.CreateAndFindNearestCheckpoint (0 ms) [OK]
[ RUN      ] CheckpointManagerTest.CoverageDescriptionContent (0 ms) [OK]
[----------] 3 tests from ReplayEngineTest
[ RUN      ] ReplayEngineTest.SeekExactSnapshot (0 ms) [OK]
[ RUN      ] ReplayEngineTest.SeekInterpolatedCheckpoint (0 ms) [OK]
[ RUN      ] ReplayEngineTest.SeekOutOfRangeReportsError (0 ms) [OK]
[----------] 1 test from IntegrationTest
[ RUN      ] IntegrationTest.FullEndToEndSessionExecution (156 ms) [OK]
[==========] 16 tests from 6 test suites ran. (596 ms total)
[  PASSED  ] 16 tests.
```

---

## 4. Known Limitations & Technical Decisions

- **Architecture:** Linux x86_64 native `user_regs_struct` via `PTRACE_GETREGS`. Non-x86_64 environments are detected and rejected with an explicit error.
- **Process Isolation:** The tracer launches user code as a restricted child process with redirected pipes and CPU/time limits.
- **Time-Travel Semantics:** Captured state is distinguished from live state. When an address or register is not recorded in a historical checkpoint, it is surfaced as `unavailable` rather than fabricated.

---

## 5. Current Verification Blocker and Next Steps

1. Start WSL Ubuntu, rebuild the backend, and run the updated ptrace integration tests.
2. Reproduce Resume and verify caller/owner TIDs, Pause, repeated Resume, Stop, exit handling, and reaping against a real Linux target.
3. Verify the historical UI against the resulting instruction-level event history.
