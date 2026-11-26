add_executable(test_pvr test_pvr.cpp)
target_link_libraries(test_pvr PRIVATE quickpvr::pvr)
target_compile_definitions(test_pvr PRIVATE PVR_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/pvrsamples")
if(NOT MSVC)
  target_compile_options(test_pvr PRIVATE -fno-exceptions)
endif()
add_test(NAME pvr COMMAND test_pvr)

if(WIN32 AND QUICKPVR_BUILD_WINDOWS_SHELL)
  add_executable(test_windows_shell test_windows_shell.cpp)
  target_compile_definitions(test_windows_shell PRIVATE NOMINMAX UNICODE _UNICODE
                                                        PVR_SAMPLES_DIR_WIDE=L"${PROJECT_SOURCE_DIR}/pvrsamples")
  target_link_libraries(test_windows_shell PRIVATE ole32 propsys shlwapi user32 gdi32 uuid)
  if(MINGW)
    target_link_options(test_windows_shell PRIVATE -municode -static)
  endif()
  add_dependencies(test_windows_shell quickpvr_shell)
  add_test(NAME windows-shell COMMAND test_windows_shell $<TARGET_FILE:quickpvr_shell>)
endif()

if(QUICKPVR_BUILD_QT_PLUGIN)
  find_package(Qt6 6.2 REQUIRED COMPONENTS Gui)
  add_executable(test_qt_plugin test_qt_plugin.cpp)
  target_link_libraries(test_qt_plugin PRIVATE Qt6::Gui)
  target_compile_definitions(
    test_qt_plugin PRIVATE ${QUICKPVR_QT_DEFINITIONS}
                           PVR_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/pvrsamples"
                           PVR_PLUGIN_DIR="${PROJECT_BINARY_DIR}/plugins")
  add_dependencies(test_qt_plugin qpvr)
  add_test(NAME qt-plugin COMMAND test_qt_plugin)
  set_tests_properties(qt-plugin PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(QUICKPVR_BUILD_KDE_THUMBNAILER)
  find_package(KF6CoreAddons 6.0 REQUIRED)
  find_package(KF6KIO 6.0 REQUIRED)
  add_executable(test_kde_thumbnailer test_kde_thumbnailer.cpp)
  target_link_libraries(test_kde_thumbnailer PRIVATE KF6::CoreAddons KF6::KIOGui)
  target_compile_definitions(
    test_kde_thumbnailer PRIVATE ${QUICKPVR_QT_DEFINITIONS}
                                 PVR_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/pvrsamples"
                                 PVR_PLUGIN_DIR="$<TARGET_FILE_DIR:pvrthumbnail>/../..")
  add_dependencies(test_kde_thumbnailer pvrthumbnail)
  add_test(NAME kde-thumbnailer COMMAND test_kde_thumbnailer)
  set_tests_properties(kde-thumbnailer PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(QUICKPVR_BUILD_KDE_METADATA)
  find_package(KF6FileMetaData 6.0 REQUIRED)
  add_executable(test_kde_metadata test_kde_metadata.cpp)
  target_link_libraries(test_kde_metadata PRIVATE KF6::FileMetaData)
  target_compile_definitions(
    test_kde_metadata PRIVATE ${QUICKPVR_QT_DEFINITIONS}
                              PVR_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/pvrsamples"
                              PVR_PLUGIN_DIR="$<TARGET_FILE_DIR:quickpvrextractor>/../..")
  add_dependencies(test_kde_metadata quickpvrextractor)
  add_test(NAME kde-metadata COMMAND test_kde_metadata)
endif()

if(QUICKPVR_BUILD_CLI)
  set(_samples "${PROJECT_SOURCE_DIR}/pvrsamples")
  add_test(NAME pvr2png-help COMMAND pvr2png --help)
  add_test(NAME pvr2png-info COMMAND pvr2png --info "${_samples}/brighterday.pvr")
  set_tests_properties(pvr2png-info PROPERTIES PASS_REGULAR_EXPRESSION "256x64 RGBA4444")
  add_test(NAME pvr2png-pack COMMAND pvr2png --info "${_samples}/dreamcast/PACK_T.PVR")
  set_tests_properties(pvr2png-pack PROPERTIES PASS_REGULAR_EXPRESSION "pack, 2 textures")
  add_test(NAME pvr2png-palette COMMAND pvr2png --info "${_samples}/dreamcast/logo_pal8.pvr")
  set_tests_properties(pvr2png-palette PROPERTIES FAIL_REGULAR_EXPRESSION "no palette")
  add_test(NAME pvr2png-convert COMMAND pvr2png -q -o "${CMAKE_CURRENT_BINARY_DIR}/out"
                                        "${_samples}/brighterday.pvr")
  add_test(NAME pvr2png-unrecognized COMMAND pvr2png -i "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
  set_tests_properties(pvr2png-unrecognized PROPERTIES WILL_FAIL TRUE)
  add_test(NAME pvr2png-usage COMMAND pvr2png)
  set_tests_properties(pvr2png-usage PROPERTIES WILL_FAIL TRUE)
endif()
