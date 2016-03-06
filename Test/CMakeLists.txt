enable_testing()
add_custom_target(test-all COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
add_dependencies(test-all spin2cpp)

# Find propeller-load
find_program(PROPELLER_LOAD propeller-load
    PATHS /opt/parallax/bin)
if (NOT PROPELLER_LOAD)
    message(FATAL_ERROR "Can not find propeller-load")
endif ()

# Find PropGCC (and store its directory, since we need to add it to PATH)
find_program(PROPGCC propeller-elf-gcc
    PATHS /opt/parallax/bin)
if (NOT PROPGCC)
    message(FATAL_ERROR "Can not find PropGCC")
endif ()
get_filename_component(PROPGCC_BIN_PATH "${PROPGCC}" DIRECTORY)

function(add_test_target target)
    add_custom_target(test-${target}
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ${target}
        DEPENDS ${ARGN})
endfunction()

function(add_test_script name)
    add_test(NAME ${name}
        COMMAND "${CMAKE_CURRENT_LIST_DIR}/${name}.sh" $<TARGET_FILE:spin2cpp>
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
    add_test_target(${name} ${ARGN})
endfunction()

function(add_runtime_test spin_file)
    add_test(NAME ${spin_file}
        COMMAND "${CMAKE_COMMAND}"
            -D "PROPELLER_LOAD=${PROPELLER_LOAD}"
            -D "spin_file=${spin_file}"
            -D "spin2cpp=$<TARGET_FILE:spin2cpp>"
            -D "PROPGCC_BIN_PATH=${PROPGCC_BIN_PATH}"
            -D "SPIN_LIB_DIR=${PROJECT_SOURCE_DIR}/Lib"
            -P "${CMAKE_CURRENT_LIST_DIR}/CMakeRuntimeTester.cmake")
    set(GENERATED_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.out
        ${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.txt
        ${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.binary
        ${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.h
        ${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.h
        ${CMAKE_CURRENT_BINARY_DIR}/dattest.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/dattest.h
    )
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${GENERATED_FILES}")
    add_test_target(${spin_file} spin2cpp)
endfunction()

# Unit tests
add_test_script(cpptests spin2cpp)
add_test_script(errtests spin2cpp)

# Lexer tests
add_executable(testlex ${PROJECT_SOURCE_DIR}/testlex)
target_link_libraries(testlex lex m)
add_dependencies(test-all testlex)
add_test(NAME lextest
    COMMAND testlex)
add_test_target(lextest testlex)

# Integration tests
add_runtime_test(exec01)
add_runtime_test(exec02)
add_runtime_test(exec03)
add_runtime_test(exec04)
add_runtime_test(exec05)
add_runtime_test(exec06)
add_runtime_test(exec07)
add_runtime_test(exec08)
add_runtime_test(exec09)
add_runtime_test(exec10)
add_runtime_test(exec11)
add_runtime_test(exec12)
add_runtime_test(exec13)
