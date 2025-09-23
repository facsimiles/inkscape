# SPDX-License-Identifier: GPL-2.0-or-later
#
# Setup for unit tests.
add_custom_target(unit_tests)

function(make_target_unit_testable target_name)
    target_compile_definitions(${target_name} PRIVATE "-D_GLIBCXX_ASSERTIONS")
    target_compile_options(${target_name} PRIVATE "-fsanitize=address" "-fno-omit-frame-pointer" "-UNDEBUG")
    target_link_options(${target_name} PRIVATE "-fsanitize=address")
endfunction()

# Add a unit test as follows:
# add_unit_test(name-of-my-test TEST_SOURCE foo-test.cpp [SOURCES foo.cpp ...] [EXTRA_LIBS ...])
function(add_unit_test test_name)
    set(MULTI_VALUE_ARGS "SOURCES" "EXTRA_LIBS")
    cmake_parse_arguments(ARG "UNUSED_OPTIONS" "TEST_SOURCE" "${MULTI_VALUE_ARGS}" ${ARGN})
    foreach(source_file ${ARG_SOURCES})
        list(APPEND test_sources "${CMAKE_SOURCE_DIR}/src/${source_file}")
    endforeach()

    if(EXISTS "${CMAKE_SOURCE_DIR}/testfiles/src/${test_name}.cpp")
        list(APPEND test_sources "${CMAKE_SOURCE_DIR}/testfiles/src/${test_name}.cpp")
    else()
        if (ARG_TEST_SOURCE)
            if (EXISTS "${CMAKE_SOURCE_DIR}/testfiles/src/${ARG_TEST_SOURCE}")
                list(APPEND test_sources "${CMAKE_SOURCE_DIR}/testfiles/src/${ARG_TEST_SOURCE}")
            else()
                message(WARNING "'${CMAKE_SOURCE_DIR}/testfiles/src/${ARG_TEST_SOURCE}' not found")
            endif()
        else()
            message(WARNING "'${CMAKE_SOURCE_DIR}/testfiles/src/${test_name}.cpp' not found")
        endif()
    endif()

    add_executable(${test_name} ${test_sources})
    target_include_directories(${test_name} SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
    set_target_properties(${test_name} PROPERTIES LINKER_LANGUAGE CXX)

    make_target_unit_testable(${test_name})

    target_link_libraries(${test_name} GTest::gtest GTest::gmock GTest::gmock_main ${ARG_EXTRA_LIBS})
    add_test(NAME ${test_name} COMMAND ${test_name})
    add_dependencies(unit_tests ${test_name} ${ARG_EXTRA_LIBS})
endfunction(add_unit_test)

function(add_unit_tests)
    set(MULTI_VALUE_ARGS "TEST_SOURCES" "SOURCES" "EXTRA_LIBS")
    cmake_parse_arguments(ARG "UNUSED_OPTIONS" "" "${MULTI_VALUE_ARGS}" ${ARGN})

    foreach(testsource ${ARG_TEST_SOURCES})
        # Build a testname from the testsource filename
        string(REPLACE "/" "-" testname "${testsource}")
        get_filename_component(testname "${testname}" NAME_WE)
        string(REPLACE "_" "-" testname "${testname}")
        add_unit_test(${testname} TEST_SOURCE "${testsource}"
                                  SOURCES ${ARG_SOURCES}
                                  EXTRA_LIBS ${ARG_EXTRA_LIBS})
    endforeach()

endfunction(add_unit_tests)
