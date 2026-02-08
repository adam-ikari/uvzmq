cmake_minimum_required(VERSION 3.10)
project(uvzmq VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -march=native -mtune=native")

option(UVZMQ_BUILD_TESTS "Build tests" ON)
option(UVZMQ_BUILD_BENCHMARKS "Build benchmarks" ON)
option(UVZMQ_BUILD_EXAMPLES "Build examples" ON)
option(UVZMQ_ENABLE_COVERAGE "Enable code coverage" OFF)
option(UVZMQ_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(UVZMQ_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
endif()

# Remove -Werror for sanitizer builds to avoid treating warnings as errors
if(UVZMQ_ENABLE_ASAN OR UVZMQ_ENABLE_TSAN)
    string(REPLACE "-Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Code coverage
if(UVZMQ_ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# AddressSanitizer
if(UVZMQ_ENABLE_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# ThreadSanitizer
if(UVZMQ_ENABLE_TSAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

# Check for mutually exclusive sanitizers
if(UVZMQ_ENABLE_ASAN AND UVZMQ_ENABLE_TSAN)
    message(FATAL_ERROR "ASAN and TSAN cannot be enabled together")
endif()

# gtest
if(UVZMQ_BUILD_TESTS)
    add_subdirectory(third_party/gtest EXCLUDE_FROM_ALL)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/gtest/googletest/include)
endif()

# libuv
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/libuv EXCLUDE_FROM_ALL)

# libzmq
set(ENABLE_CURVE OFF CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/zmq EXCLUDE_FROM_ALL)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/libuv/include
    ${CMAKE_SOURCE_DIR}/third_party/zmq/include
)

if(UVZMQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(UVZMQ_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(UVZMQ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()