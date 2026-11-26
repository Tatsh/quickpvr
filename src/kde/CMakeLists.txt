find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)

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
