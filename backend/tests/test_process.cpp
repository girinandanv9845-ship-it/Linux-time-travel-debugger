#include <gtest/gtest.h>
#include "process/ProcessManager.hpp"
#include <filesystem>

using namespace ttd;
namespace fs = std::filesystem;

TEST(ProcessManagerTest, CompileValidCSource) {
    ProcessManager pm;
    CompilationOptions opts;
    opts.language = "c";
    opts.optimization = "O0";
    opts.debug_symbols = true;

    std::string source = "#include <stdio.h>\nint main(void) { printf(\"hello\\n\"); return 0; }\n";
    auto result = pm.compile("test_valid_sess", source, opts);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(fs::exists(result.binary_path));

    pm.cleanup_session_dir("test_valid_sess");
}

TEST(ProcessManagerTest, RejectInvalidSyntaxWithStderrDiagnostics) {
    ProcessManager pm;
    CompilationOptions opts;
    opts.language = "c";

    std::string bad_source = "int main(void) { deliberate_syntax_error ;; ;; return 0; }\n";
    auto result = pm.compile("test_invalid_sess", bad_source, opts);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.exit_code, 0);
    EXPECT_FALSE(result.stderr_output.empty());

    pm.cleanup_session_dir("test_invalid_sess");
}

TEST(ProcessManagerTest, RejectEmptySource) {
    ProcessManager pm;
    CompilationOptions opts;

    auto result = pm.compile("test_empty_sess", "", opts);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}
