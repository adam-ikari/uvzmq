enable_testing()

add_executable(test_uvzmq test_uvzmq.cpp)
target_link_libraries(test_uvzmq uvzmq gtest_main gtest pthread)
add_test(NAME test_uvzmq COMMAND test_uvzmq)

add_executable(e2e_test e2e_test.cpp)
target_link_libraries(e2e_test uvzmq gtest_main gtest pthread)
add_test(NAME e2e_test COMMAND e2e_test)