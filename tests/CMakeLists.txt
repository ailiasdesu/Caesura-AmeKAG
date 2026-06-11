cmake_minimum_required(VERSION 3.25)
find_package(SDL3 REQUIRED)

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

set(TEST_SOURCES
    cpp/test_main.cpp
    cpp/test_core.cpp
    cpp/test_carc.cpp
    cpp/test_system.cpp
    cpp/test_resource.cpp
    cpp/test_debug.cpp
    cpp/test_input.cpp
    cpp/test_async.cpp
    cpp/test_job_system.cpp
    cpp/test_image_decoder.cpp
    cpp/test_audio.cpp
    cpp/test_lua_manager.cpp
    cpp/test_texture_budget_edge.cpp
    cpp/test_resource_handle.cpp
    cpp/test_error_ui.cpp
    cpp/test_save_migration.cpp
    cpp/test_mobile_adapter.cpp
    cpp/test_render.cpp
    cpp/test_kag_binding.cpp
    cpp/test_kag_integration.cpp
    cpp/test_sandbox_quota.cpp
    cpp/test_particle_system.cpp
    cpp/test_engine_lifecycle.cpp
    cpp/test_mini_game.cpp
    cpp/test_steam.cpp

    # Source files needed for linking
    ${CMAKE_SOURCE_DIR}/src/Core/BackendRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/Debug/DebugManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/TextureBudget.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/SandboxQuota.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/RpcServer.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/ErrorUI.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/InputRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/EditorServer.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/JobSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/SDL3PlatformBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/Carc/CryptoEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/Carc/CARCReader.cpp
    ${CMAKE_SOURCE_DIR}/src/Carc/CARCWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/Carc/CarcAssetProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/Carc/CRLManager.cpp
    ${CMAKE_SOURCE_DIR}/src/System/SaveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/System/SaveBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/DirAssetProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/ProviderChain.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/AssetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/ImageDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/AsyncLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/Resource/XP3Archive.cpp
    ${CMAKE_SOURCE_DIR}/src/Debug/HotReload.cpp
    ${CMAKE_SOURCE_DIR}/src/Debug/DebugProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/LuaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/GameState.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/KAGBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/RenderBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/VFXBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/DevCoreBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/DebugBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/UnifiedBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/TextRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/BgfxRenderDevice.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/BgfxShaderManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/BgfxDebugCallback.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/EmbeddedShaders.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/ShaderCache.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/TextureManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/LayerManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/FreeTypeContext.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/ParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/RTTManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/GpuMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/VideoPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/Audio/SoLoudAudioEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/Audio/NullAudioBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/MobileAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/MiniGame/NullMiniGameBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/MiniGame/BgfxMiniGameBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/MiniGame/MiniGeometry.cpp
    ${CMAKE_SOURCE_DIR}/src/MiniGame/MiniCollision.cpp
    ${CMAKE_SOURCE_DIR}/src/Steam/SteamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/Scripting/SteamBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/System/CloudSaveProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/EmbeddedShaders_MiniGame.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/EmbeddedShaders_MiniGame_GLSL.cpp
    ${CMAKE_SOURCE_DIR}/src/Render/EmbeddedShaders_MiniGame_Metal.cpp
)

add_executable(CaesuraTests ${TEST_SOURCES})

if(WIN32)
    target_link_libraries(CaesuraTests PRIVATE bcrypt)
endif()

target_include_directories(CaesuraTests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/tinyexr/deps/miniz
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Platform
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/soloud/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/bx/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/bimg/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/include
    ${CMAKE_SOURCE_DIR}/external/bgfx/bgfx/3rdparty
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/lua
    ${CMAKE_SOURCE_DIR}/external/ed25519
    ${CMAKE_SOURCE_DIR}/external/zstd/lib
    ${CMAKE_SOURCE_DIR}/external/freetype/include
)

target_link_libraries(CaesuraTests PRIVATE
    doctest lua ed25519 SDL3::SDL3 soloud bgfx bimg bx
    freetype libzstd_static
)

if(WIN32)
    target_link_libraries(CaesuraTests PRIVATE winmm ws2_32 psapi ole32 d3d11 dxgi d3dcompiler)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(QUARTZCORE QuartzCore REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(CaesuraTests PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${METAL_LIBRARY} ${QUARTZCORE} ${FOUNDATION_LIBRARY} pthread OpenSSL::SSL OpenSSL::Crypto)
elseif(UNIX)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(CaesuraTests PRIVATE Threads::Threads dl X11::X11 OpenSSL::SSL OpenSSL::Crypto)
endif()

target_compile_definitions(CaesuraTests PRIVATE
    _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
    __STDC_LIMITS_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS
    SDL_MAIN_HANDLED
    $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>$<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0>
)

if(MSVC)
    target_compile_options(CaesuraTests PRIVATE /W3 /wd4100 /wd4189 /wd4244 /Zc:__cplusplus /Zc:preprocessor /utf-8 /FS)
endif()

enable_testing()
add_test(NAME CaesuraUnitTests COMMAND CaesuraTests)

# Copy scripts to test output directory so sandbox.lua et al are found
add_custom_command(TARGET CaesuraTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:CaesuraTests>/scripts"
    COMMENT "Copying scripts to test output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tests/audio"
    "$<TARGET_FILE_DIR:CaesuraTests>/tests/audio"
    COMMENT "Copying test audio files..."
)
