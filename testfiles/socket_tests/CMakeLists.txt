# SPDX-License-Identifier: GPL-2.0-or-later
# -----------------------------------------------------------------------------
# Socket Protocol Tests
# Tests for the Inkscape socket server protocol implementation

# Socket protocol test sources
set(SOCKET_TEST_SOURCES
    test_socket_protocol
    test_socket_commands
    test_socket_responses
    test_socket_handshake
    test_socket_integration
)

# Add socket protocol tests to the main test suite
foreach(test_source ${SOCKET_TEST_SOURCES})
    string(REPLACE "_test" "" testname "test_${test_source}")
    add_executable(${testname} ${test_source}.cpp)
    target_include_directories(${testname} SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
    target_link_libraries(${testname} cpp_test_static_library 2Geom::2geom)
    add_test(NAME ${testname} COMMAND ${testname})
    set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${INKSCAPE_TEST_PROFILE_DIR_ENV}/${testname};${CMAKE_CTEST_ENV}")
    add_dependencies(tests ${testname})
endforeach()

# Add socket tests to the main tests target
add_dependencies(tests ${SOCKET_TEST_SOURCES}) 