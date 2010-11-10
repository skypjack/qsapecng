project( qsapecng )
cmake_minimum_required( VERSION 2.6 )

set(
  CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_SOURCE_DIR}/cmake/Modules/"
)
set( CMAKE_BUILD_TYPE "Release" )

set( PROJECT_VERSION_MAJOR 1 )
set( PROJECT_VERSION_MINOR 2 )
set( PROJECT_VERSION_PATCH 0 )
set( PROJECT_VERSION_REVISION 1 )
set( PROJECT_VERSION
  ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} )

set( PACKAGE_VERSION "${PROJECT_VERSION}" )
set( PACKAGE_NAME "QSapecNG" )
set( SETTINGS_ORGANIZATION "Unifi" )
set( SETTINGS_APPLICATION "${PACKAGE_NAME}" )
set( PACKAGE_CHEF "Michele Caini <michele.caini@gmail.com>" )

add_definitions( -ansi -Wall -O0 -g )
# add_definitions( -ansi -Wall -O2 )
add_definitions(
  -DPACKAGE_NAME="${PACKAGE_NAME}"
  -DPACKAGE_VERSION="${PACKAGE_VERSION}"
)
if(WIN32)
  add_definitions( -static )
endif(WIN32)

if(UNIX)
  set( UNIX_SYS TRUE )
endif(UNIX)

if(WIN32)
  set( WIN32_SYS TRUE )
endif(WIN32)

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in src/config.h )

add_subdirectory( src )



#################################################################
##################                             ##################
##################            CPACK            ##################
##################                             ##################
#################################################################

include( InstallRequiredSystemLibraries )


set( CPACK_GENERATOR "TZ;TGZ;TBZ2" )

set( CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}" )
set( CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}" )
set( CPACK_PACKAGE_NAME ${PACKAGE_NAME} )
set( CPACK_PACKAGE_VENDOR ${PACKAGE_CHEF} )
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "QSapecNG - Qt-based GUI for SapecNG" )
SET( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" )
SET( CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README" )
set( CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}" )
set( CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}" )
set( CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}" )
set(
  CPACK_SOURCE_IGNORE_FILES
  ".*~;.*kdev*;/CMakeFiles/;/_CPack_Packages/"
)
set(
  CPACK_PACKAGE_INSTALL_DIRECTORY
  "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)
set(
  CPACK_INSTALL_CMAKE_PROJECTS
  ${CMAKE_CURRENT_BINARY_DIR};${PACKAGE_NAME};ALL;/
)


if(WIN32 AND NOT UNIX)

  set( CPACK_GENERATOR "${CPACK_GENERATOR};NSIS" )
  set( CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_NAME} )
  set( CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${CPACK_PACKAGE_NAME}.exe" )
  set( CPACK_NSIS_DISPLAY_NAME ${CPACK_PACKAGE_DESCRIPTION_SUMMARY} )
  set( CPACK_NSIS_CONTACT ${PACKAGE_CHEF} )
  set( CPACK_NSIS_MODIFY_PATH ON )
  set( CPACK_GENERATOR NSIS ZIP )
  set( CPACK_SOURCE_GENERATOR ZIP )
  set(
    CPACK_NSIS_MUI_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}\\\\src\\\\gui\\\\images\\\\qsapecng-ico.ico"
  )
  set(
    CPACK_NSIS_MUI_UNIICON
    "${CMAKE_CURRENT_SOURCE_DIR}\\\\src\\\\gui\\\\images\\\\qsapecng-ico.ico"
  )
  set(
    CPACK_PACKAGE_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}\\\\src\\\\gui\\\\images\\\\qsapecng.svg"
  )

else(WIN32 AND NOT UNIX)

  set( CPACK_STRIP_FILES "usr/bin/${CPACK_PACKAGE_NAME}" )
  set( CPACK_SOURCE_STRIP_FILES "" )
  set(
    CPACK_PACKAGE_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/images/qsapecng.svg"
  )

  set( CPACK_GENERATOR "${CPACK_GENERATOR};DEB" )
  set( CPACK_DEBIAN_PACKAGE_MAINTAINER ${PACKAGE_CHEF} )
  set( CPACK_DEBIAN_PACKAGE_SECTION Electronics )
  set( CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}+sid" )
  set(
    CPACK_DEBIAN_PACKAGE_DEPENDS
     "libstdc++6 (>=4.3.2-1.1),
      libqt4-core (>=4.6),
      libqt4-gui (>=4.6),
      libqwt5-qt4 (>=5.0)"
  )

endif(WIN32 AND NOT UNIX)


include( CPack )
