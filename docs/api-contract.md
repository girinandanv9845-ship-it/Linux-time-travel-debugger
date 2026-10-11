# API Contract — Web-Based Linux Process Time-Travel Debugger

## 1. Overview

- **Protocol:** HTTP/1.1 REST + WebSocket (RFC 6455)
- **Data Format:** JSON (`application/json; charset=utf-8`)
- **Default Port:** `8080` (Backend API), `3000` (Frontend Dev)
- **CORS:** Enabled for all development origins (`Access-Control-Allow-Origin: *`)

---

## 2. Standard Error Envelope

When an endpoint fails or validation fails, it returns an HTTP error code (`400`, `404`, `422`, `500`) with this JSON body:

```json
{
  "success": false,
  "error": "Human-readable explanation of error"
}
```

---

## 3. REST Endpoints

### 3.1 Health & Discovery

#### `GET /api/health`
Checks server health, uptime, and architecture support.

**Response (200 OK):**
```json
{
  "status": "healthy",
  "service": "linux-time-travel-debugger",
  "version": "1.0.0",
  "architecture": "x86_64",
  "isSupportedArch": true
}
```

#### `GET /api/capabilities`
Returns system tracing capabilities and operational constraints.

**Response (200 OK):**
```json
{
  "architecture": "x86_64",
  "isSupportedArch": true,
  "tracingEngine": "Linux ptrace (PTRACE_SINGLESTEP, PTRACE_GETREGS, PTRACE_PEEKDATA, process_vm_readv)",
  "supportedLanguages": ["c", "cpp"],
  "maxMemoryReadBytes": 1024,
  "maxEventsPerSession": 10000,
  "maxCheckpointsPerSession": 200,
  "eventTypes": [
    "process_started", "initial_tracing_stop", "execution_resumed",
    "single_step_completed", "register_snapshot_captured", "signal_observed",
    "syscall_entry", "syscall_exit", "checkpoint_captured",
    "process_exited", "process_terminated_by_signal", "tracing_error"
  ],
  "replayGuarantees": {
    "mode": "Captured State & Checkpoint Navigation",
    "description": "Allows historical navigation to any recorded sequence number. Register snapshots and bounded stack dumps are displayed honestly from recorded checkpoints without falsifying uncaptured state.",
    "limitations": "Full arbitrary deterministic process rewind is bounded by external I/O and unrecorded kernel states."
  }
}
```

---

### 3.2 Session Lifecycle

#### `POST /api/debug/start`
Compiles submitted source code and launches target process under ptrace.

**Request:**
```json
{
  "language": "c",
  "source": "#include <stdio.h>\nint main(void) { printf(\"hello\\n\"); return 0; }",
  "options": {
    "optimization": "O0",
    "debugSymbols": true
  }
}
```

**Response (201 Created):**
```json
{
  "success": true,
  "sessionId": "sess_1760089000_1",
  "state": "paused",
  "pid": 12345,
  "initialState": { ... }
}
```

**Failure (422 Unprocessable Entity - Compilation Failed):**
```json
{
  "success": false,
  "sessionId": "sess_1760089000_1",
  "error": "Compilation failed:\ntarget.c:3:5: error: ...",
  "compilation": {
    "success": false,
    "exitCode": 1,
    "stdout": "",
    "stderr": "target.c:3:5: error: expected ';' before 'return'\n"
  }
}
```

#### `POST /api/debug/{sessionId}/pause`
Suspends execution of the running tracee (`kill(SIGSTOP)`).

#### `POST /api/debug/{sessionId}/resume`
Resumes continuous execution of the target process.

#### `POST /api/debug/{sessionId}/step`
Advances the CPU by exactly one instruction (`PTRACE_SINGLESTEP`).

**Response (200 OK):**
```json
{
  "success": true,
  "state": "paused",
  "event": {
    "sequence": 5,
    "type": "single_step_completed",
    "timestamp": 1760089025123,
    "pid": 12345,
    "rip": "0x0000000000401138",
    "description": "Executed instruction at single-step #3",
    "registers": { ... }
  },
  "currentState": { ... }
}
```

#### `POST /api/debug/{sessionId}/stop`
Terminates tracee, cleans up temp directories, and finalizes session.

---

### 3.3 Historical Inspection

#### `GET /api/debug/{sessionId}/state`
Returns the current session lifecycle state, PID, event counts, and live registers.

#### `GET /api/debug/{sessionId}/timeline?from=1&to=100&limit=50&offset=0`
Returns recorded event summaries and checkpoint markers.

#### `POST /api/debug/{sessionId}/seek`
Seeks the historical playhead to a target sequence number.

**Request:**
```json
{
  "targetSequence": 12
}
```

**Response (200 OK):**
```json
{
  "targetSequence": 12,
  "actualSequence": 12,
  "stateAvailability": "exact_snapshot",
  "isLiveProcess": false,
  "success": true,
  "event": { ... },
  "registers": { ... },
  "registerDiff": [
    { "name": "RAX", "oldValue": "0x0000000000000000", "newValue": "0x000000000000002a", "changed": true },
    { "name": "RIP", "oldValue": "0x0000000000401124", "newValue": "0x000000000040112b", "changed": true }
  ],
  "stackMemory": { ... },
  "nearestCheckpointId": "ckpt_1",
  "nearestCheckpointSequence": 2,
  "replayLimitationNotice": "Historical Inspection: Viewing exact captured register snapshot recorded at sequence #12. Live process execution state is untouched."
}
```

#### `GET /api/debug/{sessionId}/memory?address=0x7fffffffe000&length=64`
Reads bounded virtual memory from the process (max 1024 bytes).

#### `GET /api/debug/{sessionId}/registers`
Returns live or most recent historical register values.

#### `GET /api/debug/{sessionId}/output`
Returns captured stdout and stderr.

---

## 4. WebSocket Protocol

**Endpoint:** `ws://<host>:8080/ws/debug/{sessionId}`

### Client -> Server Messages
- Subscribe: `{"action": "subscribe", "sessionId": "sess_1760089000_1"}`
- Ping: `{"action": "ping"}`

### Server -> Client Messages
- `trace_event`: New execution event emitted.
- `checkpoint_created`: New checkpoint snapshot taken.
- `session_state_changed`: Process state transition.
- `program_output`: Target stdout/stderr output drained.
- `compilation_failed`: Build diagnostic error.
- `subscribed`: Subscription acknowledgement.

