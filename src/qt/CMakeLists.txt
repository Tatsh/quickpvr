find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui)

set(QUICKPVR_QT_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_SIGNALS_SLOTS_KEYWORDS
    PARENT_SCOPE)
set(_quickpvr_qt_definitions QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
                             QT_NO_SIGNALS_SLOTS_KEYWORDS)

# The image format plugin and the KDE thumbnailer share qp_qt_image for decoding to QImage.
add_library(qp_qt_image STATIC pvrimage.cpp pvrimage.h)
target_include_directories(qp_qt_image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qp_qt_image PUBLIC ${_quickpvr_qt_definitions})
target_link_libraries(qp_qt_image PUBLIC Qt6::Gui quickpvr::pvr)
set_target_properties(qp_qt_image PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(QUICKPVR_INSTALL_QTPLUGINDIR
    "${QT6_INSTALL_PLUGINS}"
    CACHE PATH "Qt plugin directory for the image format plugin and the thumbnailer.")

if(QUICKPVR_BUILD_QT_PLUGIN)
  qt_add_plugin(qpvr PLUGIN_TYPE imageformats CLASS_NAME PvrPlugin)
  target_sources(qpvr PRIVATE pvrhandler.cpp pvrhandler.h pvrplugin.cpp pvrplugin.h)
  set_target_properties(qpvr PROPERTIES AUTOMOC ON LIBRARY_OUTPUT_DIRECTORY
                                                   "${PROJECT_BINARY_DIR}/plugins/imageformats")
  target_link_libraries(qpvr PRIVATE qp_qt_image)

  install(TARGETS qpvr LIBRARY DESTINATION "${QUICKPVR_INSTALL_QTPLUGINDIR}/imageformats")
endif()

# The MIME type lets file managers and image viewers on freedesktop systems identify the files.
if(NOT APPLE)
  install(FILES quickpvr.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
endif()

# Gwenview fixes its list of MIME types at build time, and the list does not include image/x-pvr.
# The hidden desktop entry starts Gwenview for image/x-pvr and is ignored where Gwenview is not
# installed.
option(QUICKPVR_INSTALL_GWENVIEW_ENTRY "Open .pvr files with Gwenview (freedesktop systems)." ON)
if(QUICKPVR_BUILD_QT_PLUGIN
   AND QUICKPVR_INSTALL_GWENVIEW_ENTRY
   AND NOT APPLE
   AND NOT WIN32)
  install(FILES sh.tat.quickpvr.gwenview.desktop
          DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
