#include "replay/ReplayEngine.hpp"
#include <iomanip>
#include <sstream>

namespace ttd {

static std::string hex_str(uint64_t val) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << val;
    return ss.str();
}

nlohmann::json SeekResult::to_json() const {
    nlohmann::json j;
    j["targetSequence"] = target_sequence;
    j["actualSequence"] = actual_sequence;
    j["stateAvailability"] = to_string(state_availability);
    j["isLiveProcess"] = is_live_process;
    j["success"] = success;
    j["errorMessage"] = error_message;
    j["event"] = event.to_json();

    if (registers.has_value()) {
        j["registers"] = registers.value().to_json();
    } else {
        j["registers"] = nullptr;
    }

    nlohmann::json diff_arr = nlohmann::json::array();
    for (const auto& d : register_diff) {
        diff_arr.push_back({
            {"name", d.name},
            {"oldValue", d.old_value},
            {"newValue", d.new_value},
            {"changed", d.changed}
        });
    }
    j["registerDiff"] = diff_arr;

    if (stack_memory.has_value()) {
        j["stackMemory"] = stack_memory.value().to_json();
    } else {
        j["stackMemory"] = nullptr;
    }

    j["nearestCheckpointId"] = nearest_checkpoint_id;
    j["nearestCheckpointSequence"] = nearest_checkpoint_sequence;
    j["replayLimitationNotice"] = replay_limitation_notice;

    return j;
}

ReplayEngine::ReplayEngine(const EventRecorder& recorder, const CheckpointManager& checkpoints)
    : recorder_(recorder), checkpoints_(checkpoints) {}

ReplayEngine::~ReplayEngine() = default;

std::vector<RegisterDiffItem> ReplayEngine::compute_register_diff(const Registers& before, const Registers& after) const {
    std::vector<RegisterDiffItem> diffs;

    auto check_reg = [&](const std::string& name, uint64_t val_before, uint64_t val_after) {
        RegisterDiffItem item;
        item.name = name;
        item.old_value = hex_str(val_before);
        item.new_value = hex_str(val_after);
        item.changed = (val_before != val_after);
        diffs.push_back(item);
    };

    check_reg("RIP", before.rip, after.rip);
    check_reg("RSP", before.rsp, after.rsp);
    check_reg("RBP", before.rbp, after.rbp);
    check_reg("RAX", before.rax, after.rax);
    check_reg("RBX", before.rbx, after.rbx);
    check_reg("RCX", before.rcx, after.rcx);
    check_reg("RDX", before.rdx, after.rdx);
    check_reg("RSI", before.rsi, after.rsi);
    check_reg("RDI", before.rdi, after.rdi);
    check_reg("R8",  before.r8,  after.r8);
    check_reg("R9",  before.r9,  after.r9);
    check_reg("R10", before.r10, after.r10);
    check_reg("R11", before.r11, after.r11);
    check_reg("R12", before.r12, after.r12);
    check_reg("R13", before.r13, after.r13);
    check_reg("R14", before.r14, after.r14);
    check_reg("R15", before.r15, after.r15);
    check_reg("EFLAGS", before.eflags, after.eflags);

    return diffs;
}

SeekResult ReplayEngine::seek(uint64_t target_sequence) const {
    SeekResult result;
    result.target_sequence = target_sequence;
    result.is_live_process = false;

    uint64_t max_seq = recorder_.get_latest_sequence();
    if (target_sequence == 0 || target_sequence > max_seq) {
        result.success = false;
        result.error_message = "Target sequence " + std::to_string(target_sequence) +
                               " is out of range (recorded 1.." + std::to_string(max_seq) + ")";
        return result;
    }

    auto ev_opt = recorder_.get_by_sequence(target_sequence);
    if (!ev_opt.has_value()) {
        result.success = false;
        result.error_message = "Event with sequence " + std::to_string(target_sequence) + " not found";
        return result;
    }

    result.event = ev_opt.value();
    result.actual_sequence = result.event.sequence;
    result.success = true;

    // Check if the target event has its own exact captured register snapshot
    if (result.event.registers.has_value()) {
        result.state_availability = StateAvailability::EXACT_SNAPSHOT;
        result.registers = result.event.registers.value();
        result.replay_limitation_notice = "Historical Inspection: Viewing exact captured register snapshot recorded at sequence #" +
                                          std::to_string(result.actual_sequence) + ". Live process execution state is untouched.";

        // Find immediately preceding event with registers to compute difference
        if (target_sequence > 1) {
            auto prev_ev = recorder_.get_by_sequence(target_sequence - 1);
            if (prev_ev.has_value() && prev_ev->registers.has_value()) {
                result.register_diff = compute_register_diff(prev_ev->registers.value(), result.registers.value());
            }
        }
    } else {
        // Fall back to nearest prior checkpoint
        auto ckpt_opt = checkpoints_.find_at_or_before(target_sequence);
        if (ckpt_opt.has_value()) {
            result.state_availability = StateAvailability::INTERPOLATED_CHECKPOINT;
            result.registers = ckpt_opt->registers;
            result.nearest_checkpoint_id = ckpt_opt->id;
            result.nearest_checkpoint_sequence = ckpt_opt->sequence;
            result.stack_memory = ckpt_opt->stack_sample;
            result.replay_limitation_notice = "Historical Inspection: Event #" + std::to_string(target_sequence) +
                                              " has no direct snapshot. Displaying nearest captured checkpoint #" +
                                              std::to_string(ckpt_opt->sequence) + " (" + ckpt_opt->id + ").";
        } else {
            result.state_availability = StateAvailability::EVENT_METADATA_ONLY;
            result.registers = std::nullopt;
            result.replay_limitation_notice = "Historical Inspection: Only event telemetry was recorded at sequence #" +
                                              std::to_string(target_sequence) + ". No register or memory snapshot was captured.";
        }
    }

    // Attach nearest checkpoint info if available
    auto nearest_ckpt = checkpoints_.find_at_or_before(target_sequence);
    if (nearest_ckpt.has_value()) {
        result.nearest_checkpoint_id = nearest_ckpt->id;
        result.nearest_checkpoint_sequence = nearest_ckpt->sequence;
        if (!result.stack_memory.has_value()) {
            result.stack_memory = nearest_ckpt->stack_sample;
        }
    }

    return result;
}

} // namespace ttd
