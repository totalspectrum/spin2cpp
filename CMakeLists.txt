cmake_minimum_required(VERSION 3.0)

if (CROSS STREQUAL win32)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/CMakeModules/Toolchain-mingw64.cmake)
elseif (CROSS STREQUAL rpi)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/CMakeModules/Toolchain-arm-linux.cmake)
endif ()

project(spin2cpp
    VERSION 1.96.0)

find_package(BISON REQUIRED)

# Compilation flags
add_compile_options(-Wall)
include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Set source files
set(PASMBACK
    ${PROJECT_SOURCE_DIR}/backends/asm/outasm.c
    ${PROJECT_SOURCE_DIR}/backends/asm/assemble_ir.c
    ${PROJECT_SOURCE_DIR}/backends/asm/optimize_ir.c
    ${PROJECT_SOURCE_DIR}/backends/asm/inlineasm.c)
set(CPPBACK
    ${PROJECT_SOURCE_DIR}/backends/cpp/outcpp.c
    ${PROJECT_SOURCE_DIR}/backends/cpp/cppfunc.c
    ${PROJECT_SOURCE_DIR}/backends/cpp/outgas.c
    # ${PROJECT_SOURCE_DIR}/backends/cpp/cppexpr.c
    )
set(SPIN_SOURCES
    ${PROJECT_SOURCE_DIR}/spinc.c
    ${PROJECT_SOURCE_DIR}/functions.c
    ${PROJECT_SOURCE_DIR}/pasm.c
    ${PROJECT_SOURCE_DIR}/backends/dat/outdat.c
    spin.tab.c
    ${PASMBACK}
    ${CPPBACK})
set(UTIL
    ${PROJECT_SOURCE_DIR}/util/dofmt.c
    ${PROJECT_SOURCE_DIR}/util/flexbuf.c
    ${PROJECT_SOURCE_DIR}/util/lltoa_prec.c
    ${PROJECT_SOURCE_DIR}/util/strupr.c
    ${PROJECT_SOURCE_DIR}/util/strrev.c)
set(LEX_SOURCES
    ${PROJECT_SOURCE_DIR}/lexer.c
    ${PROJECT_SOURCE_DIR}/symbol.c
    ${PROJECT_SOURCE_DIR}/ast.c
    ${PROJECT_SOURCE_DIR}/expr.c
    ${PROJECT_SOURCE_DIR}/preprocess.c
    ${PROJECT_SOURCE_DIR}/backends/cpp/cppexpr.c # FIXME lexer should not need cppexpr.c (it belongs in CPPBACK)
    ${UTIL})
add_custom_command(OUTPUT spin.tab.c spin.tab.h
    COMMAND ${BISON_EXECUTABLE} -t -b spin -d "${CMAKE_CURRENT_LIST_DIR}/spin.y"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/spin.y")

# Make sure spin.tab.h is generated prior to compiling any other sources
add_custom_target(spin_tab
    DEPENDS spin.tab.h spin.tab.c)
add_library(lex ${LEX_SOURCES})
target_link_libraries(lex m)
add_dependencies(lex spin_tab)

add_library(spin ${SPIN_SOURCES})
target_link_libraries(spin lex)

add_executable(spin2cpp ${PROJECT_SOURCE_DIR}/spin2cpp.c)
target_link_libraries(spin2cpp spin)

add_executable(fastspin ${PROJECT_SOURCE_DIR}/fastspin.c)
target_link_libraries(fastspin spin)

# Installation rules
install(TARGETS spin2cpp fastspin
    DESTINATION bin)
install(FILES
    README.md
    COPYING
    Changelog.txt
    DESTINATION share/doc/${PROJECT_NAME})

# Add unit tests
enable_testing()
add_subdirectory(Test)

# Packaging rules
include(CMakeCPack.cmake)
