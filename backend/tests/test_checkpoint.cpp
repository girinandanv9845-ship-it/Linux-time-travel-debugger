#include <gtest/gtest.h>
#include "checkpoint/CheckpointManager.hpp"

using namespace ttd;

TEST(CheckpointManagerTest, CreateAndFindNearestCheckpoint) {
    CheckpointManager mgr(10);

    Registers r1;
    r1.rip = 0x401000;
    r1.rax = 1;

    Registers r2;
    r2.rip = 0x401050;
    r2.rax = 2;

    MemoryDump mem;
    mem.address = 0x7fffffffe000;
    mem.length = 8;
    mem.is_accessible = true;

    mgr.create_checkpoint(10, 1234, r1, mem, "At start");
    mgr.create_checkpoint(50, 1234, r2, mem, "Mid execution");

    EXPECT_EQ(mgr.count(), 2);

    // Query sequence 5: before any checkpoint
    auto c0 = mgr.find_at_or_before(5);
    EXPECT_FALSE(c0.has_value());

    // Query sequence 15: nearest is sequence 10
    auto c1 = mgr.find_at_or_before(15);
    ASSERT_TRUE(c1.has_value());
    EXPECT_EQ(c1->sequence, 10);
    EXPECT_EQ(c1->registers.rax, 1);

    // Query sequence 60: nearest is sequence 50
    auto c2 = mgr.find_at_or_before(60);
    ASSERT_TRUE(c2.has_value());
    EXPECT_EQ(c2->sequence, 50);
    EXPECT_EQ(c2->registers.rax, 2);
}

TEST(CheckpointManagerTest, CoverageDescriptionContent) {
    CheckpointManager mgr(10);
    Registers r;
    MemoryDump mem;

    auto ckpt = mgr.create_checkpoint(1, 100, r, mem, "Test desc");
    EXPECT_FALSE(ckpt.coverage_description.empty());
    EXPECT_NE(ckpt.coverage_description.find("Notice"), std::string::npos);
    EXPECT_NE(ckpt.coverage_description.find("Test desc"), std::string::npos);
}
