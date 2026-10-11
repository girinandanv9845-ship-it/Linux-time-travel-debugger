#include <gtest/gtest.h>
#include "session/Session.hpp"
#include <thread>
#include <chrono>

using namespace ttd;

TEST(IntegrationTest, FullEndToEndSessionExecution) {
    CompilationOptions opts;
    opts.language = "c";
    opts.optimization = "O0";
    opts.debug_symbols = true;

    std::string source =
        "#include <stdio.h>\n"
        "int main(void) {\n"
        "    int counter = 10;\n"
        "    counter += 5;\n"
        "    printf(\"Counter is %d\\n\", counter);\n"
        "    return 0;\n"
        "}\n";

    Session session("integ_test_sess", source, opts);
    std::string err;

    // 1. Start session (compiles and enters initial trace stop)
    bool started = session.start(err);
    ASSERT_TRUE(started) << "Failed to start session: " << err;
    EXPECT_EQ(session.get_state(), SessionState::PAUSED);
    EXPECT_GT(session.get_pid(), 0);
    EXPECT_GE(session.get_checkpoints().count(), 1u);

    // 2. Read live registers at initial stop
    auto regs = session.get_live_registers();
    ASSERT_TRUE(regs.has_value());
    EXPECT_GT(regs->rip, 0);
    EXPECT_GT(regs->rsp, 0);

    // 3. Inspect valid memory (stack pointer RSP)
    MemoryDump stack_dump = session.read_memory(regs->rsp, 32);
    EXPECT_TRUE(stack_dump.is_accessible);
    EXPECT_EQ(stack_dump.length, 32);
    EXPECT_FALSE(stack_dump.hex_string.empty());

    // 4. Inspect invalid memory address (NULL address 0x0)
    MemoryDump null_dump = session.read_memory(0x0, 32);
    EXPECT_FALSE(null_dump.is_accessible);
    EXPECT_FALSE(null_dump.error_message.empty());

    // 5. Perform multiple single-instruction steps
    uint64_t initial_rip = regs->rip;
    bool rip_moved = false;
    for (int i = 0; i < 5; ++i) {
        TraceEvent step_ev;
        std::string step_err;
        bool stepped = session.step(step_ev, step_err);
        ASSERT_TRUE(stepped) << "Step failed: " << step_err;

        auto curr_regs = session.get_live_registers();
        if (curr_regs.has_value() && curr_regs->rip != initial_rip) {
            rip_moved = true;
        }
    }
    EXPECT_TRUE(rip_moved);

    // 6. Verify event recorder sequence monotonicity
    EXPECT_GE(session.get_recorder().get_total_events(), 7); // start + init_stop + 5 steps

    // 7. Time-travel seek to sequence 2
    SeekResult seek_res = session.seek(2);
    EXPECT_TRUE(seek_res.success);
    EXPECT_EQ(seek_res.actual_sequence, 2);
    EXPECT_FALSE(seek_res.is_live_process);
    EXPECT_FALSE(seek_res.replay_limitation_notice.empty());

    // 8. Stop session and verify clean termination
    bool stopped = session.stop(err);
    EXPECT_TRUE(stopped);
    EXPECT_EQ(session.get_state(), SessionState::STOPPED);
}

