cmake_minimum_required(VERSION 3.10)
project(uvzmq VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -march=native -mtune=native")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
endif()

option(UVZMQ_USE_MIMALLOC "Use mimalloc allocator" ON)
option(UVZMQ_BUILD_TESTS "Build tests" ON)
option(UVZMQ_BUILD_BENCHMARKS "Build benchmarks" ON)
option(UVZMQ_BUILD_EXAMPLES "Build examples" ON)

if(UVZMQ_USE_MIMALLOC)
    add_subdirectory(third_party/mimalloc EXCLUDE_FROM_ALL)
    set(MIMALLOC_LIB mimalloc-static)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/mimalloc/include)
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

# Use mimalloc for ZMQ via linking order
if(UVZMQ_USE_MIMALLOC)
    # Link mimalloc first so its symbols override standard malloc/free
    set(ZMQ_MIMALLOC_LINK ${MIMALLOC_LIB})
endif()

add_subdirectory(third_party/zmq EXCLUDE_FROM_ALL)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/libuv/include
    ${CMAKE_SOURCE_DIR}/third_party/zmq/include
)

set(UVZMQ_SOURCES
    src/uvzmq.c
)

set(UVZMQ_HEADERS
    include/uvzmq.h
    include/uvzmq_types.h
    include/uvzmq_features.h
)

add_library(uvzmq STATIC ${UVZMQ_SOURCES} ${UVZMQ_HEADERS})
set_target_properties(uvzmq PROPERTIES LINKER_LANGUAGE C)

if(UVZMQ_USE_MIMALLOC)
    # Link mimalloc BEFORE ZMQ to override malloc/free symbols
    target_link_libraries(uvzmq ${MIMALLOC_LIB} uv_a libzmq-static)
    target_compile_definitions(uvzmq PRIVATE UVZMQ_USE_MIMALLOC=1)
    
    # Use whole archive for mimalloc to ensure symbols are included
    set_target_properties(${MIMALLOC_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    target_link_libraries(uvzmq uv_a libzmq-static)
endif()

target_include_directories(uvzmq PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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