add_library(
  pvr STATIC
  blocks.cpp
  common.cpp
  dreamcast.cpp
  internal.h
  legacy.cpp
  pvr.h
  pvrtc.cpp
  pvrtc.h
  v3.cpp)
add_library(quickpvr::pvr ALIAS pvr)
target_include_directories(pvr PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(pvr PRIVATE PVR_VERSION="${PROJECT_VERSION}")
set_target_properties(pvr PROPERTIES POSITION_INDEPENDENT_CODE ON)
# The library is built without exceptions. It reports every failure through std::expected and is
# linked into QuickLook extensions.
if(MSVC)
  target_compile_options(pvr PRIVATE /EHs-c-)
  target_compile_definitions(pvr PRIVATE _HAS_EXCEPTIONS=0)
else()
  target_compile_options(pvr PRIVATE -fno-exceptions)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(pvr PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                     -Wno-sign-conversion)
  # Imagination's reference decoder predates these warnings.
  set_source_files_properties(
    pvrtc.cpp PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-shadow;-Wno-unused-parameter")
endif()
