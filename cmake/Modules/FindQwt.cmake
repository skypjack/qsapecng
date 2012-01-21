# - Try to find the Qwt includes and library
# Once done this will define:
#
#  QWT_ROOT_DIR       - The base directory of Qwt library.
#  QWT_INCLUDE_DIRS   - Where to find qwt*.h header files.
#  QWT_LIBRARIES      - Libraries to be linked with executable.
#  QWT_FOUND          - True if Qwt library is found.
#
#  QWT_VERSION_STRING - The version of Qwt library found (x.y.z)
#  QWT_VERSION_MAJOR  - The major version of Qwt library
#  QWT_VERSION_MINOR  - The minor version of Qwt library
#  QWT_VERSION_PATCH  - The patch version of Qwt library
#
# If Qwt libraries are installed in a custom folder (this usually happens on
# Windows) the environment variable QWT_ROOT_DIR can be set to the base
# folder of the libraries.
#
# Copyright (c) 2012 Simone Rossetto <simros85@gmail.com>
#
# +---------------------------------------------------------------------------
# |	This file is FindQwt.cmake, part of "QSapecNG"
# |
# |	This program is free software: you can redistribute it and/or modify
# |	it under the terms of the GNU General Public License as published by
# |	the Free Software Foundation, either version 3 of the License, or
# |	(at your option) any later version.
# |
# |	This program is distributed in the hope that it will be useful,
# |	but WITHOUT ANY WARRANTY; without even the implied warranty of
# |	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# |	GNU General Public License for more details.
# |
# |	You should have received a copy of the GNU General Public License
# |	along with this program.  If not, see <http://www.gnu.org/licenses/>.
# +---------------------------------------------------------------------------
#

if(NOT QT4_FOUND)
	find_package(Qt4 REQUIRED QUIET)
endif(NOT QT4_FOUND)

find_path(QWT_ROOT_DIR
	NAMES
		"include/qwt.h"
		"include/qwt/qwt.h"
		"lib/qwt.framework/Headers/qwt.h"
	PATHS ENV QWT_ROOT_DIR
	PATH_SUFFIXES "qwt")
    	
find_path(QWT_INCLUDE_DIR
	NAMES "qwt.h" 
	HINTS ${QWT_ROOT_DIR}
	PATHS ENV QWT_ROOT_DIR
	PATH_SUFFIXES
		"include"
		"include/qwt"
		"lib/qwt.framework/Headers")

find_library(QWT_LIBRARY
	NAMES qwt libqwt
	HINTS ${QWT_ROOT_DIR}
	PATHS ENV QWT_ROOT_DIR
	PATH_SUFFIXES "lib")

if(QWT_LIBRARY)
	file(STRINGS "${QWT_INCLUDE_DIR}/qwt_global.h" _qwt_version REGEX "^#define QWT_VERSION[ ]+0x.*$")
	
	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x([1-9]?).*$" "\\1" _qwt_ver_major1 "${_qwt_version}")
	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x[0-9]([0-9]).*$" "\\1" _qwt_ver_major2 "${_qwt_version}")
	set(QWT_VERSION_MAJOR "${_qwt_ver_major1}${_qwt_ver_major2}")
   	
	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x[0-9][0-9]([1-9]?).*$" "\\1" _qwt_ver_minor1 "${_qwt_version}")
	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x[0-9][0-9][0-9]([0-9]).*$" "\\1" _qwt_ver_minor2 "${_qwt_version}")
	set(QWT_VERSION_MINOR "${_qwt_ver_minor1}${_qwt_ver_minor2}")

	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x[0-9][0-9][0-9][0-9]([1-9]?).*$" "\\1" _qwt_ver_patch1 "${_qwt_version}")
	string(REGEX REPLACE "^#define QWT_VERSION[ ]+0x[0-9][0-9][0-9][0-9][0-9]([0-9]).*$" "\\1" _qwt_ver_patch2 "${_qwt_version}")
	set(QWT_VERSION_PATCH "${_qwt_ver_patch1}${_qwt_ver_patch2}")
   	
   	set(QWT_VERSION_STRING "${QWT_VERSION_MAJOR}.${QWT_VERSION_MINOR}.${QWT_VERSION_PATCH}")

	# to be tested
	IF (CYGWIN)
		IF(BUILD_SHARED_LIBS)
			# No need to define QWT_USE_DLL here, because it's default for Cygwin.
		ELSE(BUILD_SHARED_LIBS)
			SET (QWT_DEFINITIONS -DQwt_STATIC)
		ENDIF(BUILD_SHARED_LIBS)
	ENDIF (CYGWIN)
endif(QWT_LIBRARY)

mark_as_advanced(
    QWT_LIBRARY
    QWT_INCLUDE_DIR
    _qwt_version
    _qwt_ver_major1
    _qwt_ver_major2
    _qwt_ver_minor1
    _qwt_ver_minor2
    _qwt_ver_patch1
    _qwt_ver_patch2)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qwt
	REQUIRED_VARS QWT_LIBRARY QWT_INCLUDE_DIR
	VERSION_VAR QWT_VERSION_STRING)

if(QWT_FOUND)
	set(QWT_INCLUDE_DIRS ${QWT_INCLUDE_DIR})
	set(QWT_LIBRARIES ${QWT_LIBRARY})
endif(QWT_FOUND)