enable_language(OBJC OBJCXX)

if(QUICKPVR_BUILD_QUICKLOOK AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 12.0)
  message(WARNING "The QuickLook app extensions need macOS 12 or later "
                  "(CMAKE_OSX_DEPLOYMENT_TARGET is ${CMAKE_OSX_DEPLOYMENT_TARGET}). "
                  "Only the legacy generator is built.")
  set(QUICKPVR_BUILD_QUICKLOOK OFF)
endif()

set(QUICKPVR_INSTALL_APPDIR
    "${CMAKE_INSTALL_PREFIX}/Applications"
    CACHE PATH "Where to install QuickPVR.app.")
set(QUICKPVR_INSTALL_QLDIR
    "${CMAKE_INSTALL_PREFIX}/Library/QuickLook"
    CACHE PATH "Where to install QuickPVR.qlgenerator when the app is not built.")

set(_quickpvr_objc_options -fobjc-arc -Wall -Wextra)
set(_quickpvr_entitlements "${CMAKE_CURRENT_SOURCE_DIR}/extension.entitlements")
set(_quickpvr_sign_script "${CMAKE_CURRENT_SOURCE_DIR}/sign-bundle.cmake")

# Code shared by the app, the extensions, and the generator. Extensions may only link code built
# for application extensions.
add_library(qp_rendering STATIC common/QPImageRendering.h common/QPImageRendering.mm)
target_include_directories(qp_rendering PUBLIC common)
target_compile_options(qp_rendering PRIVATE ${_quickpvr_objc_options} -fapplication-extension)
target_link_libraries(
  qp_rendering PUBLIC quickpvr::pvr "-framework CoreGraphics" "-framework Foundation"
                      "-framework ImageIO" "-framework UniformTypeIdentifiers")

if(QUICKPVR_BUILD_QLGENERATOR)
  add_library(qp_qlgenerator MODULE qlgenerator/GeneratePreviewForURL.mm
                                    qlgenerator/GenerateThumbnailForURL.mm qlgenerator/main.c)
  set_target_properties(
    qp_qlgenerator
    PROPERTIES BUNDLE TRUE
               BUNDLE_EXTENSION qlgenerator
               OUTPUT_NAME QuickPVR
               MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/qlgenerator/Info.plist.in")
  # The legacy generator uses a QuickLook API deprecated in favour of the extensions.
  target_compile_options(qp_qlgenerator PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
                                                -Wno-deprecated-declarations)
  target_link_libraries(qp_qlgenerator PRIVATE qp_rendering "-framework CoreServices"
                                               "-framework QuickLook")
endif()

if(QUICKPVR_BUILD_QUICKLOOK)
  function(quickpvr_add_extension target output_name directory source frameworks)
    add_executable(${target} MACOSX_BUNDLE ${source})
    set_target_properties(
      ${target}
      PROPERTIES BUNDLE_EXTENSION appex
                 OUTPUT_NAME ${output_name}
                 MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/Info.plist.in")
    target_include_directories(${target} PRIVATE ${directory})
    target_compile_options(${target} PRIVATE ${_quickpvr_objc_options} -fapplication-extension)
    target_link_options(${target} PRIVATE -fapplication-extension -e _NSExtensionMain)
    target_link_libraries(${target} PRIVATE qp_rendering ${frameworks})
  endfunction()
  quickpvr_add_extension(
    qp_preview QuickPVRPreview preview preview/QPPreviewProvider.mm
    "-framework AppKit;-framework Quartz;-framework QuickLookUI")
  quickpvr_add_extension(
    qp_thumbnail QuickPVRThumbnail thumbnail thumbnail/QPThumbnailProvider.mm
    "-framework AppKit;-framework QuickLookThumbnailing")

  add_executable(qp_app MACOSX_BUNDLE app/main.mm app/QPAppDelegate.h app/QPAppDelegate.mm
                                      app/QPDocument.h app/QPDocument.mm)
  set_target_properties(qp_app PROPERTIES OUTPUT_NAME QuickPVR MACOSX_BUNDLE_INFO_PLIST
                                                             "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist.in")
  target_include_directories(qp_app PRIVATE app)
  target_compile_options(qp_app PRIVATE ${_quickpvr_objc_options})
  target_link_libraries(qp_app PRIVATE qp_rendering "-framework AppKit")
  add_dependencies(qp_app qp_preview qp_thumbnail)

  # Extensions go in Contents/PlugIns. The legacy generator goes in Contents/Library/QuickLook,
  # where QuickLook on macOS 14 and earlier finds generators inside apps.
  set(_app "$<TARGET_BUNDLE_DIR:qp_app>")
  set(_embed
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_app}/Contents/PlugIns"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_app}/Contents/PlugIns"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:qp_preview>"
              "${_app}/Contents/PlugIns/QuickPVRPreview.appex"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:qp_thumbnail>"
              "${_app}/Contents/PlugIns/QuickPVRThumbnail.appex")
  if(QUICKPVR_BUILD_QLGENERATOR)
    add_dependencies(qp_app qp_qlgenerator)
    list(APPEND _embed COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_app}/Contents/Library" COMMAND
         "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:qp_qlgenerator>"
         "${_app}/Contents/Library/QuickLook/QuickPVR.qlgenerator")
  endif()
  add_custom_command(
    TARGET qp_app
    POST_BUILD ${_embed}
    COMMAND "${CMAKE_COMMAND}" "-DIDENTITY=${QUICKPVR_CODESIGN_IDENTITY}"
            "-DENTITLEMENTS=${_quickpvr_entitlements}" "-DBUNDLE=${_app}" -P "${_quickpvr_sign_script}"
    COMMENT "Embedding and signing QuickPVR.app"
    VERBATIM)

  install(TARGETS qp_app BUNDLE DESTINATION "${QUICKPVR_INSTALL_APPDIR}")
  # The installed copy is signed again because installing may rewrite the executables.
  install(
    CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" \"-DIDENTITY=${QUICKPVR_CODESIGN_IDENTITY}\"
          \"-DENTITLEMENTS=${_quickpvr_entitlements}\"
          \"-DBUNDLE=\$ENV{DESTDIR}${QUICKPVR_INSTALL_APPDIR}/QuickPVR.app\"
          -P \"${_quickpvr_sign_script}\" COMMAND_ERROR_IS_FATAL ANY)")
elseif(QUICKPVR_BUILD_QLGENERATOR)
  add_custom_command(
    TARGET qp_qlgenerator
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DIDENTITY=${QUICKPVR_CODESIGN_IDENTITY}"
            "-DENTITLEMENTS=${_quickpvr_entitlements}"
            "-DBUNDLE=$<TARGET_BUNDLE_DIR:qp_qlgenerator>" -P "${_quickpvr_sign_script}"
    VERBATIM)
  install(TARGETS qp_qlgenerator LIBRARY DESTINATION "${QUICKPVR_INSTALL_QLDIR}")
endif()
