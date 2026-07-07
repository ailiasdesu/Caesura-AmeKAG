cmake_minimum_required(VERSION 3.25)
find_package(SDL3 REQUIRED)

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

set(TEST_SOURCES
    cpp/test_main.cpp
    cpp/test_core.cpp
    cpp/test_carc.cpp
    cpp/test_system.cpp
    cpp/test_source_encoding.cpp
    cpp/test_resource.cpp
    cpp/test_debug.cpp
    cpp/test_hotreload_integration.cpp
    cpp/test_demo_smoke.cpp
    cpp/test_demo_e2e.cpp
    cpp/test_input.cpp
    cpp/test_async.cpp
    cpp/test_null_jobsystem.cpp
    cpp/test_job_system.cpp
    cpp/test_image_decoder.cpp
    cpp/test_audio.cpp
    cpp/test_audio_integration.cpp
    cpp/test_lua_manager.cpp
    cpp/test_script_bindings.cpp
    cpp/test_script_boundary.cpp
    cpp/test_game_state.cpp
    cpp/test_texture_budget_edge.cpp
    cpp/test_resource_handle.cpp
    cpp/test_error_ui.cpp
    cpp/test_entry.cpp
    cpp/test_di.cpp
    cpp/test_minigame.cpp
    cpp/test_platform.cpp
    cpp/test_storage.cpp
    cpp/test_save_migration.cpp
    cpp/test_save_roundtrip.cpp
    cpp/test_mobile_adapter.cpp
    cpp/test_render_device.cpp
    cpp/test_render_pipeline.cpp
    cpp/test_render_integration.cpp
    cpp/test_rpc.cpp
    cpp/test_backend_registry.cpp
    cpp/test_layer_manager.cpp
    cpp/test_texture_manager.cpp
    cpp/test_kag_binding.cpp
    cpp/test_kag_integration.cpp
    cpp/test_kag_execution.cpp
    cpp/test_galgame_startup.cpp
    cpp/test_sandbox_quota.cpp
    cpp/test_particle_system.cpp
    cpp/test_engine_lifecycle.cpp
    cpp/test_mini_game.cpp
    cpp/test_steam.cpp
    cpp/test_live2d.cpp
    cpp/test_archive.cpp

    # Source files needed for linking
    ${CMAKE_SOURCE_DIR}/src/di/BackendRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/debug/DebugManager.cpp
    ${CMAKE_SOURCE_DIR}/src/di/TextureBudget.cpp
    ${CMAKE_SOURCE_DIR}/src/di/SandboxQuota.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc/RpcServer.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/ErrorUI.cpp
    ${CMAKE_SOURCE_DIR}/src/input/InputRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/Engine.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/Engine_Assets.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/Engine_Backends.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/Engine_Gpu.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/Engine_LuaRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/entry/StartupScripts.cpp
    ${CMAKE_SOURCE_DIR}/src/rpc/EditorServer.cpp
    ${CMAKE_SOURCE_DIR}/src/job/JobSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/SDL3PlatformBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CryptoEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CARCReader.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CARCWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CarcAssetProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CRLManager.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/SaveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/SaveBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/DirAssetProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/ProviderChain.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/AssetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/ImageDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/AsyncLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/XP3Archive.cpp
    ${CMAKE_SOURCE_DIR}/src/debug/HotReload.cpp
    ${CMAKE_SOURCE_DIR}/src/debug/DebugProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/script/vm/LuaManager.cpp
    ${CMAKE_SOURCE_DIR}/src/script/state/GameState.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/KAGBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/RenderBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/VFXBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/DevCoreBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/DebugBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/UnifiedBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/render/TextRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxRenderDevice.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxDeviceCore.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxDraw_Batch.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxDraw_Blit.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxDraw_Effects.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxQuadBatch.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxShaderManager.cpp
    ${CMAKE_SOURCE_DIR}/src/render/BgfxDebugCallback.cpp
    ${CMAKE_SOURCE_DIR}/src/render/EmbeddedShaders.cpp
    ${CMAKE_SOURCE_DIR}/src/render/ShaderCache.cpp
    ${CMAKE_SOURCE_DIR}/src/render/TextureManager.cpp
    ${CMAKE_SOURCE_DIR}/src/render/LayerManager.cpp
    ${CMAKE_SOURCE_DIR}/src/render/FreeTypeContext.cpp
    ${CMAKE_SOURCE_DIR}/src/render/ParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/render/RTTManager.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GpuMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/render/VideoPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/SoLoudAudioEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/NullAudioBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/MobileAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/minigame/NullMiniGameBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/minigame/BgfxMiniGameBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/minigame/MiniGeometry.cpp
    ${CMAKE_SOURCE_DIR}/src/minigame/MiniCollision.cpp
    ${CMAKE_SOURCE_DIR}/src/steam/SteamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/live2d/NullAnimationBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/script/bindings/SteamBinding.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/CloudSaveProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/render/EmbeddedShaders_MiniGame.cpp
    ${CMAKE_SOURCE_DIR}/src/render/EmbeddedShaders_MiniGame_GLSL.cpp
    ${CMAKE_SOURCE_DIR}/src/render/EmbeddedShaders_MiniGame_Metal.cpp
)

