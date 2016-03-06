set(CMD "${spin2cpp}" -I "${SPIN_LIB_DIR}" ${extra_args} --noheader -D COUNT=4 "${CMAKE_CURRENT_LIST_DIR}/${spin_file}.spin")
execute_process(COMMAND ${CMD}
    OUTPUT_VARIABLE ACTUAL_OUTPUT
    ERROR_VARIABLE ACTUAL_OUTPUT)

file(READ "${CMAKE_CURRENT_LIST_DIR}/Expect/${spin_file}.err" EXPECTED_OUTPUT)

if (NOT EXPECTED_OUTPUT STREQUAL ACTUAL_OUTPUT)
    message(FATAL_ERROR "***Expected:\n`${EXPECTED_OUTPUT}`\n***Actual:\n`${ACTUAL_OUTPUT}`")
endif ()
