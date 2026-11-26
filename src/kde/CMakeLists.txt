find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)

# KDEInstallDirs6 uses the plugin directory of Qt only when CMAKE_INSTALL_PREFIX at configure time
# matches the prefix of Qt. Otherwise it uses <prefix>/lib/plugins (lib64 on some systems), where Qt
# and KIO do not look. Setting KDE_INSTALL_PLUGINDIR to the plugin directory of Qt relative to the
# prefix, as the image format plugin does, also works with a prefix given to cmake --install.
if(NOT KDE_INSTALL_USE_QT_SYS_PATHS)
  find_package(Qt6 6.2 REQUIRED COMPONENTS Core)
  set(KDE_INSTALL_PLUGINDIR "${QT6_INSTALL_PLUGINS}")
endif()

# KDE Frameworks has no umbrella package configuration. Each framework is found separately.
find_package(KF6CoreAddons 6.0 REQUIRED)

if(QUICKPVR_BUILD_KDE_THUMBNAILER)
  find_package(KF6KIO 6.0 REQUIRED)
  kcoreaddons_add_plugin(pvrthumbnail INSTALL_NAMESPACE "kf6/thumbcreator" SOURCES
                         pvrthumbnailcreator.cpp pvrthumbnailcreator.h)
  target_compile_definitions(pvrthumbnail PRIVATE ${QUICKPVR_QT_DEFINITIONS})
  target_link_libraries(pvrthumbnail PRIVATE KF6::KIOGui qp_qt_image)
endif()

if(QUICKPVR_BUILD_KDE_METADATA)
  find_package(KF6FileMetaData 6.0 REQUIRED)
  kcoreaddons_add_plugin(quickpvrextractor INSTALL_NAMESPACE "kf6/kfilemetadata" SOURCES
                         pvrextractor.cpp pvrextractor.h)
  target_compile_definitions(quickpvrextractor PRIVATE ${QUICKPVR_QT_DEFINITIONS})
  target_link_libraries(quickpvrextractor PRIVATE KF6::FileMetaData quickpvr::pvr)
endif()
