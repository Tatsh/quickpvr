# Cross-compile for 64-bit Windows with MinGW-w64, for example:
#
#     cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(_quickpvr_mingw_prefix x86_64-w64-mingw32)
find_program(CMAKE_C_COMPILER ${_quickpvr_mingw_prefix}-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER ${_quickpvr_mingw_prefix}-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER ${_quickpvr_mingw_prefix}-windres REQUIRED)

# Libraries and packages for the target are installed in the sysroot beside the compiler's bin
# directory (for example /usr/x86_64-w64-mingw32 or /opt/local/x86_64-w64-mingw32). Searching only
# there excludes the host's packages, built for another system, from the build.
get_filename_component(_quickpvr_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_quickpvr_mingw_root "${_quickpvr_mingw_bin}/../${_quickpvr_mingw_prefix}"
                       ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH "${_quickpvr_mingw_root}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
