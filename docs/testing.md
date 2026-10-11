# Testing & Quality Verification — Web-Based Linux Process Time-Travel Debugger

## 1. Automated Test Execution

All tests can be executed via the unified test script or individual target runners.

### Master Test Command
```bash
./scripts/test.sh
```

---

## 2. Backend GoogleTest Suite

The backend uses GoogleTest (`libgtest-dev`) to execute 16 unit and integration test cases covering data models, compilation, tracing, recording, checkpoints, historical replay, and process isolation.

### Running Backend Tests
```bash
cd build
./backend/ttd_tests --gtest_color=yes
```

### Coverage Breakdown

| Test Suite | Test Name | Target Tested | Status |
|---|---|---|---|
| **TypesTest** | `RegistersSerializationAndDeserialization` | x86_64 register JSON serialization & roundtrip | PASS |
| **TypesTest** | `MemoryDumpFormatting` | Hex dump and ASCII translation | PASS |
| **TypesTest** | `TraceEventSerialization` | Event JSON structure | PASS |
| **TypesTest** | `ArchitectureCheck` | Compile-time x86_64 architecture verification | PASS |
| **ProcessManagerTest** | `CompileValidCSource` | Subprocess compilation with `-g -O0` | PASS |
| **ProcessManagerTest** | `RejectInvalidSyntaxWithStderrDiagnostics` | Compiler error capture and nonzero exit code | PASS |
| **ProcessManagerTest** | `RejectEmptySource` | Empty source input validation | PASS |
| **EventRecorderTest** | `MonotonicSequenceOrdering` | Monotonic sequence assignment (`1..N`) | PASS |
| **EventRecorderTest** | `QueryAndFilterEvents` | Sequence range queries and type filtering | PASS |
| **EventRecorderTest** | `MaxCapacityEnforcement` | 10,000-event ring buffer capacity bound | PASS |
| **CheckpointManagerTest** | `CreateAndFindNearestCheckpoint` | Checkpoint creation and sequence lookup | PASS |
| **CheckpointManagerTest** | `CoverageDescriptionContent` | Honest coverage metadata disclaimer | PASS |
| **ReplayEngineTest** | `SeekExactSnapshot` | Exact register snapshot retrieval and delta calculation | PASS |
| **ReplayEngineTest** | `SeekInterpolatedCheckpoint` | Nearest checkpoint fallback for metadata events | PASS |
| **ReplayEngineTest** | `SeekOutOfRangeReportsError` | Out-of-bounds sequence error reporting | PASS |
| **IntegrationTest** | `FullEndToEndSessionExecution` | Real Linux compilation, `ptrace` tracing, registers, single-stepping, memory inspection, and cleanup | PASS |

---

## 3. Frontend Validation

The frontend is validated for strict TypeScript type checking and production bundling using Vite.

```bash
cd frontend
npm run build
```

**Verification Criteria:**
- 0 TypeScript compiler errors (`tsc --noEmit`).
- Fast Vite bundling producing production assets in `frontend/dist/`.

