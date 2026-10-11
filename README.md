# 🐧 Linux Time-Travel Debugger

**A web-based debugging tool for tracing Linux processes, recording execution history, and inspecting program state across past execution events.**

The Linux Time-Travel Debugger combines a modern web interface with a native C++ tracing backend to make low-level process debugging more interactive and understandable.

Instead of relying only on traditional forward execution, the project aims to let developers explore recorded execution events, inspect historical state, and understand how a program progresses over time.

## ✨ Key Features

- **Process Control** — Start, resume, pause, stop, and single-step a target process.
- **Execution Recording** — Capture execution events and state information for debugging.
- **Historical Navigation** — Move between previously recorded events using Previous Event and Next Event controls.
- **Register Inspection** — Inspect recorded CPU register information where available.
- **Memory Inspection** — Examine recorded memory information within the supported snapshot data.
- **Interactive Timeline** — Navigate execution history through a visual timeline.
- **Event Log** — View execution events and relevant debugging information.
- **Web-Based Interface** — Use a React and TypeScript frontend to interact with the debugger.
- **Native Linux Tracing** — Use C++ and Linux `ptrace` facilities to trace real processes.
- **Error Reporting** — Surface debugging and process-control errors to help diagnose failures.

> **Important:** Historical navigation is intended for inspecting recorded execution history. It does not automatically restore a running process to an earlier state or provide full reverse execution.

## 🏗️ Architecture

The project is organized into three main parts:

### 1. Frontend

Built with React, TypeScript, and Vite, the frontend provides the debugging interface.

Responsibilities include:
- Process execution controls
- Interactive execution timeline
- Event log and status displays
- Register and memory inspection
- Communication with the backend

### 2. Backend

Built with C++, the backend manages the target process and its recorded execution history.

Responsibilities include:
- Process lifecycle management
- Linux process tracing using `ptrace`
- Execution control and event recording
- Register and memory inspection
- Session management and error handling

### 3. Linux Execution Environment

The tracing backend requires a compatible Linux environment with the necessary permissions and tracing support. Windows users can use WSL with Ubuntu, subject to the relevant WSL and `ptrace` limitations.

## 🧰 Technology Stack

| Component | Technology |
|---|---|
| Frontend | React |
| Language | TypeScript |
| Frontend tooling | Vite |
| Backend | C++ |
| Build system | CMake |
| Process tracing | Linux `ptrace` |
| Development environment | Linux / WSL Ubuntu |
| Version control | Git and GitHub |

## 📂 Project Structure

```text
PROCESS_DEBUGGER/
├── backend/
│   ├── include/       # C++ header files
│   ├── src/           # Backend implementation
│   └── tests/         # Backend tests
├── frontend/
│   └── src/           # React and TypeScript application
├── data/
│   └── sessions/      # Runtime session data, if generated
├── docs/              # Architecture and development documentation
├── examples/          # Example programs and related resources
├── scripts/           # Development scripts
├── CMakeLists.txt     # Root CMake configuration
└── README.md
```

## ⚙️ Prerequisites

Install the following before running the project:

- Git
- CMake
- A compatible C++ compiler and build tools
- Node.js and npm
- Linux or WSL with Ubuntu
- The dependencies required by the project's CMake and npm configurations

For Windows, install and configure WSL with Ubuntu and ensure that the backend can launch and trace processes in that environment.

## 🚀 Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/SUJALAGARWAL10/Linux-time-travel-debugger.git

cd Linux-time-travel-debugger
```

### 2. Build the backend

Run these commands inside the Linux or WSL Ubuntu environment:

```bash
# Configure the CMake build.
cmake -S . -B build

# Compile the backend and its configured targets.
cmake --build build -j2
```

### 3. Start the backend

From the project root, run:

```bash
# Enable detailed tracing diagnostics.
TTD_TRACE_DEBUG=1 ./build/backend/ttd_backend
```

Keep the backend terminal open while using the frontend.

### 4. Install frontend dependencies

Open another terminal and navigate to the frontend directory:

```bash
# Enter the frontend directory.
cd frontend

# Install the project's npm dependencies.
npm install
```

### 5. Start the frontend

```bash
# Start the Vite development server.
npm run dev -- --host 0.0.0.0 --port 3000
```

Open the local development URL in your browser:

**http://localhost:3000**

If the backend or frontend uses different ports in your local configuration, use the values configured in the project.

## 🧪 Testing

Build and test the backend using the project's configured CMake targets.

```bash
# Build the project.
cmake --build build -j2

# Run the tests registered with CTest.
ctest --test-dir build --output-on-failure
```

To validate frontend types and production compilation:

```bash
# Check TypeScript without emitting output.
cd frontend
npx tsc --noEmit

# Build the frontend for production.
npx vite build --configLoader runner
```

Successful compilation alone does not guarantee that live process tracing works. Runtime testing should also verify process startup, resume, pause, single-step, historical navigation, and cleanup.

## 🔍 How It Works

1. **Start a debugging session** and select or launch a supported target program.
2. **Trace execution** through the native Linux backend.
3. **Record execution history** and the state information supported by the recorder.
4. **Inspect events** using the event log and timeline.
5. **Examine recorded state** through the register and memory inspection panels.
6. **Navigate historical events** to investigate the recorded execution sequence.
7. **Stop the session** and verify that the target process and tracing resources are cleaned up.

## 🎯 Project Goals

The long-term goal is to make low-level Linux debugging more accessible by combining native process tracing with a visual, browser-based interface.

Potential future improvements include:

- More complete execution snapshots and checkpoint management
- Improved historical state inspection
- Reverse execution through safe and well-defined state restoration
- Better process lifecycle and error diagnostics
- Expanded automated integration tests
- More detailed execution visualizations

These are development goals, not claims that every capability is already implemented.

## ⚠️ Limitations

- The backend requires a compatible Linux tracing environment.
- WSL behavior may differ depending on its configuration and permissions.
- Historical inspection is limited to the events and state actually recorded.
- Full reverse execution requires additional mechanisms to restore or reproduce program state.
- Runtime behavior and supported target programs should be verified through integration tests.

## 🛠️ Troubleshooting

**Backend fails to build**

Check that CMake, the C++ compiler, and required development dependencies are installed.

**Frontend cannot connect to the backend**

Confirm that the backend is running, the configured API address and port match, and no firewall or port conflict is preventing communication.

**Process tracing fails**

Check the Linux environment, process permissions, `ptrace` restrictions, and the backend logs. Restart the backend after rebuilding it.

**Resume or stepping fails**

Inspect the reported tracing error and verify the behavior with a freshly built backend. Do not assume the issue is fixed until the target process successfully resumes and new execution events are recorded.

## 🤝 Contributing

Contributions, bug reports, and suggestions are welcome.

1. Fork the repository.
2. Create a feature branch.
3. Make and test your changes.
4. Submit a pull request describing the changes and the tests performed.

## 📜 License

No license has been specified yet. Add a `LICENSE` file before distributing the project under a particular open-source license.

## 👨‍💻 Author

**Sujal Agarwal**

GitHub: [@SUJALAGARWAL10](https://github.com/SUJALAGARWAL10)

---

*Built to explore Linux process execution, native tracing, and interactive debugging through a web-based interface.*
