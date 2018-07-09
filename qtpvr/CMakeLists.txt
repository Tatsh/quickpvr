cmake_minimum_required(VERSION 3.11.4)
project(qtpvr)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)

set(qtpvr_SRC
  src/main.cpp
  src/qtpvr.cpp
)

# Create code from a list of Qt designer ui files.
set(CMAKE_AUTOUIC ON) # use this if you have CMake 3.x instead of the following

# FIXME Remove this later. This is only for -lpvr/pvr.h
if(APPLE)
    include_directories(../libpvr/build/pvr.framework/Headers)
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_INSTALL_RPATH "/Users/tatsh/dev/quickpvr/qtpvr/build")
# find_library(PVR_LIB NAMES pvr PATHS ../libpvr NO_DEFAULT_PATH)

# Tell CMake to create the helloworld executable
add_executable(qtpvr ${qtpvr_SRC})

if (APPLE)
    target_link_libraries(qtpvr "-framework pvr -F../../libpvr/build")
endif()
# Use the Widgets module from Qt 5.
target_link_libraries(qtpvr ${PVR_LIB} Qt5::Widgets)

# Install the executable
# install(TARGETS qtpvr DESTINATION bin)
