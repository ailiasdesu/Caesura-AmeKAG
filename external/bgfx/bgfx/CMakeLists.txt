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
    ${BGFX_DIR}/3rdparty/khronos
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
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_DIRECT3D11=1 BGFX_CONFIG_RENDERER_DIRECT3D12=0 BGFX_CONFIG_RENDERER_OPENGL=43)
elseif(APPLE)
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_METAL=1)
elseif(ANDROID)
    # Track M: without a renderer define bgfx defaults to NO backend on
    # Android (OpenGL init fails, renderer falls back to Noop). GLES 3.0
    # matches the device OpenGLES driver; context comes from SDL (see
    # SDL3PlatformBackend::createGLContext).
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_OPENGLES=30)
elseif(UNIX)
    # Sprint6-L1 (t92/round-4): same trap as the Android note above -- with no
    # renderer define bgfx compiles ZERO backends on desktop Linux, auto-select
    # scores everything 0 and Noop wins, so the packaged game never really
    # rendered (round-4 evidence: requested=Direct3D 11, actual=Noop). Desktop
    # GL 4.3 matches the embedded shader profile; mesa provides the context
    # via GLX under xvfb on CI. Placed after ANDROID because CMake sets UNIX=1
    # there too.
    target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_OPENGL=43)
    # Round-5: with the GL renderer live, renderer_gl.cpp instantiates bx
    # float4x4_inverse -> simd128_f32_dot, whose _mm_dp_ps builtin GCC gates
    # behind -msse4.1 (simd128_sse.inl:374). MSVC emits the intrinsic without
    # an /arch gate, so the Windows binary already assumes SSE4.1 -- this only
    # aligns Linux x86_64 with that de-facto baseline. Non-x86 keeps defaults
    # (bx selects the NEON path there).
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        target_compile_options(bgfx PRIVATE -msse4.1)
    endif()
endif()

target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_VULKAN=0)

if(MSVC)
    target_compile_options(bgfx PRIVATE /W0 /wd4201 /wd4324 /wd4127 /Zc:__cplusplus /Zc:preprocessor /EHa)
    target_compile_definitions(bgfx PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(bgfx PRIVATE -w)
endif()