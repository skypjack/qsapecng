# Qt 6.2 or newer (Qwt must be built against Qt 6 as well).
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport)
set(QT_VERSION_MAJOR 6)
set(QT_VERSION ${Qt6_VERSION})

find_package(Qwt 6.2 REQUIRED)

message(STATUS "Building with Qt ${QT_VERSION}, Qwt ${QWT_VERSION_STRING}, Boost ${Boost_VERSION}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(qtpropertyeditor_SRCS
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertymanager.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowserutils.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.h
  gui/qtsolutions/qtpropertyeditor/qtpropertymanager.h
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowserutils_p.h
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.qrc
)

set(qsapecng_SRCS
  logger/logger.cpp
  model/circuit.cpp
  model/metacircuit.cpp
  model/numericsolve.cpp
  parser/ir_circuit.cpp
  parser/crc_circuit.cpp
  parser/spice_circuit.cpp
  functor/rpoly-adapter.cpp
  functor/rpoly.cpp
  gui/qsapecng.cpp
  gui/theme.cpp
  gui/resultpanel.cpp
  gui/units.cpp
  gui/formula.cpp
  gui/plotdialog.cpp
  gui/theme.h
  gui/resultpanel.h
  gui/units.h
  gui/formula.h
  gui/plotdialog.h
  gui/settings.cpp
  gui/qsapecngwindow.cpp
  gui/configdialog/configdialog.cpp
  gui/configdialog/configpage.cpp
  gui/sidebarmodel.cpp
  gui/delegate.cpp
  gui/editor/schematiceditor.cpp
  gui/editor/componentdialog.cpp
  gui/editor/propertytextitem.cpp
  gui/editor/undoredocommand.cpp
  gui/editor/schematicsceneparser.cpp
  gui/editor/schematicscene.cpp
  gui/editor/graphicsnode.cpp
  gui/editor/component.cpp
  gui/editor/label.cpp
  gui/editor/wire.cpp
  gui/editor/item.cpp
  gui/workplane/workplane.cpp
  # headers with Q_OBJECT (listed so AUTOMOC sees them)
  gui/qsapecngwindow.h
  gui/extendedmdiarea.h
  gui/configdialog/configdialog.h
  gui/configdialog/configpage.h
  gui/sidebarmodel.h
  gui/delegate.h
  gui/qlogger.h
  gui/editor/schematiceditor.h
  gui/editor/componentdialog.h
  gui/editor/propertytextitem.hpp
  gui/editor/metacircuit_thread.hpp
  gui/editor/schematicscene.h
  gui/editor/schematicview.h
  gui/workplane/workplane.h
  gui/qsapecng.qrc
  gui/icons.qrc
)

if(WIN32)
  list(APPEND qsapecng_SRCS gui/qsapecng.rc)
endif()

