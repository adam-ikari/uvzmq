# UVZMQ Unit Tests

# Enable testing
enable_testing()

# Test 1: uvzmq_socket_new basic functionality
add_executable(test_uvzmq_socket_new test_uvzmq_socket_new.cpp)
target_link_libraries(test_uvzmq_socket_new
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_socket_new COMMAND test_uvzmq_socket_new)

# Test 2: uvzmq_socket_close functionality
add_executable(test_uvzmq_socket_close test_uvzmq_socket_close.cpp)
target_link_libraries(test_uvzmq_socket_close
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_socket_close COMMAND test_uvzmq_socket_close)

# Test 3: uvzmq_socket_free functionality
add_executable(test_uvzmq_socket_free test_uvzmq_socket_free.cpp)
target_link_libraries(test_uvzmq_socket_free
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_socket_free COMMAND test_uvzmq_socket_free)

# Test 4: getter functions
add_executable(test_uvzmq_getters test_uvzmq_getters.cpp)
target_link_libraries(test_uvzmq_getters
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_getters COMMAND test_uvzmq_getters)

# Test 5: error handling
add_executable(test_uvzmq_error_handling test_uvzmq_error_handling.cpp)
target_link_libraries(test_uvzmq_error_handling
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_error_handling COMMAND test_uvzmq_error_handling)

# Test 6: integration test (full workflow)
add_executable(test_uvzmq_integration test_uvzmq_integration.cpp)
target_link_libraries(test_uvzmq_integration
    gtest
    gtest_main
    uv_a
    ${CMAKE_BINARY_DIR}/third_party/zmq/lib/libzmq.a
    pthread
    stdc++
)

add_test(NAME test_uvzmq_integration COMMAND test_uvzmq_integration)