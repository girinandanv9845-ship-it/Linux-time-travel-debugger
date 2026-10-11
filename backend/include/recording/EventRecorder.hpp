#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <optional>
#include <atomic>
#include "common/Types.hpp"

namespace ttd {

class EventRecorder {
public:
    explicit EventRecorder(size_t max_events = 10000);
    ~EventRecorder();

    // Appends a new event, automatically assigning the next monotonic sequence number
    TraceEvent record(EventType type,
                      int32_t pid,
                      uint64_t rip,
                      const std::string& description,
                      const std::optional<Registers>& registers = std::nullopt,
                      const std::optional<int32_t>& signal_number = std::nullopt,
                      const std::string& signal_name = "",
                      const std::optional<int64_t>& syscall_number = std::nullopt,
                      const std::string& syscall_name = "",
                      const std::string& checkpoint_id = "",
                      const nlohmann::json& metadata = nlohmann::json::object());

    // Retrieves an event by exact sequence number
    std::optional<TraceEvent> get_by_sequence(uint64_t sequence) const;

    // Queries events with filtering and pagination
    std::vector<TraceEvent> query_events(uint64_t from_seq = 1,
                                         uint64_t to_seq = UINT64_MAX,
                                         const std::string& type_filter = "",
                                         size_t limit = 100,
                                         size_t offset = 0) const;

    // Sequence queries
    uint64_t get_latest_sequence() const;
    size_t get_total_events() const;
    size_t get_max_events() const { return max_events_; }
    bool is_limit_reached() const { return limit_reached_.load(std::memory_order_acquire); }
    size_t get_overflow_count() const { return overflow_count_.load(std::memory_order_relaxed); }
    void clear();

private:
    size_t max_events_;
    std::atomic<uint64_t> sequence_counter_{1};
    std::atomic<size_t> overflow_count_{0};
    std::atomic<bool> limit_reached_{false};
    mutable std::mutex mutex_;
    std::vector<TraceEvent> events_;
};

} // namespace ttd
