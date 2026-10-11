#pragma once

#include <vector>
#include <mutex>
#include <optional>
#include <memory>
#include <string>
#include "common/Types.hpp"

namespace ttd {

class CheckpointManager {
public:
    explicit CheckpointManager(size_t max_checkpoints = 200);
    ~CheckpointManager();

    // Creates and records an immutable checkpoint
    Checkpoint create_checkpoint(uint64_t sequence,
                                 int32_t pid,
                                 const Registers& regs,
                                 const MemoryDump& stack_sample,
                                 const std::string& description_suffix = "");

    // Finds the nearest checkpoint created at or prior to the specified sequence number
    std::optional<Checkpoint> find_at_or_before(uint64_t sequence) const;

    // Retrieves a checkpoint by its ID
    std::optional<Checkpoint> get_by_id(const std::string& id) const;

    // Returns all recorded checkpoints
    std::vector<Checkpoint> get_all() const;

    // Count of recorded checkpoints
    size_t count() const;

    void clear();

private:
    size_t max_checkpoints_;
    size_t next_id_num_{1};
    mutable std::mutex mutex_;
    std::vector<Checkpoint> checkpoints_;
};

} // namespace ttd
