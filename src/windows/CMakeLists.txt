add_library(
  quickpvr_shell SHARED
  classfactory.cpp
  classfactory.h
  dllmain.cpp
  module.cpp
  module.h
  previewhandler.cpp
  previewhandler.h
  quickpvrshell.def
  registration.cpp
  registration.h
  shellimage.cpp
  shellimage.h
  thumbnailprovider.cpp
  thumbnailprovider.h)
set_target_properties(quickpvr_shell PROPERTIES OUTPUT_NAME QuickPVRShell PREFIX "")
target_compile_definitions(quickpvr_shell PRIVATE NOMINMAX UNICODE WIN32_LEAN_AND_MEAN _UNICODE)
target_link_libraries(quickpvr_shell PRIVATE quickpvr::pvr advapi32 gdi32 ole32 shell32 shlwapi
                                             user32 uuid)
if(MINGW)
  # The MinGW runtime is linked in because Explorer loads the DLL into its own process.
  target_link_options(quickpvr_shell PRIVATE -static -static-libgcc -static-libstdc++)
endif()
install(TARGETS quickpvr_shell RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
