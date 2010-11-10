find_package( Boost 1.41.0 REQUIRED )
find_package( Qt4 COMPONENTS QtCore QtGui REQUIRED )
find_package( Qwt5 REQUIRED )


if(WIN32)
  set( QT_USE_QTMAIN 1 )
endif(WIN32)

include( ${QT_USE_FILE} )
add_definitions( ${QT_DEFINITIONS} )

include_directories(
  ${QT_INCLUDES}
  ${Qwt5_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)


set(
  LIBS
  ${LIBS} 
  ${QT_LIBRARIES}
  ${Qwt5_Qt4_LIBRARY}
)

IF(WIN32)
  set(
    LIBS
    ${LIBS}
    Imm32.lib
    Winmm.lib
    ws2_32.lib
  )
ENDIF(WIN32)


set(
  qtpropertyeditor_SRCS
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertymanager.cpp
  gui/qtsolutions/qtpropertyeditor/qteditorfactory.cpp
  gui/qtsolutions/qtpropertyeditor/qtvariantproperty.cpp
  gui/qtsolutions/qtpropertyeditor/qttreepropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtbuttonpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtgroupboxpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowserutils.cpp
)

qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtpropertybrowser.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtpropertymanager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtpropertymanager.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qteditorfactory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qteditorfactory.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtvariantproperty.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtvariantproperty.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qttreepropertybrowser.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qttreepropertybrowser.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtbuttonpropertybrowser.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtbuttonpropertybrowser.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtgroupboxpropertybrowser.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtgroupboxpropertybrowser.cpp
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtpropertymanager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtpropertymanager.moc
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qteditorfactory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qteditorfactory.moc
)
qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qttreepropertybrowser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qttreepropertybrowser.moc
)

set(
  qtpropertyeditor_HEADERS_ONLY_MOC
  gui/qtsolutions/qtpropertyeditor/moc_qtpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qtpropertymanager.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qteditorfactory.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qtvariantproperty.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qttreepropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qtbuttonpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/moc_qtgroupboxpropertybrowser.cpp
  gui/qtsolutions/qtpropertyeditor/qtpropertymanager.moc
  gui/qtsolutions/qtpropertyeditor/qteditorfactory.moc
  gui/qtsolutions/qtpropertyeditor/qttreepropertybrowser.moc
)

set_source_files_properties(
  ${qtpropertyeditor_HEADERS_ONLY_MOC}
  PROPERTIES
    HEADER_FILE_ONLY true
)

qt4_generate_moc(
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/qtpropertybrowserutils_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gui/qtsolutions/qtpropertyeditor/moc_qtpropertybrowserutils_p.cpp
)

set(
  qtpropertyeditor_HEADERS_MOC
  gui/qtsolutions/qtpropertyeditor/moc_qtpropertybrowserutils_p.cpp
)

set(
  qtpropertyeditor_MOC
  ${qtpropertyeditor_HEADERS_MOC}
  ${qtpropertyeditor_HEADERS_ONLY_MOC}
)

qt4_add_resources(
  qtpropertyeditor_RESOURCES
  gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.qrc
)

set(
  qsapecng_SRCS
  logger/logger.cpp
  model/circuit.cpp
  model/metacircuit.cpp
  parser/ir_circuit.cpp
  parser/crc_circuit.cpp
  functor/rpoly-adapter.cpp
  functor/rpoly.cpp
  gui/qsapecng.cpp
  gui/settings.cpp
  gui/qsapecngwindow.cpp
  gui/userdefdialog/userdefdialog.cpp
  gui/configdialog/configdialog.cpp
  gui/configdialog/configpage.cpp
  gui/sidebarmodel.cpp
  gui/sidebarview.cpp
  gui/delegate.cpp
  gui/editor/schematiceditor.cpp
  gui/editor/undoredocommand.cpp
  gui/editor/schematicsceneparser.cpp
  gui/editor/schematicscene.cpp
  gui/editor/graphicsnode.cpp
  gui/editor/component.cpp
  gui/editor/label.cpp
  gui/editor/wire.cpp
  gui/editor/item.cpp
  gui/workplane/workplane.cpp
)

set(
  qsapecng_HEADERS_MOC
  gui/qsapecngwindow.h
  gui/extendedmdiarea.h
  gui/userdefdialog/userdefdialog.h
  gui/configdialog/configdialog.h
  gui/configdialog/configpage.h
  gui/sidebarmodel.h
  gui/sidebarview.h
  gui/delegate.h
  gui/qlogger.h
  gui/editor/schematiceditor.h
  gui/editor/propertytextitem.hpp
  gui/editor/metacircuit_thread.hpp
  gui/editor/schematicscene.h
  gui/editor/schematicview.h
  gui/workplane/workplane.h
)

qt4_wrap_cpp( qsapecng_MOC ${qsapecng_HEADERS_MOC} )

qt4_add_resources(
  qsapecng_RESOURCES
  gui/qsapecng.qrc
)


set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin )

add_executable(
  qsapecng WIN32
  gui/qsapecng.rc
  ${qsapecng_MOC}
  ${qsapecng_SRCS}
  ${qsapecng_RESOURCES}
  ${qtpropertyeditor_MOC}
  ${qtpropertyeditor_SRCS}
  ${qtpropertyeditor_RESOURCES}
)

target_link_libraries( qsapecng ${LIBS} )

install(
  TARGETS qsapecng
  RUNTIME DESTINATION bin
)
