cmake_minimum_required(VERSION 3.25)
project(CaesuraAmeKAG VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force modern CMake policies (FreeType sets 3.0...3.5 which can cause issues)
cmake_policy(SET CMP0053 NEW)  # Variable expansion in strings
cmake_policy(SET CMP0054 NEW)  # if() argument handling
cmake_policy(SET CMP0057 NEW)  # if() IN_LIST operator
cmake_policy(SET CMP0063 NEW)  # Honor visibility properties
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
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

# -- FFmpeg (hardware-accelerated video decoding, ON by default) -----------
option(CAESURA_ENABLE_FFMPEG "Enable FFmpeg video decoding" ON)
if(CAESURA_ENABLE_FFMPEG)
    # Method 1: bundled in external/ffmpeg (Windows prebuilt)
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/ffmpeg/include")
        set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/external/ffmpeg/include")
        set(FFMPEG_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/external/ffmpeg/lib")
        find_library(FFMPEG_avcodec_LIBRARY  avcodec  PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
        find_library(FFMPEG_avformat_LIBRARY avformat PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
        find_library(FFMPEG_swscale_LIBRARY  swscale  PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
        find_library(FFMPEG_avutil_LIBRARY   avutil   PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
        if(FFMPEG_avcodec_LIBRARY AND FFMPEG_avformat_LIBRARY AND FFMPEG_swscale_LIBRARY AND FFMPEG_avutil_LIBRARY)
            set(FFMPEG_FOUND TRUE)
            set(FFMPEG_LIBRARIES ${FFMPEG_avcodec_LIBRARY} ${FFMPEG_avformat_LIBRARY} ${FFMPEG_swscale_LIBRARY} ${FFMPEG_avutil_LIBRARY})
            message(STATUS "FFmpeg: bundled (external/ffmpeg)")
        endif()
    endif()

    # Method 2: vcpkg
    if(NOT FFMPEG_FOUND AND DEFINED VCPKG_INSTALLED_DIR)
        find_path(FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h PATHS ${VCPKG_INSTALLED_DIR}/include)
        find_library(FFMPEG_avcodec_LIBRARY  avcodec  PATHS ${VCPKG_INSTALLED_DIR}/lib)
        find_library(FFMPEG_avformat_LIBRARY avformat PATHS ${VCPKG_INSTALLED_DIR}/lib)
        find_library(FFMPEG_swscale_LIBRARY  swscale  PATHS ${VCPKG_INSTALLED_DIR}/lib)
        find_library(FFMPEG_avutil_LIBRARY   avutil   PATHS ${VCPKG_INSTALLED_DIR}/lib)
        if(FFMPEG_avcodec_LIBRARY AND FFMPEG_avformat_LIBRARY AND FFMPEG_swscale_LIBRARY AND FFMPEG_avutil_LIBRARY)
            set(FFMPEG_FOUND TRUE)
            set(FFMPEG_LIBRARIES ${FFMPEG_avcodec_LIBRARY} ${FFMPEG_avformat_LIBRARY} ${FFMPEG_swscale_LIBRARY} ${FFMPEG_avutil_LIBRARY})
            message(STATUS "FFmpeg: vcpkg")
        endif()
    endif()

    # Method 3: pkg-config (Linux/macOS)
    if(NOT FFMPEG_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(FFMPEG QUIET libavcodec libavformat libswscale libavutil)
        endif()
    endif()

    if(FFMPEG_FOUND)
        set(CAESURA_FFMPEG_FOUND TRUE)
        set(CAESURA_FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
        set(CAESURA_FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES})
        message(STATUS "FFmpeg video decoding: ENABLED")
    else()
        message(STATUS "FFmpeg not found -- pl_mpeg fallback only")
        message(STATUS "  To enable: place FFmpeg dev build in external/ffmpeg/")
        message(STATUS "  Download: https://github.com/BtbN/FFmpeg-Builds/releases")
    endif()
endif()

# SoLoud backend config -- disable SDL2 (we use SDL3), enable WINMM+WASAPI on Windows
set(SOLOUD_BACKEND_SDL2 OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(SOLOUD_BACKEND_WINMM ON CACHE BOOL "" FORCE)
    set(SOLOUD_BACKEND_WASAPI ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx/bx   ${CMAKE_BINARY_DIR}/bx)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx/bimg ${CMAKE_BINARY_DIR}/bimg)
# bimg uses SSE4.1 intrinsics ( _mm_blendv_ps ) 鈥?enable on x86 Linux
if(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  target_compile_options(bimg PRIVATE -msse4.1)
endif()
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
# LPeg is pure-Lua (scripts/lpeg.lua) 锟斤拷??no C build needed

# Engine sources

set(ENGINE_SOURCES
    src/main.cpp
    src/entry/Engine.cpp
    src/job/JobSystem.cpp
    src/input/InputRouter.cpp
    src/di/BackendRegistry.cpp
    src/Debug/DebugManager.cpp
    src/di/TextureBudget.cpp
    src/di/SandboxQuota.cpp
    src/rpc/RpcServer.h
    src/rpc/EditorServer.h
    src/rpc/RpcServer.cpp
    src/rpc/EditorServer.cpp
    src/entry/ErrorUI.cpp
    src/platform/SDL3PlatformBackend.cpp
    src/Resource/XP3Archive.cpp
    src/Render/BgfxRenderDevice.cpp
    src/Render/BgfxDeviceCore.cpp
    src/Render/BgfxShaderManager.cpp
    src/Render/BgfxDebugCallback.cpp
    src/Render/RTTManager.cpp
    src/Render/EmbeddedShaders.cpp
    src/Render/EmbeddedShaders_MiniGame.cpp
    src/Render/EmbeddedShaders_MiniGame_GLSL.cpp
    src/Render/EmbeddedShaders_MiniGame_Metal.cpp
    src/Render/TextRenderer.cpp
    src/Render/ParticleSystem.cpp
    src/Render/GpuMonitor.cpp
    src/Render/VideoPlayer.cpp
    src/Render/TextureManager.cpp
    src/Render/FreeTypeContext.cpp
    src/Render/LayerManager.cpp
    src/MiniGame/NullMiniGameBackend.cpp
    src/MiniGame/BgfxMiniGameBackend.cpp
    src/MiniGame/MiniGeometry.cpp
    src/MiniGame/MiniCollision.cpp
    src/Live2D/NullAnimationBackend.cpp
    src/Render/ShaderCache.cpp
    src/Audio/SoLoudAudioEngine.cpp
    src/Audio/NullAudioBackend.cpp
    src/script/vm/LuaManager.cpp
    src/script/bindings/KAGBinding.cpp
    src/script/state/GameState.cpp
    src/script/bindings/RenderBinding.cpp
    src/script/bindings/VFXBinding.cpp
    src/script/bindings/DevCoreBinding.cpp
    src/script/bindings/DebugBinding.cpp
    src/script/bindings/UnifiedBinding.cpp
    src/script/bindings/SteamBinding.cpp
    src/storage/SaveManager.cpp
    src/storage/ISaveProvider.cpp
    src/storage/SaveBinding.cpp
    src/archive/CryptoEngine.cpp
    src/archive/CARCReader.cpp
    src/archive/CARCWriter.cpp
    src/archive/CarcAssetProvider.cpp
    src/Resource/DirAssetProvider.cpp
    src/Resource/ProviderChain.cpp
    src/Resource/AssetManager.cpp
    src/Resource/ImageDecoder.cpp
    src/Resource/AsyncLoader.cpp
    # Phase 8: Dev Tools
    src/Debug/HotReload.cpp
    src/Debug/DebugProtocol.cpp
    # Phase 9: Security & Platform
    src/archive/CRLManager.cpp
    src/archive/DeltaCARC.cpp
    src/platform/MobileAdapter.cpp
)

set(ENGINE_HEADERS
    src/entry/Engine.h
    src/entry/EngineConfig.h
    src/job/JobSystem.h
    src/input/InputRouter.h
    src/Audio/api/IAudioBackend.h
    src/platform/api/IPlatformBackend.h
    src/platform/SDL3PlatformBackend.h
    src/Resource/XP3Archive.h
    src/di/BackendRegistry.h
    src/di/SandboxQuota.h
    src/di/thread/ThreadAssert.h
    src/Resource/ResourceHandle.h
    src/Debug/DebugManager.h
    src/di/TextureBudget.h
    src/entry/ErrorUI.h
    src/Render/IRenderDevice.h
    src/Render/BgfxRenderDevice.h
    src/Render/BgfxShaderManager.h
    src/Render/BgfxDeviceCore.h
    src/Render/BgfxDebugCallback.h
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
    src/MiniGame/BgfxMiniGameBackend.h
    src/MiniGame/MiniMaterial.h
    src/MiniGame/MiniLight.h
    src/MiniGame/MiniGeometry.h
    src/MiniGame/MiniCollision.h
    src/Live2D/IAnimationBackend.h
    src/Live2D/NullAnimationBackend.h
    src/Render/ShaderCache.h
    src/Audio/SoLoudAudioEngine.h
    src/Audio/NullAudioBackend.h
    src/script/vm/LuaManager.h
    src/script/bindings/KAGBinding.h
    src/script/state/GameState.h
    src/script/bindings/RenderBinding.h
    src/script/bindings/VFXBinding.h
    src/script/bindings/DevCoreBinding.h
    src/script/bindings/DebugBinding.h
    src/script/bindings/UnifiedBinding.h
    src/script/bindings/SteamBinding.h
    src/storage/SaveManager.h
    src/storage/ISaveProvider.h
    src/storage/SaveBinding.h
    src/archive/CARCFormat.h
    src/archive/CryptoEngine.h
    src/archive/CARCReader.h
    src/archive/CARCWriter.h
    src/archive/CarcAssetProvider.h
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
    src/archive/CRLManager.h
    src/archive/DeltaCARC.h
    src/platform/MobileAdapter.h
)

# Executable
add_executable(${PROJECT_NAME} ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# -- pl_mpeg (header-only MPEG-1 decoder, zero-dep fallback) ---------------
target_include_directories(${PROJECT_NAME} PRIVATE external/pl_mpeg)

# -- FFmpeg (apply if found above) -----------------------------------------
if(CAESURA_FFMPEG_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_VIDEO_FFMPEG)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CAESURA_FFMPEG_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CAESURA_FFMPEG_LIBRARIES})
    # Copy FFmpeg DLLs to output at configure time (avoids MSBuild Unicode path issues)
    if(WIN32)
        file(COPY ${FFMPEG_LIBRARY_DIR}/ DESTINATION ${CMAKE_BINARY_DIR}/Release
            FILES_MATCHING PATTERN "av*.dll" PATTERN "sw*.dll")
    endif()
endif()


target_include_directories(${PROJECT_NAME} PRIVATE 
    src 
    src/Platform
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/soloud/include 
    ${CMAKE_SOURCE_DIR}/external/bgfx/bx/include 
    external/cpp-httplib
    external/json
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

target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>$<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0>)

# Copy SDL3 DLL to output directory for runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL3.dll"
    COMMENT "Copying SDL3.dll to output"
)



if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4100 /wd4189 /wd4244 /Zc:__cplusplus /Zc:preprocessor /utf-8 /FS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN __STDC_LIMITS_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm ws2_32 psapi ole32 d3d11 dxgi d3dcompiler)
    # BCrypt is built into Windows; no extra OpenSSL needed
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(QUARTZCORE QuartzCore REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${METAL_LIBRARY} ${QUARTZCORE} ${FOUNDATION_LIBRARY} pthread OpenSSL::SSL OpenSSL::Crypto)
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

# -- Steam (optional)
if(CAESURA_HAS_STEAM)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_HAS_STEAM)
    target_include_directories(${PROJECT_NAME} PRIVATE ${STEAM_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${STEAM_LIBRARY})
endif()

# -- Live2D Cubism animation (optional, proprietary SDK) ---------------------
option(CAESURA_LIVE2D "Enable Live2D Cubism animation (requires manual SDK download)" OFF)

if(CAESURA_LIVE2D)
    set(CUBISM_SDK_ROOT "${CMAKE_SOURCE_DIR}/CubismSdkForNative-5-r.5" CACHE PATH "Path to Live2D Cubism SDK root")
    set(GLEW_ROOT "C:/glew/glew-2.2.0")

    if(NOT EXISTS "${CUBISM_SDK_ROOT}/Core/include/Live2DCubismCore.h")
        message(WARNING "Cubism SDK not found at ${CUBISM_SDK_ROOT}. Download from live2d.com")
    else()
        message(STATUS "Live2D SDK + GLEW found")

        set(CUBISM_CORE_DIR "${CUBISM_SDK_ROOT}/Core")
        set(CUBISM_FRAMEWORK_DIR "${CUBISM_SDK_ROOT}/Framework")

        # --- Cubism Core (prebuilt) ---
        add_library(Live2DCubismCore STATIC IMPORTED)
        if(WIN32)
            set_target_properties(Live2DCubismCore PROPERTIES
                IMPORTED_LOCATION "${CUBISM_CORE_DIR}/lib/windows/x86_64/143/Live2DCubismCore_MT.lib")
        elseif(UNIX)
            set_target_properties(Live2DCubismCore PROPERTIES
                IMPORTED_LOCATION "${CUBISM_CORE_DIR}/lib/linux/x86_64/libLive2DCubismCore.a")
        endif()

        # --- Cubism Framework + OpenGL ES2 backend ---
        file(GLOB_RECURSE CUBISM_FRAMEWORK_SRCS
            "${CUBISM_FRAMEWORK_DIR}/src/CubismCdiJson.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/CubismDefaultParameterId.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/CubismFramework.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/CubismModelSettingJson.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Effect/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Id/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Math/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Model/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Motion/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Physics/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/csmBlendMode.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/CubismRenderer.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/OpenGL/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/D3D11/*.cpp"  # Windows only (D3D11)
            "${CUBISM_FRAMEWORK_DIR}/src/Type/*.cpp"
            "${CUBISM_FRAMEWORK_DIR}/src/Utils/*.cpp"
        )

        add_library(CubismFramework STATIC ${CUBISM_FRAMEWORK_SRCS})
        target_include_directories(CubismFramework PUBLIC
            "${CUBISM_FRAMEWORK_DIR}/src"
            "${CUBISM_CORE_DIR}/include"
        )
        target_include_directories(CubismFramework PRIVATE
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/OpenGL"
            "${CUBISM_FRAMEWORK_DIR}/src/Rendering/D3D11"  # Windows only
        )
        if(WIN32)
            target_include_directories(CubismFramework PRIVATE "${GLEW_ROOT}/include")
            target_link_libraries(CubismFramework PRIVATE
                debug "${GLEW_ROOT}/lib/Release/x64/glew32s.lib"
                optimized "${GLEW_ROOT}/lib/Release/x64/glew32s.lib")
        endif()
        target_compile_definitions(CubismFramework PRIVATE
            CSM_TARGET_WIN_GL
            _CRT_SECURE_NO_WARNINGS
            GLEW_STATIC
        )

        # --- Engine Live2D backend ---
        target_sources(${PROJECT_NAME} PRIVATE
            src/Live2D/Live2D/Live2DBackend.h
            src/Live2D/Live2D/Live2DBackend.cpp
            src/Live2D/Live2D/ILive2DRenderPath.h
            src/Live2D/Live2D/OpenGLReadbackRenderPath.h
            src/Live2D/Live2D/OpenGLReadbackRenderPath.cpp
            src/Live2D/Live2D/D3D11NativeRenderPath.h
            src/Live2D/Live2D/D3D11NativeRenderPath.cpp
            src/Live2D/Live2D/OpenGLSharedRenderPath.h
            src/Live2D/Live2D/OpenGLSharedRenderPath.cpp
            src/Live2D/Live2D/MetalNativeRenderPath.h
            src/Live2D/Live2D/MetalNativeRenderPath.cpp
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE CubismFramework Live2DCubismCore)
        target_include_directories(${PROJECT_NAME} PRIVATE
            "${CUBISM_CORE_DIR}/include"
            "${CUBISM_FRAMEWORK_DIR}/src"
        )
        if(WIN32)
            target_include_directories(${PROJECT_NAME} PRIVATE "${GLEW_ROOT}/include")
            target_link_libraries(${PROJECT_NAME} PRIVATE
                debug "${GLEW_ROOT}/lib/Release/x64/glew32s.lib"
                optimized "${GLEW_ROOT}/lib/Release/x64/glew32s.lib")
        endif()
        target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_HAS_LIVE2D)
    endif()
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
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-Platform Visual Novel Engine 锟斤拷??SDL3 + bgfx + SoLoud + Lua 5.4")
set(CPACK_GENERATOR "ZIP")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

# Include SDL3 DLL in package
install(FILES "$<TARGET_FILE:SDL3::SDL3>" DESTINATION . OPTIONAL)

include(CPack)

# -- Tools ----------------------------------------------------------------
add_subdirectory(tools/carc_pack)

# -- Unit Tests
add_subdirectory(tests)
