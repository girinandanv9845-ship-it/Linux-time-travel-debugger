# Replay & Time-Travel Limitations — Conceptual & Technical Truths

## 1. Executive Summary

Time travel in software systems can mean very different things depending on context. To maintain academic and technical integrity, this debugger explicitly distinguishes between:

1. **Event History Navigation:** Browsing the recorded chronicle of events, timestamps, and metadata observed during tracing.
2. **Captured State Inspection:** Inspecting CPU registers and bounded memory snapshots preserved at specific tracing stops.
3. **State Restoration:** Physically rewinding the live target process to execute previously retired instructions.

---

## 2. What Is Supported

- **Monotonic Historical Telemetry:** Every single step, signal, entry/exit point, and error has a stable monotonic sequence ID (`1..N`).
- **Exact Register Snapshots:** Every single-step event and pause point captures the full set of general-purpose x86_64 registers (`RIP`, `RSP`, `RBP`, `RAX` through `R15`, `EFLAGS`).
- **Register Differencing:** When scrubbing history, the debugger computes the exact delta between the selected sequence and its predecessor, highlighting altered registers.
- **Nearest Checkpoint Interpolation:** When inspecting an event that only recorded telemetry (e.g. process launch), the nearest prior checkpoint is located and its stack slice is presented with an honest metadata explanation.
- **Bounded Stack Memory Samples:** Checkpoints preserve a bounded stack window (`[RSP - 16, RSP + 64]`) to inspect local activation records.

---

## 3. What Is NOT Claimed (Technical Boundaries)

- **Arbitrary External I/O Rewind:** If a program calls `printf("Hello\n")` or writes to a file descriptor, those bytes are consumed by the operating system kernel and standard output. Re-seeking to sequence #1 in history displays the historical registers, but does not un-print bytes from the terminal.
- **Uncaptured Virtual Memory Pages:** Bounded checkpoints capture general-purpose registers and stack activation slices. They do not duplicate 4GB of anonymous heap pages or memory-mapped device files. Any uncaptured address is surfaced as `unavailable` rather than fabricated.
- **Non-deterministic System Calls:** Deterministic rewind of non-deterministic syscalls (`getrandom`, `gettimeofday`, network sockets) requires kernel-level record-and-replay (such as Mozilla `rr`). This application operates in user-space via Linux `ptrace`.
- **Live Reverse Execution:** While the frontend playhead moves freely backward and forward to inspect historical records, the live target process itself is not rewound in hardware.

---

## 4. Truthful User Interface Indicators

The debugger UI enforces transparency:
- A prominent amber badge appears whenever viewing historical state: `"Historical Inspection: Seq #X of Y"`.
- A telemetry banner details whether the displayed registers represent an `exact_snapshot`, an `interpolated_checkpoint`, or `event_metadata_only`.
- A `"Return to Live"` button enables instant synchronization with the live process state.

