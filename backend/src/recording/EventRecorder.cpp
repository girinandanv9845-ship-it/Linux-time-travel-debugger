#include "recording/EventRecorder.hpp"
#include <chrono>
#include <algorithm>

namespace ttd {

EventRecorder::EventRecorder(size_t max_events)
    : max_events_(max_events) {
    events_.reserve(std::min<size_t>(max_events, 2048));
}

EventRecorder::~EventRecorder() = default;

TraceEvent EventRecorder::record(EventType type,
                                 int32_t pid,
                                 uint64_t rip,
                                 const std::string& description,
                                 const std::optional<Registers>& registers,
                                 const std::optional<int32_t>& signal_number,
                                 const std::string& signal_name,
                                 const std::optional<int64_t>& syscall_number,
                                 const std::string& syscall_name,
                                 const std::string& checkpoint_id,
                                 const nlohmann::json& metadata) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    TraceEvent event;
    event.sequence = sequence_counter_.fetch_add(1);
    event.type = type;
    event.timestamp_ms = now;
    event.pid = pid;
    event.thread_id = pid;
    event.rip = rip;
    event.description = description;
    event.registers = registers;
    event.signal_number = signal_number;
    event.signal_name = signal_name;
    event.syscall_number = syscall_number;
    event.syscall_name = syscall_name;
    event.checkpoint_id = checkpoint_id;
    event.metadata = metadata;

    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.size() >= max_events_) {
        overflow_count_.fetch_add(1, std::memory_order_relaxed);
        limit_reached_.store(true, std::memory_order_release);
        return event;
    }

    events_.push_back(event);
    if (events_.size() >= max_events_) {
        limit_reached_.store(true, std::memory_order_release);
    }

    return event;
}

std::optional<TraceEvent> EventRecorder::get_by_sequence(uint64_t sequence) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& ev : events_) {
        if (ev.sequence == sequence) {
            return ev;
        }
    }
    return std::nullopt;
}

std::vector<TraceEvent> EventRecorder::query_events(uint64_t from_seq,
                                                    uint64_t to_seq,
                                                    const std::string& type_filter,
                                                    size_t limit,
                                                    size_t offset) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TraceEvent> result;

    size_t matches_seen = 0;
    for (const auto& ev : events_) {
        if (ev.sequence < from_seq) continue;
        if (ev.sequence > to_seq) break;

        if (!type_filter.empty() && to_string(ev.type) != type_filter) {
            continue;
        }

        if (matches_seen < offset) {
            matches_seen++;
            continue;
        }

        result.push_back(ev);
        if (result.size() >= limit) break;
    }

    return result;
}

uint64_t EventRecorder::get_latest_sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.empty()) return 0;
    return events_.back().sequence;
}

size_t EventRecorder::get_total_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
}

void EventRecorder::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    sequence_counter_.store(1);
    overflow_count_.store(0, std::memory_order_release);
    limit_reached_.store(false, std::memory_order_release);
}

} // namespace ttd
