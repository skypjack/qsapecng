# FindQwt.cmake - locate the Qwt plotting library built for the Qt major
# version in use (QT_VERSION_MAJOR must be set before calling find_package).
#
# Result:
#   Qwt::Qwt            imported target
#   QWT_FOUND, QWT_INCLUDE_DIRS, QWT_LIBRARIES, QWT_VERSION_STRING
#
# Hints: set QWT_ROOT_DIR (CMake or environment variable) to the Qwt install
# prefix, e.g. C:/Qwt-6.3.0 on Windows or /opt/qwt-qt6 on Linux.
#
# Distribution packages name the library differently per Qt version:
#   Debian/Ubuntu: libqwt-qt6.so / libqwt-qt5.so (headers in include/qwt)
#   Fedora:        libqwt-qt6.so / libqwt-qt5.so (headers in include/qt6/qwt)
#   vcpkg/MSYS2:   qwt.lib / libqwt-qt6.dll.a
#   source build:  libqwt.so / qwt.lib
#
# Copyright (c) 2012 Simone Rossetto, 2026 updated for Qt5/Qt6.
# Distributed under the GNU GPL v3 or later.

if(NOT QT_VERSION_MAJOR)
  set(QT_VERSION_MAJOR 6)
endif()

set(_qwt_hints ${QWT_ROOT_DIR} $ENV{QWT_ROOT_DIR})

find_path(QWT_INCLUDE_DIR
  NAMES qwt_plot.h
  HINTS ${_qwt_hints}
  PATH_SUFFIXES
    include
    include/qwt
    include/qwt-qt${QT_VERSION_MAJOR}
    include/qt${QT_VERSION_MAJOR}/qwt
    qwt
    lib/qwt.framework/Headers
)

find_library(QWT_LIBRARY_RELEASE
  NAMES qwt-qt${QT_VERSION_MAJOR} qwt
  HINTS ${_qwt_hints}
  PATH_SUFFIXES lib
)
find_library(QWT_LIBRARY_DEBUG
  NAMES qwt-qt${QT_VERSION_MAJOR}d qwtd
  HINTS ${_qwt_hints}
  PATH_SUFFIXES lib debug/lib
)

include(SelectLibraryConfigurations)
select_library_configurations(QWT)

if(QWT_INCLUDE_DIR AND EXISTS "${QWT_INCLUDE_DIR}/qwt_global.h")
  file(STRINGS "${QWT_INCLUDE_DIR}/qwt_global.h" _qwt_ver
       REGEX "^#define[ \t]+QWT_VERSION_STR[ \t]+\"[0-9.]+\"")
  string(REGEX REPLACE ".*\"([0-9.]+)\".*" "\\1" QWT_VERSION_STRING "${_qwt_ver}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qwt
  REQUIRED_VARS QWT_LIBRARY QWT_INCLUDE_DIR
  VERSION_VAR QWT_VERSION_STRING)

if(QWT_FOUND)
  set(QWT_INCLUDE_DIRS ${QWT_INCLUDE_DIR})
  set(QWT_LIBRARIES ${QWT_LIBRARY})
  if(NOT TARGET Qwt::Qwt)
    add_library(Qwt::Qwt UNKNOWN IMPORTED)
    set_target_properties(Qwt::Qwt PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${QWT_INCLUDE_DIR}")
    if(QWT_LIBRARY_RELEASE)
      set_property(TARGET Qwt::Qwt APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION_RELEASE "${QWT_LIBRARY_RELEASE}")
    endif()
    if(QWT_LIBRARY_DEBUG)
      set_property(TARGET Qwt::Qwt APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION_DEBUG "${QWT_LIBRARY_DEBUG}")
    endif()
    if(NOT QWT_LIBRARY_RELEASE)
      set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION "${QWT_LIBRARY_DEBUG}")
    else()
      set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION "${QWT_LIBRARY_RELEASE}")
    endif()
    # Shared Qwt on Windows needs QWT_DLL so headers use dllimport.
    if(WIN32 AND NOT QWT_STATIC)
      set_property(TARGET Qwt::Qwt APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QWT_DLL)
    endif()
  endif()
endif()

mark_as_advanced(QWT_INCLUDE_DIR QWT_LIBRARY_RELEASE QWT_LIBRARY_DEBUG)
