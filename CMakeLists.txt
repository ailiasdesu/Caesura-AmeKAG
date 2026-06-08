cmake_minimum_required(VERSION 3.25)
project(CaesuraAmeKAG VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# -- FreeType 2 (font rendering, spec [2.4]) ---------------------------------
# Disable optional deps: bzip2, PNG, HarfBuzz, Brotli
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/freetype ${CMAKE_BINARY_DIR}/freetype)

# -- zstd (CARC decompression, spec [9.4]) -----------------------------------
# Static lib only, no programs/tests
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/zstd/build/cmake ${CMAKE_BINARY_DIR}/zstd)

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


# -- ed25519 (public-domain Ed25519 implementation) -------------------------
set(ED25519_SOURCES
    external/ed25519/fe.c
    external/ed25519/ge.c
    external/ed25519/keypair.c
    external/ed25519/sc.c
    external/ed25519/sha512.c
    external/ed25519/sign.c
    external/ed25519/verify.c
    external/ed25519/seed.c
    external/ed25519/add_scalar.c
)
add_library(ed25519 STATIC ${ED25519_SOURCES})
target_include_directories(ed25519 PUBLIC external/ed25519)
target_compile_definitions(ed25519 PRIVATE _CRT_SECURE_NO_WARNINGS)
# LPeg is pure-Lua (scripts/lpeg.lua) ¡ª no C build needed

# Engine sources

set(ENGINE_SOURCES
    src/main.cpp
    src/Core/Engine.cpp
    src/Core/JobSystem.cpp
    src/Core/InputRouter.cpp
    src/Core/BackendRegistry.cpp
    src/Core/DebugManager.cpp
    src/Core/TextureBudget.cpp
    src/Core/SandboxQuota.cpp
    src/Core/RpcServer.cpp
    src/Editor/EditorServer.cpp
    src/Core/ErrorUI.cpp
    src/Core/SDL3PlatformBackend.cpp
    src/Resource/XP3Archive.cpp
    src/Render/BgfxRenderDevice.cpp
    src/Render/RTTManager.cpp
    src/Render/EmbeddedShaders.cpp
    src/Render/TextRenderer.cpp
    src/Render/ParticleSystem.cpp
    src/Render/GpuMonitor.cpp
    src/Render/VideoPlayer.cpp
    src/Render/TextureManager.cpp
    src/Render/FreeTypeContext.cpp
    src/Render/LayerManager.cpp
    src/MiniGame/NullMiniGameBackend.cpp
    src/Render/ShaderCache.cpp
    src/Audio/SoLoudAudioEngine.cpp
    src/Audio/NullAudioBackend.cpp
    src/Scripting/LuaManager.cpp
    src/Scripting/KAGBinding.cpp
    src/Scripting/GameState.cpp
    src/Scripting/RenderBinding.cpp
    src/Scripting/VFXBinding.cpp
    src/Scripting/DevCoreBinding.cpp
    src/Scripting/DebugBinding.cpp
    src/Scripting/UnifiedBinding.cpp
    src/System/SaveManager.cpp
    src/System/SaveBinding.cpp
    src/Carc/CryptoEngine.cpp
    src/Carc/CARCReader.cpp
    src/Carc/CARCWriter.cpp
    src/Carc/CarcAssetProvider.cpp
    src/Resource/DirAssetProvider.cpp
    src/Resource/ProviderChain.cpp
    src/Resource/AssetManager.cpp
    src/Resource/ImageDecoder.cpp
    src/Resource/AsyncLoader.cpp
    # Phase 8: Dev Tools
    src/Debug/HotReload.cpp
    src/Debug/DebugProtocol.cpp
    # Phase 9: Security & Platform
    src/Carc/CRLManager.cpp
    src/Platform/MobileAdapter.cpp
)

