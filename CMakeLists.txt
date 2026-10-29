cmake_minimum_required(VERSION 3.16)

project(qsapecng
  VERSION 3.0.0
  DESCRIPTION "Qt-based GUI for symbolic analysis of linear analog circuits"
  LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PACKAGE_NAME "QSapecNG")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(SETTINGS_ORGANIZATION "Unifi")
set(SETTINGS_APPLICATION "${PACKAGE_NAME}")
set(PROJECT_AUTHOR "Michele Caini")
set(PROJECT_AUTHOR_EMAIL "michele.caini@gmail.com")
set(PACKAGE_CHEF "${PROJECT_AUTHOR} <${PROJECT_AUTHOR_EMAIL}>")

if(UNIX)
  set(UNIX_SYS TRUE)
endif()
if(WIN32)
  set(WIN32_SYS TRUE)
endif()

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/src/config.h")

# Uninstall target
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# Only Boost headers are needed. Prefer the installed BoostConfig.cmake
# (distro packages, MSYS2, vcpkg); otherwise accept a plain unpacked Boost
# source tree given via BOOST_ROOT / Boost_ROOT (CMake >= 3.30 no longer
# ships FindBoost, so it is located by hand).
find_package(Boost 1.66 CONFIG QUIET)
if(NOT Boost_FOUND)
  find_path(BOOST_INCLUDE_DIR boost/version.hpp
    HINTS ${BOOST_ROOT} ${Boost_ROOT} $ENV{BOOST_ROOT} $ENV{Boost_ROOT}
    PATH_SUFFIXES include)
  if(NOT BOOST_INCLUDE_DIR)
    message(FATAL_ERROR "Boost headers not found: install Boost or pass -DBOOST_ROOT=<path to boost_1_xx_x>")
  endif()
  file(STRINGS "${BOOST_INCLUDE_DIR}/boost/version.hpp" _bv REGEX "define BOOST_LIB_VERSION")
  string(REGEX REPLACE ".*\"([0-9_]+)\".*" "\\1" Boost_VERSION "${_bv}")
  add_library(Boost::headers INTERFACE IMPORTED)
  set_target_properties(Boost::headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}")
endif()

add_subdirectory(src)

option(QSAPECNG_BUILD_TESTS "Build the solver regression tests (ctest)" OFF)
if(QSAPECNG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()


# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
set(CPACK_PACKAGE_VENDOR "${PACKAGE_CHEF}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "QSapecNG - Qt-based GUI for SapecNG")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PROJECT_VERSION}-source")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PACKAGE_NAME}")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/" "/build.*/" "\\\\.user$" "~$")

if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PROJECT_VERSION}-win64")
  set(CPACK_GENERATOR ZIP)
  find_program(MAKENSIS_EXECUTABLE makensis)
  if(MAKENSIS_EXECUTABLE)
    list(APPEND CPACK_GENERATOR NSIS)
  endif()
  set(CPACK_PACKAGE_EXECUTABLES "QSapecNG" "QSapecNG")
  set(CPACK_NSIS_DISPLAY_NAME "${PACKAGE_NAME}")
  set(CPACK_NSIS_CONTACT "${PACKAGE_CHEF}")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\QSapecNG.exe")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/images/qsapecng-ico.ico")
  set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/images/qsapecng-ico.ico")
  set(CPACK_SOURCE_GENERATOR ZIP)
elseif(APPLE)
  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_SOURCE_GENERATOR TGZ)
else()
  set(CPACK_GENERATOR TGZ DEB)
  set(CPACK_SOURCE_GENERATOR TGZ)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PACKAGE_CHEF}")
  set(CPACK_DEBIAN_PACKAGE_SECTION electronics)
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
