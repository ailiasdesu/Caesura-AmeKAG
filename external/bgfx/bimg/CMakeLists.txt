# bimg - image library (depends on bx)
cmake_minimum_required(VERSION 3.25)
project(bimg C CXX)

set(BIMG_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE BIMG_SOURCES ${BIMG_DIR}/src/*.cpp)
file(GLOB_RECURSE BIMG_HEADERS ${BIMG_DIR}/include/bimg/*.h)

file(GLOB_RECURSE ASTCENC_SOURCES ${BIMG_DIR}/3rdparty/astc-encoder/source/*.cpp)
add_library(bimg STATIC ${BIMG_SOURCES} ${BIMG_HEADERS} ${ASTCENC_SOURCES})
target_include_directories(bimg PUBLIC 
    ${BIMG_DIR}/include 
    ${BIMG_DIR}/3rdparty
    ${BIMG_DIR}/3rdparty/astc-encoder/include
    ${BIMG_DIR}/3rdparty/iqa/include
    ${BIMG_DIR}/3rdparty/tinyexr/deps
    ${BIMG_DIR}/3rdparty/nvtt
    ${BIMG_DIR}/3rdparty/libsquish
)
target_link_libraries(bimg PUBLIC bx)
target_compile_definitions(bimg PRIVATE BX_CONFIG_DEBUG=0 ASTCENC_SSE=0 ASTCENC_NEON=0 ASTCENC_AVX=0)

if(MSVC)
    target_compile_options(bimg PRIVATE /W0 /wd4201 /Zc:__cplusplus /Zc:preprocessor /EHa)
else()
    target_compile_options(bimg PRIVATE -w)
endif()
