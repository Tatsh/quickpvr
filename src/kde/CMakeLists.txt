find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons KIO)

kcoreaddons_add_plugin(pvrthumbnail INSTALL_NAMESPACE "kf6/thumbcreator" SOURCES
                       pvrthumbnailcreator.cpp pvrthumbnailcreator.h)
set_target_properties(pvrthumbnail PROPERTIES AUTOMOC ON)
target_compile_definitions(pvrthumbnail PRIVATE ${QUICKPVR_QT_DEFINITIONS})
target_link_libraries(pvrthumbnail PRIVATE KF6::KIOGui qp_qt_image)
