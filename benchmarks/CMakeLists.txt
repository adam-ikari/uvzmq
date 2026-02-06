add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark uvzmq pthread)

add_executable(zmq_benchmark zmq_benchmark.cpp)
target_link_libraries(zmq_benchmark libzmq-static pthread)