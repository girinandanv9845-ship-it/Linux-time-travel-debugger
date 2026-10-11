# Live Demonstration Guide — Academic Presentation Script

Follow this step-by-step guide to demonstrate the complete, end-to-end capabilities of the **Web-Based Linux Process Time-Travel Debugger**.

---

## 1. Startup & Environment Verification

1. Open your terminal in WSL/Linux at the project root:
   ```bash
   ./scripts/run.sh
   ```
2. Open your web browser at:
   `http://localhost:3000`
3. **What to point out:**
   - In the header, note the **Backend Connected** green indicator and the verified host architecture: **Linux x86_64 (Native ptrace)**.
   - The initial mode indicates **Live Process Mode**.

---

## 2. Compilation Diagnostics Demo

1. In the source editor dropdown, select **"6. Invalid Compilation (Diagnostics)"**.
2. Click **"Launch Target"**.
3. **What to point out:**
   - The backend safely invokes `/usr/bin/gcc` via `fork()` + `execv()`, without crashing or hanging.
   - A bright red compilation diagnostics banner appears in the editor, displaying the exact GCC error message and line number.
   - The session state transitions cleanly to `FAILED`.

---

## 3. Real Process Execution & Step Tracing Demo

1. In the source editor dropdown, select **"1. Loop & Changing Variable"** (`simple_loop.c`).
2. Click **"Launch Target"**.
3. **What to point out:**
   - The target program compiles cleanly with `-g -O0 -no-pie`.
   - The tracer forks and intercepts the initial `SIGTRAP` at program entry.
   - The state transitions to `PAUSED`.
   - The header displays the active Linux **PID** (e.g., `PID: 48921`).
   - The **Register Inspector** immediately populates with live x86_64 CPU registers (`RIP`, `RSP`, `RAX`, etc.).
   - The **Interactive Timeline** shows Checkpoint `#1` (the entry point state).
4. Click **"Single Step"** 4–5 times:
   - Notice the **RIP** (Instruction Pointer) advances with each step.
   - Notice the yellow pulse highlighting registers that changed value during the step.
   - Look at the **Event Log**: each step appends a monotonic sequence event (`#3`, `#4`, `#5`...).
5. Click **"Resume"** to run the program continuously:
   - The program executes the loop.
   - Watch the **Program Console (I/O)** stream live `printf` output:
     `"Starting loop execution..."`
     `"Iteration 0: val = 0"`
     `...`
   - The state transitions to `EXITED` upon normal program termination.

---

## 4. Time-Travel & Historical Navigation Demo

1. While the program is stopped or exited, click an earlier sequence on the **Interactive Timeline** (e.g. Sequence `#3` or `#5`), or click **"Previous Event"** on the toolbar.
2. **What to point out:**
   - The top header immediately transitions from green to amber:
     **"Historical Inspection: Seq #3 of 42"**.
   - A telemetry advisory banner appears in the status panel:
     *"Historical Inspection: Viewing exact captured register snapshot recorded at sequence #3. Live process execution state is untouched."*
   - The **Register Inspector** displays the historical snapshot captured at that exact moment.
   - Changed registers between Sequence #2 and #3 are highlighted with delta tags.
3. Click on **Checkpoint #1** (cyan marker) on the timeline:
   - Demonstrates nearest checkpoint interpolation and bounded stack memory retrieval.
4. Click **"Return to Live"**:
   - The playhead immediately snaps back to the final execution state.

---

## 5. Virtual Memory Inspection Demo

1. In the **Memory Inspector** panel, click the quick jump button: **`$rsp`**.
2. Click **"Inspect"**:
   - The tracer queries virtual memory using `process_vm_readv`.
   - The panel displays a 16-byte formatted hexadecimal grid alongside printable ASCII representations.
3. Now type an invalid/null address into the search box: `0x0`.
4. Click **"Inspect"**:
   - The inspector cleanly rejects the address and surfaces:
     *"Null memory address cannot be inspected"*.
   - Demonstrates memory bounds enforcement and safety against segmentation faults.

---

## 6. Multi-Variable & Function Demos

1. Select **"4. Nested Functions & Stack Frame"** (`functions.c`).
2. Launch and step through function calls:
   - Observe the stack pointer `RSP` and base pointer `RBP` change as call frames are allocated and returned.
3. This completes the live demonstration!