if(APPLE)
  set(MACOSX_BUNDLE_ICON_FILE qsapecng.icns)
  set(_bundle_icon "${CMAKE_CURRENT_SOURCE_DIR}/gui/images/qsapecng.icns")
  set_source_files_properties(${_bundle_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  list(APPEND qsapecng_SRCS ${_bundle_icon})
endif()

add_executable(qsapecng WIN32 MACOSX_BUNDLE
  ${qsapecng_SRCS}
  ${qtpropertyeditor_SRCS}
)

target_include_directories(qsapecng PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor"
)

target_link_libraries(qsapecng PRIVATE
  Qt::Core Qt::Gui Qt::Widgets Qt::Svg Qt::PrintSupport
  Qwt::Qwt
  Boost::headers
)

target_compile_definitions(qsapecng PRIVATE
  PACKAGE_NAME="${PACKAGE_NAME}"
  PACKAGE_VERSION="${PACKAGE_VERSION}"
  BOOST_BIND_GLOBAL_PLACEHOLDERS
  BOOST_ALLOW_DEPRECATED_HEADERS
  QSAPECNG_SOURCE_EXAMPLES="${PROJECT_SOURCE_DIR}/examples"
  $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)

if(MSVC)
  target_compile_options(qsapecng PRIVATE /W3 /bigobj /permissive- /Zc:__cplusplus /utf-8)
  target_compile_definitions(qsapecng PRIVATE _USE_MATH_DEFINES NOMINMAX)
else()
  target_compile_options(qsapecng PRIVATE -Wall)
  if(MINGW)
    target_compile_options(qsapecng PRIVATE -Wa,-mbig-obj)
  endif()
endif()

if(WIN32)
  set(_out_name QSapecNG)
elseif(APPLE)
  set(_out_name QSapecNG)
else()
  set(_out_name qsapecng)
endif()

set_target_properties(qsapecng PROPERTIES
  OUTPUT_NAME "${_out_name}"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
  MACOSX_BUNDLE_BUNDLE_NAME "QSapecNG"
  MACOSX_BUNDLE_GUI_IDENTIFIER "net.sourceforge.qsapecng"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(GNUInstallDirs)

# Keep the path to a Qwt installed outside the system directories (e.g. a
# source build in /usr/local/qwt-6.3.0) in the installed binary's RPATH.
if(UNIX AND NOT APPLE)
  set_target_properties(qsapecng PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

install(TARGETS qsapecng
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE DESTINATION .
)

# On Windows, copy the Qwt DLL next to the executable and run windeployqt
# after each build so bin/ is directly runnable; do the same at install time
# for the ZIP/NSIS packages.
if(WIN32)
  # official Qt: <prefix>/bin/windeployqt.exe, MSYS2: /ucrt64/bin/windeployqt6.exe
  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt6 windeployqt-qt6
    HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" "${QT6_INSTALL_PREFIX}/bin")

  if(QWT_LIBRARY_RELEASE)
    get_filename_component(_qwt_libdir "${QWT_LIBRARY_RELEASE}" DIRECTORY)
  else()
    get_filename_component(_qwt_libdir "${QWT_LIBRARY_DEBUG}" DIRECTORY)
  endif()
  find_file(QWT_DLL
    NAMES qwt.dll qwt-qt6.dll libqwt-qt6.dll
    HINTS "${_qwt_libdir}" "${_qwt_libdir}/../bin" "${QWT_ROOT_DIR}/lib" "${QWT_ROOT_DIR}/bin"
    NO_DEFAULT_PATH)
  if(QWT_DLL)
    get_filename_component(_qwt_dll_name "${QWT_DLL}" NAME)
    install(FILES "${QWT_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
  else()
    message(WARNING "Qwt DLL not found; copy it next to QSapecNG.exe yourself")
  endif()

  # With MSVC the C++ runtime is added by InstallRequiredSystemLibraries
  # (below); windeployqt could only find it from a Visual Studio developer
  # prompt (VCINSTALLDIR) and otherwise just warns, e.g. on CI.
  # MinGW builds keep windeployqt's copy of the GCC runtime.
  set(_deploy_flags --no-translations)
  if(MSVC)
    list(APPEND _deploy_flags --no-compiler-runtime)
  endif()

  option(QSAPECNG_DEPLOY_AFTER_BUILD "Copy Qwt and run windeployqt after building" ON)
  if(QSAPECNG_DEPLOY_AFTER_BUILD)
    if(QWT_DLL)
      add_custom_command(TARGET qsapecng POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QWT_DLL}" "$<TARGET_FILE_DIR:qsapecng>")
    endif()
    if(WINDEPLOYQT_EXECUTABLE)
      # Qwt itself links extra Qt modules, so it is deployed as well.
      set(_deploy_targets "$<TARGET_FILE:qsapecng>")
      if(QWT_DLL)
        list(APPEND _deploy_targets "$<TARGET_FILE_DIR:qsapecng>/${_qwt_dll_name}")
      endif()
      add_custom_command(TARGET qsapecng POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${_deploy_flags} ${_deploy_targets}
        COMMENT "Deploying the Qt runtime next to QSapecNG.exe")
    else()
      message(WARNING "windeployqt not found; Qt DLLs will not be copied next to the executable")
    endif()
  endif()

  if(WINDEPLOYQT_EXECUTABLE)
    set(_qwt_inst_arg "")
    if(QWT_DLL)
      set(_qwt_inst_arg "\"\${_bin}/${_qwt_dll_name}\"")
    endif()
    install(CODE "
      set(_bin \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
      execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" ${_deploy_flags}
        \"\${_bin}/${_out_name}.exe\" ${_qwt_inst_arg})
    ")
  endif()

  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
  include(InstallRequiredSystemLibraries)
endif()

# example circuits: File > Open Example finds them next to the program
file(GLOB _examples "${PROJECT_SOURCE_DIR}/examples/*.info")
if(WIN32)
  install(FILES ${_examples} "${PROJECT_SOURCE_DIR}/examples/README"
    DESTINATION examples)
elseif(NOT APPLE)
  install(FILES ${_examples} "${PROJECT_SOURCE_DIR}/examples/README"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qsapecng/examples)
endif()

if(UNIX AND NOT APPLE)
  install(FILES gui/images/qsapecng.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  foreach(_sz 16 22 32 48 64 128 256)
    install(FILES gui/images/qsapecng-${_sz}x${_sz}.png
      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_sz}x${_sz}/apps
      RENAME qsapecng.png)
  endforeach()
  install(FILES gui/images/qsapecng.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
