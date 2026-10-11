#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include "common/Types.hpp"
#include "recording/EventRecorder.hpp"
#include "checkpoint/CheckpointManager.hpp"

namespace ttd {

enum class StateAvailability {
    EXACT_SNAPSHOT,          // An exact register snapshot was captured at this sequence
    INTERPOLATED_CHECKPOINT, // Nearest prior checkpoint state is available
    EVENT_METADATA_ONLY      // Only event metadata was recorded; no state snapshot
};

inline std::string to_string(StateAvailability sa) {
    switch (sa) {
        case StateAvailability::EXACT_SNAPSHOT:          return "exact_snapshot";
        case StateAvailability::INTERPOLATED_CHECKPOINT: return "interpolated_checkpoint";
        case StateAvailability::EVENT_METADATA_ONLY:      return "event_metadata_only";
    }
    return "unknown";
}

struct RegisterDiffItem {
    std::string name;
    std::string old_value;
    std::string new_value;
    bool changed{false};
};

struct SeekResult {
    uint64_t target_sequence{0};
    uint64_t actual_sequence{0};
    StateAvailability state_availability{StateAvailability::EVENT_METADATA_ONLY};
    bool is_live_process{false};
    TraceEvent event;
    std::optional<Registers> registers;
    std::vector<RegisterDiffItem> register_diff;
    std::optional<MemoryDump> stack_memory;
    std::string nearest_checkpoint_id;
    uint64_t nearest_checkpoint_sequence{0};
    std::string replay_limitation_notice;
    bool success{false};
    std::string error_message;

    nlohmann::json to_json() const;
};

class ReplayEngine {
public:
    ReplayEngine(const EventRecorder& recorder, const CheckpointManager& checkpoints);
    ~ReplayEngine();

    // Seeks to a specific sequence in the execution history
    SeekResult seek(uint64_t target_sequence) const;

    // Compares registers between two points in history
    std::vector<RegisterDiffItem> compute_register_diff(const Registers& before, const Registers& after) const;

private:
    const EventRecorder& recorder_;
    const CheckpointManager& checkpoints_;
};

} // namespace ttd
