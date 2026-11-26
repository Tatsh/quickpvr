add_library(
  quickpvr_shell SHARED
  classfactory.cpp
  classfactory.h
  dllmain.cpp
  module.cpp
  module.h
  previewhandler.cpp
  previewhandler.h
  propertyhandler.cpp
  propertyhandler.h
  quickpvrshell.def
  registration.cpp
  registration.h
  shellimage.cpp
  shellimage.h
  thumbnailprovider.cpp
  thumbnailprovider.h)
set_target_properties(quickpvr_shell PROPERTIES OUTPUT_NAME QuickPVRShell PREFIX "")
quickpvr_add_version_resource(
  quickpvr_shell "QuickPVR Explorer thumbnails, previews, and properties for PVR textures" VFT_DLL)
target_compile_definitions(quickpvr_shell PRIVATE NOMINMAX UNICODE WIN32_LEAN_AND_MEAN _UNICODE)
target_link_libraries(quickpvr_shell PRIVATE quickpvr::pvr advapi32 gdi32 ole32 propsys shell32
                                             shlwapi user32 uuid)
if(MINGW)
  # The MinGW runtime is linked in because Explorer loads the DLL into its own process.
  target_link_options(quickpvr_shell PRIVATE -static -static-libgcc -static-libstdc++)
endif()
# Registration for every user reads the property schema from the directory of the DLL.
add_custom_command(
  TARGET quickpvr_shell
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/QuickPVR.propdesc"
          "$<TARGET_FILE_DIR:quickpvr_shell>"
  VERBATIM)
install(TARGETS quickpvr_shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES QuickPVR.propdesc DESTINATION "${CMAKE_INSTALL_BINDIR}")
