# KDE Frameworks has no umbrella package configuration, so each framework is found on its own.
find_package(KF6CoreAddons 6.0 REQUIRED)
find_package(KF6KIO 6.0 REQUIRED)

# kcoreaddons_add_plugin() installs under KDE_INSTALL_PLUGINDIR. Only KDEInstallDirs from ECM
# defines KDE_INSTALL_PLUGINDIR. When the variable is unset, the plugin goes to /kf6/thumbcreator,
# where KIO never looks.
if(NOT KDE_INSTALL_PLUGINDIR)
  set(KDE_INSTALL_PLUGINDIR "${QUICKPVR_INSTALL_QTPLUGINDIR}")
endif()

kcoreaddons_add_plugin(pvrthumbnail INSTALL_NAMESPACE "kf6/thumbcreator" SOURCES
                       pvrthumbnailcreator.cpp pvrthumbnailcreator.h)
set_target_properties(pvrthumbnail PROPERTIES AUTOMOC ON)
target_compile_definitions(pvrthumbnail PRIVATE ${QUICKPVR_QT_DEFINITIONS})
target_link_libraries(pvrthumbnail PRIVATE KF6::KIOGui qp_qt_image)
