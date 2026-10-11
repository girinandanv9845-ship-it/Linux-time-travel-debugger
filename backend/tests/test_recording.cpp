#include <gtest/gtest.h>
#include "recording/EventRecorder.hpp"

using namespace ttd;

TEST(EventRecorderTest, MonotonicSequenceOrdering) {
    EventRecorder recorder(100);

    auto ev1 = recorder.record(EventType::PROCESS_STARTED, 100, 0x401000, "Start");
    auto ev2 = recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401004, "Step 1");
    auto ev3 = recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401008, "Step 2");

    EXPECT_EQ(ev1.sequence, 1);
    EXPECT_EQ(ev2.sequence, 2);
    EXPECT_EQ(ev3.sequence, 3);
    EXPECT_EQ(recorder.get_latest_sequence(), 3);
    EXPECT_EQ(recorder.get_total_events(), 3);
}

TEST(EventRecorderTest, QueryAndFilterEvents) {
    EventRecorder recorder(100);

    recorder.record(EventType::PROCESS_STARTED, 100, 0x401000, "Start");
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401004, "Step 1");
    recorder.record(EventType::SIGNAL_OBSERVED, 100, 0x401008, "Signal 11", std::nullopt, 11, "SIGSEGV");
    recorder.record(EventType::PROCESS_EXITED, 100, 0, "Exit");

    auto steps = recorder.query_events(1, 10, "single_step_completed");
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].sequence, 2);

    auto all_events = recorder.query_events(2, 3);
    ASSERT_EQ(all_events.size(), 2);
    EXPECT_EQ(all_events[0].sequence, 2);
    EXPECT_EQ(all_events[1].sequence, 3);
}

TEST(EventRecorderTest, MaxCapacityEnforcement) {
    EventRecorder recorder(5); // Capacity 5

    for (int i = 0; i < 10; ++i) {
        recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401000 + i, "Step " + std::to_string(i));
    }

    EXPECT_TRUE(recorder.is_limit_reached());
    EXPECT_EQ(recorder.get_total_events(), 5);
    EXPECT_EQ(recorder.get_latest_sequence(), 5);
    EXPECT_GE(recorder.get_overflow_count(), 1u);

    auto first = recorder.get_by_sequence(1);
    EXPECT_TRUE(first.has_value());

    auto beyond_limit = recorder.get_by_sequence(6);
    EXPECT_FALSE(beyond_limit.has_value());
}

TEST(EventRecorderTest, RapidTraceDoesNotSilentlyDropHistory) {
    EventRecorder recorder(3);

    recorder.record(EventType::PROCESS_STARTED, 100, 0x401000, "Start");
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401004, "Step 1");
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x401008, "Step 2");
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 100, 0x40100c, "Step 3");

    EXPECT_TRUE(recorder.is_limit_reached());
    EXPECT_EQ(recorder.get_total_events(), 3);
    EXPECT_EQ(recorder.get_latest_sequence(), 3);
    EXPECT_GT(recorder.get_overflow_count(), 0u);

    auto first = recorder.get_by_sequence(1);
    EXPECT_TRUE(first.has_value());
    auto newest = recorder.get_by_sequence(3);
    EXPECT_TRUE(newest.has_value());
    EXPECT_EQ(newest->description, "Step 2");
}
