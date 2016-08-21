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

macro(add_files_to_clean)
    set(TEMP_LIST ${ARGN})  # Not sure why this is needed, but using ARGN directly in foreach doesn't work
    # Only save new files in the "output list" variable and add to the clean target
    foreach (file_path IN LISTS TEMP_LIST)
        list(FIND FILES_GENERATED_IN_DIRECTORY "${file_path}" INDEX)
        if ("-1" STREQUAL INDEX)
            list(APPEND FILES_GENERATED_IN_DIRECTORY "${file_path}")
        endif ()
    endforeach ()
    set(FILES_GENERATED_IN_DIRECTORY ${FILES_GENERATED_IN_DIRECTORY} PARENT_SCOPE)
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${FILES_GENERATED_IN_DIRECTORY}")
endmacro()

function(add_scripted_test script spin_file label source_ext)
    add_test(NAME ${spin_file}
        COMMAND "${CMAKE_COMMAND}"
            -D "spin_file=${spin_file}"
            -D "spin2cpp=$<TARGET_FILE:spin2cpp>"
            -D "SPIN_LIB_DIR=${PROJECT_SOURCE_DIR}/Lib"
            -D "extra_args=${ARGN}"
            -D "source_ext=${source_ext}"
            -P "${script}")
    add_files_to_clean("${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.${source_ext}"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.spin"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.h"
        "${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.h")
    add_test_target("${spin_file}" spin2cpp)
    set_tests_properties(${spin_file} PROPERTIES LABELS "${label}")
endfunction()

function(add_integration_test spin_file label source_ext)
    add_scripted_test("${CMAKE_CURRENT_LIST_DIR}/CMakeIntegrationTester.cmake"
        "${spin_file}"
        "${label}"
        "${source_ext}"
        ${ARGN})
endfunction()

function(add_error_test spin_file label source_ext)
    add_scripted_test("${CMAKE_CURRENT_LIST_DIR}/CMakeErrorTester.cmake"
        "${spin_file}"
        "${label}"
        "${source_ext}"
        ${ARGN})
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
    add_files_to_clean("${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.out"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.binary"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.h"
        "${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/FullDuplexSerial.h"
        "${CMAKE_CURRENT_BINARY_DIR}/dattest.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/dattest.h")
    add_test_target(${spin_file} spin2cpp)
    set_tests_properties(${spin_file} PROPERTIES LABELS runtimetests)
endfunction()

function(c_test file)
    add_integration_test("${file}" ctests c --ccode)
endfunction()
c_test(ctest01)
c_test(ctest02)

function(cpp_test file)
    add_integration_test("${file}" cpptests cpp -n)
endfunction()
cpp_test(test01)
cpp_test(test02)
cpp_test(test03)
cpp_test(test04)
cpp_test(test05)
cpp_test(test06)
cpp_test(test07)
cpp_test(test08)
cpp_test(test09)
cpp_test(test10)
cpp_test(test11)
cpp_test(test12)
cpp_test(test13)
cpp_test(test14)
cpp_test(test15)
cpp_test(test16)
cpp_test(test17)
cpp_test(test18)
cpp_test(test19)
cpp_test(test20)
cpp_test(test21)
cpp_test(test22)
cpp_test(test23)
cpp_test(test24)
cpp_test(test25)
cpp_test(test26)
cpp_test(test27)
cpp_test(test28)
cpp_test(test29)
cpp_test(test30)
cpp_test(test31)
cpp_test(test32)
cpp_test(test33)
cpp_test(test34)
cpp_test(test35)
cpp_test(test36)
cpp_test(test37)
cpp_test(test38)
cpp_test(test39)
cpp_test(test40)
cpp_test(test41)
cpp_test(test42)
cpp_test(test43)
cpp_test(test44)
cpp_test(test45)
cpp_test(test46)
cpp_test(test47)
cpp_test(test48)
cpp_test(test49)
cpp_test(test50)
cpp_test(test51)
cpp_test(test52)
cpp_test(test53)
cpp_test(test54)
cpp_test(test55)
cpp_test(test56)
cpp_test(test57)
cpp_test(test58)
cpp_test(test59)
cpp_test(test60)
cpp_test(test61)
cpp_test(test62)
cpp_test(test63)
cpp_test(test64)
cpp_test(test65)
cpp_test(test66)
cpp_test(test67)
cpp_test(test68)
cpp_test(test69)
cpp_test(test70)
cpp_test(test71)
cpp_test(test72)
cpp_test(test73)
cpp_test(test74)
cpp_test(test75)
cpp_test(test76)
cpp_test(test77)
cpp_test(test78)
cpp_test(test79)
cpp_test(test80)
cpp_test(test81)
cpp_test(test82)
cpp_test(test83)
cpp_test(test84)
cpp_test(test85)
cpp_test(test86)
cpp_test(test87)
cpp_test(test88)
cpp_test(test89)
cpp_test(test90)
cpp_test(test91)
cpp_test(test92)
cpp_test(test93)
cpp_test(test94)
cpp_test(test95)
cpp_test(test96)
cpp_test(test97)
cpp_test(test98)
cpp_test(test99)
cpp_test(test100)
cpp_test(test101)
cpp_test(test102)
cpp_test(test103)
cpp_test(test104)
cpp_test(test105)
cpp_test(test106)
cpp_test(test107)
cpp_test(test108)
cpp_test(test109)
cpp_test(test110)
cpp_test(test111)
cpp_test(test112)
cpp_test(test113)
cpp_test(test114)
cpp_test(test115)
cpp_test(test116)
cpp_test(test117)
cpp_test(test118)
cpp_test(test119)
cpp_test(test120)
cpp_test(test121)
cpp_test(test122)
cpp_test(test123)
cpp_test(test124)
cpp_test(test125)

# Debug tests
function(debug_test file)
    add_integration_test("${file}" debugtests cpp -g)
endfunction()
debug_test(gtest01)
debug_test(gtest02)

# Error tests
function(error_test file)
    add_error_test("${file}" errortests cpp)
endfunction()
error_test(error01)
error_test(error02)
error_test(error03)  # FIXME: Included file still fails with absolute vs relative path difference
error_test(error04)
error_test(error05)
error_test(error06)

# Lexer tests
add_executable(testlex "${PROJECT_SOURCE_DIR}/testlex.c")
target_link_libraries(testlex lex)
add_dependencies(test-all testlex)
add_test(NAME lextest
    COMMAND testlex)
add_test_target(lextest testlex)

# Runtime tests
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
