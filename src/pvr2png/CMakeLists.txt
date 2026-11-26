# Each dependency comes from vcpkg or the system when available, otherwise from the pinned release
# below.
include(FetchContent)
set(ARGPARSE_INSTALL OFF)
set(SPDLOG_INSTALL OFF)
FetchContent_Declare(
  argparse
  URL https://github.com/p-ranav/argparse/archive/refs/tags/v3.2.tar.gz
  URL_HASH SHA256=9dcb3d8ce0a41b2a48ac8baa54b51a9f1b6a2c52dd374e28cc713bab0568ec98
  FIND_PACKAGE_ARGS CONFIG)
FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
  URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
  FIND_PACKAGE_ARGS CONFIG)
FetchContent_MakeAvailable(argparse spdlog)

# stb has no releases or CMake package. vcpkg and MacPorts install stb_image_write.h, under an stb
# directory in the case of MacPorts. Otherwise a pinned commit is downloaded.
find_path(
  STB_IMAGE_WRITE_INCLUDE_DIR
  NAMES stb_image_write.h
  PATH_SUFFIXES stb)
if(NOT STB_IMAGE_WRITE_INCLUDE_DIR)
  FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/5c6ed623b9a65d7410639b8aa311f7ed0d9966e4.tar.gz
    URL_HASH SHA256=60bdb566f1b0859101a34cb838eb581bddb101b311724aa3d44c54761f9c0a58)
  FetchContent_MakeAvailable(stb)
  set(STB_IMAGE_WRITE_INCLUDE_DIR "${stb_SOURCE_DIR}")
endif()
message(STATUS "stb_image_write.h comes from ${STB_IMAGE_WRITE_INCLUDE_DIR}.")

add_executable(pvr2png logging.cpp logging.h main.cpp png.cpp png.h)
target_include_directories(pvr2png SYSTEM PRIVATE "${STB_IMAGE_WRITE_INCLUDE_DIR}")
target_link_libraries(pvr2png PRIVATE argparse::argparse quickpvr::pvr spdlog::spdlog)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(pvr2png PRIVATE -Wall -Wextra -Wpedantic)
endif()
# The manifest selects UTF-8 as the process code page to preserve arguments and paths that are not
# ASCII on Windows.
if(MSVC)
  target_sources(pvr2png PRIVATE pvr2png.manifest)
elseif(WIN32)
  enable_language(RC)
  target_sources(pvr2png PRIVATE pvr2png.rc)
  set_source_files_properties(pvr2png.rc PROPERTIES OBJECT_DEPENDS
                                                    "${CMAKE_CURRENT_SOURCE_DIR}/pvr2png.manifest")
  target_link_options(pvr2png PRIVATE -static)
endif()
if(WIN32)
  quickpvr_add_version_resource(pvr2png "PowerVR and Sega Dreamcast PVR texture converter" VFT_APP)
endif()
install(TARGETS pvr2png RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(NOT WIN32)
  install(FILES "${PROJECT_SOURCE_DIR}/man/pvr2png.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif()
