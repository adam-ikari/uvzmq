if(UVZMQ_BUILD_EXAMPLES)
    # Include directory for header-only library
    include_directories(${CMAKE_SOURCE_DIR}/include)
    
    add_executable(simple simple.c)
    target_link_libraries(simple uv_a libzmq-static)

    add_executable(test_allocator test_allocator.c)
    target_link_libraries(test_allocator uv_a libzmq-static)

    add_executable(quick_test quick_test.c)
    target_link_libraries(quick_test uv_a libzmq-static)

    add_executable(pub_sub pub_sub.c)
    target_link_libraries(pub_sub uv_a libzmq-static)

    add_executable(test_zmq_pubsub test_zmq_pubsub.c)
    target_link_libraries(test_zmq_pubsub libzmq-static)

    add_executable(multi_thread multi_thread.c)
    target_link_libraries(multi_thread uv_a libzmq-static pthread)

    add_executable(test_push_pull test_push_pull.c)
    target_link_libraries(test_push_pull uv_a libzmq-static pthread)

    add_executable(test_push_pull_event test_push_pull_event.c)
    target_link_libraries(test_push_pull_event uv_a libzmq-static pthread)

    add_executable(test_loop_modes test_loop_modes.c)
    target_link_libraries(test_loop_modes uv_a libzmq-static pthread)

    add_executable(test_loop_simple test_loop_simple.c)
    target_link_libraries(test_loop_simple uv_a libzmq-static pthread)

    add_executable(test_minimal test_minimal.c)
    target_link_libraries(test_minimal uv_a libzmq-static pthread)

    add_executable(test_perf_analysis test_perf_analysis.c)
    target_link_libraries(test_perf_analysis uv_a libzmq-static pthread)

    add_executable(test_zmq_perf test_zmq_perf.c)
    target_link_libraries(test_zmq_perf ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a pthread stdc++)

    add_executable(best_practices best_practices.c)
    target_link_libraries(best_practices uv_a libzmq-static pthread)

    add_executable(test_simple test_simple.c)
    target_link_libraries(test_simple uv_a libzmq-static pthread)

    add_executable(test_uv_only test_uv_only.c)
    target_link_libraries(test_uv_only uv_a)

    add_executable(test_client test_client.c)
    target_link_libraries(test_client libzmq-static pthread)

    add_executable(test_poll test_poll.c)
    target_link_libraries(test_poll uv_a libzmq-static pthread)

    add_executable(test_uvzmq test_uvzmq.c)
    target_link_libraries(test_uvzmq uv_a libzmq-static pthread)

    add_executable(test_loop test_loop.c)
    target_link_libraries(test_loop uv_a)

    add_executable(test_zmq test_zmq.c)
    target_link_libraries(test_zmq libzmq-static pthread)

    add_executable(test_simple2 test_simple2.c)
    target_link_libraries(test_simple2 uv_a libzmq-static pthread)

    add_executable(test_bp test_bp.c)
    target_link_libraries(test_bp uv_a libzmq-static pthread)

    add_executable(test_bp2 test_bp2.c)
    target_link_libraries(test_bp2 uv_a libzmq-static pthread)

    add_executable(test_bp3 test_bp3.c)
    target_link_libraries(test_bp3 uv_a libzmq-static pthread)

    add_executable(test_bp4 test_bp4.c)
    target_link_libraries(test_bp4 uv_a libzmq-static pthread)

    add_executable(bench_test bench_test.c)
    target_link_libraries(bench_test uv_a libzmq-static pthread)
endif()