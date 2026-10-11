#include <gtest/gtest.h>
#include "recording/EventRecorder.hpp"
#include "checkpoint/CheckpointManager.hpp"
#include "replay/ReplayEngine.hpp"

using namespace ttd;

TEST(ReplayEngineTest, SeekExactSnapshot) {
    EventRecorder recorder;
    CheckpointManager checkpoints;
    ReplayEngine replay(recorder, checkpoints);

    Registers r1;
    r1.rip = 0x401000;
    r1.rax = 10;

    Registers r2;
    r2.rip = 0x401004;
    r2.rax = 20;

    recorder.record(EventType::PROCESS_STARTED, 1234, 0x401000, "Start", r1);
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 1234, 0x401004, "Step 1", r2);

    auto result = replay.seek(2);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.actual_sequence, 2);
    EXPECT_EQ(result.state_availability, StateAvailability::EXACT_SNAPSHOT);
    ASSERT_TRUE(result.registers.has_value());
    EXPECT_EQ(result.registers->rax, 20);

    // Register diff should highlight RAX changed from 10 to 20
    bool rax_changed = false;
    for (const auto& d : result.register_diff) {
        if (d.name == "RAX") {
            rax_changed = d.changed;
        }
    }
    EXPECT_TRUE(rax_changed);
}

TEST(ReplayEngineTest, SeekInterpolatedCheckpoint) {
    EventRecorder recorder;
    CheckpointManager checkpoints;
    ReplayEngine replay(recorder, checkpoints);

    Registers ckpt_regs;
    ckpt_regs.rip = 0x401000;
    ckpt_regs.rbx = 999;
    MemoryDump mem;

    recorder.record(EventType::PROCESS_STARTED, 1234, 0x401000, "Start");
    checkpoints.create_checkpoint(1, 1234, ckpt_regs, mem, "Base");

    // Event without register snapshot
    recorder.record(EventType::SINGLE_STEP_COMPLETED, 1234, 0x401020, "Step without snapshot");

    auto result = replay.seek(2);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.state_availability, StateAvailability::INTERPOLATED_CHECKPOINT);
    ASSERT_TRUE(result.registers.has_value());
    EXPECT_EQ(result.registers->rbx, 999);
    EXPECT_FALSE(result.replay_limitation_notice.empty());
}

TEST(ReplayEngineTest, SeekOutOfRangeReportsError) {
    EventRecorder recorder;
    CheckpointManager checkpoints;
    ReplayEngine replay(recorder, checkpoints);

    recorder.record(EventType::PROCESS_STARTED, 1234, 0x401000, "Start");

    auto result = replay.seek(999);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}
