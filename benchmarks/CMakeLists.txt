add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark ${CMAKE_SOURCE_DIR}/build/third_party/libuv/libuv.a ${CMAKE_SOURCE_DIR}/build/third_party/zmq/lib/libzmq.a pthread dl)

add_executable(quick_benchmark quick_benchmark.cpp)
target_link_libraries(quick_benchmark ${CMAKE_SOURCE_DIR}/build/third_party/libuv/libuv.a ${CMAKE_SOURCE_DIR}/build/third_party/zmq/lib/libzmq.a pthread dl)

add_executable(zmq_benchmark zmq_benchmark.cpp)
target_link_libraries(zmq_benchmark libzmq-static pthread)