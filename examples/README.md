# Example Programs for Time-Travel Debugger

This directory contains deterministic sample programs demonstrating different aspects of execution, tracing, and historical state reconstruction.

| Program | Filename | Key Concepts Demonstrated | Supported Replay Mode |
|---|---|---|---|
| **Simple Loop** | `simple_loop.c` | Monotonic steps, register value changes (`RAX`, `RIP`), iteration boundaries, standard output buffering | Captured State & Historical Telemetry |
| **Variables** | `variables.c` | Arithmetic operations, multi-register delta inspection (`RAX`, `RDX`, `RSI`), stack locals | Full Register Snapshots & Diffing |
| **Branching** | `branching.c` | Conditional flags (`EFLAGS`), jump execution (`JNE`/`JE`), conditional paths | Branch Points & Checkpoint Interception |
| **Functions** | `functions.c` | Call frames, stack pointer movement (`RSP`, `RBP`), return instruction stepping | Stack Frame Inspection & Bounded Dumps |

## Compilation & Run

These programs are compiled automatically by the backend when running via the Web UI. They can also be compiled natively with:

```bash
gcc -g -O0 -no-pie -o simple_loop simple_loop.c
./simple_loop
```

