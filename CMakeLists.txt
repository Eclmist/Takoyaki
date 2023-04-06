cmake_minimum_required(VERSION 3.18)

# Set the project name
project(Takoyaki)

# Set C standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Add the Tako subdirectory
add_subdirectory(dependencies/Tako)

# Automatically grab all files in the src/includes directory
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")
file(GLOB_RECURSE INCLUDES "includes/*.h")
file(GLOB_RECURSE RESOURCES "resources/*")

file(GLOB VS_SHADER "src/shaders/vs.hlsl")
file(GLOB PS_SHADER "src/shaders/ps.hlsl")

# Add the include directories
include_directories(includes)
include_directories(dependencies)
include_directories(resources)
include_directories(
    $<IF:$<CONFIG:Debug>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug,>
    $<IF:$<CONFIG:Release>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release,>
    $<IF:$<CONFIG:RelWithDebInfo>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RelWithDebInfo,>
    $<IF:$<CONFIG:MinSizeRel>,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MinSizeRel,>
)

set(ALL_FILES ${SOURCES} ${HEADERS} ${INCLUDES} ${RESOURCES} ${VS_SHADER} ${PS_SHADER})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ALL_FILES})

# Build the executable
add_executable(Takoyaki WIN32 ${ALL_FILES})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Takoyaki)

target_compile_definitions(Takoyaki PRIVATE UNICODE)
target_link_libraries(Takoyaki PRIVATE Tako d3d11)

# Set the VS shader properties
set_property(SOURCE ${VS_SHADER} PROPERTY VS_SHADER_TYPE Vertex)
set_property(SOURCE ${VS_SHADER} PROPERTY VS_SHADER_ENTRYPOINT "VS_Main")
set_property(SOURCE ${VS_SHADER} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "$(OutDir)/data/%(Filename).h")
set_property(SOURCE ${VS_SHADER} PROPERTY VS_SHADER_OBJECT_FILE_NAME "")

# Set the PS shader properties
set_property(SOURCE ${PS_SHADER} PROPERTY VS_SHADER_TYPE Pixel)
set_property(SOURCE ${PS_SHADER} PROPERTY VS_SHADER_ENTRYPOINT "PS_Main")
set_property(SOURCE ${PS_SHADER} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "$(OutDir)/data/%(Filename).h")
set_property(SOURCE ${PS_SHADER} PROPERTY VS_SHADER_OBJECT_FILE_NAME "")


