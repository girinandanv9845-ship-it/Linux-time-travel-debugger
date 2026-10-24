#include <gtest/gtest.h>
#include "common/Types.hpp"

using namespace ttd;

TEST(TypesTest, RegistersSerializationAndDeserialization) {
    Registers r;
    r.rip = 0x401000;
    r.rsp = 0x7fffffffe000;
    r.rax = 42;
    r.rbx = 100;
    r.eflags = 0x202;

    nlohmann::json j = r.to_json();
    EXPECT_TRUE(j.contains("rip"));
    EXPECT_TRUE(j.contains("raw"));
    EXPECT_EQ(j["raw"]["rip"], 0x401000);
    EXPECT_EQ(j["raw"]["rax"], 42);

    Registers restored = Registers::from_json(j);
    EXPECT_EQ(restored.rip, 0x401000);
    EXPECT_EQ(restored.rsp, 0x7fffffffe000);
    EXPECT_EQ(restored.rax, 42);
    EXPECT_EQ(restored.rbx, 100);
    EXPECT_EQ(restored.eflags, 0x202);
}

TEST(TypesTest, MemoryDumpFormatting) {
    MemoryDump dump;
    dump.address = 0x1000;
    dump.length = 4;
    dump.bytes = {'T', 'E', 'S', 'T'};
    dump.is_accessible = true;
    dump.ascii_string = "TEST";
    dump.hex_string = "54 45 53 54";

    nlohmann::json j = dump.to_json();
    EXPECT_EQ(j["address"], "0x0000000000001000");
    EXPECT_EQ(j["length"], 4);
    EXPECT_EQ(j["asciiString"], "TEST");
    EXPECT_EQ(j["hexString"], "54 45 53 54");
    EXPECT_TRUE(j["isAccessible"]);
}

TEST(TypesTest, TraceEventSerialization) {
    TraceEvent ev;
    ev.sequence = 15;
    ev.type = EventType::SINGLE_STEP_COMPLETED;
    ev.timestamp_ms = 123456789;
    ev.pid = 9999;
    ev.rip = 0x401020;
    ev.description = "Test step event";

    nlohmann::json j = ev.to_json();
    EXPECT_EQ(j["sequence"], 15);
    EXPECT_EQ(j["type"], "single_step_completed");
    EXPECT_EQ(j["pid"], 9999);
    EXPECT_EQ(j["description"], "Test step event");
}

TEST(TypesTest, ArchitectureCheck) {
    EXPECT_TRUE(IS_SUPPORTED_ARCH);
#if defined(__aarch64__)
    EXPECT_STREQ(SUPPORTED_ARCHITECTURE, "aarch64");
#else
    EXPECT_STREQ(SUPPORTED_ARCHITECTURE, "x86_64");
#endif
}
