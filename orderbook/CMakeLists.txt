cmake_minimum_required(VERSION 3.16)
project(orderbook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(include tests)
find_package(Threads REQUIRED)
enable_testing()

# ---- M1 ----
add_executable(test_engine_ref  tests/test_engine.cpp)
target_include_directories(test_engine_ref  PRIVATE reference)
add_test(NAME m1_reference COMMAND test_engine_ref)

add_executable(test_engine_skel tests/test_engine.cpp)   # your code (not a required test)
target_include_directories(test_engine_skel PRIVATE skeleton)

# ---- M2 ----
add_executable(test_engine_m2   tests/test_engine_m2.cpp)
target_include_directories(test_engine_m2   PRIVATE reference)
add_test(NAME m2_reference COMMAND test_engine_m2)

# ---- M3 ----
add_executable(test_spsc        tests/test_spsc.cpp)
target_include_directories(test_spsc        PRIVATE reference)
target_link_libraries(test_spsc PRIVATE Threads::Threads)
add_test(NAME m3_spsc COMMAND test_spsc)

# ---- M4 / tools ----
add_executable(benchmark bench/benchmark.cpp)
target_include_directories(benchmark PRIVATE reference)
target_compile_options(benchmark PRIVATE -O2 -DNDEBUG)   # a latency harness at -O0 is meaningless

# NOTE: sanitizers (ASan/UBSan/TSan) and skeleton grading live in build.sh, not here.
# This CMake path builds the reference targets, benchmark and CLI only.
add_executable(cli tools/cli.cpp)
target_include_directories(cli PRIVATE reference)

# Build:  cmake -S . -B build && cmake --build build -j && ctest --test-dir build
