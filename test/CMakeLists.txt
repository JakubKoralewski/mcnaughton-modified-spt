include_directories(${TEST_SOURCE_DIR}/src ${TEST_SOURCE_DIR}/test)

add_executable(mcn_test mcnaughton.c)
add_test(MCN mcn_test)

add_executable(spt_test spt.c)
add_test(SPT spt_test)

add_executable(input_file input_file.c)
add_test(input_file input_file WORKING_DIRECTORY ${TEST_SOURCE_DIR})
IF (NOT WIN32)
    target_link_libraries(mcn_test PRIVATE m)
    target_link_libraries(spt_test PRIVATE m)
    target_link_libraries(input_file PRIVATE m)
ENDIF()
target_compile_definitions(input_file PUBLIC TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/")
