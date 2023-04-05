cmake_minimum_required(VERSION 3.18)

# Set the project name
project(Takoyaki)

# Set C standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set output directories
set(CMAKE_BINARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Add the Tako subdirectory
add_subdirectory(dependencies/Tako)

# Automatically grab all files in the src/includes directory
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")
file(GLOB_RECURSE INCLUDES "includes/*.h")

# Add the include directories
include_directories(includes)

set(ALL_FILES ${SOURCES} ${HEADERS} ${INCLUDES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ALL_FILES})

# Build the executable
add_executable(Takoyaki ${ALL_FILES})


