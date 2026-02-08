add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark uv_a libzmq-static pthread dl)

add_executable(quick_benchmark quick_benchmark.cpp)
target_link_libraries(quick_benchmark uv_a libzmq-static pthread dl)

add_executable(zmq_benchmark zmq_benchmark.cpp)
target_link_libraries(zmq_benchmark libzmq-static pthread)