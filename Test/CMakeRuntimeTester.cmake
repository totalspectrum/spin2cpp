macro(check_exit_code exit_code)
    if (exit_code)
        string(REPLACE ";" " " CMD_STRING "${ARGN}")
        message(FATAL_ERROR "Error (${exit_code}) running ${CMD_STRING}")
    endif ()
endmacro()

macro(check_and_print_exit_code exit_code stdout)
    if (exit_code)
        message(WARNING "${stdout}")
    endif ()
    check_exit_code(${exit_code} ${ARGN})
endmacro()

set(ENV{PATH} "${PROPGCC_BIN_PATH}:$ENV{PATH}")

set(CMD "${spin2cpp}" -I "${SPIN_LIB_DIR}" --binary --gas -Os -u __serial_exit -o "${spin_file}.binary"
    "${CMAKE_CURRENT_LIST_DIR}/${spin_file}.spin")
execute_process(COMMAND ${CMD}
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE exit_code)
check_and_print_exit_code(${exit_code} "${STDERR}" ${CMD})

set(CMD "${PROPELLER_LOAD}" "${spin_file}.binary" -r -t -q)
execute_process(COMMAND ${CMD}
    OUTPUT_FILE "${spin_file}.out"
    RESULT_VARIABLE exit_code)
check_exit_code(${exit_code} ${CMD})

set(CMD tail --lines=+6 "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.out")
execute_process(COMMAND ${CMD}
    RESULT_VARIABLE exit_code
    OUTPUT_FILE "${spin_file}.txt")
check_exit_code(${exit_code} ${CMD})

set(CMD diff -ub "${CMAKE_CURRENT_LIST_DIR}/Expect/${spin_file}.txt" "${CMAKE_CURRENT_BINARY_DIR}/${spin_file}.txt")
execute_process(COMMAND ${CMD}
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE exit_code)
check_and_print_exit_code(${exit_code} "${STDERR}" ${CMD})
