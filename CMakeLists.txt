cmake_minimum_required(VERSION 3.25)
# The QuickLook app extensions need macOS 12. The deployment target is set before project() to
# apply to every target.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET
      "12.0"
      CACHE STRING "Minimum macOS version.")
endif()
project(
  quickpvr
  VERSION 1.0.0
  DESCRIPTION "QuickLook previews and a converter for PowerVR and Sega Dreamcast PVR textures."
  HOMEPAGE_URL "https://github.com/Tatsh/quickpvr"
  LANGUAGES C CXX)

option(BUILD_DOCS "Build documentation alongside the sources." OFF)
option(BUILD_DOCS_ONLY "Build documentation and nothing else." OFF)
option(INSTALL_DOCS "Install documentation." OFF)

set(_quickpvr_macos_default OFF)
set(_quickpvr_freedesktop_default OFF)
set(_quickpvr_windows_default OFF)
if(APPLE)
  set(_quickpvr_macos_default ON)
elseif(WIN32)
  set(_quickpvr_windows_default ON)
else()
  set(_quickpvr_freedesktop_default ON)
endif()
option(QUICKPVR_BUILD_WINDOWS_SHELL
       "Build the Explorer thumbnail provider and preview handler (Windows only)."
       ${_quickpvr_windows_default})
option(QUICKPVR_BUILD_CLI "Build the pvr2png converter." ON)
option(QUICKPVR_BUILD_QT_PLUGIN "Build the Qt image format plugin (Gwenview and other Qt apps)."
       ${_quickpvr_freedesktop_default})
option(QUICKPVR_BUILD_KDE_THUMBNAILER "Build the KIO thumbnailer (Dolphin)."
       ${_quickpvr_freedesktop_default})
option(QUICKPVR_BUILD_QUICKLOOK "Build QuickPVR.app with its QuickLook extensions (macOS only)."
       ${_quickpvr_macos_default})
option(QUICKPVR_BUILD_QLGENERATOR
       "Build the legacy QuickPVR.qlgenerator for macOS 14 and earlier (macOS only)."
       ${_quickpvr_macos_default})
set(QUICKPVR_CODESIGN_IDENTITY
    "-"
    CACHE STRING "Identity to sign the macOS bundles with (- signs ad hoc).")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(BUILD_DOCS OR BUILD_DOCS_ONLY)
  set(DOXYGEN_GENERATE_HTML YES)
  set(DOXYGEN_HTML_OUTPUT "${CMAKE_BINARY_DIR}/html")
  set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
  set(DOXYGEN_MARKDOWN_ID_STYLE GITHUB)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")
  set(DOXYGEN_GENERATE_TAGFILE "${CMAKE_BINARY_DIR}/html/${CMAKE_PROJECT_NAME}.tags")
  set(DOXYGEN_EXCLUDE_PATTERNS "*/node_modules/*" "*/build*/*" "*/pvrtc.h")
  # Headers sit beside their implementations under src, and only headers are documented.
  set(DOXYGEN_FILE_PATTERNS "*.h")
  find_package(Doxygen REQUIRED)
  doxygen_add_docs(docs README.md src ALL)
  if(INSTALL_DOCS)
    include(GNUInstallDirs)
    install(
      DIRECTORY "${CMAKE_BINARY_DIR}/html"
      DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}")
  endif()
endif()

if(NOT BUILD_DOCS_ONLY)
  include(CTest)
  include(GNUInstallDirs)
  if(WIN32)
    enable_language(RC)
    include(cmake/VersionResource.cmake)
  endif()
  add_subdirectory(src)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()
