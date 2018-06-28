cmake_minimum_required(VERSION 2.8.11)
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
#set(CMAKE_AUTOUIC ON) # use this if you have CMake 3.x instead of the following
qt5_wrap_ui(qtpvr_SRC src/qtpvr.ui)

# FIXME Remove this later. This is only for -lpvr/pvr.h
include_directories(../src)
find_library(PVR_LIB NAMES pvr PATHS ../src NO_DEFAULT_PATH)

# Tell CMake to create the helloworld executable
add_executable(qtpvr ${qtpvr_SRC})

# Use the Widgets module from Qt 5.
target_link_libraries(qtpvr ${PVR_LIB} Qt5::Widgets)

# Install the executable
install(TARGETS qtpvr DESTINATION bin)
