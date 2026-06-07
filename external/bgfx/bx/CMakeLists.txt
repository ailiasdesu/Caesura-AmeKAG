# bx - base library for bgfx (no dependencies)
cmake_minimum_required(VERSION 3.25)
project(bx C CXX)

set(BX_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE BX_SOURCES ${BX_DIR}/src/*.cpp)
list(REMOVE_ITEM BX_SOURCES "${BX_DIR}/src/amalgamated.cpp")

file(GLOB_RECURSE BX_HEADERS ${BX_DIR}/include/bx/*.h ${BX_DIR}/include/compat/*.h)

add_library(bx STATIC ${BX_SOURCES} ${BX_HEADERS})
target_include_directories(bx PUBLIC 
    ${BX_DIR}/include 
    ${BX_DIR}/3rdparty
    ${BX_DIR}/include/compat/msvc
)
target_compile_definitions(bx PRIVATE 
    __STDC_LIMITS_MACROS 
    __STDC_FORMAT_MACROS 
    __STDC_CONSTANT_MACROS
    BX_CONFIG_DEBUG=0
)

if(MSVC)
    target_compile_options(bx PRIVATE /W0 /wd4201 /wd4530 /Zc:__cplusplus /Zc:preprocessor)
else()
    target_compile_options(bx PRIVATE -w)
endif()
