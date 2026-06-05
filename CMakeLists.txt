cmake_minimum_required(VERSION 3.25)
project(CaesuraAmeKAG VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(SDL3 REQUIRED)

# SoLoud backend config -- disable SDL2 (we use SDL3), enable WINMM+WASAPI on Windows
set(SOLOUD_BACKEND_SDL2 OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(SOLOUD_BACKEND_WINMM ON CACHE BOOL "" FORCE)
    set(SOLOUD_BACKEND_WASAPI ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx/bx   ${CMAKE_BINARY_DIR}/bx)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx/bimg ${CMAKE_BINARY_DIR}/bimg)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx/bgfx ${CMAKE_BINARY_DIR}/bgfx_lib)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/soloud/contrib ${CMAKE_BINARY_DIR}/soloud)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/lua ${CMAKE_BINARY_DIR}/lua)

# -- Miniz (zlib replacement) single-source library -------------------------
add_library(miniz STATIC
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/tinyexr/deps/miniz/miniz.c
)
target_include_directories(miniz PUBLIC
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/tinyexr/deps/miniz
)
target_compile_definitions(miniz PRIVATE _CRT_SECURE_NO_WARNINGS)

# Engine sources
set(ENGINE_SOURCES
    src/main.cpp
    src/Core/Engine.cpp
    src/Core/InputRouter.cpp
    src/Core/BackendRegistry.cpp
    src/Core/DebugManager.cpp
    src/Core/SDL3PlatformBackend.cpp
    src/Core/XP3Archive.cpp
    src/Render/BgfxRenderDevice.cpp
    src/Render/RTTManager.cpp
    src/Render/EmbeddedShaders.cpp
    src/Render/TextRenderer.cpp
    src/Render/ParticleSystem.cpp
    src/Audio/SoLoudAudioEngine.cpp
    src/Audio/NullAudioBackend.cpp
    src/Scripting/LuaManager.cpp
    src/Scripting/KAGBinding.cpp
    src/Scripting/RenderBinding.cpp
    src/Scripting/DevCoreBinding.cpp
    src/Scripting/DebugBinding.cpp
    src/Scripting/UnifiedBinding.cpp
)

set(ENGINE_HEADERS
    src/Core/Engine.h
    src/Core/InputRouter.h
    src/Core/IAudioBackend.h
    src/Core/IPlatformBackend.h
    src/Core/SDL3PlatformBackend.h
    src/Core/XP3Archive.h
    src/Core/BackendRegistry.h
    src/Core/DebugManager.h
    src/Render/IRenderDevice.h
    src/Render/BgfxRenderDevice.h
    src/Render/RTTManager.h
    src/Render/EmbeddedShaders.h
    src/Render/TextRenderer.h
    src/Render/ParticleSystem.h
    src/Audio/SoLoudAudioEngine.h
    src/Audio/NullAudioBackend.h
    src/Scripting/LuaManager.h
    src/Scripting/KAGBinding.h
    src/Scripting/RenderBinding.h
    src/Scripting/DevCoreBinding.h
    src/Scripting/DebugBinding.h
    src/Scripting/UnifiedBinding.h
)

# Executable
add_executable(${PROJECT_NAME} ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE 
    src 
    ${CMAKE_SOURCE_DIR}/external/soloud/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bx/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/3rdparty
    # stb single-headers
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/stb
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/3rdparty/stb
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 bgfx bimg bx soloud lua miniz)

target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

# Copy SDL3 DLL to output directory for runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_DIR}/../lib/${CMAKE_VS_PLATFORM_NAME}/SDL3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL3.dll"
    COMMENT "Copying SDL3.dll to output"
)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4100 /wd4189 /Zc:__cplusplus /Zc:preprocessor /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>$<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0> __STDC_LIMITS_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm ws2_32 psapi ole32 d3d11 dxgi d3dcompiler)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(QUARTZCORE QuartzCore REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${METAL_LIBRARY} ${QUARTZCORE} pthread)
elseif(UNIX)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads dl X11::X11)
endif()

# Copy scripts to each config output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
    COMMENT "Copying scripts to output directory..."
)
message(STATUS "Caesura (AmeKAG) configured successfully.")