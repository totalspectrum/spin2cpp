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

set(CMD "${spin2cpp}" -I "${SPIN_LIB_DIR}" ${extra_args} --noheader -D COUNT=4 "${CMAKE_CURRENT_LIST_DIR}/${spin_file}.spin")
execute_process(COMMAND ${CMD}
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE exit_code)
check_and_print_exit_code(${exit_code} "${STDERR}" ${CMD})

set(CMD diff -ub ${CMAKE_CURRENT_LIST_DIR}/Expect/${spin_file}.h ${spin_file}.h)
execute_process(COMMAND ${CMD}
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE exit_code)
check_and_print_exit_code(${exit_code} "${STDERR}" ${CMD})

set(CMD diff -ub "${CMAKE_CURRENT_LIST_DIR}/Expect/${spin_file}.${source_ext}" "${spin_file}.${source_ext}")
execute_process(COMMAND ${CMD}
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE exit_code)
check_and_print_exit_code(${exit_code} "${STDERR}" ${CMD})
