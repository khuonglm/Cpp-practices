cmake_minimum_required(VERSION 3.20)
project(cpp_quant_prep LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(QUANT_ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(QUANT_ENABLE_TSAN       "Enable ThreadSanitizer" OFF)

if(QUANT_ENABLE_ASAN_UBSAN AND QUANT_ENABLE_TSAN)
    message(FATAL_ERROR "ASan/UBSan and TSan cannot be enabled simultaneously.")
endif()

find_package(Threads REQUIRED)

add_library(quant_warnings INTERFACE)
if(MSVC)
    target_compile_options(quant_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(quant_warnings INTERFACE
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -fno-omit-frame-pointer
    )
endif()

if(QUANT_ENABLE_ASAN_UBSAN AND NOT MSVC)
    target_compile_options(quant_warnings INTERFACE -fsanitize=address,undefined)
    target_link_options(quant_warnings INTERFACE -fsanitize=address,undefined)
endif()

if(QUANT_ENABLE_TSAN AND NOT MSVC)
    target_compile_options(quant_warnings INTERFACE -fsanitize=thread)
    target_link_options(quant_warnings INTERFACE -fsanitize=thread)
endif()

include(CTest)
enable_testing()

function(add_quant_task TARGET_NAME SUBDIR TIER_LABEL)
    add_executable(${TARGET_NAME} "tasks/${SUBDIR}/test.cpp")
    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/tasks"
        "${CMAKE_CURRENT_SOURCE_DIR}/tasks/${SUBDIR}"
    )
    target_link_libraries(${TARGET_NAME} PRIVATE quant_warnings Threads::Threads)
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        LABELS "${TIER_LABEL}"
        TIMEOUT 30
    )
endfunction()

# Tier 1: Highest Payoff (2-3 Hour Budget)
add_quant_task(test_01_limit_order_book                01_limit_order_book                tier1)
add_quant_task(test_02_spsc_ring_buffer                02_spsc_ring_buffer                tier1)
add_quant_task(test_03_smart_pointers                  03_smart_pointers                  tier1)
add_quant_task(test_04_pool_and_arena                  04_pool_and_arena                  tier1)

# Tier 2: Core Systems (5-6 Hour Budget)
add_quant_task(test_05_rolling_statistics              05_rolling_statistics              tier2)
add_quant_task(test_06_thread_pool                     06_thread_pool                     tier2)
add_quant_task(test_07_vector_from_scratch             07_vector_from_scratch             tier2)
add_quant_task(test_08_market_data_parser              08_market_data_parser              tier2)

# Tier 3: Complete Mastery (8+ Hour Budget)
add_quant_task(test_09_option_pricing                  09_option_pricing                  tier3)
add_quant_task(test_10_lru_cache                       10_lru_cache                       tier3)
add_quant_task(test_11_blocking_queue                  11_blocking_queue                  tier3)
add_quant_task(test_12_memory_layout_and_false_sharing 12_memory_layout_and_false_sharing tier3)
add_quant_task(test_13_sbo_function                    13_sbo_function                    tier3)
add_quant_task(test_14_async_logger                    14_async_logger                    tier3)
