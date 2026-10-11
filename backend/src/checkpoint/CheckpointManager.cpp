#include "checkpoint/CheckpointManager.hpp"
#include <chrono>

namespace ttd {

CheckpointManager::CheckpointManager(size_t max_checkpoints)
    : max_checkpoints_(max_checkpoints) {
    checkpoints_.reserve(max_checkpoints_);
}

CheckpointManager::~CheckpointManager() = default;

Checkpoint CheckpointManager::create_checkpoint(uint64_t sequence,
                                               int32_t pid,
                                               const Registers& regs,
                                               const MemoryDump& stack_sample,
                                               const std::string& description_suffix) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(mutex_);
    Checkpoint ckpt;
    ckpt.id = "ckpt_" + std::to_string(next_id_num_++);
    ckpt.sequence = sequence;
    ckpt.timestamp_ms = now;
    ckpt.pid = pid;
    ckpt.registers = regs;
    ckpt.stack_sample = stack_sample;
    ckpt.coverage_description = "General-purpose x86_64 registers (RIP, RSP, RBP, RAX..R15, EFLAGS) and bounded stack slice. "
                               "Notice: Kernel state, file descriptors, signals, and uncaptured heap pages are not saved." +
                               (description_suffix.empty() ? "" : " (" + description_suffix + ")");

    ckpt.metadata = {
        {"rip", regs.to_json()["rip"]},
        {"rsp", regs.to_json()["rsp"]},
        {"stack_bytes", stack_sample.length}
    };

    if (checkpoints_.size() >= max_checkpoints_) {
        checkpoints_.erase(checkpoints_.begin());
    }
    checkpoints_.push_back(ckpt);

    return ckpt;
}

std::optional<Checkpoint> CheckpointManager::find_at_or_before(uint64_t sequence) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const Checkpoint* best = nullptr;
    for (const auto& ckpt : checkpoints_) {
        if (ckpt.sequence <= sequence) {
            best = &ckpt;
        } else {
            break;
        }
    }
    if (best) return *best;
    return std::nullopt;
}

std::optional<Checkpoint> CheckpointManager::get_by_id(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& ckpt : checkpoints_) {
        if (ckpt.id == id) return ckpt;
    }
    return std::nullopt;
}

std::vector<Checkpoint> CheckpointManager::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return checkpoints_;
}

size_t CheckpointManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return checkpoints_.size();
}

void CheckpointManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    checkpoints_.clear();
    next_id_num_ = 1;
}

} // namespace ttd