set(ENGINE_HEADERS
    src/Core/Engine.h
    src/Core/JobSystem.h
    src/Core/InputRouter.h
    src/Core/IAudioBackend.h
    src/Core/IPlatformBackend.h
    src/Core/SDL3PlatformBackend.h
    src/Resource/XP3Archive.h
    src/Core/BackendRegistry.h
    src/Resource/ResourceHandle.h
    src/Core/DebugManager.h
    src/Core/TextureBudget.h
    src/Core/ErrorUI.h
    src/Render/IRenderDevice.h
    src/Render/BgfxRenderDevice.h
    src/Render/RTTManager.h
    src/Render/EmbeddedShaders.h
    src/Render/TextRenderer.h
    src/Render/ParticleSystem.h
    src/Render/GpuMonitor.h
    src/Render/VideoPlayer.h
    src/Render/TextureManager.h
    src/Render/LayerManager.h
    src/MiniGame/IMiniGameBackend.h
    src/MiniGame/NullMiniGameBackend.h
    src/Render/ShaderCache.h
    src/Audio/SoLoudAudioEngine.h
    src/Audio/NullAudioBackend.h
    src/Scripting/LuaManager.h
    src/Scripting/KAGBinding.h
    src/Scripting/GameState.h
    src/Scripting/RenderBinding.h
    src/Scripting/VFXBinding.h
    src/Scripting/DevCoreBinding.h
    src/Scripting/DebugBinding.h
    src/Scripting/UnifiedBinding.h
    src/System/SaveManager.h
    src/System/SaveBinding.h
    src/Carc/CARCFormat.h
    src/Carc/CryptoEngine.h
    src/Carc/CARCReader.h
    src/Carc/CARCWriter.h
    src/Carc/CarcAssetProvider.h
    src/Resource/IAssetProvider.h
    src/Resource/DirAssetProvider.h
    src/Resource/ProviderChain.h
    src/Resource/AssetManager.h
    src/Resource/ImageDecoder.h
    src/Resource/AsyncLoader.h
    # Phase 8: Dev Tools
    src/Debug/HotReload.h
    src/Debug/DebugProtocol.h
    # Phase 9: Security & Platform
    src/Carc/CRLManager.h
    src/Platform/MobileAdapter.h
)

# Executable
add_executable(${PROJECT_NAME} ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE 
    src 
    src/Platform
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/soloud/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bx/include 
    external/cpp-httplib
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/3rdparty
    # stb single-headers
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/stb
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/3rdparty/stb
    # pl_mpeg single-header
    ${CMAKE_SOURCE_DIR}/external/pl_mpeg
    # FreeType + zstd (external/)
    ${CMAKE_SOURCE_DIR}/external/freetype/include
    ${CMAKE_SOURCE_DIR}/external/zstd/lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 bgfx bimg bx soloud lua miniz ed25519 freetype libzstd_static)

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
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4100 /wd4189 /wd4244 /Zc:__cplusplus /Zc:preprocessor /utf-8 /FS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>$<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0> __STDC_LIMITS_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm ws2_32 psapi ole32 d3d11 dxgi d3dcompiler)
    # BCrypt is built into Windows; no extra OpenSSL needed
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(QUARTZCORE QuartzCore REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${METAL_LIBRARY} ${QUARTZCORE} pthread OpenSSL::SSL OpenSSL::Crypto)
elseif(UNIX)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads dl X11::X11 OpenSSL::SSL OpenSSL::Crypto)
endif()


# Copy scripts to each config output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
    COMMENT "Copying scripts to output directory..."
)

# -- Debug / Release configuration ------------------------------------------
# CAESURA_DEBUG: defaults ON for Debug builds, OFF for Release/MinSizeRel
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    option(CAESURA_DEBUG "Enable debug logging and assertions" ON)
else()
    option(CAESURA_DEBUG "Enable debug logging and assertions" OFF)
endif()
if(CAESURA_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_DEBUG)
endif()

# -- FFmpeg video decoding (optional) ----------------------------------------
option(CAESURA_VIDEO_FFMPEG "Enable FFmpeg video decoding" OFF)

if(CAESURA_VIDEO_FFMPEG)
    find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat swscale avutil)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        FFmpeg::avcodec FFmpeg::avformat FFmpeg::swscale FFmpeg::avutil)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_VIDEO_FFMPEG)
endif()
message(STATUS "Caesura (AmeKAG) configured successfully.")



# -- CPack Release packaging ------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY scripts/ DESTINATION scripts)
install(DIRECTORY assets/ DESTINATION assets)
install(DIRECTORY shaders/ DESTINATION shaders OPTIONAL)
install(FILES README.md LICENSE DESTINATION . OPTIONAL)

set(CPACK_PACKAGE_NAME "CaesuraAmeKAG")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "AiliasDesu")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-Platform Visual Novel Engine ¡ª SDL3 + bgfx + SoLoud + Lua 5.4")
set(CPACK_GENERATOR "ZIP")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

# Include SDL3 DLL in package
install(FILES "${SDL3_DIR}/../lib/${CMAKE_VS_PLATFORM_NAME}/SDL3.dll" DESTINATION . OPTIONAL)

include(CPack)

# -- Tools ----------------------------------------------------------------
add_subdirectory(tools/carc_pack)

# -- Unit Tests
add_subdirectory(tests)