TEST(IntegrationTest, ResumePauseAndStopWhileRunning) {
    CompilationOptions opts;
    opts.language = "c";
    opts.optimization = "O0";
    opts.debug_symbols = true;

    std::string source =
        "int main(void) {\n"
        "    volatile unsigned long total = 0;\n"
        "    for (;;) {\n"
        "        ++total;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    Session session("pause_resume_session", source, opts);
    std::string err;

    ASSERT_TRUE(session.start(err)) << "Failed to start session: " << err;
    EXPECT_EQ(session.get_state(), SessionState::PAUSED);

    ASSERT_TRUE(session.resume(err)) << "Resume failed: " << err;
    EXPECT_EQ(session.get_state(), SessionState::RUNNING);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool paused = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (session.pause(err)) {
            paused = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(paused) << "Pause did not stop the running trace within the timeout: " << err;
    EXPECT_EQ(session.get_state(), SessionState::PAUSED);
    EXPECT_GT(session.get_recorder().get_total_events(), 0u);
    EXPECT_FALSE(session.pause(err)) << "A second Pause while paused must be rejected";

    const auto first_pause_sequence = session.get_recorder().get_latest_sequence();
    ASSERT_TRUE(session.resume(err)) << "Second resume failed: " << err;
    EXPECT_EQ(session.get_state(), SessionState::RUNNING);
    EXPECT_FALSE(session.resume(err)) << "A second Resume while running must not create another worker";
    ASSERT_TRUE(session.pause(err)) << "Second pause failed: " << err;
    EXPECT_EQ(session.get_state(), SessionState::PAUSED);
    EXPECT_GT(session.get_recorder().get_latest_sequence(), first_pause_sequence);

    bool stopped = session.stop(err);
    EXPECT_TRUE(stopped) << "Stop failed while tracing was active: " << err;
    EXPECT_EQ(session.get_state(), SessionState::STOPPED);
}

TEST(IntegrationTest, ResumeRecordsInstructionsUntilNormalExit) {
    CompilationOptions opts;
    opts.language = "c";
    opts.optimization = "O0";

    Session session("resume_exit_session",
                    "int main(void) { volatile int value = 1; value += 2; return value == 3 ? 0 : 1; }\n",
                    opts);
    std::string err;
    ASSERT_TRUE(session.start(err)) << "Failed to start session: " << err;
    ASSERT_TRUE(session.resume(err)) << "Resume failed: " << err;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (session.get_state() == SessionState::RUNNING && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_EQ(session.get_state(), SessionState::EXITED);
    const auto events = session.get_recorder().query_events(
        1, session.get_recorder().get_latest_sequence(), "",
        session.get_recorder().get_total_events(), 0);
    bool saw_instruction = false;
    bool saw_exit = false;
    for (const auto& event : events) {
        saw_instruction = saw_instruction || event.type == EventType::SINGLE_STEP_COMPLETED;
        saw_exit = saw_exit || event.type == EventType::PROCESS_EXITED;
    }
    EXPECT_TRUE(saw_instruction);
    EXPECT_TRUE(saw_exit);
    EXPECT_TRUE(session.stop(err)) << "Stop after exit failed: " << err;
}

TEST(TracingEngineTest, SingleStepAfterWaitStatusWasReapedReportsUnavailableStatus) {
    pid_t child = fork();
    if (child == 0) {
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    EXPECT_TRUE(WIFEXITED(status));

    TracingEngine engine(child);
    StopInfo stop;
    bool stepped = engine.single_step(stop);

    EXPECT_FALSE(stepped);
    EXPECT_FALSE(stop.is_terminated);
    EXPECT_EQ(stop.reason, StopReason::ERROR);
    EXPECT_NE(stop.error_message.find("ECHILD"), std::string::npos);
}

TEST(IntegrationTest, EventHistoryIsKeptAfterStop) {
    CompilationOptions opts;
    opts.language = "c";
    opts.optimization = "O0";
    opts.debug_symbols = true;

    std::string source =
        "#include <stdio.h>\n"
        "int main(void) {\n"
        "    int v = 7;\n"
        "    printf(\"v=%d\\n\", v);\n"
        "    return 0;\n"
        "}\n";

    Session session("history_stop_session", source, opts);
    std::string err;
    ASSERT_TRUE(session.start(err)) << "Failed to start session: " << err;

    TraceEvent ev;
    ASSERT_TRUE(session.step(ev, err)) << "Step failed: " << err;
    EXPECT_GT(session.get_recorder().get_total_events(), 0u);

    ASSERT_TRUE(session.stop(err)) << "Stop failed: " << err;
    EXPECT_EQ(session.get_state(), SessionState::STOPPED);
    EXPECT_GT(session.get_recorder().get_total_events(), 0u);
    EXPECT_EQ(session.get_recorder().get_latest_sequence(), session.get_recorder().get_total_events());
}
