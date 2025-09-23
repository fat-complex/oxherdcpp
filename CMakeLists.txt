cmake_minimum_required(VERSION 3.20)
project(
    oxherdcpp
    VERSION 1.0.0
    DESCRIPTION "C++ actor fan library"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(tools)
include(CPM)

option(ACTOR_BUILD_TESTS "Build tests" OFF)
option(ACTOR_BUILD_INTEGRATION_TESTS "Build integration tests" OFF)
option(ACTOR_BUILD_EXAMPLES "Build examples" OFF)
option(ACTOR_BUILD_SHARED "Build shared library" OFF)

add_subdirectory(src)

if(ACTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ACTOR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(ACTOR_ENABLE_INSTALL "Enable install and package config" OFF)

if(ACTOR_ENABLE_INSTALL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS oxherdcpp
    EXPORT oxherdcpp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/oxherdcpp-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/oxherdcpp-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oxherdcpp)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/oxherdcpp-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(
    EXPORT oxherdcpp-targets
    FILE oxherdcpp-targets.cmake
    NAMESPACE oxherdcpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oxherdcpp)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/oxherdcpp-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/oxherdcpp-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oxherdcpp)
endif()
