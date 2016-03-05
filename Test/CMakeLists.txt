enable_testing()
add_custom_target(test-all COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

add_test(NAME cpptests
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/cpptests.sh" $<TARGET_FILE:spin2cpp>
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
add_test(NAME errtests
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/errtests.sh" $<TARGET_FILE:spin2cpp>
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
add_test(NAME runtests
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/runtests.sh" $<TARGET_FILE:spin2cpp>
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
add_dependencies(test-all spin2cpp)

add_executable(testlex ${PROJECT_SOURCE_DIR}/testlex)
target_link_libraries(testlex lex_common m)
add_dependencies(test-all testlex)
add_test(NAME lextest
    COMMAND testlex)
