add_executable(simple simple.c)
target_link_libraries(simple uvzmq)

add_executable(test_allocator test_allocator.c)
target_link_libraries(test_allocator uvzmq)

add_executable(quick_test quick_test.c)
target_link_libraries(quick_test uvzmq)

add_executable(pub_sub pub_sub.c)
target_link_libraries(pub_sub uvzmq)

add_executable(test_zmq_pubsub test_zmq_pubsub.c)
target_link_libraries(test_zmq_pubsub libzmq-static)

add_executable(multi_thread multi_thread.c)
target_link_libraries(multi_thread uvzmq pthread)

add_executable(test_push_pull test_push_pull.c)
target_link_libraries(test_push_pull uvzmq pthread)

add_executable(test_push_pull_event test_push_pull_event.c)
target_link_libraries(test_push_pull_event uvzmq pthread)

add_executable(test_loop_modes test_loop_modes.c)
target_link_libraries(test_loop_modes uvzmq pthread)

add_executable(test_loop_simple test_loop_simple.c)
target_link_libraries(test_loop_simple uvzmq pthread)

add_executable(test_minimal test_minimal.c)
target_link_libraries(test_minimal uvzmq pthread)

add_executable(test_perf_analysis test_perf_analysis.c)
target_link_libraries(test_perf_analysis uvzmq pthread)

add_executable(test_zmq_perf test_zmq_perf.c)
target_link_libraries(test_zmq_perf ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a pthread stdc++)

add_executable(bench_test bench_test.c)
target_link_libraries(bench_test uvzmq pthread)