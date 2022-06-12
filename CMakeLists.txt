cmake_minimum_required(VERSION 3.22)
project(queue)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS NO)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
enable_testing()

option(QUEUE_ENABLE_TESTING "enable testing using googletest" ON)
option(QUEUE_ENABLE_BENCHMARK "enable benchmarking using googlebenchmark" ON)

if(QUEUE_ENABLE_TESTING)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG main)
    if(WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(googletest)
endif()

if(QUEUE_ENABLE_BENCHMARK)
    FetchContent_Declare(googlebenchmark GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG master) 
    set(BENCHMARK_ENABLE_TESTING off)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_library(queue INTERFACE)
target_include_directories(queue INTERFACE include)

target_sources(queue INTERFACE
    include/queue/queue.h
)

if(QUEUE_ENABLE_TESTING)
    add_executable(queue_test
        test/queue.cpp
    )

    add_test(queue_test COMMAND queue_test)
    target_link_libraries(queue_test queue gtest_main)
    target_include_directories(queue_test PRIVATE test)
endif()

if(QUEUE_ENABLE_BENCHMARK)
    add_executable(queue_benchmark
        benchmark/queue.cpp
    )

    target_link_libraries(queue_benchmark queue benchmark::benchmark)
    target_include_directories(queue_benchmark PRIVATE benchmark)
endif()
