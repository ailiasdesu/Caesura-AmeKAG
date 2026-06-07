# bgfx - cross-platform rendering library (depends on bx, bimg)
cmake_minimum_required(VERSION 3.25)
project(bgfx_lib C CXX)

set(BGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE BGFX_SOURCES ${BGFX_DIR}/src/*.cpp)
# Remove amalgamated.cpp - we compile individual sources
list(REMOVE_ITEM BGFX_SOURCES "${BGFX_DIR}/src/amalgamated.cpp")

file(GLOB_RECURSE BGFX_HEADERS ${BGFX_DIR}/include/bgfx/*.h ${BGFX_DIR}/include/bgfx/platform.h)

add_library(bgfx STATIC ${BGFX_SOURCES} ${BGFX_HEADERS})
target_include_directories(bgfx PUBLIC 
    ${BGFX_DIR}/include 
    ${BGFX_DIR}/3rdparty
    ${BGFX_DIR}/3rdparty/directx-headers/include/directx
    ${BGFX_DIR}/../bx/include
    ${BGFX_DIR}/../bimg/include
)
target_link_libraries(bgfx PUBLIC bx bimg)

target_compile_definitions(bgfx PRIVATE 
    __STDC_LIMITS_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS
    BX_CONFIG_DEBUG=0 BGFX_CONFIG_MULTITHREADED=0
)

if(WIN32)
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_DIRECT3D11=1 BGFX_CONFIG_RENDERER_DIRECT3D12=0 BGFX_CONFIG_RENDERER_OPENGL=0)
elseif(APPLE)
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_METAL=1)
endif()

target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_VULKAN=0)

if(MSVC)
    target_compile_options(bgfx PRIVATE /W0 /wd4201 /wd4324 /wd4127 /Zc:__cplusplus /Zc:preprocessor /EHa)
    target_compile_definitions(bgfx PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(bgfx PRIVATE -w)
endif()
