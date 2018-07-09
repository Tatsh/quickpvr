cmake_minimum_required(VERSION 3.11.4)
project(libpvr)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(libpvr_SRC
    pvr.cc
    pvrtc.cc
)
set_source_files_properties(${libpvr_SRC}
    PROPERTIES
    COMPILE_FLAGS "-pedantic -Wall -Wextra -Werror"
)

add_library(pvr SHARED pvr.h pvr.cc pvrtc.cc)

set_target_properties(pvr
    PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER sh.tat.pvr
    VERSION 1.0.0
    SOVERSION 1.0.0
    PUBLIC_HEADER pvr.h
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
)
