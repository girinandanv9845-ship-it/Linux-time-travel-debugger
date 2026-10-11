cmake_minimum_required(VERSION 3.20)

find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)

# Core library sources
set(CORE_SOURCES
    src/common/Types.cpp
    src/process/ProcessManager.cpp
    src/tracing/TracingEngine.cpp
    src/recording/EventRecorder.cpp
    src/checkpoint/CheckpointManager.cpp
    src/replay/ReplayEngine.cpp
    src/session/Session.cpp
    src/api/HttpServer.cpp
)

add_library(ttd_core STATIC ${CORE_SOURCES})

target_include_directories(ttd_core PUBLIC
    include
    third_party/crow
)

target_link_libraries(ttd_core PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Main backend daemon
add_executable(ttd_backend src/main.cpp)
target_link_libraries(ttd_backend PRIVATE ttd_core)

# Tests executable
set(TEST_SOURCES
    tests/test_types.cpp
    tests/test_process.cpp
    tests/test_recording.cpp
    tests/test_checkpoint.cpp
    tests/test_replay.cpp
    tests/test_integration.cpp
)

add_executable(ttd_tests ${TEST_SOURCES})
target_link_libraries(ttd_tests PRIVATE
    ttd_core
    GTest::gtest
    GTest::gtest_main
)

enable_testing()
add_test(NAME AllTests COMMAND ttd_tests)