add_executable(CaesuraTests ${TEST_SOURCES})

if(WIN32)
    target_link_libraries(CaesuraTests PRIVATE bcrypt)
endif()

target_include_directories(CaesuraTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sync_caesura_test_assets_$<CONFIG>.cmake"
    CONTENT "
file(COPY [[${CMAKE_SOURCE_DIR}/scripts/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/scripts]])
file(COPY [[${CMAKE_SOURCE_DIR}/tests/audio/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/tests/audio]])
file(COPY [[${CMAKE_SOURCE_DIR}/demo/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/demo]])
file(COPY [[${CMAKE_SOURCE_DIR}/scripts/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraAmeKAG>/scripts]])
"
)

add_test(
    NAME CaesuraSyncTestAssets
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/sync_caesura_test_assets_$<CONFIG>.cmake"
)
set_tests_properties(CaesuraSyncTestAssets PROPERTIES
    FIXTURES_SETUP caesura-test-assets
    LABELS "setup;assets"
)

function(add_caesura_doctest name)
    set(options RUN_SERIAL)
    set(oneValueArgs RESOURCE_LOCK)
    set(multiValueArgs LABELS ARGS)
    cmake_parse_arguments(CAESURA_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_test(NAME ${name} COMMAND $<TARGET_FILE:CaesuraTests> ${CAESURA_TEST_ARGS})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraTests>"
        FIXTURES_REQUIRED caesura-test-assets
    )
    if(CAESURA_TEST_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${CAESURA_TEST_LABELS}")
    endif()
    if(CAESURA_TEST_RUN_SERIAL)
        set_tests_properties(${name} PROPERTIES RUN_SERIAL TRUE)
    endif()
    if(CAESURA_TEST_RESOURCE_LOCK)
        set_tests_properties(${name} PROPERTIES RESOURCE_LOCK "${CAESURA_TEST_RESOURCE_LOCK}")
    endif()
endfunction()

add_caesura_doctest(CaesuraUnitTests
    LABELS full regression
    RUN_SERIAL
    RESOURCE_LOCK global-singletons
)
add_caesura_doctest(CaesuraEntryTests
    LABELS entry lifecycle
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_entry.cpp,*test_engine_lifecycle.cpp,*test_galgame_startup.cpp
)
add_caesura_doctest(CaesuraStorageTests
    LABELS storage filesystem
    RESOURCE_LOCK save-files
    ARGS --source-file=*test_storage.cpp,*test_system.cpp,*test_save_migration.cpp,*test_save_roundtrip.cpp
)
add_caesura_doctest(CaesuraScriptTests
    LABELS script lua
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_lua_manager.cpp,*test_script_bindings.cpp,*test_script_boundary.cpp,*test_kag_binding.cpp,*test_kag_integration.cpp,*test_kag_execution.cpp,*test_game_state.cpp
)
add_caesura_doctest(CaesuraRenderResourceTests
    LABELS render resource
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_render_device.cpp,*test_render_pipeline.cpp,*test_render_integration.cpp,*test_texture_manager.cpp,*test_texture_budget_edge.cpp,*test_resource.cpp,*test_resource_handle.cpp,*test_async.cpp,*test_image_decoder.cpp,*test_layer_manager.cpp,*test_particle_system.cpp
)
add_caesura_doctest(CaesuraAudioTests
    LABELS audio
    RUN_SERIAL
    RESOURCE_LOCK audio-device
    ARGS --source-file=*test_audio.cpp,*test_audio_integration.cpp
)

add_test(
    NAME CaesuraHeadlessCliSmoke
    COMMAND $<TARGET_FILE:CaesuraAmeKAG> --headless
)
set_tests_properties(CaesuraHeadlessCliSmoke PROPERTIES
    LABELS "entry;headless;cli"
    RUN_SERIAL TRUE
    RESOURCE_LOCK "global-singletons"
    FIXTURES_REQUIRED caesura-test-assets
    PASS_REGULAR_EXPRESSION "Backends ready: render=NullRender audio=NullAudio platform=NullPlatform"
    FAIL_REGULAR_EXPRESSION "Backends ready: render=bgfx audio=soloud platform=sdl3|\\[SDL3\\] Platform backend initialized|\\[BgfxRenderDevice\\] nwh=|\\[Audio\\] SoLoud initialized"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraAmeKAG>"
)

# Copy scripts to test output directory so sandbox.lua et al are found
add_custom_command(TARGET CaesuraTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:CaesuraTests>/scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tests/audio"
    "$<TARGET_FILE_DIR:CaesuraTests>/tests/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/demo"
    "$<TARGET_FILE_DIR:CaesuraTests>/demo"
    COMMENT "Copying test scripts and audio files..."
)

# Copy FFmpeg DLLs to test output so VideoPlayer can load them
if(CAESURA_FFMPEG_FOUND AND WIN32)
    add_custom_command(TARGET CaesuraTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_BIN_DIR}"
        "$<TARGET_FILE_DIR:CaesuraTests>"
        COMMENT "Copying FFmpeg DLLs to test output directory..."
    )
endif()
