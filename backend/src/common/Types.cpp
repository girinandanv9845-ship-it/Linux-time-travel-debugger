#include "common/Types.hpp"
#include <iomanip>
#include <sstream>

namespace ttd {

static std::string to_hex_string(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << val;
    return ss.str();
}

nlohmann::json Registers::to_json() const {
    nlohmann::json j;
    j["rip"] = to_hex_string(rip);
    j["rsp"] = to_hex_string(rsp);
    j["rbp"] = to_hex_string(rbp);
    j["rax"] = to_hex_string(rax);
    j["rbx"] = to_hex_string(rbx);
    j["rcx"] = to_hex_string(rcx);
    j["rdx"] = to_hex_string(rdx);
    j["rsi"] = to_hex_string(rsi);
    j["rdi"] = to_hex_string(rdi);
    j["r8"]  = to_hex_string(r8);
    j["r9"]  = to_hex_string(r9);
    j["r10"] = to_hex_string(r10);
    j["r11"] = to_hex_string(r11);
    j["r12"] = to_hex_string(r12);
    j["r13"] = to_hex_string(r13);
    j["r14"] = to_hex_string(r14);
    j["r15"] = to_hex_string(r15);
    j["eflags"] = to_hex_string(eflags);
    j["cs"] = to_hex_string(cs);
    j["ss"] = to_hex_string(ss);
    j["ds"] = to_hex_string(ds);
    j["es"] = to_hex_string(es);
    j["fs"] = to_hex_string(fs);
    j["gs"] = to_hex_string(gs);

    // Provide numeric values for calculation and diffs
    j["raw"] = {
        {"rip", rip}, {"rsp", rsp}, {"rbp", rbp},
        {"rax", rax}, {"rbx", rbx}, {"rcx", rcx},
        {"rdx", rdx}, {"rsi", rsi}, {"rdi", rdi},
        {"r8", r8},   {"r9", r9},   {"r10", r10},
        {"r11", r11}, {"r12", r12}, {"r13", r13},
        {"r14", r14}, {"r15", r15}, {"eflags", eflags}
    };
    return j;
}

Registers Registers::from_json(const nlohmann::json& j) {
    Registers r;
    if (j.contains("raw")) {
        const auto& raw = j["raw"];
        if (raw.contains("rip")) r.rip = raw["rip"].get<uint64_t>();
        if (raw.contains("rsp")) r.rsp = raw["rsp"].get<uint64_t>();
        if (raw.contains("rbp")) r.rbp = raw["rbp"].get<uint64_t>();
        if (raw.contains("rax")) r.rax = raw["rax"].get<uint64_t>();
        if (raw.contains("rbx")) r.rbx = raw["rbx"].get<uint64_t>();
        if (raw.contains("rcx")) r.rcx = raw["rcx"].get<uint64_t>();
        if (raw.contains("rdx")) r.rdx = raw["rdx"].get<uint64_t>();
        if (raw.contains("rsi")) r.rsi = raw["rsi"].get<uint64_t>();
        if (raw.contains("rdi")) r.rdi = raw["rdi"].get<uint64_t>();
        if (raw.contains("r8"))  r.r8  = raw["r8"].get<uint64_t>();
        if (raw.contains("r9"))  r.r9  = raw["r9"].get<uint64_t>();
        if (raw.contains("r10")) r.r10 = raw["r10"].get<uint64_t>();
        if (raw.contains("r11")) r.r11 = raw["r11"].get<uint64_t>();
        if (raw.contains("r12")) r.r12 = raw["r12"].get<uint64_t>();
        if (raw.contains("r13")) r.r13 = raw["r13"].get<uint64_t>();
        if (raw.contains("r14")) r.r14 = raw["r14"].get<uint64_t>();
        if (raw.contains("r15")) r.r15 = raw["r15"].get<uint64_t>();
        if (raw.contains("eflags")) r.eflags = raw["eflags"].get<uint64_t>();
    }
    return r;
}

nlohmann::json MemoryDump::to_json() const {
    nlohmann::json j;
    j["address"] = to_hex_string(address);
    j["rawAddress"] = address;
    j["length"] = length;
    j["bytes"] = bytes;
    j["hexString"] = hex_string;
    j["asciiString"] = ascii_string;
    j["isAccessible"] = is_accessible;
    j["errorMessage"] = error_message;
    j["isHistorical"] = is_historical;
    return j;
}

nlohmann::json TraceEvent::to_json() const {
    nlohmann::json j;
    j["sequence"] = sequence;
    j["type"] = to_string(type);
    j["timestamp"] = timestamp_ms;
    j["pid"] = pid;
    j["threadId"] = thread_id;
    j["rip"] = to_hex_string(rip);
    j["rawRip"] = rip;

    if (source_line.has_value()) {
        j["sourceLine"] = source_line.value();
    } else {
        j["sourceLine"] = nullptr;
    }
    j["sourceFile"] = source_file;
    j["description"] = description;

    if (registers.has_value()) {
        j["registers"] = registers.value().to_json();
    } else {
        j["registers"] = nullptr;
    }

    if (signal_number.has_value()) {
        j["signalNumber"] = signal_number.value();
        j["signalName"] = signal_name;
    }
    if (syscall_number.has_value()) {
        j["syscallNumber"] = syscall_number.value();
        j["syscallName"] = syscall_name;
    }
    if (!checkpoint_id.empty()) {
        j["checkpointId"] = checkpoint_id;
    }
    j["metadata"] = metadata;
    return j;
}

nlohmann::json Checkpoint::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["sequence"] = sequence;
    j["timestamp"] = timestamp_ms;
    j["pid"] = pid;
    j["registers"] = registers.to_json();
    j["stackSample"] = stack_sample.to_json();
    j["coverageDescription"] = coverage_description;
    j["metadata"] = metadata;
    return j;
}

nlohmann::json ProgramOutput::to_json() const {
    nlohmann::json j;
    j["stdout"] = stdout_data;
    j["stderr"] = stderr_data;
    j["timestamp"] = timestamp_ms;
    j["sequence"] = sequence;
    return j;
}

} // namespace ttd
