set(CPACK_GENERATOR
    ZIP
    TGZ
    DEB
    RPM
    NSIS
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR}"
    "${PROJECT_NAME}"
    ALL
    /
)
set(CPACK_PROJECT_URL                           "https://github.com/totalspectrum/spin2cpp")
set(CPACK_PACKAGE_VENDOR                        "Eric Smith")
set(CPACK_PACKAGE_CONTACT                       "Eric Smith <FIXME@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY           "Tool to convert Parallax Propeller Spin code to C++ or C")
set(CPACK_RESOURCE_FILE_README                  "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_CMAKE_GENERATOR                       "Unix Makefiles")
set(CPACK_PACKAGE_VERSION_MAJOR                 ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR                 ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH                 ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION
    ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
if (PROJECT_VERSION_TWEAK)
    set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION}.${PROJECT_VERSION_TWEAK})
endif ()

# NSIS Specific
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL  ON)
set(CPACK_NSIS_HELP_LINK                        "${CPACK_PROJECT_URL}")
set(CPACK_NSIS_URL_INFO_ABOUT                   "${CPACK_PROJECT_URL}")
set(CPACK_NSIS_CONTACT                          "${CPACK_PACKAGE_CONTACT}")
set(CPACK_NSIS_INSTALL_ROOT                     C:)
set(CPACK_PACKAGE_INSTALL_DIRECTORY             spin2cpp) # Required because default contains spaces and version number
set(CPACK_NSIS_EXECUTABLES_DIRECTORY            bin)
set(CPACK_PACKAGE_EXECUTABLES
    "${CUSTOM_WIN32_CMAKE_INSTALL_DIR}\\\\bin\\\\spin2cpp.exe" Spin2Cpp)

# Debian Specific
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE               "${CPACK_PROJECT_URL}")
set(CPACK_DEBIAN_PACKAGE_SECTION                devel)
set(CPACK_DEBIAN_PACKAGE_PRIORITY               optional)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS              ON)

# RPM Specific
set(CPACK_RPM_PACKAGE_URL                       "${CPACK_PROJECT_URL}")
set(CPACK_PACKAGE_RELOCATABLE                   ON)

include(CPack)
