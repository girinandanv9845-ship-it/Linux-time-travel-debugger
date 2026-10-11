# Execution Flow — Web-Based Linux Process Time-Travel Debugger

## 1. Complete Session Lifecycle

```text
[User Code Input] 
       │
       ▼
1. Compilation (fork + execv gcc/g++)
       │
       ├── Failure ──> Return Compiler Diagnostics & State = FAILED
       │
       ▼ (Success)
2. Session Owner Thread forks child (PTRACE_TRACEME + execv)
       │
       ▼
3. Same Owner Thread Intercepts Initial Trap (SIGTRAP on execv)
       │
       ▼
4. Initialize ptrace (PTRACE_SETOPTIONS: EXITKILL)
       │
       ▼
5. Capture Initial Telemetry (PTRACE_GETREGS, Stack Slice, Checkpoint #1)
       │
       ▼
6. State = PAUSED (Ready for Stepping / Execution)
       │
  ┌────┴─────────────────────────────┐
  ▼                                  ▼
[Step Instruction]             [Resume Recorded Run]
  │                                  │
  ▼                                  ▼
Owner Queue: PTRACE_SINGLESTEP Owner Queue: repeated PTRACE_SINGLESTEP
  │                                  │
  ▼                                  ▼
Record Registers + Event       Record each instruction until stop / exit
  │                                  │
  └───────────────┬──────────────────┘
                  │
                  ▼
7. Process Exit (WIFEXITED / WIFSIGNALED)
       │
       ▼
8. Final Telemetry Recorded & State = EXITED
       │
       ▼
9. Time-Travel History Remains Fully Inspectable
```

The persistent owner thread creates and initially waits for the tracee, then remains alive for the whole active session. HTTP handlers and the Resume loop submit serialized operations to that thread; no other thread calls `ptrace` or waits for the tracee. Pause and Stop use the session pidfd to signal the exact child and let the owner collect the resulting wait status.

---

## 2. Historical Seek Sequence

When the user selects an earlier historical position or scrubs the execution timeline, the system processes the request as shown in the sequence diagram below:

```mermaid
sequenceDiagram
    autonumber
    actor User as User / Browser
    participant FE as Frontend UI
    participant API as REST API (/seek)
    participant SM as Session Manager
    participant RE as Replay Engine
    participant ER as Event Recorder
    participant CM as Checkpoint Manager

    User->>FE: Clicks Historical Sequence #X or drags Timeline
    FE->>API: POST /api/debug/{id}/seek { targetSequence: X }
    API->>SM: session->seek(X)
    SM->>RE: replay->seek(X)

    RE->>ER: get_by_sequence(X)
    ER-->>RE: Return TraceEvent (or not found)

    alt Target event has exact captured register snapshot
        RE->>ER: get_by_sequence(X - 1)
        ER-->>RE: Return Preceding TraceEvent
        RE->>RE: Compute Register Delta Diff (Old vs New)
        RE-->>SM: SeekResult [EXACT_SNAPSHOT + Regs + Diff]
    else Event has no direct register snapshot
        RE->>CM: find_at_or_before(X)
        CM-->>RE: Return Nearest Checkpoint (e.g. ckpt_1 @ seq Y)
        RE-->>SM: SeekResult [INTERPOLATED_CHECKPOINT + Checkpoint Regs + Stack]
    end

    SM-->>API: Return SeekResult JSON
    API-->>FE: HTTP 200 { stateAvailability, registers, registerDiff, stackMemory, notice }
    FE->>FE: Render Historical State with Yellow Delta Glow & Notice Banner
    FE-->>User: Displays Historical Inspection State @ Seq #X
```

